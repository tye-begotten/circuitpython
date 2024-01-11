
#pragma once

#include "py/obj.h"
#include "shared-bindings/digitalio/DigitalInOut.h"
#include "shared-bindings/busio/SPI.h"
#include "shared-bindings/microcontroller/Pin.h"
#include "shared-bindings/digitalio/DigitalInOut.h"

typedef enum {
    RAM_TYPE_SRAM,
    RAM_TYPE_FRAM,
} ram_types;

typedef struct {
    mp_obj_base_t base;
    ram_types ram_type;
    uint16_t pg_size;
    uint16_t pg_cnt;
    uint8_t wrd_size;
    busio_spi_obj_t spi;
    digitalio_digitalinout_obj_t cs;
    digitalio_digitalinout_obj_t hold;
    uint8_t mode;
    uint8_t cmd[5];
} rambus_ram_obj_t;

void rambus_ram_construct(rambus_ram_obj_t *self, ram_types ram_type, uint16_t pg_size, uint16_t pg_cnt, uint8_t wrd_size, 
    busio_spi_obj_t *spi, mcu_pin_obj_t *cs, mcu_pin_obj_t *hold);

uint16_t rambus_ram_get_size(rambus_ram_obj_t *self);
uint16_t rambus_ram_get_start_addr(rambus_ram_obj_t *self);
uint16_t rambus_ram_get_end_addr(rambus_ram_obj_t *self);

uint8_t rambus_ram_get_mode(rambus_ram_obj_t *self);
void rambus_ram_set_mode(rambus_ram_obj_t *self, uint8_t mode);

void rambus_ram_write_byte(rambus_ram_obj_t *self, uint32_t addr, uint8_t data);
uint8_t rambus_ram_read_byte(rambus_ram_obj_t *self, uint32_t addr, uint8_t *buf, uint8_t start);

uint8_t* rambus_ram_make_cmd(rambus_ram_obj_t *self, uint8_t cmd, uint32_t addr, uint8_t data);
void rambus_ram_begin_op(rambus_ram_obj_t *self);
void rambus_ram_end_op(rambus_ram_obj_t *self);

void rambus_ram_release(rambus_ram_obj_t *self);