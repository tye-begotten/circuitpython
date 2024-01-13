/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Scott Shawcroft for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>

#include "shared-module/displayio/__init__.h"

#include "shared/runtime/interrupt_char.h"
#include "py/runtime.h"
#include "shared-bindings/board/__init__.h"
#include "shared-bindings/displayio/Bitmap.h"
#include "shared-bindings/displayio/RAMBusBitmap.h"
#include "shared-bindings/displayio/Group.h"
#include "shared-bindings/displayio/Palette.h"
#include "shared-module/displayio/area.h"
#include "supervisor/shared/display.h"
#include "supervisor/shared/reload.h"

#include "supervisor/spi_flash_api.h"
#include "py/mpconfig.h"

#if CIRCUITPY_BUSDISPLAY
#include "shared-bindings/busdisplay/BusDisplay.h"
#endif

#if CIRCUITPY_DOTCLOCKFRAMEBUFFER
#include "shared-bindings/dotclockframebuffer/DotClockFramebuffer.h"
#endif

#if CIRCUITPY_SHARPDISPLAY
#include "shared-bindings/sharpdisplay/SharpMemoryFramebuffer.h"
#include "shared-module/sharpdisplay/SharpMemoryFramebuffer.h"
#endif

primary_display_bus_t display_buses[CIRCUITPY_DISPLAY_LIMIT];
primary_display_t displays[CIRCUITPY_DISPLAY_LIMIT];

displayio_buffer_transform_t null_transform = {
    .x = 0,
    .y = 0,
    .dx = 1,
    .dy = 1,
    .scale = 1,
    .width = 0,
    .height = 0,
    .mirror_x = false,
    .mirror_y = false,
    .transpose_xy = false
};

#if CIRCUITPY_RGBMATRIX || CIRCUITPY_IS31FL3741 || CIRCUITPY_VIDEOCORE
STATIC bool any_display_uses_this_framebuffer(mp_obj_base_t *obj) {
    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        if (displays[i].display_base.type == &framebufferio_framebufferdisplay_type) {
            framebufferio_framebufferdisplay_obj_t *display = &displays[i].framebuffer_display;
            if (display->framebuffer == obj) {
                return true;
            }
        }
    }
    return false;
}
#endif


void displayio_background(void) {
    if (mp_hal_is_interrupted()) {
        return;
    }
    if (autoreload_ready()) {
        // Reload is about to happen, so don't redisplay.
        return;
    }

    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        mp_const_obj_t display_type = displays[i].display_base.type;
        if (display_type == NULL || display_type == &mp_type_NoneType) {
            // Skip null display.
            continue;
        }
        if (false) {
        #if CIRCUITPY_BUSDISPLAY
        } else if (display_type == &busdisplay_busdisplay_type) {
            busdisplay_busdisplay_background(&displays[i].display);
        #endif
        #if CIRCUITPY_FRAMEBUFFERIO
        } else if (display_type == &framebufferio_framebufferdisplay_type) {
            framebufferio_framebufferdisplay_background(&displays[i].framebuffer_display);
        #endif
        #if CIRCUITPY_EPAPERDISPLAY
        } else if (display_type == &epaperdisplay_epaperdisplay_type) {
            epaperdisplay_epaperdisplay_background(&displays[i].epaper_display);
        #endif
        }
    }

}

