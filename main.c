/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Scott Shawcroft for Adafruit Industries
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

#include <stdint.h>
#include <string.h>

#include "extmod/vfs.h"
#include "extmod/vfs_fat.h"

#include "genhdr/mpversion.h"
#include "py/nlr.h"
#include "py/compile.h"
#include "py/frozenmod.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/stackctrl.h"

#include "shared/readline/readline.h"
#include "shared/runtime/pyexec.h"

#include "background.h"
#include "mpconfigboard.h"
#include "supervisor/background_callback.h"
#include "supervisor/board.h"
#include "supervisor/cpu.h"
#include "supervisor/filesystem.h"
#include "supervisor/port.h"
#include "supervisor/serial.h"
#include "supervisor/shared/reload.h"
#include "supervisor/shared/safe_mode.h"
#include "supervisor/shared/stack.h"
#include "supervisor/shared/status_leds.h"
#include "supervisor/shared/tick.h"
#include "supervisor/shared/traceback.h"
#include "supervisor/shared/workflow.h"
#include "supervisor/usb.h"
#include "supervisor/workflow.h"
#include "supervisor/shared/external_flash/external_flash.h"

#include "shared-bindings/microcontroller/__init__.h"
#include "shared-bindings/microcontroller/Processor.h"
#include "shared-bindings/supervisor/__init__.h"
#include "shared-bindings/supervisor/Runtime.h"

#include "shared-bindings/os/__init__.h"

#if CIRCUITPY_ALARM
#include "shared-bindings/alarm/__init__.h"
#endif

#if CIRCUITPY_ATEXIT
#include "shared-module/atexit/__init__.h"
#endif

#if CIRCUITPY_BLEIO
#include "shared-bindings/_bleio/__init__.h"
#include "supervisor/shared/bluetooth/bluetooth.h"
#endif

#if CIRCUITPY_BOARD
#include "shared-module/board/__init__.h"
#endif

#if CIRCUITPY_CANIO
#include "common-hal/canio/CAN.h"
#endif

#if CIRCUITPY_DISPLAYIO
#include "shared-module/displayio/__init__.h"
#endif

#if CIRCUITPY_EPAPERDISPLAY
#include "shared-bindings/epaperdisplay/EPaperDisplay.h"
#endif

#if CIRCUITPY_KEYPAD
#include "shared-module/keypad/__init__.h"
#endif

#if CIRCUITPY_MEMORYMONITOR
#include "shared-module/memorymonitor/__init__.h"
#endif

#if CIRCUITPY_SOCKETPOOL
#include "shared-bindings/socketpool/__init__.h"
#endif

#if CIRCUITPY_STATUS_BAR
#include "supervisor/shared/status_bar.h"
#endif

#if CIRCUITPY_USB_HID
#include "shared-module/usb_hid/__init__.h"
#endif

#if CIRCUITPY_WIFI
#include "shared-bindings/wifi/__init__.h"
#endif

#if CIRCUITPY_BOOT_COUNTER
#include "shared-bindings/nvm/ByteArray.h"
uint8_t value_out = 0;
#endif

#if MICROPY_ENABLE_PYSTACK && CIRCUITPY_OS_GETENV
#include "shared-module/os/__init__.h"
#endif

static void reset_devices(void) {
    #if CIRCUITPY_BLEIO_HCI
    bleio_reset();
    #endif
}

STATIC uint8_t *_heap;
STATIC uint8_t *_pystack;

STATIC const char line_clear[] = "\x1b[2K\x1b[0G";

#if MICROPY_ENABLE_PYSTACK || MICROPY_ENABLE_GC
STATIC uint8_t *_allocate_memory(safe_mode_t safe_mode, const char *env_key, size_t default_size, size_t *final_size) {
    *final_size = default_size;
    #if CIRCUITPY_OS_GETENV
    if (safe_mode == SAFE_MODE_NONE) {
        (void)common_hal_os_getenv_int(env_key, (mp_int_t *)final_size);
        if (*final_size < 0) {
            *final_size = default_size;
        }
    }
    #endif
    uint8_t *ptr = port_malloc(*final_size, false);

    #if CIRCUITPY_OS_GETENV
    if (ptr == NULL) {
        // Fallback to the build size.
        ptr = port_malloc(default_size, false);
    }
    #endif
    if (ptr == NULL) {
        reset_into_safe_mode(SAFE_MODE_NO_HEAP);
    }
    return ptr;
}
#endif

