#include "shared-module/rambus/__init__.h"
#include <stdlib.h>
#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/objarray.h"
#include "py/mpprint.h"
#include "shared-bindings/time/__init__.h"
#include "extmod/modtime.h"
#include "shared-bindings/util.h"
#include "shared-module/rambus/RAM.h"
#include "shared-bindings/rambus/RAM.h"
#include "shared-bindings/digitalio/DigitalInOut.h"
#include "shared-bindings/digitalio/Direction.h"





STATIC void wait_ns(uint32_t ns) {
    uint64_t deadline = common_hal_time_monotonic_ns() + ns;
    while (common_hal_time_monotonic_ns() < deadline) {
        RUN_BACKGROUND_TASKS;
    }
}

// STATIC void print_cmd(rambus_ram_obj_t *self) {
//     mp_print_str(&mp_plat_print, "-----------------------\n");
//     mp_print_str(&mp_plat_print, "cmd bytes:\n");
//     mp_printf(&mp_plat_print, "    cmd=%x\n", self->cmd[0]);
//     mp_printf(&mp_plat_print, "    addr1=%x\n", self->cmd[1]);
//     mp_printf(&mp_plat_print, "    addr2=%x\n", self->cmd[2]);
//     mp_printf(&mp_plat_print, "    addr3=%x\n", self->cmd[3]);
//     mp_printf(&mp_plat_print, "    data=%x\n", self->cmd[4]);
//     mp_printf(&mp_plat_print, "    b5=%x\n", self->cmd[5]);
//     mp_print_str(&mp_plat_print, "-----------------------\n");
// }

STATIC uint8_t* rambus_ram_make_cmd(rambus_ram_obj_t *self, uint8_t cmd, addr_t addr, uint8_t data) {
    self->cmd[0] = cmd;
    self->cmd[1] = (uint8_t)((addr >> 16) & 0xFF);
    self->cmd[2] = (uint8_t)((addr >> 8) & 0xFF);
    self->cmd[3] = (uint8_t)(addr & 0xFF);
    self->cmd[4] = data;
    
    // print_cmd(self);
    return self->cmd;
}

void rambus_ram_construct(rambus_ram_obj_t *self, ram_types ram_type, uint16_t pg_size, uint16_t pg_cnt, uint8_t wrd_size, 
    busio_spi_obj_t *spi, const mcu_pin_obj_t *cs, const mcu_pin_obj_t *hold) {
    self->ram_type = ram_type;
    self->pg_size = pg_size;
    self->pg_cnt = pg_cnt;
    self->wrd_size = wrd_size;
    self->spi = spi;
    self->mode = __MODE_NONE; // TODO: default mode param

    common_hal_digitalio_digitalinout_construct(&self->cs, cs);
    common_hal_digitalio_digitalinout_switch_to_output(&self->cs, true, DRIVE_MODE_PUSH_PULL);

    common_hal_digitalio_digitalinout_construct(&self->hold, hold);
    common_hal_digitalio_digitalinout_switch_to_output(&self->hold, true, DRIVE_MODE_PUSH_PULL);

    rambus_ram_begin_op(self, __MODE_NONE);
    // initialize the ram
    wait_ns(35);
    rambus_ram_end_op(self);
    wait_ns(35);

    // rambus_ram_set_mode(self, self->mode);
}

