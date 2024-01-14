


#include "shared-module/rambus/RAM.h"

#include "shared-bindings/displayio/OnDiskBitmap.h"
#include "shared-bindings/displayio/ColorConverter.h"
#include "shared-bindings/displayio/Palette.h"
#include "shared-bindings/displayio/RAMBusBitmap.h"
#include "shared-module/displayio/ColorConverter.h"
#include "shared-module/displayio/Palette.h"

#include <string.h>

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mpprint.h"
#include "py/gc.h"
#include "extmod/vfs_fat.h"


enum { ALIGN_BITS = 8 * sizeof(uint32_t) };

static int stride(uint32_t width, uint32_t bits_per_value) {
    uint32_t row_width = width * bits_per_value;
    // align to uint32_t
    return (row_width + ALIGN_BITS - 1) / ALIGN_BITS;
}

static uint32_t read_word(uint16_t *bmp_header, uint16_t index) {
    return bmp_header[index] | bmp_header[index + 1] << 16;
}

// TODO: constructor that doesn't require RAM instance and looks for a globally registered MemPool to alloc from?
// void common_hal_displayio_rambusbitmap_construct(displayio_rambusbitmap_t *self, uint32_t width,
//     uint32_t height, uint32_t bits_per_value, bool read_only) {
//     common_hal_displayio_rambusbitmap_construct_from_ram(self, width, height, bits_per_value, NULL, 0, read_only);
// }

void common_hal_displayio_rambusbitmap_construct(displayio_rambusbitmap_t *self, uint32_t width,
    uint32_t height, rambus_ram_obj_t *ram, addr_t addr, pyb_file_obj_t *file, uint32_t bits_per_pixel, bool read_only) {
    self->ram = ram;
    self->addr = addr;
    self->pxbuf = m_malloc(4);
    self->read_only = read_only;

    if (file != NULL) {
        mp_printf(&mp_plat_print, "Constructing RAMBusBitmap from file: %x\n", file);
        common_hal_displayio_rambusbitmap_load_from_file(self, file);
    } else {
        if (bits_per_pixel > 8 && bits_per_pixel != 16 && bits_per_pixel != 32) {
            mp_raise_NotImplementedError(MP_ERROR_TEXT("Invalid bits per value"));
        }
        
        self->width = width;
        self->height = height;
        self->stride = stride(width, bits_per_pixel);
        self->bits_per_pixel = bits_per_pixel;
        self->size = self->width * self->height * (self->bits_per_pixel / 8);
    }

    // Division and modulus can be slow because it has to handle any integer. We know bits_per_value
    // is a power of two. We divide and mod by bits_per_value to compute the offset into the byte
    // array. So, we can the offset computation to simplify to a shift for division and mask for mod.

    self->x_shift = 0; // Used to divide the index by the number of pixels per word. Its used in a
                       // shift which effectively divides by 2 ** x_shift.
    uint32_t power_of_two = 1;
    while (power_of_two < ALIGN_BITS / bits_per_pixel) {
        self->x_shift++;
        power_of_two <<= 1;
    }
    self->x_mask = (1u << self->x_shift) - 1u; // Used as a modulus on the x value
    self->bitmask = (1u << bits_per_pixel) - 1u;

    self->dirty_area.x1 = 0;
    self->dirty_area.x2 = width;
    self->dirty_area.y1 = 0;
    self->dirty_area.y2 = height;
}

