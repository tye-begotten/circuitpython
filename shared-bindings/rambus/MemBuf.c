#include "shared-bindings/rambus/__init__.h"
#include "shared-bindings/rambus/MemBuf.h"


// STATIC mp_obj_t rambus_membuf_make_new(mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
//     enum { ARG_pool, ARG_addr, ARG_size };
//     static const mp_arg_t allowed_args[] = {
//         { MP_QSTR_pool, MP_ARG_REQUIRED | MP_ARG_OBJ },
//         { MP_QSTR_addr, MP_ARG_REQUIRED | MP_ARG_INT },
//         { MP_QSTR_size, MP_ARG_REQUIRED | MP_ARG_INT },
//     };
//     mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
//     mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
//     rambus_membuf_obj_t *self = MP_OBJ_TO_PTR(mp_obj_malloc(rambus_membuf_obj_t, &rambus_membuf_type));

//     mempool_p_t *pool = mp_proto_get_or_throw(MP_QSTR_protocol_mempool, args[ARG_pool].u_obj);



STATIC mp_obj_t rambus_membuf_obj_deinit(mp_obj_t self_in) {
    rambus_membuf_deinit(MP_OBJ_FROM_PTR(self_in));
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_membuf_deinit_obj, rambus_membuf_obj_deinit);


// }

//|     def __enter__(self) -> RAM:
//|         """No-op used by Context Managers."""
//|         ...
//  Provided by context manager helper.

//|     def __exit__(self) -> None:
//|         """Automatically deinitializes the hardware when exiting a context. See
//|         :ref:`lifetime-and-contextmanagers` for more info."""
//|         ...
STATIC mp_obj_t rambus_ram_obj___exit__(size_t n_args, const mp_obj_t *args) {
  return rambus_membuf_obj_deinit(args[0]);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rambus_ram___exit___obj, 4, 4, rambus_ram_obj___exit__);

STATIC mp_obj_t rambus_membuf_obj_deinited(mp_obj_t self_in) {
    return mp_obj_new_bool(rambus_membuf_deinited(MP_OBJ_TO_PTR(self_in)));
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_membuf_deinited_obj, rambus_membuf_obj_deinited);

STATIC mp_obj_t rambus_membuf_obj_get_start(mp_obj_t self_in) {
    return mp_obj_new_int(rambus_membuf_get_start(MP_OBJ_FROM_PTR(self_in)));
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_membuf_get_start_obj, rambus_membuf_obj_get_start);
MP_PROPERTY_GETTER(rambus_membuf_start_obj, (mp_obj_t)&rambus_membuf_get_start_obj);

STATIC mp_obj_t rambus_membuf_obj_get_size(mp_obj_t self_in) {
    return mp_obj_new_int(rambus_membuf_get_size(MP_OBJ_FROM_PTR(self_in)));
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_membuf_get_size_obj, rambus_membuf_obj_get_size);
MP_PROPERTY_GETTER(rambus_membuf_size_obj, (mp_obj_t)&rambus_membuf_get_size_obj);

STATIC mp_obj_t rambus_membuf_obj_get_end(mp_obj_t self_in) {
    return mp_obj_new_int(rambus_membuf_get_end(MP_OBJ_FROM_PTR(self_in)));
}
MP_DEFINE_CONST_FUN_OBJ_1(rambus_membuf_get_end_obj, rambus_membuf_obj_get_end);
MP_PROPERTY_GETTER(rambus_membuf_end_obj, (mp_obj_t)&rambus_membuf_get_end_obj);

STATIC mp_obj_t rambus_membuf_obj_addr(size_t n_args, const mp_obj_t *pos_args) {
    enum { ARG_offset };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_offset, MP_ARG_INT, {.u_int = 0} },
    };
    rambus_membuf_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, NULL, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    mp_arg_validate_int_range(args[ARG_offset].u_int, 0, rambus_membuf_get_end(self), MP_QSTR_offset);
    return mp_obj_new_int(rambus_membuf_addr(self, args[ARG_offset].u_int));
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rambus_membuf_addr_obj, 1, 2, rambus_membuf_obj_addr);

STATIC mp_obj_t rambus_membuf_obj_write(size_t n_args, const mp_obj_t *pos_args, const mp_obj_t *kw_args) {
    enum { ARG_data, ARG_addr, ARG_start, ARG_end };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_data, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_addr, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_start, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_end, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
    };
    rambus_membuf_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    
    check_addr(self->ram, ARG_addr, args);
    // mp_arg_validate_int_range(args[ARG_addr], rambus_ram_get_start_addr(self->ram), rambus_ram_get_end_addr(self->ram), MP_QSTR_addr);
    size_t size = 0;
    uint8_t *data = check_buffer(self->ram, MP_BUFFER_READ, ARG_data, args, args[ARG_start].u_int, args[ARG_end].u_int, &size);

    rambus_membuf_write(self, args[ARG_addr].u_int, data, size);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(rambus_membuf_write_obj, 2, rambus_membuf_obj_write);

STATIC mp_obj_t rambus_membuf_obj_read(size_t n_args, const mp_obj_t *pos_args, const mp_obj_t *kw_args) {
    enum { ARG_addr, ARG_size, ARG_buf, ARG_start };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_addr, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_size, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_buf, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none } },
        { MP_QSTR_start, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
    };
    rambus_membuf_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    check_addr(self->ram, ARG_addr, args);
    mp_arg_validate_int_range(args[ARG_size], 1, rambus_ram_get_end_addr(self->ram) - args[ARG_addr], MP_QSTR_addr);
    addr_t len = 0;
    uint8_t *buf = check_buffer(self->ram, MP_BUFFER_WRITE, ARG_buf, args, args[ARG_start], args[ARG_start] + args[ARG_size], &len);

    if (len < args[ARG_size]) {
        mp_raise_ValueError(MP_ERROR_TEXT("Provided buffer is too short for requested start/size"));
    }

    rambus_membuf_read(self, args[ARG_addr], buf, args[ARG_size]);
    return mp_const_none;
}

// bool rambus_membuf_deinited(rambus_membuf_obj_t *self);
// bool rambus_membuf_deinit(rambus_membuf_obj_t *self);
// bool rambus_membuf_check_deinit(rambus_membuf_obj_t *self);

// addr_t rambus_membuf_get_start(rambus_membuf_obj_t *self);
// addr_t rambus_membuf_get_size(rambus_membuf_obj_t *self);
// addr_t rambus_membuf_get_end(rambus_membuf_obj_t *self);
// addr_t rambus_membuf_addr(rambus_membuf_obj_t *self, addr_t offset);

// void rambus_membuf_write(rambus_membuf_obj_t *self, addr_t addr, uint8_t *data, addr_t size);
// void rambus_membuf_read(rambus_membuf_obj_t *self, addr_t addr, mp_buffer_info_t *buf, addr_t size);