STATIC void start_mp(safe_mode_t safe_mode) {
    supervisor_workflow_reset();

    // Stack limit should be less than real stack size, so we have a chance
    // to recover from limit hit.  (Limit is measured in bytes.) The top of the
    // stack is set to our current state. Not the actual top.
    mp_stack_ctrl_init();

    uint32_t *stack_bottom = port_stack_get_limit();
    uint32_t *stack_top = port_stack_get_top();

    size_t stack_length = (stack_top - stack_bottom) * sizeof(uint32_t);
    mp_stack_set_top(stack_top);
    mp_stack_set_limit(stack_length - CIRCUITPY_EXCEPTION_STACK_SIZE);

    #if MICROPY_MAX_STACK_USAGE
    // _ezero (same as _ebss) is an int, so start 4 bytes above it.
    if (stack_get_bottom() != NULL) {
        mp_stack_set_bottom(stack_get_bottom());
        mp_stack_fill_with_sentinel();
    }
    #endif

    // Sync the file systems in case any used RAM from the GC to cache. As soon
    // as we re-init the GC all bets are off on the cache.
    filesystem_flush();

    // Clear the readline history. It references the heap we're about to destroy.
    readline_init0();

    #if MICROPY_ENABLE_PYSTACK
    size_t pystack_size = 0;
    _pystack = _allocate_memory(safe_mode, "CIRCUITPY_PYSTACK_SIZE", CIRCUITPY_PYSTACK_SIZE, &pystack_size);
    mp_pystack_init(_pystack, _pystack + pystack_size);
    #endif

    #if MICROPY_ENABLE_GC
    size_t heap_size = 0;
    _heap = _allocate_memory(safe_mode, "CIRCUITPY_HEAP_START_SIZE", CIRCUITPY_HEAP_START_SIZE, &heap_size);
    gc_init(_heap, _heap + heap_size);
    #endif
    mp_init();
    mp_obj_list_init((mp_obj_list_t *)mp_sys_path, 0);
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR_)); // current dir (or base dir of the script)
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_));
    #if MICROPY_MODULE_FROZEN
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__dot_frozen));
    #endif
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_lib));

    mp_obj_list_init((mp_obj_list_t *)mp_sys_argv, 0);
}

STATIC void stop_mp(void) {
    #if MICROPY_VFS
    mp_vfs_mount_t *vfs = MP_STATE_VM(vfs_mount_table);

    // Unmount all heap allocated vfs mounts.
    while (gc_nbytes(vfs) > 0) {
        vfs = vfs->next;
    }
    MP_STATE_VM(vfs_mount_table) = vfs;
    MP_STATE_VM(vfs_cur) = vfs;
    #endif

    background_callback_reset();

    #if CIRCUITPY_USB
    usb_background();
    #endif

    // Set the qstr pool back to the const pools. The heap allocated ones will
    // be overwritten.
    qstr_reset();

    gc_deinit();
    port_free(_heap);
    _heap = NULL;

    #if MICROPY_ENABLE_PYSTACK
    port_free(_pystack);
    _pystack = NULL;
    #endif
}

STATIC const char *_current_executing_filename = NULL;

STATIC pyexec_result_t _exec_result = {0, MP_OBJ_NULL, 0};

#if CIRCUITPY_STATUS_BAR
void supervisor_execution_status(void) {
    mp_obj_exception_t *exception = MP_OBJ_TO_PTR(_exec_result.exception);
    if (_current_executing_filename != NULL) {
        serial_write(_current_executing_filename);
    } else if ((_exec_result.return_code & PYEXEC_EXCEPTION) != 0 &&
               _exec_result.exception_line > 0 &&
               exception != NULL) {
        mp_printf(&mp_plat_print, "%d@%s %q", _exec_result.exception_line, _exec_result.exception_filename, exception->base.type->name);
    } else {
        serial_write_compressed(MP_ERROR_TEXT("Done"));
    }
}
#endif

#if CIRCUITPY_WATCHDOG
pyexec_result_t *pyexec_result(void) {
    return &_exec_result;
}
#endif

// Look for the first file that exists in the list of filenames, using mp_import_stat().
// Return its index. If no file found, return -1.
STATIC const char *first_existing_file_in_list(const char *const *filenames, size_t n_filenames) {
    for (size_t i = 0; i < n_filenames; i++) {
        mp_import_stat_t stat = mp_import_stat(filenames[i]);
        if (stat == MP_IMPORT_STAT_FILE) {
            return filenames[i];
        }
    }
    return NULL;
}

STATIC bool maybe_run_list(const char *const *filenames, size_t n_filenames) {
    _exec_result.return_code = 0;
    _exec_result.exception = MP_OBJ_NULL;
    _exec_result.exception_line = 0;
    _current_executing_filename = first_existing_file_in_list(filenames, n_filenames);
    if (_current_executing_filename == NULL) {
        return false;
    }
    mp_hal_stdout_tx_str(line_clear);
    mp_hal_stdout_tx_str(_current_executing_filename);
    serial_write_compressed(MP_ERROR_TEXT(" output:\n"));

    #if CIRCUITPY_STATUS_BAR
    supervisor_status_bar_update();
    #endif

    pyexec_file(_current_executing_filename, &_exec_result);

    #if CIRCUITPY_ATEXIT
    shared_module_atexit_execute(&_exec_result);
    #endif

    _current_executing_filename = NULL;

    #if CIRCUITPY_STATUS_BAR
    supervisor_status_bar_update();
    #endif

    return true;
}

STATIC void count_strn(void *data, const char *str, size_t len) {
    *(size_t *)data += len;
}

