#ifndef MICROPY_INCLUDED_SHARED_BINDINGS_RAMBUS_RAM_H
#define MICROPY_INCLUDED_SHARED_BINDINGS_RAMBUS_RAM_H

#include "shared-module/rambus/RAM.h"

extern const mp_obj_type_t rambus_ram_type;

extern void shared_module_rambus_ram_construct(rambus_ram_obj_t *self, ram_types ram_type, uint16_t pg_size, uint16_t pg_cnt, uint8_t wrd_size, 
    busio_spi_obj_t *spi, mcu_pin_obj_t *cs, mcu_pin_obj_t *hold);

extern uint16_t shared_module_rambus_ram_get_size(rambus_ram_obj_t *self);
extern uint16_t shared_module_rambus_ram_get_start_addr(rambus_ram_obj_t *self);
extern uint16_t shared_module_rambus_ram_get_end_addr(rambus_ram_obj_t *self);

extern uint8_t shared_module_rambus_ram_get_mode(rambus_ram_obj_t *self);
extern void shared_module_rambus_ram_set_mode(rambus_ram_obj_t *self, uint8_t mode);

extern void shared_module_rambus_ram_write_byte(rambus_ram_obj_t *self, uint32_t addr, uint8_t data);
extern uint8_t shared_module_rambus_ram_read_byte(rambus_ram_obj_t *self, uint32_t addr, uint8_t *buf, uint8_t start);

extern uint8_t* shared_module_rambus_ram_make_cmd(rambus_ram_obj_t *self, uint8_t cmd, uint32_t addr, uint8_t data);
extern void shared_module_rambus_ram_begin_op(rambus_ram_obj_t *self);
extern void shared_module_rambus_ram_end_op(rambus_ram_obj_t *self);

#endif // MICROPY_INCLUDED_SHARED_BINDINGS_RAMBUS_RAM_H