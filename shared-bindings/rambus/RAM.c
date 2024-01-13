#include <stdint.h>
#include <string.h>
#include "shared/runtime/context_manager_helpers.h"
#include "shared/runtime/buffer_helper.h"
#include "py/mpprint.h"
#include "py/binary.h"
#include "py/objproperty.h"
#include "py/objarray.h"
#include "py/runtime.h"
#include "py/runtime0.h"
#include "shared-bindings/microcontroller/Pin.h"
#include "shared-bindings/rambus/RAM.h"
#include "shared-module/rambus/RAM.h"
#include "shared-bindings/util.h"


//| class RAM:
//|     """Bus-driven RAM external to the MCU.
//|
//|     .. note:: The bus type determines how the memory is accessed and at what speed.
//|     """
//|
//|     def __init__(
//|         self,
//|         pg_size: int,
//|         pg_cnt: int,
//|         wrd_size: int,
//|         spi: busio.SPI,
//|         cs: microcontroller.Pin,
//|         hold: microcontroller.Pin,
//|     ) -> None:
//|         """Create a RAM object with the given SPI bus and pins. Page information
//|           is hardware specific, reference the datasheet.
//|
//|         .. note:: When ``variable_frequency`` is True, further PWM outputs may be
//|           limited because it may take more internal resources to be flexible. So,
//|           when outputting both fixed and flexible frequency signals construct the
//|           fixed outputs first.
//|
//|         :param int pg_size: The size of each memory page in bytes. 16-bit
//|         :param int pg_cnt: The number of memory pages. 16-bit
//|         :param int wrd_size: The size of each memory block in bits, normally 8. 8-bit
//|         :param ~busio.SPI spi: The SPI bus to connect to the RAM.
//|         :param ~microcontroller.Pin cs: The chip select pin to activate the RAM.
//|         :param ~microcontroller.Pin hold: The hold pin to pause/resume transmission/
//|         """
//|         ...
STATIC mp_obj_t rambus_ram_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_pg_size, ARG_pg_cnt, ARG_wrd_size, ARG_spi, ARG_cs, ARG_hold };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_pg_size, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_pg_cnt, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_wrd_size, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_spi, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_cs, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_hold, MP_ARG_REQUIRED | MP_ARG_OBJ },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    uint16_t pg_size = (uint16_t)mp_arg_validate_int_range(args[ARG_pg_size].u_int, 1, 0xffff, ARG_pg_size);
    uint16_t pg_cnt = (uint16_t)mp_arg_validate_int_range(args[ARG_pg_cnt].u_int, 1, 0xffff, ARG_pg_cnt);
    uint16_t wrd_size = (uint16_t)mp_arg_validate_int_range(args[ARG_wrd_size].u_int, 1, 255, ARG_wrd_size);
    busio_spi_obj_t* spi = validate_obj_is_spi_bus(args[ARG_spi].u_obj, MP_QSTR_spi);
    const mcu_pin_obj_t* cs = validate_obj_is_pin(args[ARG_cs].u_obj, MP_QSTR_cs);
    const mcu_pin_obj_t* hold = validate_obj_is_pin(args[ARG_hold].u_obj, MP_QSTR_hold);

    rambus_ram_obj_t *self = mp_obj_malloc(rambus_ram_obj_t, &rambus_ram_type);
    shared_module_rambus_ram_construct(self, RAM_TYPE_SRAM, pg_size, pg_cnt, wrd_size, spi, cs, hold);
    return MP_OBJ_FROM_PTR(self);
}

