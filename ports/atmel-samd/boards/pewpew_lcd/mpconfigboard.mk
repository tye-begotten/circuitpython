USB_VID = 0x1209
USB_PID = 0xD1B5
USB_PRODUCT = "PewPew LCD"
USB_MANUFACTURER = "Radomir Dopieralski"

CHIP_VARIANT = SAMD21E18A
CHIP_FAMILY = samd21

INTERNAL_FLASH_FILESYSTEM = 1
LONGINT_IMPL = NONE

CIRCUITPY_FULL_BUILD = 0

# required
CIRCUITPY_DISPLAYIO = 1
CIRCUITPY_BUSDISPLAY = 1
CIRCUITPY_KEYPAD = 1
CIRCUITPY_KEYPAD_KEYS = 1
CIRCUITPY_KEYPAD_SHIFTREGISTERKEYS = 0
CIRCUITPY_KEYPAD_KEYMATRIX = 0

# bonus, can be disabled if needed
CIRCUITPY_MATH = 1
CIRCUITPY_ANALOGIO = 1
CIRCUITPY_NEOPIXEL_WRITE = 1
CIRCUITPY_SAMD = 1

CIRCUITPY_EPAPERDISPLAY = 0
CIRCUITPY_I2CDISPLAYBUS = 0
CIRCUITPY_STAGE = 0
CIRCUITPY_PWMIO = 0
CIRCUITPY_TOUCHIO = 0
CIRCUITPY_AUDIOBUSIO = 0
CIRCUITPY_AUDIOBUSIO_I2SOUT = 0
CIRCUITPY_AUDIOCORE = 0
CIRCUITPY_AUDIOIO = 0
CIRCUITPY_AUDIOMIXER = 0
CIRCUITPY_AUDIOMP3 = 0
CIRCUITPY_AUDIOPWMIO = 0
CIRCUITPY_BITBANG_APA102 = 0
CIRCUITPY_BITBANGIO = 0
CIRCUITPY_BITMAPFILTER = 0
CIRCUITPY_BITMAPTOOLS = 0
CIRCUITPY_BLEIO = 0
CIRCUITPY_BUSDEVICE = 0
CIRCUITPY_FRAMEBUFFERIO = 0
CIRCUITPY_FREQUENCYIO = 0
CIRCUITPY_I2CTARGET = 0
CIRCUITPY_MSGPACK = 0
CIRCUITPY_NVM = 0
CIRCUITPY_PIXELBUF = 0
CIRCUITPY_PS2IO = 0
CIRCUITPY_PULSEIO = 0
CIRCUITPY_RGBMATRIX = 0
CIRCUITPY_ROTARYIO = 0
CIRCUITPY_RTC = 0
CIRCUITPY_ULAB = 0
CIRCUITPY_USB_HID = 0
CIRCUITPY_USB_MIDI = 0
CIRCUITPY_USB_VENDOR = 0
CIRCUITPY_VECTORIO = 0
CIRCUITPY_RAINBOWIO = 0

CIRCUITPY_DISPLAY_FONT = $(TOP)/ports/atmel-samd/boards/ugame10/brutalist-6.bdf
OPTIMIZATION_FLAGS = -Os
FROZEN_MPY_DIRS += $(TOP)/frozen/pew-pewpew-lcd
