/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Scott Shawcroft for Adafruit Industries
 * Copyright (c) 2020 Jeff Epler for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "shared-module/rambus/__init__.h"
#include "shared-module/rambus/RAMBusDisplay.h"

#include <stdlib.h>
#include "py/gc.h"
#include "py/runtime.h"
#include "py/runtime.h"
#include "shared-bindings/microcontroller/Pin.h"
#include "shared-bindings/time/__init__.h"
#include "shared-module/displayio/__init__.h"
#include "shared-module/rambus/rambus_display_core.h"
#include "supervisor/shared/display.h"
#include "supervisor/shared/tick.h"
#include "supervisor/usb.h"

#include <stdint.h>
#include <string.h>



void rambus_rambusdisplay_construct(rambus_rambusdisplay_obj_t *self,
    rambus_ram_obj_t *ram, addr_t addr, uint16_t width, uint16_t height, uint16_t color_depth, 
    bool grayscale, uint16_t rotation, bool pixels_in_byte_share_row, uint8_t bytes_per_cell, 
    bool reverse_pixels_in_byte, bool reverse_bytes_in_word, bool auto_refresh) {
    // Turn off auto-refresh as we init.
    self->ram = ram;
    // self->addr = (rambus_ptr_t *)malloc(sizeof(rambus_ptr_t));
    self->addr->ram = ram;
    self->addr->start = addr;
    self->addr->offset = 0;
    // self->display_bus = display_bus;

    printfl("Creating RAMBusDisplay %dx%d using RAM addr %x", width, height, addr);

    displayio_display_core_construct(
        &self->core,
        width,
        height,
        rotation,
        color_depth,
        grayscale,
        pixels_in_byte_share_row,
        bytes_per_cell,
        reverse_pixels_in_byte,
        reverse_bytes_in_word
        );

    // displayio_display_bus_construct(&self->display_bus, display_bus, ram_width, ram_height, colstart, rowstart,
    //     set_column_command, set_row_command, NO_COMMAND, NO_COMMAND, data_as_commands, false /* always_toggle_chip_select */,
    //     SH1107_addressing && color_depth == 1, false /*address_little_endian */);

    self->first_pixel_offset = 0;
    self->row_stride = self->core.width * self->core.colorspace.depth / 8;

    // self->framebuffer_protocol->get_bufinfo(self->framebuffer, &self->bufinfo);
    // size_t framebuffer_size = self->first_pixel_offset + self->row_stride * (self->core.height - 1) + self->core.width * self->core.colorspace.depth / 8;

    // mp_arg_validate_length_min(self->bufinfo.len, framebuffer_size, MP_QSTR_framebuffer);

    self->first_manual_refresh = !auto_refresh;

    self->native_frames_per_second = 60;
    self->native_ms_per_frame = 1000 / self->native_frames_per_second;

    if (rotation != 0) {
        rambus_rambusdisplay_set_rotation(self, rotation);
    }

    printl("successfully constructed RAMBusDisplay");
    // Set the group after initialization otherwise we may send pixels while we delay in
    // initialization.
    displayio_display_core_set_root_group(&self->core, &circuitpython_splash);
    rambus_rambusdisplay_set_auto_refresh(self, auto_refresh);
}

uint16_t rambus_rambusdisplay_get_width(rambus_rambusdisplay_obj_t *self) {
    return displayio_display_core_get_width(&self->core);
}

uint16_t rambus_rambusdisplay_get_height(rambus_rambusdisplay_obj_t *self) {
    return displayio_display_core_get_height(&self->core);
}

mp_float_t rambus_rambusdisplay_get_brightness(rambus_rambusdisplay_obj_t *self) {
    return -1;
}

bool rambus_rambusdisplay_set_brightness(rambus_rambusdisplay_obj_t *self, mp_float_t brightness) {
    bool ok = false;
    return ok;
}