STATIC void cleanup_after_vm(mp_obj_t exception) {
    // Get the traceback of any exception from this run off the heap.
    // MP_OBJ_SENTINEL means "this run does not contribute to traceback storage, don't touch it"
    // MP_OBJ_NULL (=0) means "this run completed successfully, clear any stored traceback"
    if (exception != MP_OBJ_SENTINEL) {
        if (prev_traceback_string != NULL) {
            port_free(prev_traceback_string);
            prev_traceback_string = NULL;
        }
        // ReloadException is exempt from traceback printing in pyexec_file(), so treat it as "no
        // traceback" here too.
        if (exception && exception != MP_OBJ_FROM_PTR(&MP_STATE_VM(mp_reload_exception))) {
            size_t traceback_len = 0;
            mp_print_t print_count = {&traceback_len, count_strn};
            mp_obj_print_exception(&print_count, exception);
            prev_traceback_string = (char *)port_malloc(traceback_len + 1, false);
            // Empirically, this never fails in practice - even when the heap is totally filled up
            // with single-block-sized objects referenced by a root pointer, exiting the VM frees
            // up several hundred bytes, sufficient for the traceback (which tends to be shortened
            // because there wasn't memory for the full one). There may be convoluted ways of
            // making it fail, but at this point I believe they are not worth spending code on.
            if (prev_traceback_string != NULL) {
                vstr_t vstr;
                vstr_init_fixed_buf(&vstr, traceback_len, prev_traceback_string);
                mp_print_t print = {&vstr, (mp_print_strn_t)vstr_add_strn};
                mp_obj_print_exception(&print, exception);
                prev_traceback_string[traceback_len] = '\0';
            }
        }
    }

    // Reset port-independent devices, like CIRCUITPY_BLEIO_HCI.
    reset_devices();

    #if CIRCUITPY_ATEXIT
    atexit_reset();
    #endif

    // Turn off the display and flush the filesystem before the heap disappears.
    #if CIRCUITPY_DISPLAYIO
    reset_displays();
    #endif

    #if CIRCUITPY_MEMORYMONITOR
    memorymonitor_reset();
    #endif

    // Disable user related BLE state that uses the micropython heap.
    #if CIRCUITPY_BLEIO
    bleio_user_reset();
    #endif

    #if CIRCUITPY_CANIO
    common_hal_canio_reset();
    #endif

    #if CIRCUITPY_KEYPAD
    keypad_reset();
    #endif

    // Close user-initiated sockets.
    #if CIRCUITPY_SOCKETPOOL
    socketpool_user_reset();
    #endif

    // Turn off user initiated WiFi connections.
    #if CIRCUITPY_WIFI
    wifi_user_reset();
    #endif

    // reset_board_buses() first because it may release pins from the never_reset state, so that
    // reset_port() can reset them.
    #if CIRCUITPY_BOARD
    reset_board_buses();
    #endif
    reset_port();
    reset_board();

    // Free the heap last because other modules may reference heap memory and need to shut down.
    filesystem_flush();
    stop_mp();

    // Let the workflows know we've reset in case they want to restart.
    supervisor_workflow_reset();
}

STATIC void print_code_py_status_message(safe_mode_t safe_mode) {
    mp_hal_stdout_tx_str(line_clear);
    if (autoreload_is_enabled()) {
        serial_write_compressed(
            MP_ERROR_TEXT("Auto-reload is on. Simply save files over USB to run them or enter REPL to disable.\n"));
    } else {
        serial_write_compressed(MP_ERROR_TEXT("Auto-reload is off.\n"));
    }
    if (safe_mode != SAFE_MODE_NONE) {
        serial_write_compressed(MP_ERROR_TEXT("Running in safe mode! Not running saved code.\n"));
    }
}