//|     def deinit(self) -> None:
//|         """Deinitialises the RAM and releases any hardware resources for reuse."""
//|         ...
STATIC mp_obj_t rambus_ram_deinit(mp_obj_t self_in) {
    shared_module_rambus_ram_release(self_in);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(rambus_ram_deinit_obj, rambus_ram_deinit);

//|     def __enter__(self) -> RAM:
//|         """No-op used by Context Managers."""
//|         ...
//  Provided by context manager helper.

//|     def __exit__(self) -> None:
//|         """Automatically deinitializes the hardware when exiting a context. See
//|         :ref:`lifetime-and-contextmanagers` for more info."""
//|         ...
STATIC mp_obj_t rambus_ram_obj___exit__(size_t n_args, const mp_obj_t *args) {
  rambus_ram_deinit(args[0]);
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rambus_ram___exit___obj, 4, 4, rambus_ram_obj___exit__);

//|     mode: int
//|     """Indicates which mode the RAM is operating in. May differ per hardware implementation.
//|     """
STATIC mp_obj_t rambus_ram_obj_get_mode(mp_obj_t self_in) {
    return mp_obj_new_int_from_uint(shared_module_rambus_ram_get_mode(self_in));
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_ram_get_mode_obj, rambus_ram_obj_get_mode);
MP_PROPERTY_GETTER(rambus_ram_mode_obj, (mp_obj_t)&rambus_ram_get_mode_obj);

//|     size: int
//|     """32 bit value indicating the total capacity of the RAM in bytes.
//|     """
STATIC mp_obj_t rambus_ram_obj_get_size(mp_obj_t self_in) {
    return mp_obj_new_int_from_uint(shared_module_rambus_ram_get_size(self_in));
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_ram_get_size_obj, rambus_ram_obj_get_size);
MP_PROPERTY_GETTER(rambus_ram_size_obj, (mp_obj_t)&rambus_ram_get_size_obj);

//|     start_addr: int
//|     """The start address of read/write RAM, normally 0x000000.
//|     """
STATIC mp_obj_t rambus_ram_obj_get_start_addr(mp_obj_t self_in) {
    return mp_obj_new_int_from_uint(shared_module_rambus_ram_get_start_addr(self_in));
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_ram_get_start_addr_obj, rambus_ram_obj_get_start_addr);
MP_PROPERTY_GETTER(rambus_ram_start_addr_obj, (mp_obj_t)&rambus_ram_get_start_addr_obj);

//|     end_addr: int
//|     """The end address of read/write RAM, normally start_addr + size.
//|     """
STATIC mp_obj_t rambus_ram_obj_get_end_addr(mp_obj_t self_in) {
    return mp_obj_new_int_from_uint(shared_module_rambus_ram_get_end_addr(self_in));
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_ram_get_end_addr_obj, rambus_ram_obj_get_end_addr);
MP_PROPERTY_GETTER(rambus_ram_end_addr_obj, (mp_obj_t)&rambus_ram_get_end_addr_obj);

STATIC addr_t check_addr(rambus_ram_obj_t *self, qstr arg, mp_arg_val_t *args) {
    return (addr_t)mp_arg_validate_int_range(args[arg].u_int, shared_module_rambus_ram_get_start_addr(self), 
        shared_module_rambus_ram_get_end_addr(self), arg);
}

STATIC uint8_t* check_buffer(rambus_ram_obj_t *self, int buf_type, qstr arg, mp_arg_val_t *args, int32_t start, int32_t end, size_t *len_out) {
    if (args[arg].u_obj == mp_const_none) {
        return NULL;
    }

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[arg].u_obj, &bufinfo, buf_type);
    // Compute bounds in terms of elements, not bytes.
    int stride_in_bytes = mp_binary_get_size('@', bufinfo.typecode, NULL);
    size_t length = bufinfo.len / stride_in_bytes;
    if (end < 1) {
        normalize_buffer_bounds(&start, length, &length);
    } else {
        normalize_buffer_bounds(&start, end, &length);
    }

    // Treat start and length in terms of bytes from now on.
    start *= stride_in_bytes;
    length *= stride_in_bytes;

    if (length == 0) {
        return NULL;
    } else {
        *len_out = length;
        return ((uint8_t *)bufinfo.buf) + start;
    }
}

//|     def write_byte(self, addr: int, data: int) -> None:
//|         """Write the data byte to RAM at the given address.
//|
//|         :param int addr: the 24 bit address to write to
//|         :param int ReadableBuffer: the wrd_size data to write
//|         """
//|         ...
STATIC mp_obj_t rambus_ram_obj_write_byte(size_t n_args, const mp_obj_t *pos_args) {
    enum { ARG_addr, ARG_data };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_addr, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_data, MP_ARG_REQUIRED | MP_ARG_INT },
    };
    rambus_ram_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, NULL, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    addr_t addr = check_addr(self, ARG_addr, args);
    uint8_t data = (uint8_t)mp_arg_validate_int_range(args[ARG_data].u_int, 0, 255, ARG_data);

    shared_module_rambus_ram_write_byte(self, addr, data);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rambus_ram_write_byte_obj, 3, 3, rambus_ram_obj_write_byte);