void common_hal_displayio_rambusbitmap_load_from_file(displayio_rambusbitmap_t *self, pyb_file_obj_t *file) {
    // Load the wave
    uint16_t bmp_header[69];
    f_rewind(&file->fp);
    UINT bytes_read;
    if (f_read(&file->fp, bmp_header, 138, &bytes_read) != FR_OK) {
        mp_raise_OSError(MP_EIO);
    }
    if (bytes_read != 138 ||
        memcmp(bmp_header, "BM", 2) != 0) {
        mp_arg_error_invalid(MP_QSTR_file);
    }

    // We can't cast because we're not aligned.
    uint16_t data_offset = read_word(bmp_header, 5);

    uint32_t header_size = read_word(bmp_header, 7);
    uint16_t bits_per_pixel = bmp_header[14];
    uint32_t compression = read_word(bmp_header, 15);
    uint32_t number_of_colors = read_word(bmp_header, 23);

    // 0 is uncompressed; 3 is bitfield compressed. 1 and 2 are RLE compression.
    if (compression != 0 && compression != 3) {
        mp_raise_ValueError(MP_ERROR_TEXT("RLE-compressed BMP not supported"));
    }

    bool indexed = bits_per_pixel <= 8;
    bool bitfield_compressed = (compression == 3);
    self->bits_per_pixel = bits_per_pixel;
    self->width = read_word(bmp_header, 9);
    self->height = read_word(bmp_header, 11);

    displayio_colorconverter_t *colorconverter =
        mp_obj_malloc(displayio_colorconverter_t, &displayio_colorconverter_type);
    common_hal_displayio_colorconverter_construct(colorconverter, false, DISPLAYIO_COLORSPACE_RGB888);
    self->colorconverter = colorconverter;

    uint32_t r_bitmask;
    uint32_t g_bitmask;
    uint32_t b_bitmask;

    if (bits_per_pixel == 16) {
        if (((header_size >= 56)) || (bitfield_compressed)) {
            r_bitmask = read_word(bmp_header, 27);
            g_bitmask = read_word(bmp_header, 29);
            b_bitmask = read_word(bmp_header, 31);
        } else { // no compression or short header means 5:5:5
            r_bitmask = 0x7c00;
            g_bitmask = 0x3e0;
            b_bitmask = 0x1f;
        }
    } else if (indexed) {
        if (number_of_colors == 0) {
            number_of_colors = 1 << bits_per_pixel;
        }

        displayio_palette_t *palette = mp_obj_malloc(displayio_palette_t, &displayio_palette_type);
        common_hal_displayio_palette_construct(palette, number_of_colors, false);

        if (number_of_colors > 1) {
            uint16_t palette_size = number_of_colors * sizeof(uint32_t);
            uint16_t palette_offset = 0xe + header_size;

            uint32_t *palette_data = m_malloc(palette_size);

            f_rewind(&file->fp);
            f_lseek(&file->fp, palette_offset);

            UINT palette_bytes_read;
            if (f_read(&file->fp, palette_data, palette_size, &palette_bytes_read) != FR_OK) {
                mp_raise_OSError(MP_EIO);
            }
            if (palette_bytes_read != palette_size) {
                mp_raise_ValueError(MP_ERROR_TEXT("Unable to read color palette data\n"));
            }
            for (uint16_t i = 0; i < number_of_colors; i++) {
                common_hal_displayio_palette_set_color(palette, i, palette_data[i]);
            }
            m_free(palette_data);
        } else {
            common_hal_displayio_palette_set_color(palette, 0, 0x0);
            common_hal_displayio_palette_set_color(palette, 1, 0xffffff);
        }
        self->palette = palette;

    } else if (!(header_size == 12 || header_size == 40 || header_size == 108 || header_size == 124)) {
        mp_raise_ValueError_varg(MP_ERROR_TEXT("Only Windows format, uncompressed BMP supported: given header size is %d"), header_size);
    }

    if (bits_per_pixel == 8 && number_of_colors == 0) {
        mp_raise_ValueError_varg(MP_ERROR_TEXT("Only monochrome, indexed 4bpp or 8bpp, and 16bpp or greater BMPs supported: %d bpp given"), bits_per_pixel);
    }

    uint8_t bytes_per_pixel = (bits_per_pixel / 8)  ? (bits_per_pixel / 8) : 1;
    uint8_t pixels_per_byte = 8 / bits_per_pixel;
    if (pixels_per_byte == 0) {
        self->stride = (self->width * bytes_per_pixel);
        // Rows are word aligned.
        if (self->stride % 4 != 0) {
            self->stride += 4 - self->stride % 4;
        }
    } else {
        uint32_t bit_stride = self->width * bits_per_pixel;
        if (bit_stride % 32 != 0) {
            bit_stride += 32 - bit_stride % 32;
        }
        self->stride = (bit_stride / 8);
    }

    // Read bmp pixel data from disk into RAM
    uint32_t location = 0;
    // We don't cache here because the underlying FS caches sectors.
    f_lseek(&file->fp, data_offset);
    uint8_t *buf = m_malloc(2048);
    uint32_t pixel_data = 0;
    uint32_t result = 0;

    do {
        result = f_read(&file->fp, buf, 2048, &bytes_read);
        if (result == FR_OK) {
            // mp_printf(&mp_plat_print, "Read %d byte chunk, %d bytes total\n", bytes_read, location + bytes_read);

            if (indexed && pixels_per_byte == 1) {
                // If loading 8-bit color indices, can load chunks without processing
                shared_module_rambus_ram_write_seq(self->ram, self->addr + location, buf, 2048);
            } else {
                // TODO: keep RAM sequence open with hold while writing bmp
                for (uint32_t i = 0; i < bytes_read; i += bytes_per_pixel) {
                    // for (uint8_t pxb = 0; pxb < bytes_per_pixel; pxb++) {
                    //     // TODO: better way to convert to int size?
                    //     pixel_data = 0;
                    //     pixel_data |= buf[i + pxb] << (pxb * 8);
                    // }

                    if (bytes_per_pixel == 1) {
                        pixel_data = buf[i];
                        uint8_t offset = (i % pixels_per_byte) * self->bits_per_pixel;
                        uint8_t mask = (1 << self->bits_per_pixel) - 1;

                        shared_module_rambus_ram_write_byte(self->ram, self->addr + location, (self->pxbuf[0] >> ((8 - self->bits_per_pixel) - offset)) & mask);
                    } else if (bytes_per_pixel == 2) {
                        pixel_data = (buf[1] << 8) + (buf[0]);

                        if (g_bitmask == 0x07e0) { // 565
                            self->pxbuf[0] = ((pixel_data & r_bitmask) >> 11);
                            self->pxbuf[1] = ((pixel_data & g_bitmask) >> 5);
                            self->pxbuf[2] = ((pixel_data & b_bitmask) >> 0);
                        } else { // 555
                            self->pxbuf[0] = ((pixel_data & r_bitmask) >> 10);
                            self->pxbuf[1] = ((pixel_data & g_bitmask) >> 4);
                            self->pxbuf[2] = ((pixel_data & b_bitmask) >> 0);
                        }
                        pixel_data = (self->pxbuf[0] << 19 | self->pxbuf[1] << 10 | self->pxbuf[2] << 3);
                        self->pxbuf[0] = pixel_data >> 24 & 0xff;
                        self->pxbuf[1] = pixel_data >> 16 & 0xff;
                        shared_module_rambus_ram_write_seq(self->ram, self->addr + location, self->pxbuf, 2);
                    } else {
                        pixel_data = (buf[3] << 24) + (buf[2] << 16) + (buf[1] << 8) + (buf[0]);

                        self->pxbuf[0] = pixel_data >> 24 & 0xff;
                        self->pxbuf[1] = pixel_data >> 16 & 0xff;
                        self->pxbuf[2] = pixel_data >> 8 & 0xff;
                        self->pxbuf[3] = bitfield_compressed ? 0xff : pixel_data & 0xff;
                        shared_module_rambus_ram_write_seq(self->ram, self->addr + location, self->pxbuf, 4);
                    }
                }
            }

            location += bytes_read;
        } else {
            // raise file error?
            mp_printf(&mp_plat_print, "Got non-success result code from file read: %d\n", result);
            break;
        }
    } while (bytes_read > 0);

    self->size = location;
    
    self->dirty_area.x1 = 0;
    self->dirty_area.x2 = self->width;
    self->dirty_area.y1 = 0;
    self->dirty_area.y2 = self->height;

    gc_free(buf);
}