void common_hal_displayio_release_displays(void) {
    // Release displays before busses so that they can send any final commands to turn the display
    // off properly.
    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        mp_const_obj_t display_type = displays[i].display_base.type;
        if (display_type == NULL || display_type == &mp_type_NoneType) {
            continue;
        #if CIRCUITPY_BUSDISPLAY
        } else if (display_type == &busdisplay_busdisplay_type) {
            release_busdisplay(&displays[i].display);
        #endif
        #if CIRCUITPY_EPAPERDISPLAY
        } else if (display_type == &epaperdisplay_epaperdisplay_type) {
            release_epaperdisplay(&displays[i].epaper_display);
        #endif
        #if CIRCUITPY_FRAMEBUFFERIO
        } else if (display_type == &framebufferio_framebufferdisplay_type) {
            release_framebufferdisplay(&displays[i].framebuffer_display);
        #endif
        }
        displays[i].display_base.type = &mp_type_NoneType;
    }
    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        mp_const_obj_t bus_type = display_buses[i].bus_base.type;
        if (bus_type == NULL || bus_type == &mp_type_NoneType) {
            continue;
        #if CIRCUITPY_FOURWIRE
        } else if (bus_type == &fourwire_fourwire_type) {
            common_hal_fourwire_fourwire_deinit(&display_buses[i].fourwire_bus);
        #endif
        #if CIRCUITPY_I2CDISPLAYBUS
        } else if (bus_type == &i2cdisplaybus_i2cdisplaybus_type) {
            common_hal_i2cdisplaybus_i2cdisplaybus_deinit(&display_buses[i].i2cdisplay_bus);
        #endif
        #if CIRCUITPY_DOTCLOCKFRAMEBUFFER
        } else if (bus_type == &dotclockframebuffer_framebuffer_type) {
            common_hal_dotclockframebuffer_framebuffer_deinit(&display_buses[i].dotclock);
        #endif
        #if CIRCUITPY_PARALLELDISPLAYBUS
        } else if (bus_type == &paralleldisplaybus_parallelbus_type) {
            common_hal_paralleldisplaybus_parallelbus_deinit(&display_buses[i].parallel_bus);
        #endif
        #if CIRCUITPY_RGBMATRIX
        } else if (bus_type == &rgbmatrix_RGBMatrix_type) {
            common_hal_rgbmatrix_rgbmatrix_deinit(&display_buses[i].rgbmatrix);
        #endif
        #if CIRCUITPY_IS31FL3741
        } else if (bus_type == &is31fl3741_framebuffer_type) {
            common_hal_is31fl3741_framebuffer_deinit(&display_buses[i].is31fl3741);
        #endif
        #if CIRCUITPY_SHARPDISPLAY
        } else if (bus_type == &sharpdisplay_framebuffer_type) {
            common_hal_sharpdisplay_framebuffer_deinit(&display_buses[i].sharpdisplay);
        #endif
        #if CIRCUITPY_VIDEOCORE
        } else if (bus_type == &videocore_framebuffer_type) {
            common_hal_videocore_framebuffer_deinit(&display_buses[i].videocore);
        #endif
        #if CIRCUITPY_PICODVI
        } else if (bus_type == &picodvi_framebuffer_type) {
            common_hal_picodvi_framebuffer_deinit(&display_buses[i].picodvi);
        #endif
        }
        display_buses[i].bus_base.type = &mp_type_NoneType;
    }

    supervisor_stop_terminal();
}

