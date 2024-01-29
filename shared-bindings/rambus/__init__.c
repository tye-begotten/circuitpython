
#include "py/binary.h"
#include "shared-bindings/rambus/__init__.h"
#include "shared-bindings/rambus/RAM.h"
#include "shared-bindings/rambus/RAMBusDisplay.h"
#include "shared-bindings/rambus/MemBuf.h"
#include "shared-bindings/rambus/MemPool.h"

STATIC const mp_rom_map_elem_t rambus_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_rambus) },
    { MP_ROM_QSTR(MP_QSTR_RAM), MP_ROM_PTR(&rambus_ram_type) },
    { MP_ROM_QSTR(MP_QSTR_RAMBusDisplay), MP_ROM_PTR(&rambus_rambusdisplay_type) },
    { MP_ROM_QSTR(MP_QSTR_MemBuf), MP_ROM_PTR(&rambus_membuf_type) },
    { MP_ROM_QSTR(MP_QSTR_MemPool), MP_ROM_PTR(&rambus_mempool_type) },

};

STATIC MP_DEFINE_CONST_DICT(rambus_module_globals, rambus_module_globals_table);

const mp_obj_module_t rambus_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&rambus_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_rambus, rambus_module);


uint8_t get_byte(mp_arg_val_t *args, int arg_pos, qstr arg_name) {
    return to_byte(args[arg_pos].u_int, arg_name);
}

uint8_t to_byte(mp_int_t n, qstr arg_name) {
    return (uint8_t)mp_arg_validate_int_range(n, 0, 255, arg_name);
}

bool try_get_byte(mp_arg_val_t *args, int arg_pos, uint8_t *output) {
    return try_to_byte(args[arg_pos].u_int, output);
}

bool try_to_byte(mp_int_t n, uint8_t *output) {
    if (n < 0 || n > 255) {
        return false;
    } else {
        *output = (uint8_t)n;
        return true;
    }
}

uint8_t* check_buffer(rambus_ram_obj_t *self, int buf_type, qstr arg, mp_arg_val_t *args, int32_t start, int32_t end, size_t *len_out) {
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

addr_t check_addr(rambus_ram_obj_t *self, qstr arg, mp_arg_val_t *args) {
    return (addr_t)mp_arg_validate_int_range(args[arg].u_int, rambus_ram_get_start_addr(self), 
        rambus_ram_get_end_addr(self), arg);
}