void common_hal_displayio_rambusbitmap_deinit(displayio_rambusbitmap_t *self) {
    gc_free(self->pxbuf);
    self->pxbuf = NULL;
}

bool common_hal_displayio_rambusbitmap_deinited(displayio_rambusbitmap_t *self) {
    return self->pxbuf == NULL;
}

uint16_t common_hal_displayio_rambusbitmap_get_height(displayio_rambusbitmap_t *self) {
    return self->height;
}

uint16_t common_hal_displayio_rambusbitmap_get_width(displayio_rambusbitmap_t *self) {
    return self->width;
}

uint32_t common_hal_displayio_rambusbitmap_get_bits_per_value(displayio_rambusbitmap_t *self) {
    return self->bits_per_pixel;
}

uint32_t common_hal_displayio_rambusbitmap_get_size(displayio_rambusbitmap_t *self) {
    return self->size;
}

mp_obj_t common_hal_displayio_rambusbitmap_get_pixel_shader(displayio_rambusbitmap_t *self) {
    if (self->pixel_shader_base == NULL) {
        return mp_const_none;
    } else {
        return MP_OBJ_FROM_PTR(self->pixel_shader_base);
    }
}

uint32_t common_hal_displayio_rambusbitmap_get_pixel(displayio_rambusbitmap_t *self, int16_t x, int16_t y) {
    if (x >= self->width || x < 0 || y >= self->height || y < 0) {
        return 0;
    }

    int32_t row_start = y * self->stride;
    uint8_t bytes_per_pixel = (self->bits_per_pixel / 8) ? (self->bits_per_pixel / 8) : 1;

    shared_module_rambus_ram_read_seq(self->ram, self->addr + row_start + (x * bytes_per_pixel), self->pxbuf, bytes_per_pixel);
    
    if (bytes_per_pixel < 1) {
        if (self->pixel_shader_base != NULL) {
            // Indexed coloring
            uint8_t pixels_per_byte = 8 / self->bits_per_pixel;

            if (pixels_per_byte == 1) {
                return self->pxbuf[0];
            } else {
                uint8_t offset = (x % pixels_per_byte) * self->bits_per_pixel;
                uint8_t mask = (1 << self->bits_per_pixel) - 1;

                return (self->pxbuf[0] >> ((8 - self->bits_per_pixel) - offset)) & mask;
            }
        } else {
            uint32_t word = shared_module_rambus_ram_read_byte(self->ram, self->addr + (row_start + (x >> self->x_shift)), self->pxbuf, 0);

            return (word >> (sizeof(uint32_t) * 8 - ((x & self->x_mask) + 1) * self->bits_per_pixel)) & self->bitmask;
        }
    } else {
        if (bytes_per_pixel == 1) {
            return ((uint8_t *)self->pxbuf)[0];
        } else if (bytes_per_pixel == 2) {
            return ((uint16_t *)self->pxbuf)[0];
        } else if (bytes_per_pixel == 4) {
            return ((uint32_t *)self->pxbuf)[0];
        }
    }
    
    return 0;
}

