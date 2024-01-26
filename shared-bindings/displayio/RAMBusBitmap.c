


#include "shared-bindings/displayio/RAMBusBitmap.h"
#include "shared-module/displayio/RAMBusBitmap.h"
#include "shared-module/rambus/RAM.h"

#include <stdint.h>

#include "shared/runtime/context_manager_helpers.h"
#include "py/mpprint.h"
#include "py/binary.h"
#include "py/objarray.h"
#include "py/objproperty.h"
#include "py/objtype.h"
#include "py/runtime.h"
#include "shared-bindings/util.h"
#include "extmod/vfs_fat.h"

#include "shared-bindings/displayio/RAMBusBitmap.h"

//| class RAMBusBitmap:
//|     """Stores values of a certain size in a 2D array stored in BusRam
//|
//|     Bitmaps can be treated as read-only buffers. If the number of bits in a pixel is 8, 16, or 32; and the number of bytes
//|     per row is a multiple of 4, then the resulting memoryview will correspond directly with the bitmap's contents. Otherwise,
//|     the bitmap data is packed into the memoryview with unspecified padding.
//|
//|     A Bitmap can be treated as a buffer, allowing its content to be
//|     viewed and modified using e.g., with ``ulab.numpy.frombuffer``,
//|     but the `displayio.Bitmap.dirty` method must be used to inform
//|     displayio when a bitmap was modified through the buffer interface.
//|
//|     `bitmaptools.arrayblit` can also be useful to move data efficiently
//|     into a Bitmap."""
//|
//|     def __init__(self, width: int, height: int, ram: rambus.RAM, addr: int, file: Union[str, typing.BinaryIO, None] = None, value_count: int = 65535) -> None:
//|         """Create a Bitmap object with the given fixed size. Each pixel stores a value that is used to
//|         index into a corresponding palette. This enables differently colored sprites to share the
//|         underlying Bitmap. value_count is used to minimize the memory used to store the Bitmap.
//|
//|         :param int width: The number of values wide
//|         :param int height: The number of values high
//|         :param RAM ram: The ram to store pixel data in.
//|         :param int addr: The address within ram to begin storing this bitmap.
//|         :param file file: The optional name of the bitmap file to read into RAM. For backwards compatibility, a file opened in binary mode may also be passed.
//|         :param int value_count: The number of possible pixel values. Ignored if loading from file.
//|         """
//|         ...
STATIC mp_obj_t displayio_rambusbitmap_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_ram, ARG_addr, ARG_file, ARG_width, ARG_height, ARG_value_count };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_ram, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_addr, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_file, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_width, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_height, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_value_count, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 65535} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint32_t width = mp_arg_validate_int_range(args[ARG_width].u_int, 0, 32767, MP_QSTR_width);
    uint32_t height = mp_arg_validate_int_range(args[ARG_height].u_int, 0, 32767, MP_QSTR_height);
    uint32_t value_count = mp_arg_validate_int_range(args[ARG_value_count].u_int, 1, 65536, MP_QSTR_value_count);
    rambus_ram_obj_t *ram = MP_OBJ_TO_PTR(args[ARG_ram].u_obj);
    addr_t addr = mp_arg_validate_int_range(args[ARG_addr].u_int, 0, rambus_ram_get_size(ram), MP_QSTR_addr);
    pyb_file_obj_t *file = NULL;

    if (args[ARG_file].u_obj != mp_const_none) {
        mp_obj_t file_arg;
        if (mp_obj_is_str(args[ARG_file].u_obj)) {
            file_arg = mp_call_function_2(MP_OBJ_FROM_PTR(&mp_builtin_open_obj), args[ARG_file].u_obj, MP_ROM_QSTR(MP_QSTR_rb));
        } else {
            file_arg = args[ARG_file].u_obj;
        }

        if (!mp_obj_is_type(file_arg, &mp_type_fileio)) {
            mp_raise_TypeError(MP_ERROR_TEXT("file must be a file opened in byte mode"));
        }

        file = MP_OBJ_TO_PTR(file_arg);
    }

    uint32_t bits = 1;

    while ((value_count - 1) >> bits) {
        if (bits < 8) {
            bits <<= 1;
        } else {
            bits += 8;
        }
    }

    displayio_rambusbitmap_t *self = mp_obj_malloc(displayio_rambusbitmap_t, &displayio_rambusbitmap_type);
    common_hal_displayio_rambusbitmap_construct(self, width, height, ram, addr, file, bits, false);

    return MP_OBJ_FROM_PTR(self);
}

STATIC void check_for_deinit(displayio_rambusbitmap_t *self) {
    if (common_hal_displayio_rambusbitmap_deinited(self)) {
        raise_deinited_error();
    }
}

