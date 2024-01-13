

#ifndef MICROPY_INCLUDED_SHARED_BINDINGS_DISPLAYIO_RAMBUSBITMAP_H
#define MICROPY_INCLUDED_SHARED_BINDINGS_DISPLAYIO_RAMBUSBITMAP_H

#include "shared-module/displayio/RAMBusBitmap.h"
#include "extmod/vfs_fat.h"

extern const mp_obj_type_t displayio_rambusbitmap_type;

void common_hal_displayio_rambusbitmap_construct(displayio_rambusbitmap_t *self, uint32_t width,
    uint32_t height, rambus_ram_obj_t *ram, addr_t addr, pyb_file_obj_t *file, uint32_t bits_per_value, bool read_only);
void common_hal_displayio_rambusbitmap_load_from_file(displayio_rambusbitmap_t *self, pyb_file_obj_t *file);

uint16_t common_hal_displayio_rambusbitmap_get_height(displayio_rambusbitmap_t *self);
uint16_t common_hal_displayio_rambusbitmap_get_width(displayio_rambusbitmap_t *self);
uint32_t common_hal_displayio_rambusbitmap_get_bits_per_value(displayio_rambusbitmap_t *self);
uint32_t common_hal_displayio_rambusbitmap_get_size(displayio_rambusbitmap_t *self);
mp_obj_t common_hal_displayio_rambusbitmap_get_pixel_shader(displayio_rambusbitmap_t *self);

void common_hal_displayio_rambusbitmap_set_pixel(displayio_rambusbitmap_t *bitmap, int16_t x, int16_t y, uint32_t value);
uint32_t common_hal_displayio_rambusbitmap_get_pixel(displayio_rambusbitmap_t *bitmap, int16_t x, int16_t y);
void common_hal_displayio_rambusbitmap_fill(displayio_rambusbitmap_t *bitmap, uint32_t value);
int common_hal_displayio_rambusbitmap_get_buffer(displayio_rambusbitmap_t *self, mp_buffer_info_t *bufinfo, mp_uint_t flags);
void common_hal_displayio_rambusbitmap_deinit(displayio_rambusbitmap_t *self);
bool common_hal_displayio_rambusbitmap_deinited(displayio_rambusbitmap_t *self);

#endif // MICROPY_INCLUDED_SHARED_BINDINGS_DISPLAYIO_RAMBUSBITMAP_H