void displayio_rambusbitmap_set_dirty_area(displayio_rambusbitmap_t *self, const displayio_area_t *dirty_area) {
    if (self->read_only) {
        mp_raise_RuntimeError(MP_ERROR_TEXT("Read-only"));
    }

    displayio_area_t area = *dirty_area;
    displayio_area_canon(&area);
    displayio_area_union(&area, &self->dirty_area, &area);
    displayio_area_t bitmap_area = {0, 0, self->width, self->height, NULL};
    displayio_area_compute_overlap(&area, &bitmap_area, &self->dirty_area);
}

void displayio_rambusbitmap_write_pixel(displayio_rambusbitmap_t *self, int16_t x, int16_t y, uint32_t value) {
    if (self->read_only) {
        mp_raise_RuntimeError(MP_ERROR_TEXT("Read-only"));
    }
    // Writes the color index value into a pixel position
    // Must update the dirty area separately

    // Don't write if out of area
    if (0 > x || x >= self->width || 0 > y || y >= self->height) {
        return;
    }

    // Update one pixel of data
    int32_t row_start = y * self->stride;
    uint32_t bytes_per_value = self->bits_per_pixel / 8;
    if (bytes_per_value < 1) {
        uint32_t bit_position = (sizeof(uint32_t) * 8 - ((x & self->x_mask) + 1) * self->bits_per_pixel);
        uint32_t index = row_start + (x >> self->x_shift);
        uint32_t word = shared_module_rambus_ram_read_byte(self->ram, self->addr + index, self->pxbuf, 0);
        // uint32_t word = self->data[index];
        word &= ~(self->bitmask << bit_position);
        word |= (value & self->bitmask) << bit_position;
        shared_module_rambus_ram_write_byte(self->ram, self->addr + index, word);
        // self->data[index] = word;
    } else {
        // uint32_t *row = self->data + row_start;
        if (bytes_per_value == 1) {
            ((uint8_t *)self->pxbuf)[x] = value;
        } else if (bytes_per_value == 2) {
            ((uint16_t *)self->pxbuf)[x] = value;
        } else if (bytes_per_value == 4) {
            ((uint32_t *)self->pxbuf)[x] = value;
        }

        shared_module_rambus_ram_write_seq(self->ram, self->addr + row_start, self->pxbuf, bytes_per_value);
    }
}

