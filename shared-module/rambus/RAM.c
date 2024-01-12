
#include <stdlib.h>
#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/objarray.h"
#include "py/mpprint.h"
#include "shared-bindings/time/__init__.h"
#include "extmod/modtime.h"
#include "shared-bindings/util.h"
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


void wait_ns(uint32_t ns) {
    uint64_t i = 0;
    uint64_t start = common_hal_time_monotonic_ns();
    
    while (common_hal_time_monotonic_ns() - start < ns) {
        /* wait */
        i++;
        if (i == 0xffffffff) {
            mp_raise_OSError(ENOSYS);
            break;
        }
    }
}

void shared_module_rambus_ram_construct(rambus_ram_obj_t *self, ram_types ram_type, uint16_t pg_size, uint16_t pg_cnt, uint8_t wrd_size, 
    busio_spi_obj_t *spi, const mcu_pin_obj_t *cs, const mcu_pin_obj_t *hold) {
    self->ram_type = ram_type;
    self->pg_size = pg_size;
    self->pg_cnt = pg_cnt;
    self->wrd_size = wrd_size;
    self->spi = spi;
    self->mode = 0; // TODO: default mode param

    
    common_hal_digitalio_digitalinout_construct(&self->cs, cs);
    common_hal_digitalio_digitalinout_switch_to_output(&self->cs, true, DRIVE_MODE_PUSH_PULL);

    common_hal_digitalio_digitalinout_construct(&self->hold, hold);
    common_hal_digitalio_digitalinout_switch_to_output(&self->hold, true, DRIVE_MODE_PUSH_PULL);

    shared_module_rambus_ram_begin_op(self);
    shared_module_rambus_ram_end_op(self);
}

void shared_module_rambus_ram_release(rambus_ram_obj_t *self) {
    if (!shared_module_rambus_ram_deinited(self)) {
        common_hal_busio_spi_deinit(self->spi);
        common_hal_digitalio_digitalinout_deinit(&self->cs);
        common_hal_digitalio_digitalinout_deinit(&self->hold);

        self->spi = NULL;
        // TODO: need to delete/dealloc these?
        // self->cs = NULL;
        // self->hold = NULL;
        // free(self->cmd);
    }
}

bool shared_module_rambus_ram_deinited(rambus_ram_obj_t *self) {
    return common_hal_busio_spi_deinited(self->spi) || 
        common_hal_digitalio_digitalinout_deinited(&self->cs) ||
        common_hal_digitalio_digitalinout_deinited(&self->hold);
}

void shared_module_rambus_ram_check_deinit(rambus_ram_obj_t *self) {
    if (shared_module_rambus_ram_deinited(self)) {
        raise_deinited_error();
    }
}

addr_t shared_module_rambus_ram_get_size(rambus_ram_obj_t *self) {
    // TODO: return in bits or bytes? take wrd_size into account?
    return self->pg_size * self->pg_cnt;
}

addr_t shared_module_rambus_ram_get_start_addr(rambus_ram_obj_t *self) {
    // TODO: allow for changing start addr?
    return (addr_t)0;
}

addr_t shared_module_rambus_ram_get_end_addr(rambus_ram_obj_t *self) {
    return shared_module_rambus_ram_get_start_addr(self) + shared_module_rambus_ram_get_size(self);
}

uint8_t shared_module_rambus_ram_get_mode(rambus_ram_obj_t *self) {
    shared_module_rambus_ram_check_deinit(self);

    self->cmd[0] = __READ_MODE;
    shared_module_rambus_ram_begin_op(self);

    bool ok = common_hal_busio_spi_write(self->spi, self->cmd, 1);

    if (ok) {
        ok = common_hal_busio_spi_read(self->spi, self->cmd, 1, 0xff);
    }

    shared_module_rambus_ram_end_op(self);

    if (!ok) {
        mp_raise_OSError(MP_EIO);
    }

    self->mode = self->cmd[0];
    return self->mode;
}