//|     def write_page(self, addr: int, data: int) -> None:
//|         """Write the data page to RAM at the given address.
//|
//|         :param int addr: the 24 bit address to write to
//|         :param ReadableBuffer data: the pg_size of data to write
//|         """
//|         ...
STATIC mp_obj_t rambus_ram_obj_write_page(size_t n_args, const mp_obj_t *pos_args) {
    enum { ARG_addr, ARG_data };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_addr, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_data, MP_ARG_REQUIRED | MP_ARG_OBJ },
    };
    rambus_ram_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, NULL, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    addr_t addr = check_addr(self, ARG_addr, args);
    size_t len = 0;
    uint8_t *data = check_buffer(self, MP_BUFFER_READ, ARG_data, args, 0, -1, &len);

    if (data != NULL) {
        shared_module_rambus_ram_write_page(self, addr, data, len);
    } else {
        mp_printf(&mp_plat_print, "Writing page to addr %x received no data!\n", addr);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rambus_ram_write_page_obj, 3, 3, rambus_ram_obj_write_page);

//|     def write_seq(self, addr: int, data: int) -> None:
//|         """Write the sequence of data to RAM at the given address.
//|
//|         :param int addr: the 24 bit address to write to
//|         :param ReadableBuffer data: the buffer of data to write
//|         :param int start: the index within data to start writing from
//|         :param int end: the index within data to end writing from
//|         """
//|         ...
STATIC mp_obj_t rambus_ram_obj_write_seq(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_addr, ARG_data, ARG_start, ARG_end };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_addr, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_data, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_start, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_end, MP_ARG_INT, {.u_int = -1} },
    };
    rambus_ram_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    addr_t addr = check_addr(self, ARG_addr, args);
    size_t len = 0;
    uint8_t *data = check_buffer(self, MP_BUFFER_READ, ARG_data, args, args[ARG_start].u_int, args[ARG_end].u_int, &len);

    if (data != NULL) {
        shared_module_rambus_ram_write_seq(self, addr, data, len);
    } else {
        mp_printf(&mp_plat_print, "Writing seq to addr %x received no data!\n", addr);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(rambus_ram_write_seq_obj, 3, rambus_ram_obj_write_seq);

//|     import sys
//|     def read_byte(self, addr: int, buf: WriteableBuffer = None, start: int = 0) -> int:
//|         """Read the data byte from RAM at the given address.
//|
//|         Returns an int between 0-255.
//|
//|         :param int addr: the 24 bit address to write to
//|         :param WriteableBuffer buf: the existing buffer to write into
//|         :param int start: the index to start writing to the provided buf
//|
//|         .. note:: If no buffer is provided, one will be created and returned.
//|           start is only used if a buffer is provided.
//|         """
//|         ...
STATIC mp_obj_t rambus_ram_obj_read_byte(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_addr, ARG_buffer, ARG_start };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_addr, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_buffer, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_start, MP_ARG_INT, {.u_int = 0} },
    };
    rambus_ram_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    addr_t addr = check_addr(self, ARG_addr, args);
    size_t len = 0;
    uint8_t *bufaddr = check_buffer(self, MP_BUFFER_WRITE, ARG_buffer, args, 0, -1, &len);
    // MP_OBJ_NEW_SMALL_INT()
    return mp_obj_new_int_from_uint(shared_module_rambus_ram_read_byte(self, addr, bufaddr, 0));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(rambus_ram_read_byte_obj, 2, rambus_ram_obj_read_byte);

//|     import sys
//|     def read_page(self, addr: int, buf: WriteableBuffer = None, start: int = 0) -> None:
//|         """Read a page of bytes from RAM into the given buffer.
//|
//|         :param int addr: the 24 bit address to begin reading from
//|         :param WriteableBuffer buf: the existing buffer to read into
//|         :param int start: the index to start writing to the provided buffer
//|         :param int end: the index to end writing to the provided buffer
//|
//|         .. note:: If no buffer is provided, a pg_size buffer will be created and returned.
//|         """
//|         ...
STATIC mp_obj_t rambus_ram_obj_read_page(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_addr, ARG_buffer, ARG_start, ARG_end };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_addr, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_buffer, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_start, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_end, MP_ARG_INT, {.u_int = -1} },
    };
    rambus_ram_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    addr_t addr = check_addr(self, ARG_addr, args);
    size_t len = 0;
    uint8_t *bufaddr = check_buffer(self, MP_BUFFER_WRITE, ARG_buffer, args, args[ARG_start].u_int, args[ARG_end].u_int, &len);

    mp_obj_t result;
    if (bufaddr == NULL) {
        len = (size_t)self->pg_size;
        // alloc new buffer
        result = mp_obj_new_bytearray_of_zeros(len);
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(result, &bufinfo, MP_BUFFER_READ);
        bufaddr = (uint8_t*)bufinfo.buf;
    } else {
        result = args[ARG_buffer].u_obj;
    }

    shared_module_rambus_ram_read_page(self, addr, bufaddr, len);

    return result;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(rambus_ram_read_page_obj, 2, rambus_ram_obj_read_page);

