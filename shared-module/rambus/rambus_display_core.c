#include "stdlib.h"
#include "py/obj.h"
#include "py/objproperty.h"
#include "py/runtime.h"
#include "py/objtype.h"
#include "py/objlist.h"

#include "shared-module/rambus/RAM.h"
#include "shared-module/rambus/rambus_display_core.h"

#include "shared-bindings/displayio/Bitmap.h"
#include "shared-bindings/displayio/ColorConverter.h"
#include "shared-bindings/displayio/OnDiskBitmap.h"
#include "shared-bindings/displayio/RAMBusBitmap.h"
#include "shared-bindings/displayio/Palette.h"
#include "shared-bindings/displayio/TileGrid.h"
#include "shared-module/displayio/TileGrid.h"

#if CIRCUITPY_VECTORIO
#include "shared-module/vectorio/__init__.h"
#include "shared-bindings/vectorio/VectorShape.h"

#include "py/misc.h"
#include "shared-bindings/time/__init__.h"
#include "shared-bindings/vectorio/Circle.h"
#include "shared-bindings/vectorio/Polygon.h"
#include "shared-bindings/vectorio/Rectangle.h"
#endif


bool rambus_display_core_fill_area(displayio_display_core_t *self, displayio_area_t *area, uint32_t *mask, rambus_ptr_t *addr) {
    if (self->current_group != NULL) {
        return rambus_group_fill_area(self->current_group, &self->colorspace, area, mask, addr);
    }
    return false;
}

bool rambus_group_fill_area(displayio_group_t *self, const _displayio_colorspace_t *colorspace, 
    const displayio_area_t *area, uint32_t *mask, rambus_ptr_t *addr) {
    // Track if any of the layers finishes filling in the given area. We can ignore any remaining
    // layers at that point.
    if (self->hidden == false) {
        for (int32_t i = self->members->len - 1; i >= 0; i--) {
            mp_obj_t layer;
            #if CIRCUITPY_VECTORIO
            const vectorio_draw_protocol_t *draw_protocol = mp_proto_get(MP_QSTR_protocol_draw, self->members->items[i]);
            if (draw_protocol != NULL) {
                layer = draw_protocol->draw_get_protocol_self(self->members->items[i]);
                if (rambus_vector_shape_fill_area(layer, colorspace, area, mask, addr)) {
                    return true;
                }
                continue;
            }
            #endif
            layer = mp_obj_cast_to_native_base(
                self->members->items[i], &displayio_tilegrid_type);
            if (layer != MP_OBJ_NULL) {
                if (rambus_tilegrid_fill_area(layer, colorspace, area, mask, addr)) {
                    return true;
                }
                continue;
            }
            layer = mp_obj_cast_to_native_base(
                self->members->items[i], &displayio_group_type);
            if (layer != MP_OBJ_NULL) {
                if (rambus_group_fill_area(layer, colorspace, area, mask, addr)) {
                    return true;
                }
                continue;
            }
        }
    }
    return false;
}