//|     width: int
//|     """Width of the bitmap. (read only)"""
STATIC mp_obj_t displayio_rambusbitmap_obj_get_width(mp_obj_t self_in) {
    displayio_rambusbitmap_t *self = MP_OBJ_TO_PTR(self_in);

    check_for_deinit(self);
    return MP_OBJ_NEW_SMALL_INT(common_hal_displayio_rambusbitmap_get_width(self));
}

MP_DEFINE_CONST_FUN_OBJ_1(displayio_rambusbitmap_get_width_obj, displayio_rambusbitmap_obj_get_width);

MP_PROPERTY_GETTER(displayio_rambusbitmap_width_obj,
    (mp_obj_t)&displayio_rambusbitmap_get_width_obj);

//|     height: int
//|     """Height of the bitmap. (read only)"""
STATIC mp_obj_t displayio_rambusbitmap_obj_get_height(mp_obj_t self_in) {
    displayio_rambusbitmap_t *self = MP_OBJ_TO_PTR(self_in);

    check_for_deinit(self);
    return MP_OBJ_NEW_SMALL_INT(common_hal_displayio_rambusbitmap_get_height(self));
}

MP_DEFINE_CONST_FUN_OBJ_1(displayio_rambusbitmap_get_height_obj, displayio_rambusbitmap_obj_get_height);

MP_PROPERTY_GETTER(displayio_rambusbitmap_height_obj,
    (mp_obj_t)&displayio_rambusbitmap_get_height_obj);

//|     bits_per_value: int
//|     """Bits per Pixel of the bitmap. (read only)"""
STATIC mp_obj_t displayio_rambusbitmap_obj_get_bits_per_value(mp_obj_t self_in) {
    displayio_rambusbitmap_t *self = MP_OBJ_TO_PTR(self_in);

    check_for_deinit(self);
    return MP_OBJ_NEW_SMALL_INT(common_hal_displayio_rambusbitmap_get_bits_per_value(self));
}

MP_DEFINE_CONST_FUN_OBJ_1(displayio_rambusbitmap_get_bits_per_value_obj, displayio_rambusbitmap_obj_get_bits_per_value);

MP_PROPERTY_GETTER(displayio_rambusbitmap_bits_per_value_obj,
    (mp_obj_t)&displayio_rambusbitmap_get_bits_per_value_obj);

//|     size: int
//|     """Size of the raw bitmap data in bytes. (read only)"""
STATIC mp_obj_t displayio_rambusbitmap_obj_get_size(mp_obj_t self_in) {
    displayio_rambusbitmap_t *self = MP_OBJ_TO_PTR(self_in);

    check_for_deinit(self);
    return mp_obj_new_int(common_hal_displayio_rambusbitmap_get_size(self));
}

MP_DEFINE_CONST_FUN_OBJ_1(displayio_rambusbitmap_get_size_obj, displayio_rambusbitmap_obj_get_size);

MP_PROPERTY_GETTER(displayio_rambusbitmap_size_obj,
    (mp_obj_t)&displayio_rambusbitmap_get_size_obj);

//|     pixel_shader: Union[ColorConverter, Palette]
//|     """The image's pixel_shader.  The type depends on the underlying
//|     bitmap's structure.  The pixel shader can be modified (e.g., to set the
//|     transparent pixel or, for palette shaded images, to update the palette.)"""
//|
STATIC mp_obj_t displayio_rambusbitmap_obj_get_pixel_shader(mp_obj_t self_in) {
    displayio_rambusbitmap_t *self = MP_OBJ_TO_PTR(self_in);
    return common_hal_displayio_rambusbitmap_get_pixel_shader(self);
}
MP_DEFINE_CONST_FUN_OBJ_1(displayio_rambusbitmap_get_pixel_shader_obj, displayio_rambusbitmap_obj_get_pixel_shader);
MP_PROPERTY_GETTER(displayio_rambusbitmap_pixel_shader_obj,
    (mp_obj_t)&displayio_rambusbitmap_get_pixel_shader_obj);