// TODO: rename to deinit for consistency
void rambus_ram_deinit(rambus_ram_obj_t *self) {
    if (!rambus_ram_deinited(self)) {
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

bool rambus_ram_deinited(rambus_ram_obj_t *self) {
    return self == NULL || 
        common_hal_busio_spi_deinited(self->spi) || 
        common_hal_digitalio_digitalinout_deinited(&self->cs) ||
        common_hal_digitalio_digitalinout_deinited(&self->hold);
}

void rambus_ram_check_deinit(rambus_ram_obj_t *self) {
    if (rambus_ram_deinited(self)) {
        raise_deinited_error();
    }
}

rambus_ram_obj_t *validate_obj_is_ram(mp_obj_t obj, qstr arg_name) {
    return MP_OBJ_TO_PTR(mp_arg_validate_type(obj, &rambus_ram_type, arg_name));
}

addr_t rambus_ram_get_size(rambus_ram_obj_t *self) {
    // TODO: return in bits or bytes? take wrd_size into account?
    return self->pg_size * self->pg_cnt;
}

addr_t rambus_ram_get_start_addr(rambus_ram_obj_t *self) {
    // TODO: allow for changing start addr?
    return (addr_t)0;
}

addr_t rambus_ram_get_end_addr(rambus_ram_obj_t *self) {
    return rambus_ram_get_start_addr(self) + rambus_ram_get_size(self);
}

void rambus_ram_configure_spi(rambus_ram_obj_t *self, rambus_proto_spi_t *proto) {
    // TODO:
}
void rambus_ram_configure_sdi(rambus_ram_obj_t *self, rambus_proto_sdi_t *proto) {
    // TODO:
}
void rambus_ram_configure_sqi(rambus_ram_obj_t *self, rambus_proto_sqi_t *proto) {
    // TODO:
}

uint8_t rambus_ram_get_mode(rambus_ram_obj_t *self) {
    self->cmd[0] = __READ_MODE;
    rambus_ram_begin_op(self, __MODE_NONE);
    bool ok = common_hal_busio_spi_write(self->spi, self->cmd, 1) &&
        common_hal_busio_spi_read(self->spi, self->cmd, 1, 0xff);
    rambus_ram_end_op(self);

    if (!ok) {
        mp_raise_OSError(MP_EIO);
    }

    self->mode = self->cmd[0];
    return self->mode;
}

void rambus_ram_set_mode(rambus_ram_obj_t *self, uint8_t mode) {
    if (self->mode == mode) {
        // assuming we can trust that this is always in sync
        return;
    }

    self->cmd[0] = __WRITE_MODE;
    self->cmd[1] = mode;
    
    rambus_ram_begin_op(self, __MODE_NONE);
    // common_hal_digitalio_digitalinout_set_value(&self->cs, false);
    bool ok = common_hal_busio_spi_write(self->spi, self->cmd, 2);
    // common_hal_digitalio_digitalinout_set_value(&self->cs, true);
    rambus_ram_end_op(self);

    if (!ok) {
        mp_raise_OSError(MP_EIO);
    }

    // TODO: remove check once working?
    // if (rambus_ram_get_mode(self) != mode) {
    //     mp_raise_ValueError(MP_ERROR_TEXT("Mode set failed"));
    // }

    self->mode = mode;
}

void rambus_ram_write_byte(rambus_ram_obj_t *self, addr_t addr, uint8_t data) {
    rambus_ram_begin_op(self, __MODE_BYTE);
    bool ok = common_hal_busio_spi_write(self->spi, rambus_ram_make_cmd(self, __WRITE, addr, data), 5);
    rambus_ram_end_op(self);

    if (!ok) {
        mp_raise_OSError(MP_EIO);
    }
}

void rambus_ram_write_page(rambus_ram_obj_t *self, addr_t addr, uint8_t *data, size_t len) {
    rambus_ram_write_into(self, __MODE_PAGE, addr, data, len);
}

void rambus_ram_write_seq(rambus_ram_obj_t *self, addr_t addr, uint8_t *data, size_t len) {
    rambus_ram_write_into(self, __MODE_SEQ, addr, data, len);
}

void rambus_ram_write_into(rambus_ram_obj_t *self, uint8_t mode, addr_t addr, uint8_t *data, size_t len) {
    rambus_ram_begin_op(self, mode);
    bool ok = common_hal_busio_spi_write(self->spi, rambus_ram_make_cmd(self, __WRITE, addr, 0), 4) &&
        common_hal_busio_spi_write(self->spi, data, len);
    rambus_ram_end_op(self);

    if (!ok) {
        mp_raise_OSError(MP_EIO);
    }
}

uint8_t rambus_ram_read_byte(rambus_ram_obj_t *self, addr_t addr, uint8_t *buf, uint8_t start) {
    rambus_ram_check_deinit(self);
    
    if (buf == NULL) {
        buf = self->cmd;
    }

    rambus_ram_begin_op(self, __MODE_BYTE);

    bool ok = common_hal_busio_spi_transfer(self->spi, rambus_ram_make_cmd(self, __READ, addr, 0), buf, 6);

    rambus_ram_end_op(self);

    uint8_t result = buf[5];

    if (!ok) {
        mp_raise_OSError(MP_EIO);
    }
    
    return result;
}

void rambus_ram_read_page(rambus_ram_obj_t *self, addr_t addr, uint8_t *buf, size_t len) {
    rambus_ram_read_into(self, __MODE_PAGE, addr, buf, len);
}

void rambus_ram_read_seq(rambus_ram_obj_t *self, addr_t addr, uint8_t *buf, size_t len) {
    rambus_ram_read_into(self, __MODE_SEQ, addr, buf, len);
}

void rambus_ram_read_into(rambus_ram_obj_t *self, uint8_t mode, addr_t addr, uint8_t *buf, size_t len) {
    rambus_ram_check_deinit(self);
    
    rambus_ram_begin_op(self, mode);

    bool ok = common_hal_busio_spi_write(self->spi, rambus_ram_make_cmd(self, __READ, addr, 0), 5) &&
        common_hal_busio_spi_read(self->spi, buf, len, 0xff);
    
    rambus_ram_end_op(self);

    if (!ok) {
        mp_raise_OSError(MP_EIO);
    }
}

bool rambus_ram_exec_cmd(rambus_ram_obj_t *self, uint8_t cmd, addr_t addr, uint8_t data, uint8_t cmd_len) {
    return common_hal_busio_spi_write(self->spi, rambus_ram_make_cmd(self, cmd, addr, data), cmd_len);
}

void rambus_ram_begin_op(rambus_ram_obj_t *self, uint8_t mode) {
    rambus_ram_check_deinit(self);

    if (mode != __MODE_NONE) {
        rambus_ram_set_mode(self, mode);
    }

    if (common_hal_busio_spi_try_lock(self->spi)) {
        common_hal_busio_spi_configure(self->spi, 30000000, 0, 0, 8);
        
        common_hal_digitalio_digitalinout_set_value(&self->cs, false);
    } else {
        mp_raise_RuntimeError(MP_ERROR_TEXT("Unable to lock SPI device"));
    }
}

void rambus_ram_end_op(rambus_ram_obj_t *self) {
    rambus_ram_check_deinit(self);

    common_hal_busio_spi_unlock(self->spi);
    common_hal_digitalio_digitalinout_set_value(&self->cs, true);
}