void common_hal_displayio_rambusbitmap_set_pixel(displayio_rambusbitmap_t *self, int16_t x, int16_t y, uint32_t value) {
    if (self->read_only) {
        mp_raise_RuntimeError(MP_ERROR_TEXT("Read-only"));
    }
    // update the dirty region
    displayio_area_t a = {x, y, x + 1, y + 1, NULL};
    displayio_rambusbitmap_set_dirty_area(self, &a);

    // write the pixel
    displayio_rambusbitmap_write_pixel(self, x, y, value);

}

displayio_area_t *displayio_rambusbitmap_get_refresh_areas(displayio_rambusbitmap_t *self, displayio_area_t *tail) {
    if (self->dirty_area.x1 == self->dirty_area.x2 || self->read_only) {
        return tail;
    }
    self->dirty_area.next = tail;
    return &self->dirty_area;
}

void displayio_rambusbitmap_finish_refresh(displayio_rambusbitmap_t *self) {
    if (self->read_only) {
        return;
    }
    self->dirty_area.x1 = 0;
    self->dirty_area.x2 = 0;
}

void common_hal_displayio_rambusbitmap_fill(displayio_rambusbitmap_t *self, uint32_t value) {
    if (self->read_only) {
        mp_raise_RuntimeError(MP_ERROR_TEXT("Read-only"));
    }
    displayio_area_t a = {0, 0, self->width, self->height, NULL};
    displayio_rambusbitmap_set_dirty_area(self, &a);

    // build the packed word
    uint32_t word = 0;
    for (uint8_t i = 0; i < 32 / self->bits_per_pixel; i++) {
        word |= (value & self->bitmask) << (32 - ((i + 1) * self->bits_per_pixel));
    }
    // copy it in
    for (uint32_t i = 0; i < self->stride * self->height; i++) {
        // TODO: implement hold on RAM, hold open throughout transaction
        shared_module_rambus_ram_write_seq(self->ram, self->addr + i, (uint8_t*)word, sizeof(word));
        // self->data[i] = word;
    }
}

int common_hal_displayio_rambusbitmap_get_buffer(displayio_rambusbitmap_t *self, mp_buffer_info_t *bufinfo, mp_uint_t flags) {
    if ((flags & MP_BUFFER_WRITE) && self->read_only) {
        return 1;
    }
    bufinfo->len = self->stride * self->height * sizeof(uint32_t);
    shared_module_rambus_ram_write_seq(self->ram, self->addr, bufinfo->buf, bufinfo->len);
    switch (self->bits_per_pixel) {
        case 32:
            bufinfo->typecode = 'I';
            break;
        case 16:
            bufinfo->typecode = 'H';
            break;
        default:
            bufinfo->typecode = 'B';
            break;
    }
    return 0;
}