//|     def __getitem__(self, index: Union[Tuple[int, int], int]) -> int:
//|         """Returns the value at the given index. The index can either be an x,y tuple or an int equal
//|         to ``y * width + x``.
//|
//|         This allows you to::
//|
//|           print(bitmap[0,1])"""
//|         ...
//|     def __setitem__(self, index: Union[Tuple[int, int], int], value: int) -> None:
//|         """Sets the value at the given index. The index can either be an x,y tuple or an int equal
//|         to ``y * width + x``.
//|
//|         This allows you to::
//|
//|           bitmap[0,1] = 3"""
//|         ...
STATIC mp_obj_t rambusbitmap_subscr(mp_obj_t self_in, mp_obj_t index_obj, mp_obj_t value_obj) {
    if (value_obj == mp_const_none) {
        // delete item
        mp_raise_AttributeError(MP_ERROR_TEXT("Cannot delete values"));
        return mp_const_none;
    }

    displayio_rambusbitmap_t *self = MP_OBJ_TO_PTR(self_in);
    check_for_deinit(self);

    if (mp_obj_is_type(index_obj, &mp_type_slice)) {
        // TODO(tannewt): Implement subscr after slices support start, stop and step tuples.
        mp_raise_NotImplementedError(MP_ERROR_TEXT("Slices not supported"));
        return mp_const_none;
    }

    uint16_t x = 0;
    uint16_t y = 0;
    if (mp_obj_is_small_int(index_obj)) {
        mp_int_t i = MP_OBJ_SMALL_INT_VALUE(index_obj);
        int total_length = self->width * self->height;
        if (i < 0 || i >= total_length) {
            mp_raise_IndexError_varg(MP_ERROR_TEXT("%q must be %d-%d"), MP_QSTR_index, 0, total_length - 1);
        }

        x = i % self->width;
        y = i / self->width;
    } else {
        mp_obj_t *items;
        mp_obj_get_array_fixed_n(index_obj, 2, &items);
        mp_int_t x_in = mp_obj_get_int(items[0]);
        if (x_in < 0 || x_in >= self->width) {
            mp_raise_IndexError_varg(MP_ERROR_TEXT("%q must be %d-%d"), MP_QSTR_x, 0, self->width - 1);
        }
        mp_int_t y_in = mp_obj_get_int(items[1]);
        if (y_in < 0 || y_in >= self->height) {
            mp_raise_IndexError_varg(MP_ERROR_TEXT("%q must be %d-%d"), MP_QSTR_y, 0, self->height - 1);
        }
        x = x_in;
        y = y_in;
    }

    if (value_obj == MP_OBJ_SENTINEL) {
        // load
        return MP_OBJ_NEW_SMALL_INT(common_hal_displayio_rambusbitmap_get_pixel(self, x, y));
    } else {
        mp_uint_t value = (mp_uint_t)mp_arg_validate_int_range(
            mp_obj_get_int(value_obj), 0,
            (UINT32_MAX >> (32 - common_hal_displayio_rambusbitmap_get_bits_per_value(self))), MP_QSTR_value);
        common_hal_displayio_rambusbitmap_set_pixel(self, x, y, value);
    }
    return mp_const_none;
}