bool rambus_tilegrid_fill_area(displayio_tilegrid_t *self,
    const _displayio_colorspace_t *colorspace, const displayio_area_t *area,
    uint32_t *mask, rambus_ptr_t *addr) {
    // If no tiles are present we have no impact.
    uint8_t *tiles = self->tiles;
    if (self->inline_tiles) {
        tiles = (uint8_t *)&self->tiles;
    }
    if (tiles == NULL) {
        return false;
    }

    bool hidden = self->hidden || self->hidden_by_parent;
    if (hidden) {
        return false;
    }

    displayio_area_t overlap;
    if (!displayio_area_compute_overlap(area, &self->current_area, &overlap)) {
        return false;
    }

    int16_t x_stride = 1;
    int16_t y_stride = displayio_area_width(area);

    bool flip_x = self->flip_x;
    bool flip_y = self->flip_y;
    if (self->transpose_xy != self->absolute_transform->transpose_xy) {
        bool temp_flip = flip_x;
        flip_x = flip_y;
        flip_y = temp_flip;
    }

    // How many pixels are outside of our area between us and the start of the row.
    uint16_t start = 0;
    if ((self->absolute_transform->dx < 0) != flip_x) {
        start += (area->x2 - area->x1 - 1) * x_stride;
        x_stride *= -1;
    }
    if ((self->absolute_transform->dy < 0) != flip_y) {
        start += (area->y2 - area->y1 - 1) * y_stride;
        y_stride *= -1;
    }

    // Track if this layer finishes filling in the given area. We can ignore any remaining
    // layers at that point.
    bool full_coverage = displayio_area_equal(area, &overlap);

    // TODO(tannewt): Skip coverage tracking if all pixels outside the overlap have already been
    // set and our palette is all opaque.

    // TODO(tannewt): Check to see if the pixel_shader has any transparency. If it doesn't then we
    // can either return full coverage or bulk update the mask.
    displayio_area_t transformed;
    displayio_area_transform_within(flip_x != (self->absolute_transform->dx < 0), 
        flip_y != (self->absolute_transform->dy < 0), 
        self->transpose_xy != self->absolute_transform->transpose_xy,
        &overlap,
        &self->current_area,
        &transformed);

    int16_t start_x = (transformed.x1 - self->current_area.x1);
    int16_t end_x = (transformed.x2 - self->current_area.x1);
    int16_t start_y = (transformed.y1 - self->current_area.y1);
    int16_t end_y = (transformed.y2 - self->current_area.y1);

    int16_t y_shift = 0;
    int16_t x_shift = 0;
    if ((self->absolute_transform->dx < 0) != flip_x) {
        x_shift = area->x2 - overlap.x2;
    } else {
        x_shift = overlap.x1 - area->x1;
    }
    if ((self->absolute_transform->dy < 0) != flip_y) {
        y_shift = area->y2 - overlap.y2;
    } else {
        y_shift = overlap.y1 - area->y1;
    }

    // This untransposes x and y so it aligns with bitmap rows.
    if (self->transpose_xy != self->absolute_transform->transpose_xy) {
        int16_t temp_stride = x_stride;
        x_stride = y_stride;
        y_stride = temp_stride;
        int16_t temp_shift = x_shift;
        x_shift = y_shift;
        y_shift = temp_shift;
    }

    uint8_t pixels_per_byte = 8 / colorspace->depth;

    displayio_input_pixel_t input_pixel;
    displayio_output_pixel_t output_pixel;

    // TODO: can this be sped up at all using a buffer or holding the RAM open while we sequentially write?
    for (input_pixel.y = start_y; input_pixel.y < end_y; ++input_pixel.y) {
        int16_t row_start = start + (input_pixel.y - start_y + y_shift) * y_stride; // in pixels
        int16_t local_y = input_pixel.y / self->absolute_transform->scale;
        for (input_pixel.x = start_x; input_pixel.x < end_x; ++input_pixel.x) {
            // Compute the destination pixel in the buffer and mask based on the transformations.
            addr->offset = row_start + (input_pixel.x - start_x + x_shift) * x_stride; // in pixels

            // This is super useful for debugging out of range accesses. Uncomment to use.
            // if (offset < 0 || offset >= (int32_t) displayio_area_size(area)) {
            //     asm("bkpt");
            // }

            // Check the mask first to see if the pixel has already been set.
            if ((mask[addr->offset / 32] & (1 << (addr->offset % 32))) != 0) {
                continue;
            }
            int16_t local_x = input_pixel.x / self->absolute_transform->scale;
            uint16_t tile_location = ((local_y / self->tile_height + self->top_left_y) % self->height_in_tiles) * self->width_in_tiles + (local_x / self->tile_width + self->top_left_x) % self->width_in_tiles;
            input_pixel.tile = tiles[tile_location];
            input_pixel.tile_x = (input_pixel.tile % self->bitmap_width_in_tiles) * self->tile_width + local_x % self->tile_width;
            input_pixel.tile_y = (input_pixel.tile / self->bitmap_width_in_tiles) * self->tile_height + local_y % self->tile_height;

            output_pixel.pixel = 0;
            input_pixel.pixel = 0;
            uint8_t shared_px = 0;

            // We always want to read bitmap pixels by row first and then transpose into the destination
            // buffer because most bitmaps are row associated.
            if (mp_obj_is_type(self->bitmap, &displayio_bitmap_type)) {
                input_pixel.pixel = common_hal_displayio_bitmap_get_pixel(self->bitmap, input_pixel.tile_x, input_pixel.tile_y);
            } else if (mp_obj_is_type(self->bitmap, &displayio_ondiskbitmap_type)) {
                input_pixel.pixel = common_hal_displayio_ondiskbitmap_get_pixel(self->bitmap, input_pixel.tile_x, input_pixel.tile_y);
            } else if (mp_obj_is_type(self->bitmap, &displayio_rambusbitmap_type)) {
                input_pixel.pixel = common_hal_displayio_rambusbitmap_get_pixel(self->bitmap, input_pixel.tile_x, input_pixel.tile_y);
            }

            output_pixel.opaque = true;
            if (self->pixel_shader == mp_const_none) {
                output_pixel.pixel = input_pixel.pixel;
            } else if (mp_obj_is_type(self->pixel_shader, &displayio_palette_type)) {
                displayio_palette_get_color(self->pixel_shader, colorspace, &input_pixel, &output_pixel);
            } else if (mp_obj_is_type(self->pixel_shader, &displayio_colorconverter_type)) {
                displayio_colorconverter_convert(self->pixel_shader, colorspace, &input_pixel, &output_pixel);
            }
            if (!output_pixel.opaque) {
                // A pixel is transparent so we haven't fully covered the area ourselves.
                full_coverage = false;
            } else {
                mask[addr->offset / 32] |= 1 << (addr->offset % 32);
                if (colorspace->depth == 16) {
                    rambus_ram_write_seq(addr->ram, addr->start + addr->offset, (uint8_t*)output_pixel.pixel, 2);
                    // *(((uint16_t *)buffer) + offset) = output_pixel.pixel;
                } else if (colorspace->depth == 32) {
                    rambus_ram_write_seq(addr->ram, addr->start + addr->offset, (uint8_t*)output_pixel.pixel, 4);
                    // *(((uint32_t *)buffer) + offset) = output_pixel.pixel;
                } else if (colorspace->depth == 8) {
                    rambus_ram_write_seq(addr->ram, addr->start + addr->offset, (uint8_t*)output_pixel.pixel, 1);
                    // *(((uint8_t *)buffer) + offset) = output_pixel.pixel;
                } else if (colorspace->depth < 8) {
                    // Reorder the offsets to pack multiple rows into a byte (meaning they share a column).
                    if (!colorspace->pixels_in_byte_share_row) {
                        uint16_t width = displayio_area_width(area);
                        uint16_t row = addr->offset / width;
                        uint16_t col = addr->offset % width;
                        // Dividing by pixels_per_byte does truncated division even if we multiply it back out.
                        addr->offset = col * pixels_per_byte + (row / pixels_per_byte) * pixels_per_byte * width + row % pixels_per_byte;
                        // Also useful for validating that the bitpacking worked correctly.
                        // if (offset > displayio_area_size(area)) {
                        //     asm("bkpt");
                        // }
                    }
                    uint8_t shift = (addr->offset % pixels_per_byte) * colorspace->depth;
                    if (colorspace->reverse_pixels_in_byte) {
                        // Reverse the shift by subtracting it from the leftmost shift.
                        shift = (pixels_per_byte - 1) * colorspace->depth - shift;
                    }
                    shared_px |= output_pixel.pixel << shift;
                    if (addr->offset % pixels_per_byte == 0) {
                        rambus_ram_write_seq(addr->ram, addr->start + addr->offset, &shared_px, 1);
                        shared_px = 0;
                    }
                    // ((uint8_t *)buffer)[offset / pixels_per_byte] |= output_pixel.pixel << shift;
                }
            }
        }
    }
    return full_coverage;
}