//|     import sys
//|     def read_seq(self, addr: int, buf: WriteableBuffer = None, start: int = 0) -> None:
//|         """Read a sequence of bytes from RAM into the given buffer.
//|
//|         :param int addr: the 24 bit address to begin reading from
//|         :param int len: the number of bytes to read from RAM. if no value is provided, the enire RAM will be read.
//|                if a buffer is provided, the length must be less than or equal to the buffer length.
//|         :param WriteableBuffer buf: the optional existing buffer to read into. if no buffer is provided, one will 
//|                be created.
//|         :param int start: the index to start writing to the provided buffer
//|         :param int end: the index to end writing to the provided buffer
//|
//|         .. note:: If no buffer is provided, a pg_size buffer will be created and returned.
//|         """
//|         ...
STATIC mp_obj_t rambus_ram_obj_read_seq(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_addr, ARG_size, ARG_buffer, ARG_start, ARG_end };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_addr, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_size, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_buffer, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_start, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_end, MP_ARG_INT, {.u_int = -1} },
    };
    rambus_ram_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    addr_t addr = check_addr(self, ARG_addr, args);
    size_t len = 0;
    uint8_t *bufaddr = check_buffer(self, MP_BUFFER_WRITE, ARG_buffer, args, args[ARG_start].u_int, args[ARG_end].u_int, &len);

    mp_obj_t result;
    if (bufaddr == NULL) {
        len = (addr_t)args[ARG_size].u_int;
        if (len < 1) {
            len = shared_module_rambus_ram_get_size(self);
        }
        // alloc new buffer
        result = mp_obj_new_bytearray_of_zeros(len);
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(result, &bufinfo, MP_BUFFER_READ);
        bufaddr = (uint8_t*)bufinfo.buf;
    } else {
        // If a buffer was provided AND a length was specified, try to read that length into the buffer.
        // If that length is outside the buffer bounds, ignore it and read the buffer length.
        if ((size_t)args[ARG_size].u_int < len) {
            len = (size_t)args[ARG_size].u_int;
        }
        result = args[ARG_buffer].u_obj;
    }

    shared_module_rambus_ram_read_seq(self, addr, bufaddr, len);

    return result;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(rambus_ram_read_seq_obj, 2, rambus_ram_obj_read_seq);


STATIC const mp_rom_map_elem_t rambus_ram_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&rambus_ram_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&default___enter___obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&rambus_ram___exit___obj) },
    { MP_ROM_QSTR(MP_QSTR_mode), MP_ROM_PTR(&rambus_ram_mode_obj) },
    { MP_ROM_QSTR(MP_QSTR_size), MP_ROM_PTR(&rambus_ram_size_obj) },
    { MP_ROM_QSTR(MP_QSTR_start_addr), MP_ROM_PTR(&rambus_ram_start_addr_obj) },
    { MP_ROM_QSTR(MP_QSTR_end_addr), MP_ROM_PTR(&rambus_ram_end_addr_obj) },
    { MP_ROM_QSTR(MP_QSTR_write_byte), MP_ROM_PTR(&rambus_ram_write_byte_obj) },
    { MP_ROM_QSTR(MP_QSTR_write_page), MP_ROM_PTR(&rambus_ram_write_page_obj) },
    { MP_ROM_QSTR(MP_QSTR_write_seq), MP_ROM_PTR(&rambus_ram_write_seq_obj) },
    { MP_ROM_QSTR(MP_QSTR_read_byte), MP_ROM_PTR(&rambus_ram_read_byte_obj) },
    { MP_ROM_QSTR(MP_QSTR_read_page), MP_ROM_PTR(&rambus_ram_read_page_obj) },
    { MP_ROM_QSTR(MP_QSTR_read_seq), MP_ROM_PTR(&rambus_ram_read_seq_obj) },
};
STATIC MP_DEFINE_CONST_DICT(rambus_ram_locals_dict, rambus_ram_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    rambus_ram_type,
    MP_QSTR_RAM,
    MP_TYPE_FLAG_HAS_SPECIAL_ACCESSORS,
    make_new, rambus_ram_make_new,
    locals_dict, &rambus_ram_locals_dict
    );