STATIC bool run_code_py(safe_mode_t safe_mode, bool *simulate_reset) {
    bool serial_connected_at_start = serial_connected();
    bool printed_safe_mode_message = false;
    #if CIRCUITPY_AUTORELOAD_DELAY_MS > 0
    if (serial_connected_at_start) {
        serial_write("\r\n");
        print_code_py_status_message(safe_mode);
        print_safe_mode_message(safe_mode);
        printed_safe_mode_message = true;
    }
    #endif

    bool skip_repl = false;
    bool skip_wait = false;
    bool found_main = false;
    uint8_t next_code_options = 0;
    // Collects stickiness bits that apply in the current situation.
    uint8_t next_code_stickiness_situation = SUPERVISOR_NEXT_CODE_OPT_NEWLY_SET;

    // Do the filesystem flush check before reload in case another write comes
    // in while we're doing the flush.
    if (safe_mode == SAFE_MODE_NONE) {
        filesystem_flush();
    }
    if (safe_mode == SAFE_MODE_NONE && !autoreload_pending()) {
        static const char *const supported_filenames[] = {
            "code.txt", "code.py", "main.py", "main.txt"
        };
        #if CIRCUITPY_FULL_BUILD
        static const char *const double_extension_filenames[] = {
            "code.txt.py", "code.py.txt", "code.txt.txt", "code.py.py",
            "main.txt.py", "main.py.txt", "main.txt.txt", "main.py.py"
        };
        #endif

        start_mp(safe_mode);

        #if CIRCUITPY_USB
        usb_setup_with_vm();
        #endif

        // Make sure we are in the root directory before looking at files.
        common_hal_os_chdir("/");

        // Check if a different run file has been allocated
        if (next_code_configuration != NULL) {
            next_code_configuration->options &= ~SUPERVISOR_NEXT_CODE_OPT_NEWLY_SET;
            next_code_options = next_code_configuration->options;
            if (next_code_configuration->filename[0] != '\0') {
                // This is where the user's python code is actually executed:
                const char *const filenames[] = { next_code_configuration->filename };
                found_main = maybe_run_list(filenames, MP_ARRAY_SIZE(filenames));
                if (!found_main) {
                    serial_write(next_code_configuration->filename);
                    serial_write_compressed(MP_ERROR_TEXT(" not found.\n"));
                }
            }
        }
        // Otherwise, default to the standard list of filenames
        if (!found_main) {
            // This is where the user's python code is actually executed:
            found_main = maybe_run_list(supported_filenames, MP_ARRAY_SIZE(supported_filenames));
            // If that didn't work, double check the extensions
            #if CIRCUITPY_FULL_BUILD
            if (!found_main) {
                found_main = maybe_run_list(double_extension_filenames, MP_ARRAY_SIZE(double_extension_filenames));
                if (found_main) {
                    serial_write_compressed(MP_ERROR_TEXT("WARNING: Your code filename has two extensions\n"));
                }
            }
            #else
            (void)found_main;
            #endif
        }

        // Print done before resetting everything so that we get the message over
        // BLE before it is reset and we have a delay before reconnect.
        if ((_exec_result.return_code & PYEXEC_RELOAD) && supervisor_get_run_reason() == RUN_REASON_AUTO_RELOAD) {
            serial_write_compressed(MP_ERROR_TEXT("\nCode stopped by auto-reload. Reloading soon.\n"));
        } else {
            serial_write_compressed(MP_ERROR_TEXT("\nCode done running.\n"));
        }


        // Finished executing python code. Cleanup includes filesystem flush and a board reset.
        cleanup_after_vm(_exec_result.exception);
        _exec_result.exception = NULL;

        // If a new next code file was set, that is a reason to keep it (obviously). Stuff this into
        // the options because it can be treated like any other reason-for-stickiness bit. The
        // source is different though: it comes from the options that will apply to the next run,
        // while the rest of next_code_options is what applied to this run.
        if (next_code_configuration != NULL &&
            next_code_configuration->options & SUPERVISOR_NEXT_CODE_OPT_NEWLY_SET) {
            next_code_configuration->options |= SUPERVISOR_NEXT_CODE_OPT_NEWLY_SET;
        }

        if (_exec_result.return_code & PYEXEC_RELOAD) {
            next_code_stickiness_situation |= SUPERVISOR_NEXT_CODE_OPT_STICKY_ON_RELOAD;
            // Reload immediately unless the reload is due to autoreload. In that
            // case, we wait below to see if any other writes occur.
            if (supervisor_get_run_reason() != RUN_REASON_AUTO_RELOAD) {
                skip_repl = true;
                skip_wait = true;
            }
        } else if (_exec_result.return_code == 0) {
            next_code_stickiness_situation |= SUPERVISOR_NEXT_CODE_OPT_STICKY_ON_SUCCESS;
            if (next_code_options & SUPERVISOR_NEXT_CODE_OPT_RELOAD_ON_SUCCESS) {
                skip_repl = true;
                skip_wait = true;
            }
        } else {
            next_code_stickiness_situation |= SUPERVISOR_NEXT_CODE_OPT_STICKY_ON_ERROR;
            // Deep sleep cannot be skipped
            // TODO: settings in deep sleep should persist, using a new sleep memory API
            if (next_code_options & SUPERVISOR_NEXT_CODE_OPT_RELOAD_ON_ERROR
                && !(_exec_result.return_code & PYEXEC_DEEP_SLEEP)) {
                skip_repl = true;
                skip_wait = true;
            }
        }
        if (_exec_result.return_code & PYEXEC_FORCED_EXIT) {
            skip_repl = false;
            skip_wait = true;
        }
    }

    // Program has finished running.
    bool printed_press_any_key = false;
    #if CIRCUITPY_EPAPERDISPLAY
    size_t time_to_epaper_refresh = 1;
    #endif

    // Setup LED blinks.
    #if CIRCUITPY_STATUS_LED
    uint32_t color;
    uint8_t blink_count;
    bool led_active = false;
    #if CIRCUITPY_ALARM
    if (_exec_result.return_code & PYEXEC_DEEP_SLEEP) {
        color = BLACK;
        blink_count = 0;
    } else
    #endif
    if (_exec_result.return_code != PYEXEC_EXCEPTION) {
        if (safe_mode == SAFE_MODE_NONE) {
            color = ALL_DONE;
            blink_count = ALL_DONE_BLINKS;
        } else {
            color = SAFE_MODE;
            blink_count = SAFE_MODE_BLINKS;
        }
    } else {
        color = EXCEPTION;
        blink_count = EXCEPTION_BLINKS;
    }
    size_t pattern_start = supervisor_ticks_ms32();
    size_t single_blink_time = (OFF_ON_RATIO + 1) * BLINK_TIME_MS;
    size_t blink_time = single_blink_time * blink_count;
    size_t total_time = blink_time + LED_SLEEP_TIME_MS;
    #endif

    // This loop is waits after code completes. It waits for fake sleeps to
    // finish, user input or autoreloads.
    #if CIRCUITPY_ALARM
    bool fake_sleeping = false;
    #endif
    while (!skip_wait) {
        RUN_BACKGROUND_TASKS;

        // If a reload was requested by the supervisor or autoreload, return.
        if (autoreload_ready()) {
            next_code_stickiness_situation |= SUPERVISOR_NEXT_CODE_OPT_STICKY_ON_RELOAD;
            // Should the STICKY_ON_SUCCESS and STICKY_ON_ERROR bits be cleared in
            // next_code_stickiness_situation? I can see arguments either way, but I'm deciding
            // "no" for now, mainly because it's a bit less code. At this point, we have both a
            // success or error and a reload, so let's have both of the respective options take
            // effect (in OR combination).
            skip_repl = true;
            // We're kicking off the autoreload process so reset now. If any
            // other reloads trigger after this, then we'll want another wait
            // period.
            autoreload_reset();
            break;
        }

        // If interrupted by keyboard, return
        if (serial_connected() && serial_bytes_available() && !autoreload_pending()) {
            // Skip REPL if reload was requested.
            skip_repl = serial_read() == CHAR_CTRL_D;
            if (skip_repl) {
                supervisor_set_run_reason(RUN_REASON_REPL_RELOAD);
            }
            break;
        }

        // Check for a deep sleep alarm and restart the VM. This can happen if
        // an alarm alerts faster than our USB delay or if we pretended to deep
        // sleep.
        #if CIRCUITPY_ALARM
        if (fake_sleeping && common_hal_alarm_woken_from_sleep()) {
            serial_write_compressed(MP_ERROR_TEXT("Woken up by alarm.\n"));
            supervisor_set_run_reason(RUN_REASON_STARTUP);
            skip_repl = true;
            break;
        }
        #endif

        // If messages haven't been printed yet, print them
        if (!printed_press_any_key && serial_connected() && !autoreload_pending()) {
            if (!serial_connected_at_start) {
                print_code_py_status_message(safe_mode);
            }

            if (!printed_safe_mode_message) {
                print_safe_mode_message(safe_mode);
                printed_safe_mode_message = true;
            }
            serial_write("\r\n");
            serial_write_compressed(MP_ERROR_TEXT("Press any key to enter the REPL. Use CTRL-D to reload.\n"));
            printed_press_any_key = true;
        }
        if (!serial_connected()) {
            serial_connected_at_start = false;
            printed_press_any_key = false;
        }

        // Sleep until our next interrupt.
        #if CIRCUITPY_ALARM
        if (_exec_result.return_code & PYEXEC_DEEP_SLEEP) {
            const bool awoke_from_true_deep_sleep =
                common_hal_mcu_processor_get_reset_reason() == RESET_REASON_DEEP_SLEEP_ALARM;

            if (fake_sleeping) {
                // This waits until a pretend deep sleep alarm occurs. They are set
                // during common_hal_alarm_set_deep_sleep_alarms. On some platforms
                // it may also return due to another interrupt, that's why we check
                // for deep sleep alarms above. If it wasn't a deep sleep alarm,
                // then we'll idle here again.
                common_hal_alarm_pretending_deep_sleep();
            }
            // The first time we go into a deep sleep, make sure we have been awake long enough
            // for USB to connect (enumeration delay), or for the BLE workflow to start.
            // We wait CIRCUITPY_WORKFLOW_CONNECTION_SLEEP_DELAY seconds after a restart.
            // But if we woke up from a real deep sleep, don't wait for connection. The user will need to
            // do a hard reset to get out of the real deep sleep.
            else if (awoke_from_true_deep_sleep ||
                     port_get_raw_ticks(NULL) > CIRCUITPY_WORKFLOW_CONNECTION_SLEEP_DELAY * 1024) {
                // OK to start sleeping, real or fake.
                #if CIRCUITPY_DISPLAYIO
                common_hal_displayio_release_displays();
                #endif
                status_led_deinit();
                deinit_rxtx_leds();
                board_deinit();

                // Continue with true deep sleep even if workflow is available.
                if (awoke_from_true_deep_sleep || !supervisor_workflow_active()) {
                    // Enter true deep sleep. When we wake up we'll be back at the
                    // top of main(), not in this loop.
                    common_hal_alarm_enter_deep_sleep();
                    // Does not return.
                } else {
                    serial_write_compressed(
                        MP_ERROR_TEXT("Pretending to deep sleep until alarm, CTRL-C or file write.\n"));
                    fake_sleeping = true;
                }
            } else {
                // Loop while checking the time. We can't idle because we don't want to override a
                // time alarm set for the deep sleep.
            }
        } else
        #endif
        {
            // Refresh the ePaper display if we have one. That way it'll show an error message.
            // Skip if we're about to autoreload. Otherwise we may delay when user code can update
            // the display.
            #if CIRCUITPY_EPAPERDISPLAY
            if (time_to_epaper_refresh > 0 && !autoreload_pending()) {
                time_to_epaper_refresh = maybe_refresh_epaperdisplay();
            }

            #if !CIRCUITPY_STATUS_LED
            port_interrupt_after_ticks(time_to_epaper_refresh);
            #endif
            #endif

            #if CIRCUITPY_STATUS_LED
            uint32_t tick_diff = supervisor_ticks_ms32() - pattern_start;

            // By default, don't sleep.
            size_t time_to_next_change = 0;
            if (tick_diff < blink_time) {
                uint32_t blink_diff = tick_diff % (single_blink_time);
                if (blink_diff >= BLINK_TIME_MS) {
                    if (led_active) {
                        new_status_color(BLACK);
                        status_led_deinit();
                        led_active = false;
                    }
                    time_to_next_change = single_blink_time - blink_diff;
                } else {
                    if (!led_active) {
                        status_led_init();
                        new_status_color(color);
                        led_active = true;
                    }
                    time_to_next_change = BLINK_TIME_MS - blink_diff;
                }
            } else if (tick_diff > total_time) {
                pattern_start = supervisor_ticks_ms32();
            } else {
                if (led_active) {
                    new_status_color(BLACK);
                    status_led_deinit();
                    led_active = false;
                }
                time_to_next_change = total_time - tick_diff;
            }
            #if CIRCUITPY_EPAPERDISPLAY
            if (time_to_epaper_refresh > 0 && time_to_next_change > 0) {
                time_to_next_change = MIN(time_to_next_change, time_to_epaper_refresh);
            }
            #endif

            // time_to_next_change is in ms and ticks are slightly shorter so
            // we'll undersleep just a little. It shouldn't matter.
            if (time_to_next_change > 0) {
                port_interrupt_after_ticks(time_to_next_change);
                port_idle_until_interrupt();
            }
            #else
            // No status LED can we sleep until we are interrupted by some
            // interaction.
            port_idle_until_interrupt();
            #endif
        }
    }

    // Done waiting, start the board back up.

    // We delay resetting BLE until after the wait in case we're transferring
    // more files over.
    #if CIRCUITPY_BLEIO
    bleio_reset();
    #endif

    // free code allocation if unused
    if (next_code_configuration != NULL && (next_code_configuration->options & next_code_stickiness_situation) == 0) {
        port_free(next_code_configuration);
        next_code_configuration = NULL;
    }

    #if CIRCUITPY_STATUS_LED
    if (led_active) {
        new_status_color(BLACK);
        status_led_deinit();
    }
    #endif

    #if CIRCUITPY_ALARM
    if (fake_sleeping) {
        board_init();
        // Pretend that the next run is the first run, as if we were reset.
        *simulate_reset = true;
    }
    #endif

    return skip_repl;
}

