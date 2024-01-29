
#pragma once

#include "py/obj.h"
#include "py/runtime.h"
#include "py/proto.h"


#define printl(msg) \
    mp_printf(&mp_plat_print, "::CPY:: " msg "\n")
#define printlf(fmt, ...) \
    mp_printf(&mp_plat_print, "::CPY:: " fmt "\n", __VA_ARGS__)

// typedef void (*disposable_dispose)(mp_obj_t);
typedef void (*mempool_return_buffer)(mp_obj_t, mp_obj_t);

// typedef struct _disposable_i {
//     MP_PROTOCOL_HEAD //MP_QSTR_protocol_disposable
//     disposable_dispose dispose;
// } disposable_i;

typedef struct _mempool_p_t {
    MP_PROTOCOL_HEAD //MP_QSTR_protocol_mempool

    mempool_return_buffer return_buffer;
} mempool_p_t;
