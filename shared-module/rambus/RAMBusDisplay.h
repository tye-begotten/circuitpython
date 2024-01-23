#ifndef MICROPY_INCLUDED_SHARED_MODULE_RAMBUS_RAMBUSDISPLAY_H
#define MICROPY_INCLUDED_SHARED_MODULE_RAMBUS_RAMBUSDISPLAY_H

#include "py/obj.h"
#include "py/proto.h"

#include "shared-bindings/digitalio/DigitalInOut.h"
#include "shared-bindings/displayio/Group.h"

#include "shared-module/displayio/area.h"
#include "shared-module/displayio/display_core.h"

#include "shared-module/rambus/RAM.h"

#define NO_BRIGHTNESS_COMMAND 0x100
#define NO_FPS_LIMIT 0xffffffff

typedef struct {
    mp_obj_base_t base;
    rambus_ram_obj_t *ram;
    rambus_ptr_t *addr;
    displayio_display_core_t core;
    uint64_t last_refresh_call;
    uint16_t native_frames_per_second;
    uint16_t native_ms_per_frame;
    uint16_t first_pixel_offset;
    uint16_t row_stride;
    bool auto_refresh;
    bool first_manual_refresh;
} rambus_rambusdisplay_obj_t;

void rambus_rambusdisplay_construct(rambus_rambusdisplay_obj_t *self,
    rambus_ram_obj_t *ram, addr_t addr, uint16_t width, uint16_t height, uint16_t color_depth, 
    bool grayscale, uint16_t rotation, bool pixels_in_byte_share_row, uint8_t bytes_per_cell, 
    bool reverse_pixels_in_byte, bool reverse_bytes_in_word, bool auto_refresh);

bool rambus_rambusdisplay_refresh(rambus_rambusdisplay_obj_t *self, uint32_t target_ms_per_frame, uint32_t maximum_ms_per_real_frame);

bool rambus_rambusdisplay_get_auto_refresh(rambus_rambusdisplay_obj_t *self);
void rambus_rambusdisplay_set_auto_refresh(rambus_rambusdisplay_obj_t *self, bool auto_refresh);

uint16_t rambus_rambusdisplay_get_width(rambus_rambusdisplay_obj_t *self);
uint16_t rambus_rambusdisplay_get_height(rambus_rambusdisplay_obj_t *self);
uint16_t rambus_rambusdisplay_get_rotation(rambus_rambusdisplay_obj_t *self);
void rambus_rambusdisplay_set_rotation(rambus_rambusdisplay_obj_t *self, int rotation);

mp_float_t rambus_rambusdisplay_get_brightness(rambus_rambusdisplay_obj_t *self);
bool rambus_rambusdisplay_set_brightness(rambus_rambusdisplay_obj_t *self, mp_float_t brightness);

mp_obj_t rambus_rambusdisplay_framebuffer(rambus_rambusdisplay_obj_t *self);

mp_obj_t rambus_rambusdisplay_get_root_group(rambus_rambusdisplay_obj_t *self);
mp_obj_t rambus_rambusdisplay_set_root_group(rambus_rambusdisplay_obj_t *self, displayio_group_t *root_group);

// displayio core contracts
void rambus_rambusdisplay_background(rambus_rambusdisplay_obj_t *self);
void release_rambusdisplay(rambus_rambusdisplay_obj_t *self);
void rambus_rambusdisplay_reset(rambus_rambusdisplay_obj_t *self);

void rambus_rambusdisplay_collect_ptrs(rambus_rambusdisplay_obj_t *self);


#endif //MICROPY_INCLUDED_SHARED_MODULE_RAMBUS_RAMBUSDISPLAY_H