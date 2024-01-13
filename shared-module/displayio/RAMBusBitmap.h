#ifndef MICROPY_INCLUDED_SHARED_MODULE_DISPLAYIO_RAMBUSBITMAP_H
#define MICROPY_INCLUDED_SHARED_MODULE_DISPLAYIO_RAMBUSBITMAP_H

#include <stdbool.h>
#include <stdint.h>

#include "py/obj.h"
#include "shared-module/displayio/area.h"
#include "shared-module/rambus/RAM.h"
#include "extmod/vfs_fat.h"

typedef struct {
    mp_obj_base_t base;
    uint16_t width;
    uint16_t height;
    rambus_ram_obj_t *ram;
    addr_t addr;
    addr_t size;
    uint8_t *pxbuf;
    uint16_t stride; // uint32_t's
    uint8_t bits_per_pixel;
    uint8_t x_shift;
    size_t x_mask;
    displayio_area_t dirty_area;
    uint16_t bitmask;
    union {
        mp_obj_base_t *pixel_shader_base;
        struct displayio_palette *palette;
        struct displayio_colorconverter *colorconverter;
    };
    bool read_only;
} displayio_rambusbitmap_t;

void displayio_rambusbitmap_finish_refresh(displayio_rambusbitmap_t *self);
displayio_area_t *displayio_rambusbitmap_get_refresh_areas(displayio_rambusbitmap_t *self, displayio_area_t *tail);
void displayio_rambusbitmap_set_dirty_area(displayio_rambusbitmap_t *self, const displayio_area_t *area);
void displayio_rambusbitmap_write_pixel(displayio_rambusbitmap_t *self, int16_t x, int16_t y, uint32_t value);

#endif // MICROPY_INCLUDED_SHARED_MODULE_DISPLAYIO_RAMBUSBITMAP_H
