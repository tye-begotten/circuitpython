#pragma once

#include "py/objlist.h"
#include "py/objtuple.h"
#include "shared-module/rambus/__init__.h"
#include "shared-module/rambus/RAM.h"
#include "shared-module/rambus/MemBuf.h"


typedef struct {
    addr_t addr;
    addr_t size;
} rambus_memblock_t;

typedef struct {
    rambus_ram_obj_t *ram;
    addr_t start;
    addr_t end;
    bool auto_expand;
    bool auto_validate;
    addr_t allocated;
    mp_obj_list_t *allocs;
    mp_obj_list_t *blocks;
} rambus_mempool_obj_t;


void rambus_mempool_construct(rambus_mempool_obj_t *self, rambus_ram_obj_t *ram, 
    addr_t start, addr_t end, bool auto_expand, bool auto_validate);
bool rambus_mempool_deinited(rambus_mempool_obj_t *self);
bool rambus_mempool_deinit(rambus_mempool_obj_t *self);
bool rambus_mempool_check_deinit(rambus_mempool_obj_t *self);

addr_t rambus_mempool_get_allocated(rambus_mempool_obj_t *self);
addr_t rambus_mempool_get_free(rambus_mempool_obj_t *self);
addr_t rambus_mempool_get_total(rambus_mempool_obj_t *self);

rambus_membuf_obj_t* rambus_mempool_alloc(rambus_mempool_obj_t *self, addr_t size);
void rambus_mempool_free(rambus_mempool_obj_t *self, rambus_membuf_obj_t *buf, addr_t amount);
void rambus_mempool_validate(rambus_mempool_obj_t *self);