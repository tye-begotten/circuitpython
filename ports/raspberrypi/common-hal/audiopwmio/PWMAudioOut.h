/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Jeff Epler for Adafruit Industries
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

#ifndef MICROPY_INCLUDED_RASPBERRYPI_COMMON_HAL_AUDIOPWMIO_PWMAUDIOOUT_H
#define MICROPY_INCLUDED_RASPBERRYPI_COMMON_HAL_AUDIOPWMIO_PWMAUDIOOUT_H

#include "common-hal/pwmio/PWMOut.h"

#include "audio_dma.h"

typedef struct {
    mp_obj_base_t base;
    pwmio_pwmout_obj_t left_pwm;
    pwmio_pwmout_obj_t right_pwm;
    audio_dma_t dma;
    uint16_t quiescent_value;
    uint8_t pacing_timer;
    bool stereo;     // if false, only using left_pwm.
    bool swap_channel;
} audiopwmio_pwmaudioout_obj_t;

void audiopwmout_reset(void);

void audiopwmout_background(void);

#endif  // MICROPY_INCLUDED_RASPBERRYPI_COMMON_HAL_AUDIOPWMIO_PWMAUDIOOUT_H
