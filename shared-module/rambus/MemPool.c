#include "shared-module/rambus/__init__.h"
#include "shared-module/rambus/RAM.h"
#include "shared-module/rambus/MemBuf.h"
#include "shared-bindings/rambus/MemBuf.h"
#include "shared-module/rambus/MemPool.h"


STATIC mp_obj_tuple_t* _make_block(addr_t addr, size_t size) {
    return MP_OBJ_TO_PTR(mp_obj_new_tuple(2, 
                ((mp_obj_t []){ mp_obj_new_int(addr), 
                                mp_obj_new_int(size) 
                            })));
}

STATIC mp_obj_tuple_t* _membuf_to_block(rambus_membuf_obj_t* buf) {
    return _make_block(buf->addr, buf->size);
}

void rambus_mempool_construct(rambus_mempool_obj_t *self, rambus_ram_obj_t *ram, 
    addr_t start, addr_t end, bool auto_expand, bool auto_validate) {
        self->start = start;
        self->end = end;
        self->auto_expand = auto_expand;
        self->auto_validate = auto_validate;
        self->allocated = 0;
        self->allocs = MP_OBJ_TO_PTR(mp_obj_new_list(0, NULL));
        self->blocks = MP_OBJ_TO_PTR(mp_obj_new_list(1, _make_block(0, rambus_mempool_get_total(self))));
    }

bool rambus_mempool_deinited(rambus_mempool_obj_t *self) {
    return self->ram == NULL || rambus_ram_deinited(self->ram);
}

bool rambus_mempool_deinit(rambus_mempool_obj_t *self) {
    // TODO: deinit ram?
    self->ram = NULL;

    for (int i = 0; i < self->allocs->len; i++) {
        rambus_membuf_deinit(self->allocs->items[i]);
    }

    mp_obj_list_clear(&self->allocs);
    mp_obj_list_clear(&self->blocks);
    self->allocs = NULL;
    self->blocks = NULL;
}

bool rambus_mempool_check_deinit(rambus_mempool_obj_t *self);

addr_t rambus_mempool_get_allocated(rambus_mempool_obj_t *self) {
    return self->allocated;
}

addr_t rambus_mempool_get_free(rambus_mempool_obj_t *self) {
    return rambus_mempool_get_total(self) - self->allocated;
}

addr_t rambus_mempool_get_total(rambus_mempool_obj_t *self) {
    return self->end - self->start;
}

rambus_membuf_obj_t* rambus_mempool_alloc(rambus_mempool_obj_t *self, addr_t size) {
    // TODO: validate in binding?
    if (rambus_mempool_get_free(self) < size) {
        if (self->auto_expand) {
            mp_raise_msg_varg(&mp_type_MemoryError, MP_ERROR_TEXT("Failed to allocate %d bytes, insufficient space and auto-expand is not implemented."), size);
        } else {
            mp_raise_msg_varg(&mp_type_MemoryError, MP_ERROR_TEXT("Failed to allocate %d bytes, insufficient space."), size);
        }
    }

    rambus_membuf_obj_t *result = NULL;
    mp_obj_tuple_t *block = NULL;

    for (int i = 0; i < self->blocks->len; i++) {
        block = &self->blocks->items[i];
        
        if ((addr_t)block->items[1] == size) { // exact fit use whole block
            mp_obj_list_remove(self->blocks, block);
            result = mp_obj_malloc(rambus_membuf_obj_t, &rambus_membuf_type);
            rambus_membuf_construct(result, self, self->ram, block->items[0], block->items[1]);
            break;
        } else if ((addr_t)block->items[1] > size) {
            result = mp_obj_malloc(rambus_membuf_obj_t, &rambus_membuf_type);
            rambus_membuf_construct(result, self, self->ram, block->items[0], size);
            block->items[0] = block->items[0] + size;
            block->items[1] = block->items[1] - size;
            break;
        }
    }

    if (result == NULL) {
        if (self->auto_expand) {
            mp_raise_msg_varg(&mp_type_MemoryError, MP_ERROR_TEXT("Failed to allocate %d bytes from MemPool and auto-expand is not implemented"), size);
        } else {
            mp_raise_msg_varg(&mp_type_MemoryError, MP_ERROR_TEXT("Failed to allocate %d bytes from MemPool"), size);
        }
    } else {
        if (result->size = size) {
            mp_raise_msg_varg(&mp_type_MemoryError, MP_ERROR_TEXT("Improper allocation, requested %d bytes and received %d byte block"), size, result->size);
        }

        mp_obj_list_append(self->allocs, result);
        self->allocated += size;

        if (self->auto_validate) {
            rambus_mempool_validate(self);
        }
    }

    return result;
}

