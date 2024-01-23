// #pragma once

// #include "common-hal/microcontroller/Pin.h"
// #include "shared-bindings/displayio/__init__.h"
// #include "shared-module/displayio/Group.h"
// #include "shared-module/rambus/RAM.h"
// #include "shared-module/rambus/RAMBusDisplay.h"

// typedef struct {
//     rambus_ram_obj_t *ram;
//     rambus_rambusdisplay_obj_t *display;
//     addr_t addr;
//     uint8_t frequency;
//     uint16_t width;
//     uint16_t height;
//     uint8_t bits_per_px;
//     bool ds_idle_high;
// } rambus_framebuffer_obj_t;

// void rambus_rambusframebuffer_construct(
//     rambus_framebuffer_obj_t *self,
//     rambus_ram_obj_t *ram,
//     rambus_rambusdisplay_obj_t *display,
//     addr_t addr,
//     uint8_t frequency, 
//     uint16_t width, 
//     uint16_t height,
//     uint8_t bits_per_px,
//     bool ds_idle_high
//     );

// void rambus_rambusframebuffer_deinit(rambus_framebuffer_obj_t *self);
// bool rambus_rambusframebuffer_deinitialized(rambus_framebuffer_obj_t *self);

// mp_int_t rambus_rambusframebuffer_get_width(rambus_framebuffer_obj_t *self);
// mp_int_t rambus_rambusframebuffer_get_height(rambus_framebuffer_obj_t *self);
// mp_int_t rambus_rambusframebuffer_get_frequency(rambus_framebuffer_obj_t *self);
// mp_int_t rambus_rambusframebuffer_get_refresh_rate(rambus_framebuffer_obj_t *self);
// mp_int_t rambus_rambusframebuffer_get_row_stride(rambus_framebuffer_obj_t *self);
// mp_int_t rambus_rambusframebuffer_get_first_pixel_offset(rambus_framebuffer_obj_t *self);
// void rambus_rambusframebuffer_refresh(rambus_framebuffer_obj_t *self);
