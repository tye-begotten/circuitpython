
#pragma once

#include "py/obj.h"
#include "shared-bindings/digitalio/DigitalInOut.h"
#include "shared-bindings/busio/SPI.h"
#include "shared-bindings/microcontroller/Pin.h"
#include "shared-bindings/digitalio/DigitalInOut.h"

// TODO: these should be supplied by the ram implementation
#define __READ 0x03
#define __WRITE 0x02
#define __READ_MODE 0x5
#define __WRITE_MODE 0x1
#define __ESDI 0x3b
#define __ESQI 0x38
#define __RSTDQI 0xff
#define __MODE_BYTE 0b00000000
#define __MODE_PAGE 0b10000000
#define __MODE_SEQ 0b01000000
#define __MODE_NONE 0xff

typedef uint32_t addr_t;

typedef enum {
    RAM_TYPE_SRAM,
    RAM_TYPE_FRAM,
} ram_types;

typedef struct {
    digitalio_digitalinout_obj_t sck;
    digitalio_digitalinout_obj_t si;
    digitalio_digitalinout_obj_t so;
    digitalio_digitalinout_obj_t cs;
    digitalio_digitalinout_obj_t hold; 
} rambus_proto_spi_t;

typedef struct {
    digitalio_digitalinout_obj_t sck;
    digitalio_digitalinout_obj_t si00;
    digitalio_digitalinout_obj_t sio1;
    digitalio_digitalinout_obj_t cs;
    digitalio_digitalinout_obj_t hold; 
} rambus_proto_sdi_t;

typedef struct {
    digitalio_digitalinout_obj_t sck;
    digitalio_digitalinout_obj_t si00;
    digitalio_digitalinout_obj_t sio1;
    digitalio_digitalinout_obj_t sio2;
    digitalio_digitalinout_obj_t sio3;
    digitalio_digitalinout_obj_t cs;
} rambus_proto_sqi_t;

typedef struct {
    uint8_t write_mode;
    uint8_t read_mode;
    addr_t addr;
    uint8_t data;
} rambus_ramcmd_t;

typedef struct {
    uint8_t read;
    uint8_t read_mode;
    uint8_t write;
    uint8_t write_mode;
    uint8_t enter_sdi;
    uint8_t enter_sqi;
    uint8_t reset_sdqi;
} rambus_ramcmds_t;

typedef struct {
    uint8_t byte;
    uint8_t page;
    uint8_t seq;
} rambus_rammodes_t;

typedef struct {
    bool write;
    bool write_mode;
    bool read;
    bool read_mode;
    bool mode_byte;
    bool mode_page;
    bool mode_seq;
    bool enter_sdi;
    bool enter_sqi;
    bool reset_sdqi;
} rambus_ramcaps_t;

typedef struct {
    mp_obj_base_t base;
    ram_types ram_type;
    uint16_t pg_size;
    uint16_t pg_cnt;
    uint8_t wrd_size;
    busio_spi_obj_t *spi;
    digitalio_digitalinout_obj_t cs;
    digitalio_digitalinout_obj_t hold;
    uint8_t mode;
    uint8_t cmd[6];
} rambus_ram_obj_t;

typedef struct {
    rambus_ram_obj_t *ram;
    addr_t start;
    addr_t offset;
} rambus_ptr_t;


void rambus_ram_construct(rambus_ram_obj_t *self, ram_types ram_type, uint16_t pg_size, uint16_t pg_cnt, uint8_t wrd_size, 
    busio_spi_obj_t *spi, const mcu_pin_obj_t *cs, const mcu_pin_obj_t *hold);

void rambus_ram_configure_spi(rambus_ram_obj_t *self, rambus_proto_spi_t *proto);
void rambus_ram_configure_sdi(rambus_ram_obj_t *self, rambus_proto_sdi_t *proto);
void rambus_ram_configure_sqi(rambus_ram_obj_t *self, rambus_proto_sqi_t *proto);

void rambus_ram_deinit(rambus_ram_obj_t *self);
bool rambus_ram_deinited(rambus_ram_obj_t *self);
void rambus_ram_check_deinit(rambus_ram_obj_t *self);

rambus_ram_obj_t *validate_obj_is_ram(mp_obj_t obj, qstr arg_name);

addr_t rambus_ram_get_size(rambus_ram_obj_t *self);
addr_t rambus_ram_get_start_addr(rambus_ram_obj_t *self);
addr_t rambus_ram_get_end_addr(rambus_ram_obj_t *self);

uint8_t rambus_ram_get_mode(rambus_ram_obj_t *self);
void rambus_ram_set_mode(rambus_ram_obj_t *self, uint8_t mode);

void rambus_ram_write_byte(rambus_ram_obj_t *self, addr_t addr, uint8_t data);
void rambus_ram_write_page(rambus_ram_obj_t *self, addr_t addr, uint8_t *data, size_t len);
void rambus_ram_write_seq(rambus_ram_obj_t *self, addr_t addr, uint8_t *data, size_t len);
void rambus_ram_write_into(rambus_ram_obj_t *self, uint8_t mode, addr_t addr, uint8_t *data, size_t len);

uint8_t rambus_ram_read_byte(rambus_ram_obj_t *self, addr_t addr, uint8_t *buf, uint8_t start);
void rambus_ram_read_page(rambus_ram_obj_t *self, addr_t addr, uint8_t *buf, size_t len);
void rambus_ram_read_seq(rambus_ram_obj_t *self, addr_t addr, uint8_t *buf, size_t len);
void rambus_ram_read_into(rambus_ram_obj_t *self, uint8_t mode, addr_t addr, uint8_t *buf, size_t len);

bool rambus_ram_exec_cmd(rambus_ram_obj_t *self, uint8_t cmd, addr_t addr, uint8_t data, uint8_t cmd_len);
void rambus_ram_begin_op(rambus_ram_obj_t *self, uint8_t mode);
void rambus_ram_end_op(rambus_ram_obj_t *self);

// uint8_t* rambus_ram_make_cmd(rambus_ram_obj_t *self, uint8_t cmd, addr_t addr, uint8_t data);

// void print_cmd(rambus_ram_obj_t *self);
