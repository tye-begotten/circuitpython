#pragma once

#include "shared-module/rambus/RAM.h"

extern const mp_obj_type_t rambus_ram_type;

// extern void shared_module_rambus_ram_construct(rambus_ram_obj_t *self, ram_types ram_type, uint16_t pg_size, uint16_t pg_cnt, uint8_t wrd_size, 
//     busio_spi_obj_t *spi, const mcu_pin_obj_t *cs, const mcu_pin_obj_t *hold);

// uint8_t shared_module_rambus_ram_get_mode(rambus_ram_obj_t *self);
// extern addr_t shared_module_rambus_ram_get_size(rambus_ram_obj_t *self);
// extern addr_t shared_module_rambus_ram_get_start_addr(rambus_ram_obj_t *self);
// extern addr_t shared_module_rambus_ram_get_end_addr(rambus_ram_obj_t *self);

// extern void shared_module_rambus_ram_write_byte(rambus_ram_obj_t *self, addr_t addr, uint8_t data);
// extern void shared_module_rambus_ram_write_page(rambus_ram_obj_t *self, addr_t addr, uint8_t *data, size_t len);
// extern void shared_module_rambus_ram_write_seq(rambus_ram_obj_t *self, addr_t addr, uint8_t *data, size_t len);
// extern uint8_t shared_module_rambus_ram_read_byte(rambus_ram_obj_t *self, addr_t addr, uint8_t *buf, uint8_t start);
// extern void shared_module_rambus_ram_read_page(rambus_ram_obj_t *self, addr_t addr, uint8_t *buf, size_t len);
// extern void shared_module_rambus_ram_read_seq(rambus_ram_obj_t *self, addr_t addr, uint8_t *buf, size_t len);

// extern bool rambus_ram_exec_cmd(rambus_ram_obj_t *self, uint8_t cmd, addr_t addr, uint8_t data, uint8_t cmd_len);
// extern void shared_module_rambus_ram_begin_op(rambus_ram_obj_t *self, uint8_t mode);
// extern void shared_module_rambus_ram_end_op(rambus_ram_obj_t *self);