vstr_t *boot_output;

#if CIRCUITPY_SAFEMODE_PY
STATIC void __attribute__ ((noinline)) run_safemode_py(safe_mode_t safe_mode) {
    // Don't run if we aren't in safe mode or we won't be able to find safemode.py.
    // Also don't run if it's a user-initiated safemode (pressing button(s) during boot),
    // since that's deliberate.
    if (safe_mode == SAFE_MODE_NONE || safe_mode == SAFE_MODE_USER || !filesystem_present()) {
        return;
    }

    start_mp(safe_mode);

    static const char *const safemode_py_filenames[] = {"safemode.py", "safemode.txt"};
    maybe_run_list(safemode_py_filenames, MP_ARRAY_SIZE(safemode_py_filenames));

    // If safemode.py itself caused an error, change the safe_mode state to indicate that.
    if (_exec_result.exception != MP_OBJ_NULL &&
        _exec_result.exception != MP_OBJ_SENTINEL) {
        set_safe_mode(SAFE_MODE_SAFEMODE_PY_ERROR);
    }

    cleanup_after_vm(_exec_result.exception);
    _exec_result.exception = NULL;
}
#endif

STATIC void __attribute__ ((noinline)) run_boot_py(safe_mode_t safe_mode) {
    if (safe_mode == SAFE_MODE_NO_HEAP) {
        return;
    }

    // If not in safe mode, run boot before initing USB and capture output in a file.

    // There is USB setup to do even if boot.py is not actually run.
    const bool ok_to_run = filesystem_present()
        && safe_mode == SAFE_MODE_NONE
        && MP_STATE_VM(vfs_mount_table) != NULL;

    static const char *const boot_py_filenames[] = {"boot.py", "boot.txt"};

    // Do USB setup even if boot.py is not run.

    start_mp(safe_mode);

    #if CIRCUITPY_USB
    // Set up default USB values after boot.py VM starts but before running boot.py.
    usb_set_defaults();
    #endif

    if (ok_to_run) {
        #ifdef CIRCUITPY_BOOT_OUTPUT_FILE
        #if CIRCUITPY_STATUS_BAR
        // Turn off status bar updates when writing out to boot_out.txt.
        supervisor_status_bar_suspend();
        #endif
        vstr_t boot_text;
        vstr_init(&boot_text, 512);
        boot_output = &boot_text;
        #endif

        // Write version info
        mp_printf(&mp_plat_print, "%s\nBoard ID:%s\n", MICROPY_FULL_VERSION_INFO, CIRCUITPY_BOARD_ID);
        #if CIRCUITPY_MICROCONTROLLER && COMMON_HAL_MCU_PROCESSOR_UID_LENGTH > 0
        uint8_t raw_id[COMMON_HAL_MCU_PROCESSOR_UID_LENGTH];
        common_hal_mcu_processor_get_uid(raw_id);
        mp_cprintf(&mp_plat_print, MP_ERROR_TEXT("UID:"));
        for (size_t i = 0; i < COMMON_HAL_MCU_PROCESSOR_UID_LENGTH; i++) {
            mp_cprintf(&mp_plat_print, MP_ERROR_TEXT("%02X"), raw_id[i]);
        }
        mp_printf(&mp_plat_print, "\n");
        port_boot_info();
        #endif

        bool found_boot = maybe_run_list(boot_py_filenames, MP_ARRAY_SIZE(boot_py_filenames));
        (void)found_boot;


        #ifdef CIRCUITPY_BOOT_OUTPUT_FILE
        // Get the base filesystem.
        fs_user_mount_t *vfs = (fs_user_mount_t *)MP_STATE_VM(vfs_mount_table)->obj;
        FATFS *fs = &vfs->fatfs;

        boot_output = NULL;
        #if CIRCUITPY_STATUS_BAR
        supervisor_status_bar_resume();
        #endif
        bool write_boot_output = true;
        FIL boot_output_file;
        if (f_open(fs, &boot_output_file, CIRCUITPY_BOOT_OUTPUT_FILE, FA_READ) == FR_OK) {
            char *file_contents = m_new(char, boot_text.alloc);
            UINT chars_read;
            if (f_read(&boot_output_file, file_contents, 1 + boot_text.len, &chars_read) == FR_OK) {
                write_boot_output =
                    (chars_read != boot_text.len) || (memcmp(boot_text.buf, file_contents, chars_read) != 0);
            }
            // no need to f_close the file
        }

        if (write_boot_output) {
            // Wait 1 second before opening CIRCUITPY_BOOT_OUTPUT_FILE for write,
            // in case power is momentary or will fail shortly due to, say a low, battery.
            mp_hal_delay_ms(1000);

            // USB isn't up, so we can write the file.
            // operating at the oofatfs (f_open) layer means the usb concurrent write permission
            // is not even checked!
            f_open(fs, &boot_output_file, CIRCUITPY_BOOT_OUTPUT_FILE, FA_WRITE | FA_CREATE_ALWAYS);
            UINT chars_written;
            f_write(&boot_output_file, boot_text.buf, boot_text.len, &chars_written);
            f_close(&boot_output_file);
            filesystem_flush();
        }
        #endif
    }

    port_post_boot_py(true);

    cleanup_after_vm(_exec_result.exception);
    _exec_result.exception = NULL;

    port_post_boot_py(false);
}