#if CIRCUITPY_VECTORIO

// Lifecycle actions.
#define VECTORIO_SHAPE_DEBUG(...) (void)0
// #define VECTORIO_SHAPE_DEBUG(...) mp_printf(&mp_plat_print, __VA_ARGS__)


// Used in both logging and ifdefs, for extra variables
// #define VECTORIO_PERF(...) mp_printf(&mp_plat_print, __VA_ARGS__)


// Really verbose.
#define VECTORIO_SHAPE_PIXEL_DEBUG(...) (void)0
// #define VECTORIO_SHAPE_PIXEL_DEBUG(...) mp_printf(&mp_plat_print, __VA_ARGS__)

#define U32_TO_BINARY_FMT "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"
#define U32_TO_BINARY(u32)  \
    (u32 & 0x80000000 ? '1' : '0'), \
    (u32 & 0x40000000 ? '1' : '0'), \
    (u32 & 0x20000000 ? '1' : '0'), \
    (u32 & 0x10000000 ? '1' : '0'), \
    (u32 & 0x8000000  ? '1' : '0'), \
    (u32 & 0x4000000  ? '1' : '0'), \
    (u32 & 0x2000000  ? '1' : '0'), \
    (u32 & 0x1000000  ? '1' : '0'), \
    (u32 & 0x800000   ? '1' : '0'), \
    (u32 & 0x400000   ? '1' : '0'), \
    (u32 & 0x200000   ? '1' : '0'), \
    (u32 & 0x100000   ? '1' : '0'), \
    (u32 & 0x80000    ? '1' : '0'), \
    (u32 & 0x40000    ? '1' : '0'), \
    (u32 & 0x20000    ? '1' : '0'), \
    (u32 & 0x10000    ? '1' : '0'), \
    (u32 & 0x8000     ? '1' : '0'), \
    (u32 & 0x4000     ? '1' : '0'), \
    (u32 & 0x2000     ? '1' : '0'), \
    (u32 & 0x1000     ? '1' : '0'), \
    (u32 & 0x800      ? '1' : '0'), \
    (u32 & 0x400      ? '1' : '0'), \
    (u32 & 0x200      ? '1' : '0'), \
    (u32 & 0x100      ? '1' : '0'), \
    (u32 & 0x80       ? '1' : '0'), \
    (u32 & 0x40       ? '1' : '0'), \
    (u32 & 0x20       ? '1' : '0'), \
    (u32 & 0x10       ? '1' : '0'), \
    (u32 & 0x8        ? '1' : '0'), \
    (u32 & 0x4        ? '1' : '0'), \
    (u32 & 0x2        ? '1' : '0'), \
    (u32 & 0x1        ? '1' : '0')