void reset_displays(void) {
    // The SPI buses used by FourWires may be allocated on the heap so we need to move them inline.
    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        mp_const_obj_t display_bus_type = display_buses[i].bus_base.type;
        if (display_bus_type == NULL || display_bus_type == &mp_type_NoneType) {
            continue;
        #if CIRCUITPY_FOURWIRE
        } else if (display_bus_type == &fourwire_fourwire_type) {
            fourwire_fourwire_obj_t *fourwire = &display_buses[i].fourwire_bus;
            if (((size_t)fourwire->bus) < ((size_t)&display_buses) ||
                ((size_t)fourwire->bus) > ((size_t)&display_buses + CIRCUITPY_DISPLAY_LIMIT * sizeof(primary_display_bus_t))) {
                busio_spi_obj_t *original_spi = fourwire->bus;
                #if CIRCUITPY_BOARD_SPI
                // We don't need to move original_spi if it is a board.SPI object because it is
                // statically allocated already. (Doing so would also make it impossible to reference in
                // a subsequent VM run.)
                if (common_hal_board_is_spi(original_spi)) {
                    continue;
                }
                #endif
                #ifdef BOARD_USE_INTERNAL_SPI
                if (original_spi == (mp_obj_t)(&supervisor_flash_spi_bus)) {
                    continue;
                }
                #endif
                memcpy(&fourwire->inline_bus, original_spi, sizeof(busio_spi_obj_t));
                fourwire->bus = &fourwire->inline_bus;
                // Check for other display buses that use the same spi bus and swap them too.
                for (uint8_t j = i + 1; j < CIRCUITPY_DISPLAY_LIMIT; j++) {
                    if (display_buses[j].fourwire_bus.base.type == &fourwire_fourwire_type &&
                        display_buses[j].fourwire_bus.bus == original_spi) {
                        display_buses[j].fourwire_bus.bus = &fourwire->inline_bus;
                    }
                }
            }
        #endif
        #if CIRCUITPY_I2CDISPLAYBUS
        } else if (display_bus_type == &i2cdisplaybus_i2cdisplaybus_type) {
            i2cdisplaybus_i2cdisplaybus_obj_t *i2c = &display_buses[i].i2cdisplay_bus;
            // Check to see if we need to inline the I2C bus.
            if (((size_t)i2c->bus) < ((size_t)&display_buses) ||
                ((size_t)i2c->bus) > ((size_t)&display_buses + CIRCUITPY_DISPLAY_LIMIT * sizeof(primary_display_bus_t))) {
                busio_i2c_obj_t *original_i2c = i2c->bus;
                #if CIRCUITPY_BOARD_I2C
                // We don't need to move original_i2c if it is a board.I2C object because it is
                // statically allocated already. (Doing so would also make it impossible to reference in
                // a subsequent VM run.)
                if (common_hal_board_is_i2c(original_i2c)) {
                    continue;
                }
                #endif
                memcpy(&i2c->inline_bus, original_i2c, sizeof(busio_i2c_obj_t));
                i2c->bus = &i2c->inline_bus;
                // Check for other displays that use the same i2c bus and swap them too.
                for (uint8_t j = i + 1; j < CIRCUITPY_DISPLAY_LIMIT; j++) {
                    if (display_buses[j].i2cdisplay_bus.base.type == &i2cdisplaybus_i2cdisplaybus_type &&
                        display_buses[j].i2cdisplay_bus.bus == original_i2c) {
                        display_buses[j].i2cdisplay_bus.bus = &i2c->inline_bus;
                    }
                }
            }
        #endif
        #if CIRCUITPY_RGBMATRIX
        } else if (display_bus_type == &rgbmatrix_RGBMatrix_type) {
            rgbmatrix_rgbmatrix_obj_t *pm = &display_buses[i].rgbmatrix;
            if (!any_display_uses_this_framebuffer(&pm->base)) {
                common_hal_rgbmatrix_rgbmatrix_deinit(pm);
            } else {
                common_hal_rgbmatrix_rgbmatrix_set_paused(pm, true);
            }
        #endif
        #if CIRCUITPY_IS31FL3741
        } else if (display_bus_type == &is31fl3741_framebuffer_type) {
            is31fl3741_framebuffer_obj_t *is31fb = &display_buses[i].is31fl3741;

            if (((uint32_t)is31fb->is31fl3741->i2c) < ((uint32_t)&display_buses) ||
                ((uint32_t)is31fb->is31fl3741->i2c) > ((uint32_t)&display_buses + CIRCUITPY_DISPLAY_LIMIT)) {
                #if CIRCUITPY_BOARD_I2C
                // We don't need to move original_i2c if it is the board.I2C object because it is
                // statically allocated already. (Doing so would also make it impossible to reference in
                // a subsequent VM run.)
                if (common_hal_board_is_i2c(is31fb->is31fl3741->i2c)) {
                    continue;
                }
                #endif

                is31fl3741_IS31FL3741_obj_t *original_is31 = is31fb->is31fl3741;
                memcpy(&is31fb->inline_is31fl3741, original_is31, sizeof(is31fl3741_IS31FL3741_obj_t));
                is31fb->is31fl3741 = &is31fb->inline_is31fl3741;

                busio_i2c_obj_t *original_i2c = is31fb->is31fl3741->i2c;
                memcpy(&is31fb->is31fl3741->inline_i2c, original_i2c, sizeof(busio_i2c_obj_t));
                is31fb->is31fl3741->i2c = &is31fb->is31fl3741->inline_i2c;
            }

            if (!any_display_uses_this_framebuffer(&is31fb->base)) {
                common_hal_is31fl3741_framebuffer_deinit(is31fb);
            } else {
                common_hal_is31fl3741_framebuffer_set_paused(is31fb, true);
            }
        #endif
        #if CIRCUITPY_SHARPDISPLAY
        } else if (display_bus_type == &sharpdisplay_framebuffer_type) {
            sharpdisplay_framebuffer_obj_t *sharp = &display_buses[i].sharpdisplay;
            common_hal_sharpdisplay_framebuffer_reset(sharp);
        #endif
        #if CIRCUITPY_VIDEOCORE
        } else if (display_bus_type == &videocore_framebuffer_type) {
            videocore_framebuffer_obj_t *vc = &display_buses[i].videocore;
            if (!any_display_uses_this_framebuffer(&vc->base)) {
                common_hal_videocore_framebuffer_deinit(vc);
            }
            // The framebuffer is allocated outside of the heap so it doesn't
            // need to be moved.
        #endif
        #if CIRCUITPY_PICODVI
        } else if (display_bus_type == &picodvi_framebuffer_type) {
            picodvi_framebuffer_obj_t *vc = &display_buses[i].picodvi;
            if (!any_display_uses_this_framebuffer(&vc->base)) {
                common_hal_picodvi_framebuffer_deinit(vc);
            }
        #endif
        } else {
            // Not an active display bus.
            continue;
        }
    }

    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        // Reset the displayed group. Only the first will get the terminal but
        // that's ok.
        mp_const_obj_t display_type = displays[i].display_base.type;
        if (display_type == NULL || display_type == &mp_type_NoneType) {
            continue;
        #if CIRCUITPY_BUSDISPLAY
        } else if (display_type == &busdisplay_busdisplay_type) {
            reset_busdisplay(&displays[i].display);
        #endif
        #if CIRCUITPY_EPAPERDISPLAY
        } else if (display_type == &epaperdisplay_epaperdisplay_type) {
            epaperdisplay_epaperdisplay_obj_t *display = &displays[i].epaper_display;
            epaperdisplay_epaperdisplay_reset(display);
        #endif
        #if CIRCUITPY_FRAMEBUFFERIO
        } else if (display_type == &framebufferio_framebufferdisplay_type) {
            framebufferio_framebufferdisplay_reset(&displays[i].framebuffer_display);
        #endif
        }
    }
}