//|     def load_from_file(self, file: Union[str, typing.BinaryIO, None]) -> None:
//|         """Load a bitmap from disk into this RAMBusBitmap instance. Will overwrite any bitmap data currently
//|            loaded or drawn in the bitmap and will adjust it's properties accordingly 
//|
//|         :param file file: The name of the bitmap file to read into RAM. For backwards compatibility, a file opened in binary mode may also be passed.
//|         """
//|         ...
STATIC mp_obj_t displayio_rambusbitmap_obj_load_from_file(mp_obj_t self_in, mp_obj_t file_in) {
    pyb_file_obj_t *file = NULL;

    if (file_in == mp_const_none) {
        mp_raise_ValueError_varg(MP_ERROR_TEXT("file is required"));
    }

    if (mp_obj_is_str(file_in)) {
        file_in = mp_call_function_2(MP_OBJ_FROM_PTR(&mp_builtin_open_obj), file_in, MP_ROM_QSTR(MP_QSTR_rb));
    }

    if (!mp_obj_is_type(file_in, &mp_type_fileio)) {
        mp_raise_TypeError(MP_ERROR_TEXT("file must be a file opened in byte mode"));
    }

    file = MP_OBJ_TO_PTR(file_in);
    displayio_rambusbitmap_t *self = MP_OBJ_TO_PTR(self_in);

    common_hal_displayio_rambusbitmap_load_from_file(self, file);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(displayio_rambusbitmap_load_from_file_obj, displayio_rambusbitmap_obj_load_from_file);

//|     def fill(self, value: int) -> None:
//|         """Fills the bitmap with the supplied palette index value."""
//|         ...
STATIC mp_obj_t displayio_rambusbitmap_obj_fill(mp_obj_t self_in, mp_obj_t value_obj) {
    displayio_rambusbitmap_t *self = MP_OBJ_TO_PTR(self_in);
    check_for_deinit(self);

    mp_uint_t value = (mp_uint_t)mp_arg_validate_int_range(mp_obj_get_int(value_obj), 0, (1u << common_hal_displayio_rambusbitmap_get_bits_per_value(self)) - 1, MP_QSTR_value);
    common_hal_displayio_rambusbitmap_fill(self, value);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(displayio_rambusbitmap_fill_obj, displayio_rambusbitmap_obj_fill);

//|     def dirty(self, x1: int = 0, y1: int = 0, x2: int = -1, y2: int = -1) -> None:
//|         """Inform displayio of bitmap updates done via the buffer
//|         protocol.
//|
//|         :param int x1: Minimum x-value for rectangular bounding box to be considered as modified
//|         :param int y1: Minimum y-value for rectangular bounding box to be considered as modified
//|         :param int x2: Maximum x-value (exclusive) for rectangular bounding box to be considered as modified
//|         :param int y2: Maximum y-value (exclusive) for rectangular bounding box to be considered as modified
//|
//|         If x1 or y1 are not specified, they are taken as 0.  If x2 or y2
//|         are not specified, or are given as -1, they are taken as the width
//|         and height of the image.  Thus, calling dirty() with the
//|         default arguments treats the whole bitmap as modified.
//|
//|         When a bitmap is modified through the buffer protocol, the
//|         display will not be properly updated unless the bitmap is
//|         notified of the "dirty rectangle" that encloses all modified
//|         pixels."""
//|         ...
STATIC mp_obj_t displayio_rambusbitmap_obj_dirty(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    displayio_rambusbitmap_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    check_for_deinit(self);

    enum { ARG_x1, ARG_y1, ARG_x2, ARG_y2 };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_x1, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_y1, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_x2, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_y2, MP_ARG_INT, {.u_int = -1} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    displayio_area_t dirty_area = {
        .x1 = args[ARG_x1].u_int,
        .y1 = args[ARG_y1].u_int,
        .x2 = args[ARG_x2].u_int == -1 ? self->width : args[ARG_x2].u_int,
        .y2 = args[ARG_y2].u_int == -1 ? self->height : args[ARG_y2].u_int,
    };

    displayio_rambusbitmap_set_dirty_area(self, &dirty_area);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(displayio_rambusbitmap_dirty_obj, 0, displayio_rambusbitmap_obj_dirty);

//|     def deinit(self) -> None:
//|         """Release resources allocated by Bitmap."""
//|         ...
//|
STATIC mp_obj_t displayio_rambusbitmap_obj_deinit(mp_obj_t self_in) {
    displayio_rambusbitmap_t *self = MP_OBJ_TO_PTR(self_in);
    common_hal_displayio_rambusbitmap_deinit(self);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(displayio_rambusbitmap_deinit_obj, displayio_rambusbitmap_obj_deinit);


STATIC const mp_rom_map_elem_t displayio_rambusbitmap_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_height), MP_ROM_PTR(&displayio_rambusbitmap_height_obj) },
    { MP_ROM_QSTR(MP_QSTR_width), MP_ROM_PTR(&displayio_rambusbitmap_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_bits_per_value), MP_ROM_PTR(&displayio_rambusbitmap_bits_per_value_obj) },
    { MP_ROM_QSTR(MP_QSTR_size), MP_ROM_PTR(&displayio_rambusbitmap_size_obj) },
    { MP_ROM_QSTR(MP_QSTR_pixel_shader), MP_ROM_PTR(&displayio_rambusbitmap_pixel_shader_obj) },
    { MP_ROM_QSTR(MP_QSTR_load_from_file), MP_ROM_PTR(&displayio_rambusbitmap_load_from_file_obj) },
    { MP_ROM_QSTR(MP_QSTR_fill), MP_ROM_PTR(&displayio_rambusbitmap_fill_obj) },
    { MP_ROM_QSTR(MP_QSTR_dirty), MP_ROM_PTR(&displayio_rambusbitmap_dirty_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&displayio_rambusbitmap_deinit_obj) },
};
STATIC MP_DEFINE_CONST_DICT(displayio_rambusbitmap_locals_dict, displayio_rambusbitmap_locals_dict_table);

// (the get_buffer protocol returns 0 for success, 1 for failure)
STATIC mp_int_t rambusbitmap_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags) {
    displayio_rambusbitmap_t *self = MP_OBJ_TO_PTR(self_in);
    return common_hal_displayio_rambusbitmap_get_buffer(self, bufinfo, flags);
}

MP_DEFINE_CONST_OBJ_TYPE(
    displayio_rambusbitmap_type,
    MP_QSTR_RAMBusBitmap,
    MP_TYPE_FLAG_HAS_SPECIAL_ACCESSORS,
    make_new, displayio_rambusbitmap_make_new,
    locals_dict, &displayio_rambusbitmap_locals_dict,
    subscr, rambusbitmap_subscr,
    buffer, rambusbitmap_get_buffer
    );