STATIC const displayio_area_t *_get_refresh_areas(rambus_rambusdisplay_obj_t *self) {
    if (self->core.full_refresh) {
        self->core.area.next = NULL;
        return &self->core.area;
    } else if (self->core.current_group != NULL) {
        return displayio_group_get_refresh_areas(self->core.current_group, NULL);
    }
    return NULL;
}

STATIC const bool _deinited(rambus_rambusdisplay_obj_t *self) {
    return shared_module_rambus_ram_deinited(self->ram);
}

#define MARK_ROW_DIRTY(r) (dirty_row_bitmask[r / 8] |= (1 << (r & 7)))
STATIC bool _refresh_area(rambus_rambusdisplay_obj_t *self, const displayio_area_t *area, uint8_t *dirty_row_bitmask) {
    uint16_t buffer_size = CIRCUITPY_DISPLAY_AREA_BUFFER_SIZE / sizeof(uint32_t); // In uint32_ts

    displayio_area_t clipped;
    // Clip the area to the display by overlapping the areas. If there is no overlap then we're done.
    if (!displayio_display_core_clip_area(&self->core, area, &clipped)) {
        return true;
    }
    uint16_t subrectangles = 1;

    // If pixels are packed by row then rows are on byte boundaries
    if (self->core.colorspace.depth < 8 && self->core.colorspace.pixels_in_byte_share_row) {
        int div = 8 / self->core.colorspace.depth;
        clipped.x1 = (clipped.x1 / div) * div;
        clipped.x2 = ((clipped.x2 + div - 1) / div) * div;
    }

    uint16_t rows_per_buffer = displayio_area_height(&clipped);
    uint8_t pixels_per_word = (sizeof(uint32_t) * 8) / self->core.colorspace.depth;
    uint16_t pixels_per_buffer = displayio_area_size(&clipped);
    if (displayio_area_size(&clipped) > buffer_size * pixels_per_word) {
        rows_per_buffer = buffer_size * pixels_per_word / displayio_area_width(&clipped);
        if (rows_per_buffer == 0) {
            rows_per_buffer = 1;
        }
        // If pixels are packed by column then ensure rows_per_buffer is on a byte boundary.
        if (self->core.colorspace.depth < 8 && !self->core.colorspace.pixels_in_byte_share_row) {
            uint8_t pixels_per_byte = 8 / self->core.colorspace.depth;
            if (rows_per_buffer % pixels_per_byte != 0) {
                rows_per_buffer -= rows_per_buffer % pixels_per_byte;
            }
        }
        subrectangles = displayio_area_height(&clipped) / rows_per_buffer;
        if (displayio_area_height(&clipped) % rows_per_buffer != 0) {
            subrectangles++;
        }
        pixels_per_buffer = rows_per_buffer * displayio_area_width(&clipped);
        buffer_size = pixels_per_buffer / pixels_per_word;
        if (pixels_per_buffer % pixels_per_word) {
            buffer_size += 1;
        }
    }

    // Allocated and shared as a uint32_t array so the compiler knows the
    // alignment everywhere.
    uint32_t buffer[buffer_size];
    uint32_t mask_length = (pixels_per_buffer / 32) + 1;
    uint32_t mask[mask_length];
    uint16_t remaining_rows = displayio_area_height(&clipped);

    for (uint16_t j = 0; j < subrectangles; j++) {
        displayio_area_t subrectangle = {
            .x1 = clipped.x1,
            .y1 = clipped.y1 + rows_per_buffer * j,
            .x2 = clipped.x2,
            .y2 = clipped.y1 + rows_per_buffer * (j + 1)
        };

        if (remaining_rows < rows_per_buffer) {
            subrectangle.y2 = subrectangle.y1 + remaining_rows;
        }
        remaining_rows -= rows_per_buffer;

        memset(mask, 0, mask_length * sizeof(mask[0]));
        memset(buffer, 0, buffer_size * sizeof(buffer[0]));

        rambus_display_core_fill_area(&self->core, &subrectangle, mask, self->addr);

        // uint8_t *buf = (uint8_t *)self->bufinfo.buf, *endbuf = buf + self->bufinfo.len;
        // (void)endbuf; // Hint to compiler that endbuf is "used" even if NDEBUG
        // buf += self->first_pixel_offset;

        // size_t rowstride = self->row_stride;
        // uint8_t *dest = buf + subrectangle.y1 * rowstride + subrectangle.x1 * self->core.colorspace.depth / 8;
        // uint8_t *src = (uint8_t *)buffer;
        // size_t rowsize = (subrectangle.x2 - subrectangle.x1) * self->core.colorspace.depth / 8;

        // for (uint16_t i = subrectangle.y1; i < subrectangle.y2; i++) {
        //     assert(dest >= buf && dest < endbuf && dest + rowsize <= endbuf);
        //     MARK_ROW_DIRTY(i);
        //     memcpy(dest, src, rowsize);
        //     dest += rowstride;
        //     src += rowsize;
        // }

        // TODO(tannewt): Make refresh displays faster so we don't starve other
        // background tasks.
        #if CIRCUITPY_USB
        usb_background();
        #endif
    }
    return true;
}