STATIC int run_repl(safe_mode_t safe_mode) {
    int exit_code = PYEXEC_FORCED_EXIT;
    filesystem_flush();

    start_mp(safe_mode);

    #if CIRCUITPY_USB
    usb_setup_with_vm();
    #endif

    autoreload_suspend(AUTORELOAD_SUSPEND_REPL);

    if (get_safe_mode() == SAFE_MODE_NONE) {
        const char *const filenames[] = { "repl.py" };
        (void)maybe_run_list(filenames, MP_ARRAY_SIZE(filenames));
    }

    // Set the status LED to the REPL color before running the REPL. For
    // NeoPixels and DotStars this will be sticky but for PWM or single LED it
    // won't. This simplifies pin sharing because they won't be in use when
    // actually in the REPL.
    #if CIRCUITPY_STATUS_LED
    status_led_init();
    new_status_color(REPL_RUNNING);
    status_led_deinit();
    #endif
    if (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL) {
        #if CIRCUITPY_STATUS_BAR
        supervisor_status_bar_suspend();
        #endif
        exit_code = pyexec_raw_repl();
        #if CIRCUITPY_STATUS_BAR
        supervisor_status_bar_resume();
        #endif
    } else {
        _current_executing_filename = "REPL";
        #if CIRCUITPY_STATUS_BAR
        supervisor_status_bar_update();
        #endif
        exit_code = pyexec_friendly_repl();
        _current_executing_filename = NULL;
        #if CIRCUITPY_STATUS_BAR
        supervisor_status_bar_update();
        #endif
    }
    #if CIRCUITPY_ATEXIT
    pyexec_result_t result;
    shared_module_atexit_execute(&result);
    if (result.return_code == PYEXEC_DEEP_SLEEP) {
        exit_code = PYEXEC_DEEP_SLEEP;
    }
    #endif
    cleanup_after_vm(MP_OBJ_SENTINEL);

    // Also reset bleio. The above call omits it in case workflows should continue. In this case,
    // we're switching straight to another VM so we want to reset.
    #if CIRCUITPY_BLEIO
    bleio_reset();
    #endif

    #if CIRCUITPY_STATUS_LED
    status_led_init();
    new_status_color(BLACK);
    status_led_deinit();
    #endif

    autoreload_resume(AUTORELOAD_SUSPEND_REPL);
    return exit_code;
}

