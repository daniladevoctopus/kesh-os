// низкоуровневая графика
#include "graphics.h"
#include "gfx/gpu.h"
#include <stddef.h>
#include <stdint.h>

static size_t buffer_size_bytes;
static uint32_t *frontbuffer;
static uint32_t *backbuffer;
static uint32_t fb_width;
static uint32_t fb_height;
static uint32_t fb_pitch;

void graphics_init(uint32_t *fb_addr, uint32_t width, uint32_t height, uint32_t pitch) {
    frontbuffer = fb_addr;
    fb_width = width;
    fb_height = height;
    fb_pitch = pitch;
    buffer_size_bytes = (size_t)height * pitch;

    backbuffer = (uint32_t *)0x02000000ULL;
}

void put_pixel_backbuffer(uint32_t x, uint32_t y, uint32_t color) {
    if (!backbuffer || x >= fb_width || y >= fb_height) return;
    backbuffer[y * (fb_pitch / 4) + x] = color;
}

void swap_buffers(void) {
    if (!frontbuffer || !backbuffer) return;

    uint32_t stride = fb_pitch / 4;
    if (stride == 0) return;

    for (uint32_t y = 0; y < fb_height; ++y) {
        kesh_gpu_copy(frontbuffer + (size_t)y * stride, backbuffer + (size_t)y * stride, (size_t)fb_width * sizeof(uint32_t));
    }

    (void)buffer_size_bytes;
}
