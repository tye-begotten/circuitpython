/*
 * This file is part of the Micro Python project, http://micropython.org/
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

#include "shared-bindings/rambus/RAMBusDisplay.h"

#include <stdint.h>

#include "shared/runtime/context_manager_helpers.h"
#include "py/binary.h"
#include "py/objproperty.h"
#include "py/objtype.h"
#include "py/runtime.h"
#include "shared-bindings/displayio/Group.h"
#include "shared-bindings/microcontroller/Pin.h"
#include "shared-bindings/util.h"
#include "shared-module/displayio/__init__.h"



//| class RAMBusDisplay:
//|     """A framebuffered display that uses the RAMBus to store frame data before sending it to
//|     the target display or bus.
//|
//|     This acts like a normal display, requiring resources to be released using displayio.release_displays()
//|     which will free the RAM that is being used.
//|
//|     Most people should not use this class directly. Use a specific display driver instead that will
//|     contain the initialization sequence at minimum."""
//|
//|     def __init__(
//|         self,
//|         ram: RAM,
//|         addr: int,
//|         width: int,
//|         height: int,
//|         *,
//|         rotation: int = 0,
//|         color_depth: int = 16,
//|         grayscale: bool = False,
//|         pixels_in_byte_share_row: bool = True,
//|         bytes_per_cell: int = 1,
//|         reverse_pixels_in_byte: bool = False,
//|         auto_refresh: bool = True,
//|     ) -> None:
//|         r"""Create a RAMBusDisplay object using the given RAM instance and starting address.
//|
//|         :param rambus.RAM ram: The ram instance to write frambuffer data to.
//|         :param int addr: The starting address to write to RAM. There must be sufficient space to store
//|             one frame of pixel data based on width*height*bytes_per_pixel.
//|         :param int width: Width in pixels
//|         :param int height: Height in pixels
//|         :param int rotation: The rotation of the display in degrees clockwise. Must be in 90 degree increments (0, 90, 180, 270)
//|         :param int color_depth: The number of bits of color per pixel transmitted. (Some displays
//|             support 18 bit but 16 is easier to transmit. The last bit is extrapolated.)
//|         :param bool grayscale: True if the display only shows a single color.
//|         :param bool pixels_in_byte_share_row: True when pixels are less than a byte and a byte includes pixels from the same row of the display. When False, pixels share a column.
//|         :param int bytes_per_cell: Number of bytes per addressable memory location when color_depth < 8. When greater than one, bytes share a row or column according to pixels_in_byte_share_row.
//|         :param bool reverse_pixels_in_byte: Reverses the pixel order within each byte when color_depth < 8. Does not apply across multiple bytes even if there is more than one byte per cell (bytes_per_cell.)
//|         :param bool reverse_bytes_in_word: Reverses the order of bytes within a word when color_depth == 16
//|         :param bool auto_refresh: Automatically refresh the screen
//|         """
//|         ...
STATIC mp_obj_t rambus_rambusdisplay_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_ram, ARG_addr, ARG_width, ARG_height, ARG_rotation, ARG_color_depth, ARG_grayscale, ARG_pixels_in_byte_share_row,
           ARG_bytes_per_cell, ARG_reverse_pixels_in_byte, ARG_reverse_bytes_in_word, ARG_auto_refresh, NUM_ARGS };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_ram, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_addr, MP_ARG_REQUIRED | MP_ARG_INT},
        { MP_QSTR_width, MP_ARG_INT | MP_ARG_REQUIRED, },
        { MP_QSTR_height, MP_ARG_INT | MP_ARG_REQUIRED, },
        { MP_QSTR_rotation, MP_ARG_INT | MP_ARG_KW_ONLY, {.u_int = 0} },
        { MP_QSTR_color_depth, MP_ARG_INT | MP_ARG_KW_ONLY, {.u_int = 16} },
        { MP_QSTR_grayscale, MP_ARG_BOOL | MP_ARG_KW_ONLY, {.u_bool = false} },
        { MP_QSTR_pixels_in_byte_share_row, MP_ARG_BOOL | MP_ARG_KW_ONLY, {.u_bool = true} },
        { MP_QSTR_bytes_per_cell, MP_ARG_INT | MP_ARG_KW_ONLY, {.u_int = 1} },
        { MP_QSTR_reverse_pixels_in_byte, MP_ARG_BOOL | MP_ARG_KW_ONLY, {.u_bool = false} },
        { MP_QSTR_reverse_bytes_in_word, MP_ARG_BOOL | MP_ARG_KW_ONLY, {.u_bool = true} },
        { MP_QSTR_auto_refresh, MP_ARG_BOOL | MP_ARG_KW_ONLY, {.u_bool = true} },
    };

    MP_STATIC_ASSERT(MP_ARRAY_SIZE(allowed_args) == NUM_ARGS);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    rambus_ram_obj_t *ram = validate_obj_is_ram(args[ARG_ram].u_obj, MP_QSTR_ram);
    addr_t ram_size = rambus_ram_get_size(ram);
    uint32_t addr = args[ARG_addr].u_int;
    uint32_t width = args[ARG_width].u_int;
    uint32_t height = args[ARG_height].u_int;
    uint32_t color_depth = args[ARG_color_depth].u_int;
    uint32_t req_size = width * height * ((color_depth / 8) + (color_depth % 8 > 0 ? 1 : 0));
    if (addr + req_size > ram_size) {
        mp_raise_ValueError_varg(MP_ERROR_TEXT("Display requires %d bytes starting from addr %x but only has %d available."), 
            req_size, addr, ram_size - addr);
    }

    mp_int_t rotation = args[ARG_rotation].u_int;
    if (rotation % 90 != 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("Display rotation must be in 90 degree increments"));
    }

    printl("allocating display");

    primary_display_t *disp = allocate_display_or_raise();
    rambus_rambusdisplay_obj_t *self = &disp->rambus_display;

    printlf("got rambusdisplay obj: %s", self);

    self->base.type = &rambus_rambusdisplay_type;
    rambus_rambusdisplay_construct(
        self,
        ram,
        addr,
        width,
        height,
        rotation,
        color_depth,
        args[ARG_grayscale].u_bool,
        args[ARG_pixels_in_byte_share_row].u_bool,
        args[ARG_bytes_per_cell].u_bool,
        args[ARG_reverse_pixels_in_byte].u_bool,
        args[ARG_reverse_bytes_in_word].u_bool,
        args[ARG_auto_refresh].u_bool
        );

    return self;
}

// Helper to ensure we have the native super class instead of a subclass.
static rambus_rambusdisplay_obj_t *native_display(mp_obj_t display_obj) {
    mp_obj_t native_display = mp_obj_cast_to_native_base(display_obj, &rambus_rambusdisplay_type);
    mp_obj_assert_native_inited(native_display);
    return MP_OBJ_TO_PTR(native_display);
}

// Undocumented show() implementation with a friendly error message.
STATIC mp_obj_t rambus_rambusdisplay_obj_show(mp_obj_t self_in, mp_obj_t group_in) {
    mp_raise_AttributeError(MP_ERROR_TEXT(".show(x) removed. Use .root_group = x"));
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(rambus_rambusdisplay_show_obj, rambus_rambusdisplay_obj_show);

//|     def refresh(
//|         self,
//|         *,
//|         target_frames_per_second: Optional[int] = None,
//|         minimum_frames_per_second: int = 0
//|     ) -> bool:
//|         """When auto_refresh is off, and :py:attr:`target_frames_per_second` is not `None` this waits
//|         for the target frame rate and then refreshes the display,
//|         returning `True`. If the call has taken too long since the last refresh call for the given
//|         target frame rate, then the refresh returns `False` immediately without updating the screen to
//|         hopefully help getting caught up.
//|
//|         If the time since the last successful refresh is below the minimum frame rate, then an
//|         exception will be raised. The default :py:attr:`minimum_frames_per_second` of 0 disables this behavior.
//|
//|         When auto_refresh is off, and :py:attr:`target_frames_per_second` is `None` this
//|         will update the display immediately.
//|
//|         When auto_refresh is on, updates the display immediately. (The display will also update
//|         without calls to this.)
//|
//|         :param Optional[int] target_frames_per_second: The target frame rate that :py:func:`refresh` should try to
//|             achieve. Set to `None` for immediate refresh.
//|         :param int minimum_frames_per_second: The minimum number of times the screen should be updated per second.
//|         """
//|         ...
STATIC mp_obj_t rambus_rambusdisplay_obj_refresh(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_target_frames_per_second, ARG_minimum_frames_per_second };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_target_frames_per_second, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_obj = mp_const_none} },
        { MP_QSTR_minimum_frames_per_second, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    rambus_rambusdisplay_obj_t *self = native_display(pos_args[0]);
    uint32_t maximum_ms_per_real_frame = NO_FPS_LIMIT;
    mp_int_t minimum_frames_per_second = args[ARG_minimum_frames_per_second].u_int;
    if (minimum_frames_per_second > 0) {
        maximum_ms_per_real_frame = 1000 / minimum_frames_per_second;
    }

    uint32_t target_ms_per_frame;
    if (args[ARG_target_frames_per_second].u_obj == mp_const_none) {
        target_ms_per_frame = NO_FPS_LIMIT;
    } else {
        target_ms_per_frame = 1000 / mp_obj_get_int(args[ARG_target_frames_per_second].u_obj);
    }
    return mp_obj_new_bool(rambus_rambusdisplay_refresh(self, target_ms_per_frame, maximum_ms_per_real_frame));
}
MP_DEFINE_CONST_FUN_OBJ_KW(rambus_rambusdisplay_refresh_obj, 1, rambus_rambusdisplay_obj_refresh);

//|     auto_refresh: bool
//|     """True when the display is refreshed automatically."""
STATIC mp_obj_t rambus_rambusdisplay_obj_get_auto_refresh(mp_obj_t self_in) {
    rambus_rambusdisplay_obj_t *self = native_display(self_in);
    return mp_obj_new_bool(rambus_rambusdisplay_get_auto_refresh(self));
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_rambusdisplay_get_auto_refresh_obj, rambus_rambusdisplay_obj_get_auto_refresh);

STATIC mp_obj_t rambus_rambusdisplay_obj_set_auto_refresh(mp_obj_t self_in, mp_obj_t auto_refresh) {
    rambus_rambusdisplay_obj_t *self = native_display(self_in);

    rambus_rambusdisplay_set_auto_refresh(self, mp_obj_is_true(auto_refresh));

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(rambus_rambusdisplay_set_auto_refresh_obj, rambus_rambusdisplay_obj_set_auto_refresh);

MP_PROPERTY_GETSET(rambus_rambusdisplay_auto_refresh_obj,
    (mp_obj_t)&rambus_rambusdisplay_get_auto_refresh_obj,
    (mp_obj_t)&rambus_rambusdisplay_set_auto_refresh_obj);

//|     brightness: float
//|     """The brightness of the display as a float. 0.0 is off and 1.0 is full brightness."""
STATIC mp_obj_t rambus_rambusdisplay_obj_get_brightness(mp_obj_t self_in) {
    rambus_rambusdisplay_obj_t *self = native_display(self_in);
    mp_float_t brightness = rambus_rambusdisplay_get_brightness(self);
    if (brightness < 0) {
        mp_raise_RuntimeError(MP_ERROR_TEXT("Brightness not adjustable"));
    }
    return mp_obj_new_float(brightness);
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_rambusdisplay_get_brightness_obj, rambus_rambusdisplay_obj_get_brightness);

STATIC mp_obj_t rambus_rambusdisplay_obj_set_brightness(mp_obj_t self_in, mp_obj_t brightness_obj) {
    rambus_rambusdisplay_obj_t *self = native_display(self_in);
    mp_float_t brightness = mp_obj_get_float(brightness_obj);
    if (brightness < 0.0f || brightness > 1.0f) {
        mp_raise_ValueError_varg(MP_ERROR_TEXT("%q must be %d-%d"), MP_QSTR_brightness, 0, 1);
    }
    bool ok = rambus_rambusdisplay_set_brightness(self, brightness);
    if (!ok) {
        mp_raise_RuntimeError(MP_ERROR_TEXT("Brightness not adjustable"));
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(rambus_rambusdisplay_set_brightness_obj, rambus_rambusdisplay_obj_set_brightness);

MP_PROPERTY_GETSET(rambus_rambusdisplay_brightness_obj,
    (mp_obj_t)&rambus_rambusdisplay_get_brightness_obj,
    (mp_obj_t)&rambus_rambusdisplay_set_brightness_obj);

//|     width: int
//|     """Gets the width of the framebuffer"""
STATIC mp_obj_t rambus_rambusdisplay_obj_get_width(mp_obj_t self_in) {
    rambus_rambusdisplay_obj_t *self = native_display(self_in);
    return MP_OBJ_NEW_SMALL_INT(rambus_rambusdisplay_get_width(self));
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_rambusdisplay_get_width_obj, rambus_rambusdisplay_obj_get_width);

MP_PROPERTY_GETTER(rambus_rambusdisplay_width_obj,
    (mp_obj_t)&rambus_rambusdisplay_get_width_obj);

//|     height: int
//|     """Gets the height of the framebuffer"""
STATIC mp_obj_t rambus_rambusdisplay_obj_get_height(mp_obj_t self_in) {
    rambus_rambusdisplay_obj_t *self = native_display(self_in);
    return MP_OBJ_NEW_SMALL_INT(rambus_rambusdisplay_get_height(self));
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_rambusdisplay_get_height_obj, rambus_rambusdisplay_obj_get_height);

MP_PROPERTY_GETTER(rambus_rambusdisplay_height_obj,
    (mp_obj_t)&rambus_rambusdisplay_get_height_obj);

//|     rotation: int
//|     """The rotation of the display as an int in degrees."""
STATIC mp_obj_t rambus_rambusdisplay_obj_get_rotation(mp_obj_t self_in) {
    rambus_rambusdisplay_obj_t *self = native_display(self_in);
    return MP_OBJ_NEW_SMALL_INT(rambus_rambusdisplay_get_rotation(self));
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_rambusdisplay_get_rotation_obj, rambus_rambusdisplay_obj_get_rotation);
STATIC mp_obj_t rambus_rambusdisplay_obj_set_rotation(mp_obj_t self_in, mp_obj_t value) {
    rambus_rambusdisplay_obj_t *self = native_display(self_in);
    rambus_rambusdisplay_set_rotation(self, mp_obj_get_int(value));
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(rambus_rambusdisplay_set_rotation_obj, rambus_rambusdisplay_obj_set_rotation);


MP_PROPERTY_GETSET(rambus_rambusdisplay_rotation_obj,
    (mp_obj_t)&rambus_rambusdisplay_get_rotation_obj,
    (mp_obj_t)&rambus_rambusdisplay_set_rotation_obj);


//|     def fill_row(self, y: int, buffer: WriteableBuffer) -> WriteableBuffer:
//|         """Extract the pixels from a single row
//|
//|         :param int y: The top edge of the area
//|         :param ~circuitpython_typing.WriteableBuffer buffer: The buffer in which to place the pixel data
//|         """
//|         ...
STATIC mp_obj_t rambus_rambusdisplay_obj_fill_row(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_y, ARG_buffer };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_y, MP_ARG_INT | MP_ARG_REQUIRED, {.u_int = -1} },
        { MP_QSTR_buffer, MP_ARG_OBJ | MP_ARG_REQUIRED, {} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    rambus_rambusdisplay_obj_t *self = native_display(pos_args[0]);
    mp_int_t y = args[ARG_y].u_int;
    mp_obj_t *result = args[ARG_buffer].u_obj;

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(result, &bufinfo, MP_BUFFER_WRITE);

    if (bufinfo.typecode != BYTEARRAY_TYPECODE) {
        mp_raise_ValueError(MP_ERROR_TEXT("Buffer is not a bytearray."));
    }
    if (self->core.colorspace.depth != 16) {
        mp_raise_ValueError(MP_ERROR_TEXT("Display must have a 16 bit colorspace."));
    }

    displayio_area_t area = {
        .x1 = 0,
        .y1 = y,
        .x2 = self->core.width,
        .y2 = y + 1
    };
    uint8_t pixels_per_word = (sizeof(uint32_t) * 8) / self->core.colorspace.depth;
    uint16_t buffer_size = self->core.width / pixels_per_word;
    uint16_t pixels_per_buffer = displayio_area_size(&area);
    if (pixels_per_buffer % pixels_per_word) {
        buffer_size += 1;
    }

    uint32_t *result_buffer = bufinfo.buf;
    size_t result_buffer_size = bufinfo.len;

    if (result_buffer_size >= (buffer_size * 4)) {
        volatile uint32_t mask_length = (pixels_per_buffer / 32) + 1;
        uint32_t mask[mask_length];

        for (uint16_t k = 0; k < mask_length; k++) {
            mask[k] = 0x00000000;
        }

        displayio_display_core_fill_area(&self->core, &area, mask, result_buffer);
        return result;
    } else {
        mp_raise_ValueError(MP_ERROR_TEXT("Buffer too small"));
    }
}
MP_DEFINE_CONST_FUN_OBJ_KW(rambus_rambusdisplay_fill_row_obj, 1, rambus_rambusdisplay_obj_fill_row);

//|     root_group: displayio.Group
//|     """The root group on the display.
//|     If the root group is set to `displayio.CIRCUITPYTHON_TERMINAL`, the default CircuitPython terminal will be shown.
//|     If the root group is set to ``None``, no output will be shown.
//|     """
//|
STATIC mp_obj_t rambus_rambusdisplay_obj_get_root_group(mp_obj_t self_in) {
    rambus_rambusdisplay_obj_t *self = native_display(self_in);
    return rambus_rambusdisplay_get_root_group(self);
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_rambusdisplay_get_root_group_obj, rambus_rambusdisplay_obj_get_root_group);

STATIC mp_obj_t rambus_rambusdisplay_obj_set_root_group(mp_obj_t self_in, mp_obj_t group_in) {
    rambus_rambusdisplay_obj_t *self = native_display(self_in);
    displayio_group_t *group = NULL;
    if (group_in != mp_const_none) {
        group = MP_OBJ_TO_PTR(native_group(group_in));
    }

    bool ok = rambus_rambusdisplay_set_root_group(self, group);
    if (!ok) {
        mp_raise_ValueError(MP_ERROR_TEXT("Group already used"));
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(rambus_rambusdisplay_set_root_group_obj, rambus_rambusdisplay_obj_set_root_group);

MP_PROPERTY_GETSET(rambus_rambusdisplay_root_group_obj,
    (mp_obj_t)&rambus_rambusdisplay_get_root_group_obj,
    (mp_obj_t)&rambus_rambusdisplay_set_root_group_obj);

STATIC const mp_rom_map_elem_t rambus_rambusdisplay_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_show), MP_ROM_PTR(&rambus_rambusdisplay_show_obj) },
    { MP_ROM_QSTR(MP_QSTR_refresh), MP_ROM_PTR(&rambus_rambusdisplay_refresh_obj) },
    { MP_ROM_QSTR(MP_QSTR_fill_row), MP_ROM_PTR(&rambus_rambusdisplay_fill_row_obj) },
    { MP_ROM_QSTR(MP_QSTR_auto_refresh), MP_ROM_PTR(&rambus_rambusdisplay_auto_refresh_obj) },
    { MP_ROM_QSTR(MP_QSTR_brightness), MP_ROM_PTR(&rambus_rambusdisplay_brightness_obj) },
    { MP_ROM_QSTR(MP_QSTR_width), MP_ROM_PTR(&rambus_rambusdisplay_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_height), MP_ROM_PTR(&rambus_rambusdisplay_height_obj) },
    { MP_ROM_QSTR(MP_QSTR_rotation), MP_ROM_PTR(&rambus_rambusdisplay_rotation_obj) },
    { MP_ROM_QSTR(MP_QSTR_root_group), MP_ROM_PTR(&rambus_rambusdisplay_root_group_obj) },
};
STATIC MP_DEFINE_CONST_DICT(rambus_rambusdisplay_locals_dict, rambus_rambusdisplay_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    rambus_rambusdisplay_type,
    MP_QSTR_RAMBusDisplay,
    MP_TYPE_FLAG_HAS_SPECIAL_ACCESSORS,
    make_new, rambus_rambusdisplay_make_new,
    locals_dict, &rambus_rambusdisplay_locals_dict
    );
