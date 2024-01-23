// #include "shared-module/rambus/RAM.h"
// #include "shared-module/rambus/FrameBuffer.h"
// #include "shared-module/rambus/RAMBusDisplay.h"

// enum { ALIGN_BITS = 8 * sizeof(uint32_t) };

// static int stride(uint32_t width, uint32_t bits_per_value) {
//     uint32_t row_width = width * bits_per_value;
//     // align to uint32_t
//     return (row_width + ALIGN_BITS - 1) / ALIGN_BITS;
// }

// void rambusframebuffer_framebuffer_construct(
//     rambus_framebuffer_obj_t *self,
//     rambus_ram_obj_t *ram,
//     rambus_rambusdisplay_obj_t *display,
//     addr_t addr,
//     int frequency, 
//     int width, 
//     int height,
//     uint8_t bits_per_px) {
//         self->ram = ram;
//         self->display = display;
//         self->addr = addr;
//         self->frequency = frequency;
//         self->width = width;
//         self->height = height;
//         self->bits_per_px = bits_per_px;
//     }

// void rambusframebuffer_framebuffer_deinit(rambus_framebuffer_obj_t *self) {
//     shared_module_rambus_ram_release(self->ram);
// }

// bool rambusframebuffer_framebuffer_deinitialized(rambus_framebuffer_obj_t *self) {
//     return shared_module_rambus_ram_deinited(self->ram);
// }

// bool rambusdisplay_framebuffer_deinited(rambus_rambusdisplay_obj_t *display) {
//     rambus_framebuffer_obj_t *self = &display->framebuffer;
//     return rambusframebuffer_framebuffer_deinitialized(self);
// }

// mp_int_t rambusframebuffer_framebuffer_get_width(rambus_framebuffer_obj_t *self) {
//     return self->width;
// }

// mp_int_t rambusframebuffer_framebuffer_get_height(rambus_framebuffer_obj_t *self) {
//     return self->height;
// }

// mp_int_t rambusframebuffer_framebuffer_get_frequency(rambus_framebuffer_obj_t *self) {
//     return self->frequency;
// }

// mp_int_t rambusframebuffer_framebuffer_get_refresh_rate(rambus_framebuffer_obj_t *self) {
//     return 60;
// }

// mp_int_t rambusframebuffer_framebuffer_get_row_stride(rambus_framebuffer_obj_t *self) {
//     return stride(self->width, self->bits_per_px);
// }

// mp_int_t rambusframebuffer_framebuffer_get_first_pixel_offset(rambus_framebuffer_obj_t *self) {
//     return 0;
// }

// void rambusframebuffer_framebuffer_refresh(rambus_framebuffer_obj_t *self) {
//     // shared_module_rambus_ram_read_seq(self->ram, self->addr, )
// }