bool rambus_vector_shape_fill_area(vectorio_vector_shape_t *self, const _displayio_colorspace_t *colorspace, 
    const displayio_area_t *area, uint32_t *mask, rambus_ptr_t *addr) {
    // Shape areas are relative to 0,0.  This will allow rotation about a known axis.
    //   The consequence is that the area reported by the shape itself is _relative_ to 0,0.
    //   To make it relative to the VectorShape position, we must shift it.
    // Pixels are drawn on the screen_area (shifted) coordinate space, while pixels are _determined_ from
    //   the shape_area (unshifted) space.
    #ifdef VECTORIO_PERF
    uint64_t start = common_hal_time_monotonic_ns();
    uint64_t pixel_time = 0;
    #endif

    if (self->hidden) {
        return false;
    }

    VECTORIO_SHAPE_DEBUG("%p fill_area: fill: {(%5d,%5d), (%5d,%5d)}",
        self,
        area->x1, area->y1, area->x2, area->y2
        );
    displayio_area_t overlap;
    if (!displayio_area_compute_overlap(area, &self->current_area, &overlap)) {
        VECTORIO_SHAPE_DEBUG(" no overlap\n");
        return false;
    }
    VECTORIO_SHAPE_DEBUG(", overlap: {(%3d,%3d), (%3d,%3d)}", overlap.x1, overlap.y1, overlap.x2, overlap.y2);

    bool full_coverage = displayio_area_equal(area, &overlap);

    uint8_t pixels_per_byte = 8 / colorspace->depth;
    VECTORIO_SHAPE_DEBUG(" xy:(%3d %3d) tform:{x:%d y:%d dx:%d dy:%d scl:%d w:%d h:%d mx:%d my:%d tr:%d}",
        self->x, self->y,
        self->absolute_transform->x, self->absolute_transform->y, self->absolute_transform->dx, self->absolute_transform->dy, self->absolute_transform->scale,
        self->absolute_transform->width, self->absolute_transform->height, self->absolute_transform->mirror_x, self->absolute_transform->mirror_y, self->absolute_transform->transpose_xy
        );

    uint16_t linestride_px = displayio_area_width(area);
    uint16_t line_dirty_offset_px = (overlap.y1 - area->y1) * linestride_px;
    uint16_t column_dirty_offset_px = overlap.x1 - area->x1;
    VECTORIO_SHAPE_DEBUG(", linestride:%3d line_offset:%3d col_offset:%3d depth:%2d ppb:%2d shape:%s",
        linestride_px, line_dirty_offset_px, column_dirty_offset_px, colorspace->depth, pixels_per_byte, mp_obj_get_type_str(self->ishape.shape));

    displayio_input_pixel_t input_pixel;
    displayio_output_pixel_t output_pixel;
    uint8_t shared_px = 0;

    displayio_area_t shape_area;
    self->ishape.get_area(self->ishape.shape, &shape_area);

    uint16_t mask_start_px = line_dirty_offset_px;
    for (input_pixel.y = overlap.y1; input_pixel.y < overlap.y2; ++input_pixel.y) {
        mask_start_px += column_dirty_offset_px;
        for (input_pixel.x = overlap.x1; input_pixel.x < overlap.x2; ++input_pixel.x) {
            // Check the mask first to see if the pixel has already been set.
            addr->offset = mask_start_px + (input_pixel.x - overlap.x1);
            uint32_t *mask_doubleword = &(mask[addr->offset / 32]);
            uint8_t mask_bit = addr->offset % 32;
            VECTORIO_SHAPE_PIXEL_DEBUG("\n%p pixel_index: %5u mask_bit: %2u mask: "U32_TO_BINARY_FMT, self, pixel_index, mask_bit, U32_TO_BINARY(*mask_doubleword));
            if ((*mask_doubleword & (1u << mask_bit)) != 0) {
                VECTORIO_SHAPE_PIXEL_DEBUG(" masked");
                continue;
            }
            output_pixel.pixel = 0;

            // Cast input screen coordinates to shape coordinates to pick the pixel to draw
            int16_t pixel_to_get_x;
            int16_t pixel_to_get_y;
            screen_to_shape_coordinates(self, input_pixel.x, input_pixel.y, &pixel_to_get_x, &pixel_to_get_y);

            VECTORIO_SHAPE_PIXEL_DEBUG(" get_pixel %p (%3d, %3d) -> ( %3d, %3d )", self->ishape.shape, input_pixel.x, input_pixel.y, pixel_to_get_x, pixel_to_get_y);
            #ifdef VECTORIO_PERF
            uint64_t pre_pixel = common_hal_time_monotonic_ns();
            #endif
            input_pixel.pixel = self->ishape.get_pixel(self->ishape.shape, pixel_to_get_x, pixel_to_get_y);
            #ifdef VECTORIO_PERF
            uint64_t post_pixel = common_hal_time_monotonic_ns();
            pixel_time += post_pixel - pre_pixel;
            #endif
            VECTORIO_SHAPE_PIXEL_DEBUG(" -> %d", input_pixel.pixel);

            // vectorio shapes use 0 to mean "area is not covered."
            // We can skip all the rest of the work for this pixel if it's not currently covered by the shape.
            if (input_pixel.pixel == 0) {
                VECTORIO_SHAPE_PIXEL_DEBUG(" (encountered transparent pixel; input area is not fully covered)");
                full_coverage = false;
            } else {
                // Pixel is not transparent. Let's pull the pixel value index down to 0-base for more error-resistant palettes.
                input_pixel.pixel -= 1;
                output_pixel.opaque = true;

                if (self->pixel_shader == mp_const_none) {
                    output_pixel.pixel = input_pixel.pixel;
                } else if (mp_obj_is_type(self->pixel_shader, &displayio_palette_type)) {
                    displayio_palette_get_color(self->pixel_shader, colorspace, &input_pixel, &output_pixel);
                } else if (mp_obj_is_type(self->pixel_shader, &displayio_colorconverter_type)) {
                    displayio_colorconverter_convert(self->pixel_shader, colorspace, &input_pixel, &output_pixel);
                }

                // We double-check this to fast-path the case when a pixel is not covered by the shape & not call the color converter unnecessarily.
                if (!output_pixel.opaque) {
                    VECTORIO_SHAPE_PIXEL_DEBUG(" (encountered transparent pixel from colorconverter; input area is not fully covered)");
                    full_coverage = false;
                }

                *mask_doubleword |= 1u << mask_bit;
                if (colorspace->depth == 16) {
                    VECTORIO_SHAPE_PIXEL_DEBUG(" buffer = %04x 16", output_pixel.pixel);
                    rambus_ram_write_seq(addr->ram, addr->start + addr->offset, (uint8_t*)output_pixel.pixel, 2);
                    // *(((uint16_t *)buffer) + pixel_index) = output_pixel.pixel;
                } else if (colorspace->depth == 32) {
                    VECTORIO_SHAPE_PIXEL_DEBUG(" buffer = %04x 32", output_pixel.pixel);
                    rambus_ram_write_seq(addr->ram, addr->start + addr->offset, (uint8_t*)output_pixel.pixel, 4);
                    // *(((uint32_t *)buffer) + pixel_index) = output_pixel.pixel;
                } else if (colorspace->depth == 8) {
                    VECTORIO_SHAPE_PIXEL_DEBUG(" buffer = %02x 8", output_pixel.pixel);
                    rambus_ram_write_seq(addr->ram, addr->start + addr->offset, (uint8_t*)output_pixel.pixel, 1);
                    // *(((uint8_t *)buffer) + pixel_index) = output_pixel.pixel;
                } else if (colorspace->depth < 8) {
                    // Reorder the offsets to pack multiple rows into a byte (meaning they share a column).
                    if (!colorspace->pixels_in_byte_share_row) {
                        uint16_t row = addr->offset / linestride_px;
                        uint16_t col = addr->offset % linestride_px;
                        addr->offset = col * pixels_per_byte + (row / pixels_per_byte) * pixels_per_byte * linestride_px + row % pixels_per_byte;
                    }
                    uint8_t shift = (addr->offset % pixels_per_byte) * colorspace->depth;
                    if (colorspace->reverse_pixels_in_byte) {
                        // Reverse the shift by subtracting it from the leftmost shift.
                        shift = (pixels_per_byte - 1) * colorspace->depth - shift;
                    }
                    VECTORIO_SHAPE_PIXEL_DEBUG(" buffer = %2d %d", output_pixel.pixel, colorspace->depth);
                    shared_px |= output_pixel.pixel << shift;
                    if (addr->offset % pixels_per_byte == 0) {
                        rambus_ram_write_seq(addr->ram, addr->start + addr->offset, &shared_px, 1);
                        shared_px = 0;
                    }
                    // ((uint8_t *)buffer)[pixel_index / pixels_per_byte] |= output_pixel.pixel << shift;
                }
            }
        }
        mask_start_px += linestride_px - column_dirty_offset_px;
    }
    #ifdef VECTORIO_PERF
    uint64_t end = common_hal_time_monotonic_ns();
    uint32_t pixels = (overlap.x2 - overlap.x1) * (overlap.y2 - overlap.y1);
    VECTORIO_PERF("draw %16s -> shape:{%4dpx, %4.1fms,%9.1fpps fill}  shape_pixels:{%6.1fus total, %4.1fus/px}\n",
        mp_obj_get_type_str(self->ishape.shape),
        (overlap.x2 - overlap.x1) * (overlap.y2 - overlap.y1),
        (double)((end - start) / 1000000.0),
        (double)(MAX(1, pixels * (1000000000.0 / (end - start)))),
        (double)(pixel_time / 1000.0),
        (double)(pixel_time / 1000.0 / pixels)
        );
    #endif
    VECTORIO_SHAPE_DEBUG(" -> pixels:%4d\n", (overlap.x2 - overlap.x1) * (overlap.y2 - overlap.y1));
    return full_coverage;
}

#endif