// STATIC void _send_pixels(rambus_rambusdisplay_obj_t *self, uint8_t *pixels, uint32_t length) {
//     if (!self->display_bus.data_as_commands) {
//         self->bus.send(self->bus.bus, DISPLAY_COMMAND, CHIP_SELECT_TOGGLE_EVERY_BYTE, &self->write_ram_command, 1);
//     }
//     self->bus.send(self->bus.bus, DISPLAY_DATA, CHIP_SELECT_UNTOUCHED, pixels, length);
// }

// STATIC void _send_buffer_to_display(rambus_rambusdisplay_obj_t *self) {
//     // Can't acquire display bus; skip the rest of the data.
//     if (!displayio_display_bus_is_free(&self->bus)) {
//         return false;
//     }

//     displayio_display_bus_begin_transaction(&self->bus);
//     _send_pixels(self, (uint8_t *)buffer, subrectangle_size_bytes);
//     displayio_display_bus_end_transaction(&self->bus);
// }

STATIC void _refresh_display(rambus_rambusdisplay_obj_t *self) {
    // self->framebuffer_protocol->get_bufinfo(self->framebuffer, &self->bufinfo);
    if (_deinited(self)) {
        return;
    }
    displayio_display_core_start_refresh(&self->core);
    self->addr->offset = 0;
    const displayio_area_t *current_area = _get_refresh_areas(self);
    if (current_area) {
        bool transposed = (self->core.rotation == 90 || self->core.rotation == 270);
        int row_count = transposed ? self->core.width : self->core.height;
        uint8_t dirty_row_bitmask[(row_count + 7) / 8];
        memset(dirty_row_bitmask, 0, sizeof(dirty_row_bitmask));
        // self->framebuffer_protocol->get_bufinfo(self->framebuffer, &self->bufinfo);
        while (current_area != NULL) {
            _refresh_area(self, current_area, dirty_row_bitmask);
            current_area = current_area->next;
        }

        // TODO: This is where we need to trigger whatever protocol/program to sync from ram to display
        // self->framebuffer_protocol->swapbuffers(self->framebuffer, dirty_row_bitmask);
    }
    displayio_display_core_finish_refresh(&self->core);
}

void rambus_rambusdisplay_set_rotation(rambus_rambusdisplay_obj_t *self, int rotation) {
    bool transposed = (self->core.rotation == 90 || self->core.rotation == 270);
    bool will_transposed = (rotation == 90 || rotation == 270);
    if (transposed != will_transposed) {
        int tmp = self->core.width;
        self->core.width = self->core.height;
        self->core.height = tmp;
    }
    displayio_display_core_set_rotation(&self->core, rotation);
    // if (self == &displays[0].framebuffer_display) {
    //     supervisor_stop_terminal();
    //     supervisor_start_terminal(self->core.width, self->core.height);
    // }
    if (self->core.current_group != NULL) {
        displayio_group_update_transform(self->core.current_group, &self->core.transform);
    }
}

