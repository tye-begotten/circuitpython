
// #pragma once

// #include "common-hal/microcontroller/Pin.h"
// #include "shared-bindings/displayio/__init__.h"
// #include "shared-module/displayio/Group.h"

// extern const mp_obj_type_t rambusframebuffer_framebuffer_type;

// void rambusframebuffer_framebuffer_construct(rambusframebuffer_framebuffer_obj_t *self,
//     const mcu_pin_obj_t *de,
//     const mcu_pin_obj_t *vsync,
//     const mcu_pin_obj_t *hsync,
//     const mcu_pin_obj_t *dclk,
//     const mcu_pin_obj_t **red, uint8_t num_red,
//     const mcu_pin_obj_t **green, uint8_t num_green,
//     const mcu_pin_obj_t **blue, uint8_t num_blue,
//     int frequency, int width, int height,
//     int hsync_pulse_width, int hsync_back_porch, int hsync_front_porch, bool hsync_idle_low,
//     int vsync_pulse_width, int vsync_back_porch, int vsync_front_porch, bool vsync_idle_low,
//     bool de_idle_high, bool pclk_active_high, bool pclk_idle_high,
//     int overscan_left);

// void rambusframebuffer_framebuffer_deinit(rambusframebuffer_framebuffer_obj_t *self);
// bool rambusframebuffer_framebuffer_deinitialized(rambusframebuffer_framebuffer_obj_t *self);

// mp_int_t rambusframebuffer_framebuffer_get_width(rambusframebuffer_framebuffer_obj_t *self);
// mp_int_t rambusframebuffer_framebuffer_get_height(rambusframebuffer_framebuffer_obj_t *self);
// mp_int_t rambusframebuffer_framebuffer_get_frequency(rambusframebuffer_framebuffer_obj_t *self);
// mp_int_t rambusframebuffer_framebuffer_get_refresh_rate(rambusframebuffer_framebuffer_obj_t *self);
// mp_int_t rambusframebuffer_framebuffer_get_row_stride(rambusframebuffer_framebuffer_obj_t *self);
// mp_int_t rambusframebuffer_framebuffer_get_first_pixel_offset(rambusframebuffer_framebuffer_obj_t *self);
// void rambusframebuffer_framebuffer_refresh(rambusframebuffer_framebuffer_obj_t *self);
