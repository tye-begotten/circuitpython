#include "shared-module/rambus/__init__.h"
#include "shared-module/rambus/MemBuf.h"


void rambus_membuf_construct(rambus_membuf_obj_t *self, mp_obj_t *pool, rambus_ram_obj_t *ram, addr_t addr, addr_t size) {
    self->mempool_protocol = mp_proto_get_or_throw(MP_QSTR_protocol_mempool, pool);
    self->ram = ram;
    self->addr = addr;
    self->size = size;
}

bool rambus_membuf_deinited(rambus_membuf_obj_t *self) {
    return self->ram == NULL || self->mempool_protocol == NULL;
}

bool rambus_membuf_deinit(rambus_membuf_obj_t *self) {
    // TODO: return membuf to pool
    self->mempool_protocol->return_buffer(self);
    self->mempool_protocol = NULL;
    self->ram = NULL;
}

bool rambus_membuf_check_deinit(rambus_membuf_obj_t *self) {
    if (rambus_membuf_deinited(self)) {
        raise_deinited_error();
    }
}

addr_t rambus_membuf_get_start(rambus_membuf_obj_t *self) {
    return self->addr;
}

addr_t rambus_membuf_get_size(rambus_membuf_obj_t *self) {
    return self->size;
}

addr_t rambus_membuf_get_end(rambus_membuf_obj_t *self) {
    return self->addr + self->size;
}

addr_t rambus_membuf_addr(rambus_membuf_obj_t *self, addr_t offset) {
    if (offset > self->size) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid address offset"));
    }

    return self->addr + offset;
}

void rambus_membuf_write(rambus_membuf_obj_t *self, addr_t addr, uint8_t *data, addr_t size) {
    if (size < 1) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid membuf write size"));
    } else if (size == 1) {
        rambus_ram_write_byte(self->ram, rambus_membuf_addr(self, addr), data[0]);
    } else {
        rambus_ram_write_seq(self->ram, rambus_membuf_addr(self, addr), data, size);
    }
}

void rambus_membuf_read(rambus_membuf_obj_t *self, addr_t addr, mp_buffer_info_t *buf, addr_t size) {
    if (size < 1) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid membuf read size"));
    } else if (size == 1) {
        rambus_ram_read_byte(self->ram, rambus_membuf_addr(self, addr), buf, 0);
    } else {
        rambus_ram_read_seq(self->ram, rambus_membuf_addr(self, addr), buf, size);
    }
}