uint16_t rambus_rambusdisplay_get_rotation(rambus_rambusdisplay_obj_t *self) {
    return self->core.rotation;
}


bool rambus_rambusdisplay_refresh(rambus_rambusdisplay_obj_t *self, uint32_t target_ms_per_frame, uint32_t maximum_ms_per_real_frame) {
    printl("refreshing RAMBusDisplay");
    if (!self->auto_refresh && !self->first_manual_refresh && (target_ms_per_frame != NO_FPS_LIMIT)) {
        uint64_t current_time = supervisor_ticks_ms64();
        uint32_t current_ms_since_real_refresh = current_time - self->core.last_refresh;
        // Test to see if the real frame time is below our minimum.
        if (current_ms_since_real_refresh > maximum_ms_per_real_frame) {
            mp_raise_RuntimeError(MP_ERROR_TEXT("Below minimum frame rate"));
        }
        uint32_t current_ms_since_last_call = current_time - self->last_refresh_call;
        self->last_refresh_call = current_time;
        // Skip the actual refresh to help catch up.
        if (current_ms_since_last_call > target_ms_per_frame) {
            return false;
        }
        uint32_t remaining_time = target_ms_per_frame - (current_ms_since_real_refresh % target_ms_per_frame);
        // We're ahead of the game so wait until we align with the frame rate.
        while (supervisor_ticks_ms64() - self->last_refresh_call < remaining_time) {
            RUN_BACKGROUND_TASKS;
        }
    }
    self->first_manual_refresh = false;
    _refresh_display(self);
    return true;
}

bool rambus_rambusdisplay_get_auto_refresh(rambus_rambusdisplay_obj_t *self) {
    return self->auto_refresh;
}

void rambus_rambusdisplay_set_auto_refresh(rambus_rambusdisplay_obj_t *self,
    bool auto_refresh) {
    self->first_manual_refresh = !auto_refresh;
    if (auto_refresh != self->auto_refresh) {
        if (auto_refresh) {
            supervisor_enable_tick();
        } else {
            supervisor_disable_tick();
        }
    }
    self->auto_refresh = auto_refresh;
}

STATIC void _update_backlight(rambus_rambusdisplay_obj_t *self) {
    // TODO(tannewt): Fade the backlight based on it's existing value and a target value. The target
    // should account for ambient light when possible.
}

void rambus_rambusdisplay_background(rambus_rambusdisplay_obj_t *self) {
    _update_backlight(self);

    if (self->auto_refresh && (supervisor_ticks_ms64() - self->core.last_refresh) > self->native_ms_per_frame) {
        _refresh_display(self);
    }
}

void release_rambusdisplay(rambus_rambusdisplay_obj_t *self) {
    rambus_rambusdisplay_set_auto_refresh(self, false);
    release_display_core(&self->core);
    if (self->ram != NULL) {
        shared_module_rambus_ram_release(self->ram);
    }
    // free(self->addr);
    self->base.type = &mp_type_NoneType;
}

void rambus_rambusdisplay_collect_ptrs(rambus_rambusdisplay_obj_t *self) {
    displayio_display_core_collect_ptrs(&self->core);
}

void rambus_rambusdisplay_reset(rambus_rambusdisplay_obj_t *self) {
    rambus_rambusdisplay_set_auto_refresh(self, true);
    displayio_display_core_set_root_group(&self->core, &circuitpython_splash);
    self->core.full_refresh = true;
}

mp_obj_t rambus_rambusdisplay_get_root_group(rambus_rambusdisplay_obj_t *self) {
    if (self->core.current_group == NULL) {
        return mp_const_none;
    }
    return self->core.current_group;
}

mp_obj_t rambus_rambusdisplay_set_root_group(rambus_rambusdisplay_obj_t *self, displayio_group_t *root_group) {
    printl("setting root group");
    bool ok = displayio_display_core_set_root_group(&self->core, root_group);
    if (!ok) {
        mp_raise_ValueError(MP_ERROR_TEXT("Group already used"));
    }
    return mp_const_none;
}