int __attribute__((used)) main(void) {

    // initialise the cpu and peripherals
    set_safe_mode(port_init());

    port_heap_init();

    // Turn on RX and TX LEDs if we have them.
    init_rxtx_leds();

    #if CIRCUITPY_BOOT_COUNTER
    // Increment counter before possibly entering safe mode
    common_hal_nvm_bytearray_get_bytes(&common_hal_mcu_nvm_obj, 0, 1, &value_out);
    ++value_out;
    common_hal_nvm_bytearray_set_bytes(&common_hal_mcu_nvm_obj, 0, &value_out, 1);
    #endif

    // Start the debug serial
    serial_early_init();
    mp_hal_stdout_tx_str(line_clear);

    // Wait briefly to give a reset window where we'll enter safe mode after the reset.
    if (get_safe_mode() == SAFE_MODE_NONE) {
        set_safe_mode(wait_for_safe_mode_reset());
    }

    stack_init();

    #if CIRCUITPY_STATUS_BAR
    supervisor_status_bar_init();
    #endif

    #if CIRCUITPY_BLEIO
    // Early init so that a reset press can cause BLE public advertising.
    supervisor_bluetooth_init();
    #endif

    #if !INTERNAL_FLASH_FILESYSTEM
    // Set up anything that might need to get done before we try to use SPI flash
    // This is needed for some boards where flash relies on GPIO setup to work
    external_flash_setup();
    #endif

    // Create a new filesystem only if we're not in a safe mode.
    // A power brownout here could make it appear as if there's
    // no SPI flash filesystem, and we might erase the existing one.

    // Check whether CIRCUITPY is available. No need to reset to get safe mode
    // since we haven't run user code yet.
    if (!filesystem_init(get_safe_mode() == SAFE_MODE_NONE, false)) {
        set_safe_mode(SAFE_MODE_NO_CIRCUITPY);
    }

    #if CIRCUITPY_ALARM
    // Record which alarm woke us up, if any.
    // common_hal_alarm_record_wake_alarm() should return a static, non-heap object
    shared_alarm_save_wake_alarm(common_hal_alarm_record_wake_alarm());
    // Then reset the alarm system. It's not reset in reset_port(), because that's also called
    // on VM teardown, which would clear any alarm setup.
    alarm_reset();
    #endif

    // Reset everything and prep MicroPython to run boot.py.
    reset_port();
    // Port-independent devices, like CIRCUITPY_BLEIO_HCI.
    reset_devices();
    reset_board();

    // displays init after filesystem, since they could share the flash SPI
    board_init();

    mp_hal_stdout_tx_str(line_clear);

    // This is first time we are running CircuitPython after a reset or power-up.
    supervisor_set_run_reason(RUN_REASON_STARTUP);

    // If not in safe mode turn on autoreload by default but before boot.py in case it wants to change it.
    if (get_safe_mode() == SAFE_MODE_NONE) {
        autoreload_enable();
    }

    // By default our internal flash is readonly to local python code and
    // writable over USB. Set it here so that safemode.py or boot.py can change it.
    filesystem_set_internal_concurrent_write_protection(true);
    filesystem_set_internal_writable_by_usb(CIRCUITPY_USB == 1);

    #if CIRCUITPY_SAFEMODE_PY
    // Run safemode.py if we ARE in safe mode.
    // If safemode.py does not do a hard reset, and exits normally, we will continue on
    // and report the safe mode as usual.
    run_safemode_py(get_safe_mode());
    #endif

    run_boot_py(get_safe_mode());

    supervisor_workflow_start();

    #if CIRCUITPY_STATUS_BAR
    supervisor_status_bar_request_update(true);
    #endif

    // Boot script is finished, so now go into REPL or run code.py.
    int exit_code = PYEXEC_FORCED_EXIT;
    bool skip_repl = true;
    bool simulate_reset = true;
    for (;;) {
        if (!skip_repl) {
            exit_code = run_repl(get_safe_mode());
            supervisor_set_run_reason(RUN_REASON_REPL_RELOAD);
        }
        if (exit_code == PYEXEC_FORCED_EXIT) {
            if (!simulate_reset) {
                serial_write_compressed(MP_ERROR_TEXT("soft reboot\n"));
            }
            simulate_reset = false;
            if (pyexec_mode_kind == PYEXEC_MODE_FRIENDLY_REPL) {
                // If code.py did a fake deep sleep, pretend that we
                // are running code.py for the first time after a hard
                // reset. This will preserve any alarm information.
                skip_repl = run_code_py(get_safe_mode(), &simulate_reset);
            } else {
                skip_repl = false;
            }
        } else if (exit_code != 0) {
            break;
        }

        #if CIRCUITPY_ALARM
        shared_alarm_save_wake_alarm(simulate_reset ? common_hal_alarm_record_wake_alarm() : mp_const_none);
        alarm_reset();
        #endif
    }
    mp_deinit();
    return 0;
}

