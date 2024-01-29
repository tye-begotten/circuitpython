#pragma once

#include "shared-module/rambus/__init__.h"
#include "shared-module/rambus/RAM.h"



typedef struct {
    const struct _mempool_p_t *mempool_protocol;
    rambus_ram_obj_t *ram;
    addr_t addr;
    addr_t size;
    uint8_t buf[4];
} rambus_membuf_obj_t;

void rambus_membuf_construct(rambus_membuf_obj_t *self, mp_obj_t *pool, rambus_ram_obj_t *ram, addr_t addr, addr_t size);
bool rambus_membuf_deinited(rambus_membuf_obj_t *self);
bool rambus_membuf_deinit(rambus_membuf_obj_t *self);
bool rambus_membuf_check_deinit(rambus_membuf_obj_t *self);

addr_t rambus_membuf_get_start(rambus_membuf_obj_t *self);
addr_t rambus_membuf_get_size(rambus_membuf_obj_t *self);
addr_t rambus_membuf_get_end(rambus_membuf_obj_t *self);
addr_t rambus_membuf_addr(rambus_membuf_obj_t *self, addr_t offset);

void rambus_membuf_write(rambus_membuf_obj_t *self, addr_t addr, uint8_t *data, addr_t size);
void rambus_membuf_read(rambus_membuf_obj_t *self, addr_t addr, mp_buffer_info_t *buf, addr_t size);