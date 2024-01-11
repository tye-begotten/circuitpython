

#include "py/obj.h"
#include "py/runtime.h"

#include "shared-bindings/ramio/__init__.h"
#include "shared-bindings/ramio/RAM.h"

STATIC const mp_rom_map_elem_t ramio_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ramio) },
    { MP_ROM_QSTR(MP_QSTR_RAM), MP_ROM_PTR(&ramio_ram_type) },
};

STATIC MP_DEFINE_CONST_DICT(ramio_module_globals, ramio_module_globals_table);

const mp_obj_module_t ramio_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&ramio_module_globals,
};