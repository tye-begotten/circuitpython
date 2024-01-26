
#pragma once

#include "py/obj.h"

#define printl(msg) \
    mp_printf(&mp_plat_print, "::CPY:: " msg "\n")
#define printlf(fmt, ...) \
    mp_printf(&mp_plat_print, "::CPY:: " fmt "\n", __VA_ARGS__)

