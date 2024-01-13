#ifndef MICROPY_INCLUDED_SHARED_MODULE_DISPLAYIO_RAMBUSBITMAP_H
#define MICROPY_INCLUDED_SHARED_MODULE_DISPLAYIO_RAMBUSBITMAP_H

#include <stdbool.h>
#include <stdint.h>

#include "py/obj.h"
#include "shared-module/displayio/area.h"
#include "shared-module/rambus/RAM.h"
#include "extmod/vfs_fat.h"

typedef struct {
    pyb_file_obj_t *file;
    uint16_t data_offset;
    uint32_t r_bitmask;
    uint32_t g_bitmask;
    uint32_t b_bitmask;
    bool bitfield_compressed;
    union {
        mp_obj_base_t *pixel_shader_base;
        struct displayio_palette *palette;
        struct displayio_colorconverter *colorconverter;
    };
} bitmap_file_header_t;

typedef struct {
    mp_obj_base_t base;
    uint16_t width;
    uint16_t height;
    rambus_ram_obj_t *ram;
    bitmap_file_header_t *file_header;
    addr_t addr;
    uint8_t *pxbuf;
    uint16_t stride; // uint32_t's
    uint8_t bits_per_pixel;
    uint8_t x_shift;
    size_t x_mask;
    displayio_area_t dirty_area;
    uint16_t bitmask;
    bool read_only;
} displayio_rambusbitmap_t;

void displayio_rambusbitmap_finish_refresh(displayio_rambusbitmap_t *self);
displayio_area_t *displayio_rambusbitmap_get_refresh_areas(displayio_rambusbitmap_t *self, displayio_area_t *tail);
void displayio_rambusbitmap_set_dirty_area(displayio_rambusbitmap_t *self, const displayio_area_t *area);
void displayio_rambusbitmap_write_pixel(displayio_rambusbitmap_t *self, int16_t x, int16_t y, uint32_t value);

#endif // MICROPY_INCLUDED_SHARED_MODULE_DISPLAYIO_RAMBUSBITMAP_H
