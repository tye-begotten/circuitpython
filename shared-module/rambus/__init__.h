
#pragma once

#include "py/obj.h"

#define printl(msg) \
    mp_printf(&mp_plat_print, msg "\n")
#define printfl(fmt, ...) \
    mp_printf(&mp_plat_print, fmt "\n", __VA_ARGS__)

