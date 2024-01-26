#pragma once

#include <stdint.h>
#include <string.h>
#include "py/binary.h"
#include "py/runtime.h"
#include "py/objproperty.h"
#include "py/objtype.h"
#include "py/objarray.h"
#include "shared-bindings/util.h"
#include "shared/runtime/context_manager_helpers.h"
#include "shared-module/rambus/__init__.h"
#include "shared-module/rambus/RAM.h"
#include "shared-module/rambus/RAMBusDisplay.h"

// Nothing now.
uint8_t to_byte(mp_int_t n, qstr arg_name);
bool try_to_byte(mp_int_t n, uint8_t *output);
uint8_t get_byte(mp_arg_val_t *args, int arg_pos, qstr arg_name);
bool try_get_byte(mp_arg_val_t *args, int arg_pos, uint8_t *output);