void gc_collect(void) {
    gc_collect_start();

    mp_uint_t regs[10];
    mp_uint_t sp = cpu_get_regs_and_sp(regs);

    // This collects root pointers from the VFS mount table. Some of them may
    // have lost their references in the VM even though they are mounted.
    gc_collect_root((void **)&MP_STATE_VM(vfs_mount_table), sizeof(mp_vfs_mount_t) / sizeof(mp_uint_t));

    port_gc_collect();

    background_callback_gc_collect();

    #if CIRCUITPY_ALARM
    common_hal_alarm_gc_collect();
    #endif

    #if CIRCUITPY_ATEXIT
    atexit_gc_collect();
    #endif

    #if CIRCUITPY_DISPLAYIO
    displayio_gc_collect();
    #endif

    #if CIRCUITPY_BLEIO
    common_hal_bleio_gc_collect();
    #endif

    #if CIRCUITPY_USB_HID
    usb_hid_gc_collect();
    #endif

    #if CIRCUITPY_WIFI
    common_hal_wifi_gc_collect();
    #endif

    // This naively collects all object references from an approximate stack
    // range.
    gc_collect_root((void **)sp, ((mp_uint_t)port_stack_get_top() - sp) / sizeof(mp_uint_t));
    gc_collect_end();
}

// Ports may provide an implementation of this function if it is needed
MP_WEAK void port_gc_collect() {
}

size_t gc_get_max_new_split(void) {
    return port_heap_get_largest_free_size();
}

void NORETURN nlr_jump_fail(void *val) {
    reset_into_safe_mode(SAFE_MODE_NLR_JUMP_FAIL);
    while (true) {
    }
}

#ifndef NDEBUG
static void NORETURN __fatal_error(const char *msg) {
    #if CIRCUITPY_DEBUG == 0
    reset_into_safe_mode(SAFE_MODE_HARD_FAULT);
    #endif
    while (true) {
    }
}

void MP_WEAK __assert_func(const char *file, int line, const char *func, const char *expr) {
    mp_printf(&mp_plat_print, "Assertion '%s' failed, at file %s:%d\n", expr, file, line);
    __fatal_error("Assertion failed");
}
#endif
