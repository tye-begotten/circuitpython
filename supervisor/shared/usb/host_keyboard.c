/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Scott Shawcroft for Adafruit Industries
 * Copyright (c) 2023 Jeff Epler for Adafruit Industries
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

#include "tusb.h"

#include "py/ringbuf.h"
#include "py/runtime.h"
#include "shared/runtime/interrupt_char.h"
#include "supervisor/usb.h"
#include "supervisor/background_callback.h"
#include "supervisor/shared/tick.h"

#ifndef DEBUG
#define DEBUG (0)
#endif

// Buffer the incoming serial data in the background so that we can look for the
// interrupt character.
STATIC ringbuf_t _incoming_ringbuf;
STATIC uint8_t _buf[16];

STATIC uint8_t _dev_addr;
STATIC uint8_t _interface;

#define FLAG_SHIFT (1)
#define FLAG_NUMLOCK (2)
#define FLAG_CTRL (4)
#define FLAG_STRING (8)

STATIC uint8_t user_keymap[384];
STATIC size_t user_keymap_len = 0;

void usb_keymap_set(const uint8_t *buf, size_t len) {
    user_keymap_len = len = MIN(len, sizeof(user_keymap));
    memcpy(user_keymap, buf, len);
    memset(user_keymap + len, 0, sizeof(user_keymap) - len);
}

struct keycode_mapper {
    uint8_t first, last, code, flags;
    const char *data;
};

#define SEP "\0" // separator in FLAG_STRING sequences
#define NOTHING "" // in FLAG_STRING sequences
#define CURSOR_UP "\e[A"
#define CURSOR_DOWN "\e[B"
#define CURSOR_LEFT "\e[D"
#define CURSOR_RIGHT "\e[C"
#define CURSOR_PGUP "\e[5~"
#define CURSOR_PGDN "\e[6~"
#define CURSOR_HOME "\e[H"
#define CURSOR_END "\e[F"
#define CURSOR_INS "\e[2~"
#define CURSOR_DEL "\e[3~"

// https://aperiodic.net/phil/archives/Geekery/term-function-keys/
#define F1      "\eOP"
#define F2      "\eOQ"
#define F3      "\eOR"
#define F4      "\eOS"
#define F5      "\e[15~"
#define F6      "\e[17~"
#define F7      "\e[18~"
#define F8      "\e[19~"
#define F9      "\e[20~"
#define F10     "\e[21~"
#define F11     "\e[23~"
#define F12     "\e[24~"
#define PRINT_SCREEN     "\e[i"
#define CTRL_UP "\e[1;5A"
#define CTRL_DOWN "\e[1;5B"
#define CTRL_RIGHT "\e[1;5C"
#define CTRL_LEFT "\e[1;5D"



