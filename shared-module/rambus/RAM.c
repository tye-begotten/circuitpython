
#include "py/runtime.h"
#include "shared-module/rambus/RAM.h"
#include "shared-bindings/digitalio/DigitalInOut.h"
#include "shared-bindings/digitalio/Direction.h"

// TODO: these should be supplied by the ram implementation
#define __READ 0x03
#define __WRITE 0x02
#define __READ_MODE 0x5
#define __WRITE_MODE 0x1
#define __MODE_BYTE 0b00000000
#define __MODE_PAGE 0b10000000
#define __MODE_SEQ 0b01000000


void rambus_ram_construct(rambus_ram_obj_t *self, ram_types ram_type, uint16_t pg_size, uint16_t pg_cnt, uint8_t wrd_size, 
    busio_spi_obj_t *spi, mcu_pin_obj_t *cs, mcu_pin_obj_t *hold) {
    self->ram_type = ram_type;
    self->pg_size = pg_size;
    self->pg_cnt = pg_cnt;
    self->wrd_size = wrd_size;
    self->spi = *spi;
    self->mode = 0; // TODO: default mode param
    
    common_hal_digitalio_digitalinout_construct(&self->cs, cs);
    common_hal_digitalio_digitalinout_switch_to_output(&self->cs, true, DRIVE_MODE_PUSH_PULL);

    common_hal_digitalio_digitalinout_construct(&self->hold, hold);
    common_hal_digitalio_digitalinout_switch_to_output(&self->hold, true, DRIVE_MODE_PUSH_PULL);
    common_hal_digitalio_digitalinout_set_value(&self->hold, true);

    if (common_hal_busio_spi_try_lock(&self->spi)) {
        common_hal_busio_spi_configure(&self->spi, 30000000, 0, 0, 8);
        common_hal_busio_spi_unlock(&self->spi);
    } else {
        mp_raise_RuntimeError(MP_ERROR_TEXT("Unable to lock SPI device"));
    }
}

uint16_t rambus_ram_get_size(rambus_ram_obj_t *self) {
    // TODO: return in bits or bytes? take wrd_size into account?
    return self->pg_size * self->pg_cnt;
}

uint16_t rambus_ram_get_start_addr(rambus_ram_obj_t *self) {
    // TODO: allow for changing start addr?
    return 0x000000;
}

uint16_t rambus_ram_get_end_addr(rambus_ram_obj_t *self) {
    return rambus_ram_get_start_addr(self) + rambus_ram_get_size(self);
}

uint8_t rambus_ram_get_mode(rambus_ram_obj_t *self) {
    self->cmd[0] = __READ_MODE;
    rambus_ram_begin_op(self);
    spi_write_blocking(self->spi.peripheral, self->cmd, 1);
    spi_read_blocking(self->spi.peripheral, 0, self->cmd, 1);
    rambus_ram_end_op(self);
    self->mode = self->cmd[0];
    return self->mode;
}

void rambus_ram_set_mode(rambus_ram_obj_t *self, uint8_t mode) {
    self->cmd[0] = __WRITE_MODE;
    self->cmd[1] = mode;
    
    rambus_ram_begin_op(self);
    spi_write_blocking(self->spi.peripheral, self->cmd, 2);
    rambus_ram_end_op(self);
    // TODO: remove check once working?
    if (rambus_ram_get_mode(self) != mode) {
        mp_raise_ValueError(MP_ERROR_TEXT("Mode set failed"));
    }
}

void rambus_ram_write_byte(rambus_ram_obj_t *self, uint32_t addr, uint8_t data) {
    rambus_ram_set_mode(self, __MODE_BYTE);
    rambus_ram_begin_op(self);
    spi_write_blocking(self->spi.peripheral, rambus_ram_make_cmd(self, __WRITE, addr, data), 5);
    rambus_ram_end_op(self);
}

uint8_t rambus_ram_read_byte(rambus_ram_obj_t *self, uint32_t addr, uint8_t *buf, uint8_t start) {
    rambus_ram_set_mode(self, __MODE_BYTE);
    rambus_ram_begin_op(self);
    uint8_t result;
    spi_write_blocking(self->spi.peripheral, rambus_ram_make_cmd(self, __WRITE, addr, 0), 5);
    if (buf != NULL) {
        spi_read_blocking(self->spi.peripheral, 0, buf + start, 1);
        result = buf[start];
    } else {
        spi_read_blocking(self->spi.peripheral, 0, self->cmd, 1);
        result = self->cmd[0];
    }
    rambus_ram_end_op(self);
    return result;
}

uint8_t* rambus_ram_make_cmd(rambus_ram_obj_t *self, uint8_t cmd, uint32_t addr, uint8_t data) {
    if (addr > rambus_ram_get_end_addr(self)) {
        addr -= rambus_ram_get_end_addr(self);
    }

    self->cmd[0] = cmd;
    self->cmd[1] = addr & 0xFF;
    self->cmd[2] = (addr >> 8) & 0xFF;
    self->cmd[3] = (addr >> 16) & 0xFF;
    self->cmd[4] = data;
    return self->cmd;
}

void rambus_ram_begin_op(rambus_ram_obj_t *self) {
    if (common_hal_busio_spi_try_lock(&self->spi)) {
        common_hal_digitalio_digitalinout_set_value(&self->cs, false);
    } else {
        mp_raise_RuntimeError(MP_ERROR_TEXT("Unable to lock SPI device"));
    }
}

void rambus_ram_end_op(rambus_ram_obj_t *self) {
    common_hal_busio_spi_unlock(&self->spi);
    common_hal_digitalio_digitalinout_set_value(&self->cs, true);
}

void rambus_ram_release(rambus_ram_obj_t *self) {
    // TODO: any resources to release?
}
