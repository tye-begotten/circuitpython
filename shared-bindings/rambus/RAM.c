#include <stdint.h>
#include <string.h>
#include "shared/runtime/context_manager_helpers.h"
#include "py/objproperty.h"
#include "py/runtime.h"
#include "py/runtime0.h"
#include "shared-bindings/microcontroller/Pin.h"
#include "shared-bindings/rambus/RAM.h"
#include "shared-bindings/util.h"

// LIFECYCLE
STATIC mp_obj_t rambus_ram_make_new(const mp_obj_type_t *type, size_t n_args, 
    const mp_obj_t *pos_args, mp_map_t *kw_args) {
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
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    validate_obj_is_spi_bus(args[ARG_spi].u_obj, MP_QSTR_spi);
    validate_obj_is_pin(args[ARG_cs].u_obj, MP_QSTR_cs);
    validate_obj_is_pin(args[ARG_hold].u_obj, MP_QSTR_hold);

    uint16_t pg_size = args[ARG_pg_size].u_int;
    uint16_t pg_cnt = args[ARG_pg_cnt].u_int;
    uint16_t wrd_size = args[ARG_wrd_size].u_int;
    busio_spi_obj_t* spi = MP_OBJ_TO_PTR(args[ARG_spi].u_obj);
    mcu_pin_obj_t* cs = MP_OBJ_TO_PTR(args[ARG_cs].u_obj);
    mcu_pin_obj_t* hold = MP_OBJ_TO_PTR(args[ARG_hold].u_obj);
    
    rambus_ram_obj_t *self = m_new_obj(rambus_ram_obj_t);
    self->base.type = &rambus_ram_type;
    shared_module_rambus_ram_construct(self, RAM_TYPE_SRAM, pg_size, pg_cnt, wrd_size, spi, cs, hold);
    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t rambus_ram_deinit(mp_obj_t self_in) {
    rambus_ram_release(self_in);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(rambus_ram_deinit_obj, rambus_ram_deinit);

STATIC mp_obj_t rambus_ram_obj___exit__(size_t n_args, const mp_obj_t *args) {
  rambus_ram_deinit(args[0]);
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rambus_ram___exit___obj, 4, 4, rambus_ram_obj___exit__);

// PROPERTIES
STATIC mp_obj_t rambus_ram_obj_get_size(mp_obj_t self_in) {
    return mp_obj_new_int_from_uint(shared_module_rambus_ram_get_size(self_in));
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_ram_get_size_obj, rambus_ram_obj_get_size);
MP_PROPERTY_GETTER(rambus_ram_size_obj, (mp_obj_t)&rambus_ram_get_size_obj);

STATIC mp_obj_t rambus_ram_obj_get_start_addr(mp_obj_t self_in) {
    return mp_obj_new_int_from_uint(shared_module_rambus_ram_get_start_addr(self_in));
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_ram_get_start_addr_obj, rambus_ram_obj_get_start_addr);
MP_PROPERTY_GETTER(rambus_ram_start_addr_obj, (mp_obj_t)&rambus_ram_get_start_addr_obj);

STATIC mp_obj_t rambus_ram_obj_get_end_addr(mp_obj_t self_in) {
    return mp_obj_new_int_from_uint(shared_module_rambus_ram_get_end_addr(self_in));
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_ram_get_end_addr_obj, rambus_ram_obj_get_end_addr);
MP_PROPERTY_GETTER(rambus_ram_end_addr_obj, (mp_obj_t)&rambus_ram_get_end_addr_obj);

// METHODS
STATIC mp_obj_t rambus_ram_obj_write_byte(size_t n_args, const mp_obj_t *pos_args) {
    enum { ARG_self, ARG_addr, ARG_data };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_self, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_addr, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_data, MP_ARG_REQUIRED | MP_ARG_INT },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, NULL, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    shared_module_rambus_ram_write_byte(args[0].u_obj, args[1].u_int, args[2].u_int);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rambus_ram_write_byte_obj, 3, 3, rambus_ram_obj_write_byte);

STATIC mp_obj_t rambus_ram_obj_read_byte(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_self, ARG_addr, ARG_buf, ARG_start };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_self, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_addr, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_buf, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_start, MP_ARG_INT, {.u_int = 0} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    shared_module_rambus_ram_read_byte(args[0].u_obj, args[1].u_int, args[2].u_obj, args[3].u_int);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(rambus_ram_read_byte_obj, 4, rambus_ram_obj_read_byte);

// const mp_obj_property_t rambus_ram_size_obj = {
//     .base.type = &mp_type_property,
//     .proxy = {(mp_obj_type)&rambus_ram_get_size_obj, (mp_obj_t)&mp_const_none},
// };

// const mp_obj_property_t rambus_ram_start_addr_obj = {
//     .base.type = &mp_type_property,
//     .proxy = {(mp_obj_type)&rambus_ram_get_start_addr_obj, (mp_obj_t)&mp_const_none},
// };

// const mp_obj_property_t rambus_ram_end_addr_obj = {
//     .base.type = &mp_type_property,
//     .proxy = {(mp_obj_type)&rambus_ram_get_end_addr_obj, (mp_obj_t)&mp_const_none},
// };

STATIC const mp_rom_map_elem_t rambus_ram_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&rambus_ram_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&default___enter___obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&rambus_ram___exit___obj) },
    { MP_ROM_QSTR(MP_QSTR_size), MP_ROM_PTR(&rambus_ram_size_obj) },
    { MP_ROM_QSTR(MP_QSTR_start_addr), MP_ROM_PTR(&rambus_ram_start_addr_obj) },
    { MP_ROM_QSTR(MP_QSTR_end_addr), MP_ROM_PTR(&rambus_ram_end_addr_obj) },
    { MP_ROM_QSTR(MP_QSTR_read_byte), MP_ROM_PTR(&rambus_ram_read_byte_obj) },
    { MP_ROM_QSTR(MP_QSTR_write_byte), MP_ROM_PTR(&rambus_ram_write_byte_obj) },
};
STATIC MP_DEFINE_CONST_DICT(rambus_ram_locals_dict, rambus_ram_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    rambus_ram_type,
    MP_QSTR_RAM,
    MP_TYPE_FLAG_HAS_SPECIAL_ACCESSORS,
    make_new, rambus_ram_make_new,
    locals_dict, &rambus_ram_locals_dict
    );

// const mp_obj_type_t rambus_ram_type = {
//     { &mp_type_type },
//     .name = MP_QSTR_RAM,
//     .make_new = rambus_ram_make_new,
//     .locals_dict = (mp_obj_dict*)&rambus_ram_locals_dict,
// };