STATIC struct keycode_mapper keycode_to_ascii[] = {
    { HID_KEY_A, HID_KEY_Z, 'a', 0, NULL},

    { HID_KEY_1, HID_KEY_9, 0, FLAG_SHIFT, "!@#$%^&*()" },
    { HID_KEY_1, HID_KEY_9, '1', 0, },
    { HID_KEY_0, HID_KEY_0, ')', FLAG_SHIFT, },
    { HID_KEY_0, HID_KEY_0, '0', 0, },

    { HID_KEY_ENTER, HID_KEY_ENTER, '\n', FLAG_CTRL },
    { HID_KEY_ENTER, HID_KEY_SLASH, 0, FLAG_SHIFT, "\n\x1b\177\t _+{}|~:\"~<>?" },
    { HID_KEY_ENTER, HID_KEY_SLASH, 0, 0, "\r\x1b\10\t -=[]\\#;'`,./" },

    // { HID_KEY_F1, HID_KEY_F1, 0x1e, 0, }, // help key on xerox 820 kbd 

    { HID_KEY_KEYPAD_DIVIDE, HID_KEY_KEYPAD_DECIMAL, 0, FLAG_NUMLOCK | FLAG_STRING,
      "/\0" "*\0" "-\0" "+\0" "\n\0" CURSOR_END SEP CURSOR_DOWN SEP CURSOR_PGDN SEP CURSOR_LEFT SEP NOTHING SEP CURSOR_RIGHT SEP CURSOR_HOME SEP CURSOR_UP SEP CURSOR_PGDN SEP CURSOR_INS SEP CURSOR_DEL},
    { HID_KEY_KEYPAD_DIVIDE, HID_KEY_KEYPAD_DECIMAL, 0, 0, "/*-+\n1234567890." },

    // { HID_KEY_ARROW_RIGHT, HID_KEY_ARROW_UP, 0, FLAG_STRING, CURSOR_RIGHT SEP CURSOR_LEFT SEP CURSOR_DOWN SEP CURSOR_UP },
    { HID_KEY_PAUSE, HID_KEY_PAUSE, 0x1a, 0, },
    { HID_KEY_PAGE_DOWN, HID_KEY_PAGE_DOWN, 0, FLAG_STRING, CURSOR_PGDN },
    { HID_KEY_PAGE_UP, HID_KEY_PAGE_UP, 0, FLAG_STRING, CURSOR_PGUP },
    { HID_KEY_HOME, HID_KEY_HOME, 0, FLAG_STRING, CURSOR_HOME },
    { HID_KEY_END, HID_KEY_END, 0, FLAG_STRING, CURSOR_END },
    { HID_KEY_INSERT, HID_KEY_INSERT, 0, FLAG_STRING, CURSOR_INS },
    { HID_KEY_DELETE, HID_KEY_DELETE, 0, FLAG_STRING, CURSOR_DEL },

    { HID_KEY_F1, HID_KEY_F1, 0, FLAG_STRING, F1 },
    { HID_KEY_F2, HID_KEY_F2, 0, FLAG_STRING, F2 },
    { HID_KEY_F3, HID_KEY_F3, 0, FLAG_STRING, F3 },
    { HID_KEY_F4, HID_KEY_F4, 0, FLAG_STRING, F4 },
    { HID_KEY_F5, HID_KEY_F5, 0, FLAG_STRING, F5 },
    { HID_KEY_F6, HID_KEY_F6, 0, FLAG_STRING, F6 },
    { HID_KEY_F7, HID_KEY_F7, 0, FLAG_STRING, F7 },
    { HID_KEY_F8, HID_KEY_F8, 0, FLAG_STRING, F8 },
    { HID_KEY_F9, HID_KEY_F9, 0, FLAG_STRING, F9 },
    { HID_KEY_F10, HID_KEY_F10, 0, FLAG_STRING, F10 },
    { HID_KEY_F11, HID_KEY_F11, 0, FLAG_STRING, F11 },
    { HID_KEY_F12, HID_KEY_F12, 0, FLAG_STRING, F12 },
    { HID_KEY_PRINT_SCREEN, HID_KEY_PRINT_SCREEN, 0, FLAG_STRING, PRINT_SCREEN },
    

    { HID_KEY_ARROW_UP, HID_KEY_ARROW_UP, 0 , FLAG_STRING+FLAG_CTRL,CTRL_UP  },
    { HID_KEY_ARROW_DOWN, HID_KEY_ARROW_DOWN, 0 , FLAG_STRING+FLAG_CTRL, CTRL_DOWN },
    { HID_KEY_ARROW_LEFT, HID_KEY_ARROW_LEFT, 0 , FLAG_STRING+FLAG_CTRL, CTRL_LEFT },
    { HID_KEY_ARROW_RIGHT, HID_KEY_ARROW_RIGHT, 0 , FLAG_STRING+FLAG_CTRL, CTRL_RIGHT},
    { HID_KEY_ARROW_UP, HID_KEY_ARROW_UP, 0 , FLAG_STRING,CURSOR_UP  },
    { HID_KEY_ARROW_DOWN, HID_KEY_ARROW_DOWN, 0 , FLAG_STRING, CURSOR_DOWN },
    { HID_KEY_ARROW_LEFT, HID_KEY_ARROW_LEFT, 0 , FLAG_STRING, CURSOR_LEFT },
    { HID_KEY_ARROW_RIGHT, HID_KEY_ARROW_RIGHT, 0 , FLAG_STRING, CURSOR_RIGHT},



};

