

#include "py/runtime.h"

#include "shared-bindings/rambus/__init__.h"
#include "shared-bindings/rambus/RAM.h"
#include "shared-bindings/rambus/RAMBusDisplay.h"

STATIC const mp_rom_map_elem_t rambus_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_rambus) },
    { MP_ROM_QSTR(MP_QSTR_RAM), MP_ROM_PTR(&rambus_ram_type) },
    { MP_ROM_QSTR(MP_QSTR_RAMBusDisplay), MP_ROM_PTR(&rambus_rambusdisplay_type) },
};

STATIC MP_DEFINE_CONST_DICT(rambus_module_globals, rambus_module_globals_table);

const mp_obj_module_t rambus_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&rambus_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_rambus, rambus_module);