void shared_module_rambus_ram_set_mode(rambus_ram_obj_t *self, uint8_t mode) {
    shared_module_rambus_ram_check_deinit(self);

    self->cmd[0] = __WRITE_MODE;
    self->cmd[1] = mode;
    
    shared_module_rambus_ram_begin_op(self);

    bool ok = common_hal_busio_spi_write(self->spi, self->cmd, 2);
    shared_module_rambus_ram_end_op(self);

    if (!ok) {
        mp_raise_OSError(MP_EIO);
    }

    // TODO: remove check once working?
    if (shared_module_rambus_ram_get_mode(self) != mode) {
        mp_raise_ValueError(MP_ERROR_TEXT("Mode set failed"));
    }
}

void shared_module_rambus_ram_write_byte(rambus_ram_obj_t *self, addr_t addr, uint8_t data) {
    shared_module_rambus_ram_check_deinit(self);
    shared_module_rambus_ram_set_mode(self, __MODE_BYTE);
    shared_module_rambus_ram_begin_op(self);
    bool ok = common_hal_busio_spi_write(self->spi, shared_module_rambus_ram_make_cmd(self, __WRITE, addr, data), 5);
    shared_module_rambus_ram_end_op(self);

    if (!ok) {
        mp_raise_OSError(MP_EIO);
    }
}

uint8_t shared_module_rambus_ram_read_byte(rambus_ram_obj_t *self, addr_t addr, uint8_t *buf, uint8_t start) {
    shared_module_rambus_ram_check_deinit(self);
    
    shared_module_rambus_ram_set_mode(self, __MODE_BYTE);
    shared_module_rambus_ram_begin_op(self);
    shared_module_rambus_ram_make_cmd(self, __READ, addr, 0);

    if (buf == NULL) {
        buf = self->cmd;
    }

    bool ok = common_hal_busio_spi_transfer(self->spi, self->cmd, buf, 6);
    uint8_t result = buf[5];

    shared_module_rambus_ram_end_op(self);

    if (!ok) {
        mp_raise_OSError(MP_EIO);
    }
    
    return result;
}

void print_cmd(rambus_ram_obj_t *self) {
    mp_print_str(&mp_plat_print, "-----------------------\n");
    mp_print_str(&mp_plat_print, "cmd bytes:\n");
    mp_printf(&mp_plat_print, "    cmd=%x\n", self->cmd[0]);
    mp_printf(&mp_plat_print, "    addr1=%x\n", self->cmd[1]);
    mp_printf(&mp_plat_print, "    addr2=%x\n", self->cmd[2]);
    mp_printf(&mp_plat_print, "    addr3=%x\n", self->cmd[3]);
    mp_printf(&mp_plat_print, "    data=%x\n", self->cmd[4]);
    mp_printf(&mp_plat_print, "    b5=%x\n", self->cmd[5]);
    mp_print_str(&mp_plat_print, "-----------------------\n");
}

uint8_t* shared_module_rambus_ram_make_cmd(rambus_ram_obj_t *self, uint8_t cmd, addr_t addr, uint8_t data) {
    shared_module_rambus_ram_check_deinit(self);

    self->cmd[0] = cmd;
    self->cmd[1] = (uint8_t)((addr >> 16) & 0xFF);
    self->cmd[2] = (uint8_t)((addr >> 8) & 0xFF);
    self->cmd[3] = (uint8_t)(addr & 0xFF);
    self->cmd[4] = data;
    
    // print_cmd(self);
    return self->cmd;
}

void shared_module_rambus_ram_begin_op(rambus_ram_obj_t *self) {
    shared_module_rambus_ram_check_deinit(self);

    if (common_hal_busio_spi_try_lock(self->spi)) {
        common_hal_busio_spi_configure(self->spi, 30000000, 0, 0, 8);
        common_hal_digitalio_digitalinout_set_value(&self->cs, false);
    } else {
        mp_raise_RuntimeError(MP_ERROR_TEXT("Unable to lock SPI device"));
    }
}

void shared_module_rambus_ram_end_op(rambus_ram_obj_t *self) {
    shared_module_rambus_ram_check_deinit(self);

    common_hal_busio_spi_unlock(self->spi);
    common_hal_digitalio_digitalinout_set_value(&self->cs, true);
}