STATIC bool report_contains(const hid_keyboard_report_t *report, uint8_t key) {
    for (int i = 0; i < 6; i++) {
        if (report->keycode[i] == key) {
            return true;
        }
    }
    return false;
}

STATIC const char *old_buf = NULL;
STATIC size_t buf_size = 0;
// this matches Linux default of 500ms to first repeat, 1/20s thereafter
enum { initial_repeat_time = 500, default_repeat_time = 50 };
STATIC uint64_t repeat_deadline;
STATIC void repeat_f(void *unused);
background_callback_t repeat_cb = {repeat_f, NULL, NULL, NULL};

STATIC void set_repeat_deadline(uint64_t new_deadline) {
    repeat_deadline = new_deadline;
    background_callback_add_core(&repeat_cb);
}

STATIC void send_bufn_core(const char *buf, size_t n) {
    old_buf = buf;
    buf_size = n;
    // repeat_timeout = millis() + repeat_time;
    for (; n--; buf++) {
        int code = *buf;
        if (code == mp_interrupt_char) {
            mp_sched_keyboard_interrupt();
            return;
        }
        if (ringbuf_num_empty(&_incoming_ringbuf) == 0) {
            // Drop on the floor
            return;
        }
        ringbuf_put(&_incoming_ringbuf, code);
    }
}

STATIC void send_bufn(const char *buf, size_t n) {
    send_bufn_core(buf, n);
    set_repeat_deadline(supervisor_ticks_ms64() + initial_repeat_time);
}

STATIC void send_bufz(const char *buf) {
    send_bufn(buf, strlen(buf));
}

STATIC void send_byte(uint8_t code) {
    static char buf[1];
    buf[0] = code;
    send_bufn(buf, 1);
}

STATIC void send_repeat(void) {
    if (old_buf) {
        uint64_t now = supervisor_ticks_ms64();
        if (now >= repeat_deadline) {
            send_bufn_core(old_buf, buf_size);
            set_repeat_deadline(now + default_repeat_time);
        } else {
            background_callback_add_core(&repeat_cb);
        }
    }
}

STATIC void repeat_f(void *unused) {
    send_repeat();
}

hid_keyboard_report_t old_report;

STATIC const char *skip_nuls(const char *buf, size_t n) {
    while (n--) {
        buf += strlen(buf) + 1;
    }
    return buf;
}