void rambus_mempool_free(rambus_mempool_obj_t *self, rambus_membuf_obj_t *buf, addr_t amount) {
    mp_obj_tuple_t *rblock = NULL;

    if (amount < buf->size) {
        rblock = _membuf_to_block(buf);
    } else {
        mp_obj_tuple_t *rblock = _make_block(rambus_membuf_get_end(buf) - amount, amount);
    }

    amount = (addr_t)rblock->items[1];
    addr_t rend = rambus_membuf_get_end(buf);
    bool returned = false;
    addr_t pend = 0;
    addr_t ip = 0;

    if (self->blocks->len == 0) {
        mp_obj_list_append(self->blocks, rblock);
        returned = true;
    } else {
        for (int i = 0; i < self->blocks->len; i++) {
            mp_obj_tuple_t *nblock = MP_OBJ_TO_PTR(self->blocks->items[i]);
            if (nblock->items[0] >= rend) { // found next block
                if (i > 0) { // block belongs after previous block
                    mp_obj_tuple_t *pblock = MP_OBJ_TO_PTR(self->blocks->items[i - 1]);
                    pblock->items[1] += (addr_t)rblock->items[1];
                    pend = (addr_t)pblock->items[0] + (addr_t)pblock->items[1];

                    if (pend == rblock->items[0]) { // merge with previous block
                        pblock->items[1] = pblock->items[1] + (addr_t)rblock->items[1];
                        pend = pblock->items[0] + (addr_t)pblock->items[1];
                        ip = i - 1;
                    } else { // insert after previous block
                        pblock = _make_block(rblock->items[0], rblock->items[1]);
                        mp_obj_list_insert(self->blocks, i, pblock);
                        pend = pblock->items[0] + (addr_t)pblock->items[1];
                        ip = i;
                    }

                    if (pend == nblock->items[0]) { // merge with next block
                        pblock->items[1] += (addr_t)rblock->items[1];
                        mp_obj_list_remove(self->blocks, nblock);
                    }
                } else { // block belongs at the beginning
                    if (rend == nblock->items[0]) { // merge with next block
                        nblock->items[0] = rblock->items[0];
                    } else { //insert at beginning
                        mp_obj_list_insert(self->blocks, 0, rblock);
                    }
                }

                returned = true;
                break;
            }
        }
    }

    if (!returned) {
        mp_raise_msg_varg(&mp_type_MemoryError, MP_ERROR_TEXT("Failed to return membuf: addr=%x, size=%d"), buf->addr, buf->size);
    }

    self->allocated -= amount;

    if (self->auto_validate) {
        rambus_mempool_validate(self);
    }
}

void rambus_mempool_validate(rambus_mempool_obj_t *self) {
    assert(self->allocated >= 0 && self->allocated <= rambus_mempool_get_total(self));

    addr_t prev = 0;
    addr_t imem = 0;
    addr_t iblock = 0;
    addr_t ialloc = 0;

    while (imem < rambus_mempool_get_total(self)) {
        assert(iblock < self->blocks->len && ialloc < self->allocs->len && imem != prev);
        prev = imem;

        if (self->blocks->len > iblock) {
            mp_obj_tuple_t *block = self->blocks->items[iblock];
            if (block->items[0] == imem) {
                imem += (addr_t)block->items[1];
                iblock++;
            }
        }

        if (self->allocs->len > ialloc) {
            rambus_membuf_obj_t *buf = self->allocs->items[ialloc];
            if (buf->addr == imem) {
                imem += buf->size;
                ialloc++;
            }
        }

        if (imem == prev) {
            mp_raise_msg_varg(&mp_type_MemoryError, MP_ERROR_TEXT("block starting at: %d not accounted for. iblock=%d, ialloc=%d"), imem, iblock, ialloc);
        }
    }

    printl("Membuf: all blocks accounted for");
}

const mempool_p_t rambus_mempool_proto = {
    MP_PROTO_IMPLEMENT(MP_QSTR_protocol_mempool)
    .return_buffer = rambus_mempool_free
};