void displayio_gc_collect(void) {
    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        mp_const_obj_t display_bus_type = display_buses[i].bus_base.type;
        if (display_bus_type == NULL || display_bus_type == &mp_type_NoneType) {
            continue;
        }
        #if CIRCUITPY_RGBMATRIX
        if (display_bus_type == &rgbmatrix_RGBMatrix_type) {
            rgbmatrix_rgbmatrix_collect_ptrs(&display_buses[i].rgbmatrix);
        }
        #endif
        #if CIRCUITPY_IS31FL3741
        if (display_bus_type == &is31fl3741_framebuffer_type) {
            is31fl3741_framebuffer_collect_ptrs(&display_buses[i].is31fl3741);
        }
        #endif
        #if CIRCUITPY_SHARPDISPLAY
        if (display_bus_type == &sharpdisplay_framebuffer_type) {
            common_hal_sharpdisplay_framebuffer_collect_ptrs(&display_buses[i].sharpdisplay);
        }
        #endif
    }

    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        mp_const_obj_t display_type = displays[i].display_base.type;
        if (display_type == NULL || display_type == &mp_type_NoneType) {
            continue;

            // Alternatively, we could use gc_collect_root over the whole object,
            // but this is more precise, and is the only field that needs marking.
        #if CIRCUITPY_BUSDISPLAY
        } else if (display_type == &busdisplay_busdisplay_type) {
            busdisplay_busdisplay_collect_ptrs(&displays[i].display);
        #endif
        #if CIRCUITPY_FRAMEBUFFERIO
        } else if (display_type == &framebufferio_framebufferdisplay_type) {
            framebufferio_framebufferdisplay_collect_ptrs(&displays[i].framebuffer_display);
        #endif
        #if CIRCUITPY_EPAPERDISPLAY
        } else if (display_type == &epaperdisplay_epaperdisplay_type) {
            epaperdisplay_epaperdisplay_collect_ptrs(&displays[i].epaper_display);
        #endif
        }
    }
}

primary_display_t *allocate_display(void) {
    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        mp_const_obj_t display_type = displays[i].display_base.type;
        if (display_type == NULL || display_type == &mp_type_NoneType) {
            // Clear this memory so it is in a known state before init.
            memset(&displays[i], 0, sizeof(displays[i]));
            // Default to None so that it works as board.DISPLAY.
            displays[i].display_base.type = &mp_type_NoneType;
            return &displays[i];
        }
    }
    return NULL;
}

primary_display_t *allocate_display_or_raise(void) {
    primary_display_t *result = allocate_display();
    if (result) {
        return result;
    }
    mp_raise_RuntimeError(MP_ERROR_TEXT("Too many displays"));
}

primary_display_bus_t *allocate_display_bus(void) {
    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        mp_const_obj_t display_bus_type = display_buses[i].bus_base.type;
        if (display_bus_type == NULL || display_bus_type == &mp_type_NoneType) {
            // Clear this memory so it is in a known state before init.
            memset(&display_buses[i], 0, sizeof(display_buses[i]));
            display_buses[i].bus_base.type = &mp_type_NoneType;
            return &display_buses[i];
        }
    }
    return NULL;
}

primary_display_bus_t *allocate_display_bus_or_raise(void) {
    primary_display_bus_t *result = allocate_display_bus();
    if (result) {
        return result;
    }
    mp_raise_RuntimeError(MP_ERROR_TEXT("Too many display busses; forgot displayio.release_displays() ?"));
}