STATIC void process_event(uint8_t dev_addr, uint8_t instance, const hid_keyboard_report_t *report) {
    bool has_altgr = (user_keymap_len > 256);
    bool altgr = has_altgr && report->modifier & 0x40;
    bool alt = has_altgr ? report->modifier & 0x4 : report->modifier & 0x44;
    bool shift = report->modifier & 0x22;
    bool ctrl = report->modifier & 0x11;
    bool caps = old_report.reserved & 1;
    bool num = old_report.reserved & 2;
    uint8_t code = 0;

    if (report->keycode[0] == 1 && report->keycode[1] == 1) {
        // keyboard says it has exceeded max kro
        return;
    }

    // something was pressed or released, so cancel any key repeat
    old_buf = NULL;

    for (int i = 0; i < 6; i++) {
        uint8_t keycode = report->keycode[i];
        if (keycode == 0) {
            continue;
        }
        if (report_contains(&old_report, keycode)) {
            continue;
        }

        /* key is newly pressed */
        if (keycode == HID_KEY_NUM_LOCK) {
            num = !num;
        } else if (keycode == HID_KEY_CAPS_LOCK) {
            caps = !caps;
        } else {
            size_t idx = keycode + (altgr ? 256 : shift ? 128 : 0);
            uint8_t ascii = user_keymap[idx];
            #if DEBUG
            mp_printf(&mp_plat_print, "lookup HID keycode %d mod %x at idx %d -> ascii %d (%c)\n",
                keycode, report->modifier, idx, ascii, ascii >= 32 && ascii <= 126 ? ascii : '.');
            #endif
            if (ascii != 0) {
                if (ctrl) {
                    ascii &= 0x1f;
                } else if (ascii >= 'a' && ascii <= 'z' && caps) {
                    ascii ^= ('a' ^ 'A');
                }
                send_byte(ascii);
                continue;
            }

            for (size_t j = 0; j < MP_ARRAY_SIZE(keycode_to_ascii); j++) {
                struct keycode_mapper *mapper = &keycode_to_ascii[j];
                if (!(keycode >= mapper->first && keycode <= mapper->last)) {
                    continue;
                }
                if (mapper->flags & FLAG_SHIFT && !shift) {
                    continue;
                }
                if (mapper->flags & FLAG_NUMLOCK && !num) {
                    continue;
                }
                if (mapper->flags & FLAG_CTRL && !ctrl) {
                    continue;
                }
                if (!(mapper->flags & FLAG_CTRL) && ctrl) {
                    continue;
                }                
                if (mapper->flags & FLAG_STRING) {
                    const char *msg = skip_nuls(mapper->data, keycode - mapper->first);
                    send_bufz(msg);
                    break;
                } else if (mapper->data) {
                    code = mapper->data[keycode - mapper->first];
                } else {
                    code = keycode - mapper->first + mapper->code;
                }
                if (code >= 'a' && code <= 'z' && (shift ^ caps)) {
                    code ^= ('a' ^ 'A');
                }
                if (ctrl) {
                    code &= 0x1f;
                }
                if (alt) {
                    code ^= 0x80;
                }
                send_byte(code);
                break;
            }
        }
    }

    uint8_t leds = (caps | (num << 1));
    if (leds != old_report.reserved) {
        tuh_hid_set_report(dev_addr, instance /*idx*/, 0 /*report_id*/, HID_REPORT_TYPE_OUTPUT /*report_type*/, &leds, sizeof(leds));
    }
    old_report = *report;
    old_report.reserved = leds;
}

bool usb_keyboard_in_use(uint8_t dev_addr, uint8_t interface) {
    return _dev_addr == dev_addr && _interface == interface;
}

void usb_keyboard_detach(uint8_t dev_addr, uint8_t interface) {
    if (!usb_keyboard_in_use(dev_addr, interface)) {
        return;
    }
    _dev_addr = 0;
    _interface = 0;
}

void usb_keyboard_attach(uint8_t dev_addr, uint8_t interface) {
    if (usb_keyboard_in_use(dev_addr, interface) || _dev_addr != 0) {
        return;
    }
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, interface);
    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        _dev_addr = dev_addr;
        _interface = interface;
        tuh_hid_receive_report(dev_addr, interface);
    }
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t interface, uint8_t const *desc_report, uint16_t desc_len) {
    usb_keyboard_attach(dev_addr, interface);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t interface) {
    usb_keyboard_detach(dev_addr, interface);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len) {
    if (len != sizeof(hid_keyboard_report_t)) {
        return;
    } else {
        process_event(dev_addr, instance, (hid_keyboard_report_t *)report);
    }
    // continue to request to receive report
    tuh_hid_receive_report(dev_addr, instance);
}

void usb_keyboard_init(void) {
    ringbuf_init(&_incoming_ringbuf, _buf, sizeof(_buf));
}

bool usb_keyboard_chars_available(void) {
    return ringbuf_num_filled(&_incoming_ringbuf) > 0;
}

char usb_keyboard_read_char(void) {
    if (ringbuf_num_filled(&_incoming_ringbuf) > 0) {
        return ringbuf_get(&_incoming_ringbuf);
    }
    return -1;
}
