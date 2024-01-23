#pragma once

#include "shared-module/rambus/__init__.h"
#include "shared-bindings/rambus/__init__.h"
#include "shared-module/rambus/RAMBusDisplay.h"

extern const mp_obj_type_t rambus_rambusdisplay_type;

#define NO_BRIGHTNESS_COMMAND 0x100
#define NO_FPS_LIMIT 0xffffffff

extern void rambus_rambusdisplay_construct(rambus_rambusdisplay_obj_t *self,
    rambus_ram_obj_t *ram, addr_t addr, uint16_t width, uint16_t height, uint16_t color_depth, 
    bool grayscale, uint16_t rotation, bool pixels_in_byte_share_row, uint8_t bytes_per_cell, 
    bool reverse_pixels_in_byte, bool reverse_bytes_in_word, bool auto_refresh);

extern bool rambus_rambusdisplay_refresh(rambus_rambusdisplay_obj_t *self, uint32_t target_ms_per_frame, uint32_t maximum_ms_per_real_frame);

extern bool rambus_rambusdisplay_get_auto_refresh(rambus_rambusdisplay_obj_t *self);
extern void rambus_rambusdisplay_set_auto_refresh(rambus_rambusdisplay_obj_t *self, bool auto_refresh);

extern uint16_t rambus_rambusdisplay_get_width(rambus_rambusdisplay_obj_t *self);
extern uint16_t rambus_rambusdisplay_get_height(rambus_rambusdisplay_obj_t *self);
extern uint16_t rambus_rambusdisplay_get_rotation(rambus_rambusdisplay_obj_t *self);
extern void rambus_rambusdisplay_set_rotation(rambus_rambusdisplay_obj_t *self, int rotation);

extern mp_float_t rambus_rambusdisplay_get_brightness(rambus_rambusdisplay_obj_t *self);
extern bool rambus_rambusdisplay_set_brightness(rambus_rambusdisplay_obj_t *self, mp_float_t brightness);

extern mp_obj_t rambus_rambusdisplay_get_root_group(rambus_rambusdisplay_obj_t *self);
extern mp_obj_t rambus_rambusdisplay_set_root_group(rambus_rambusdisplay_obj_t *self, displayio_group_t *root_group);