#include "shared-bindings/rambus/__init__.h"
#include "shared-bindings/rambus/MemPool.h"


STATIC mp_obj_t rambus_mempool_obj_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_ram ARG_start, ARG_end, ARG_auto_expand, ARG_auto_validate };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_ram, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_start, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_end, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_auto_expand, MP_ARG_KW_ONLY | MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_auto_validate, MP_ARG_KW_ONLY | MP_ARG_BOOL, {.u_bool = false} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    rambus_mempool_obj_t *self = mp_obj_malloc(rambus_mempool_obj_t, &rambus_mempool_type);
    rambus_ram_obj_t *ram = MP_OBJ_TO_PTR(mp_arg_validate_type(args[ARG_ram], &rambus_ram_type, MP_QSTR_ram));
    mp_int_t start = mp_arg_validate_int_range(args[ARG_start], rambus_ram_get_start_addr(ram), rambus_ram_get_end_addr(ram), MP_QSTR_start);
    mp_int_t end = mp_arg_validate_int_range(args[ARG_end], start + 1, rambus_ram_get_end_addr(ram), MP_QSTR_end);

    rambus_mempool_construct(self, MP_OBJ_TO_PTR(ram), start, end, args[ARG_auto_expand].u_bool, args[ARG_auto_validate].u_bool);

    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t rambus_mempool_obj_get_total(mp_obj_t self_in) {
    rambus_mempool_obj_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_int(rambus_mempool_get_total(self));
}