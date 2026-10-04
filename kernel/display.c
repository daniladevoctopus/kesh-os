#include "display.h"

static display_info_t g_displays[DISPLAY_MAX_OUTPUTS];
static uint32_t g_display_count;

int display_init(struct limine_framebuffer **framebuffers, uint64_t count) {
    g_display_count = 0;
    if (!framebuffers) return -1;
    for (uint64_t i = 0; i < count && g_display_count < DISPLAY_MAX_OUTPUTS; ++i) {
        struct limine_framebuffer *fb = framebuffers[i];
        if (!fb || !fb->address || !fb->width || !fb->height || !fb->pitch || !fb->bpp) continue;
        display_info_t *output = &g_displays[g_display_count++];
        output->address = (uint64_t)fb->address;
        output->width = (uint32_t)fb->width;
        output->height = (uint32_t)fb->height;
        output->pitch = (uint32_t)fb->pitch;
        output->bpp = (uint16_t)fb->bpp;
        output->scale_milli = 1000;
    }
    return g_display_count ? 0 : -1;
}

uint32_t display_count(void) { return g_display_count; }
const display_info_t *display_get(uint32_t index) { return index < g_display_count ? &g_displays[index] : 0; }

void display_clear(uint32_t color) {
    for (uint32_t output_index = 0; output_index < g_display_count; ++output_index) {
        const display_info_t *output = &g_displays[output_index];
        uint32_t bytes_per_pixel = output->bpp / 8U;
        if (bytes_per_pixel != 3U && bytes_per_pixel != 4U) continue;
        uint8_t *base = (uint8_t *)(uint64_t)output->address;
        for (uint32_t y = 0; y < output->height; ++y) {
            uint8_t *row = base + (uint64_t)y * output->pitch;
            if (bytes_per_pixel == 4U) {
                uint32_t *pixel = (uint32_t *)row;
                for (uint32_t x = 0; x < output->width; ++x) pixel[x] = color;
            } else {
                for (uint32_t x = 0; x < output->width; ++x) {
                    row[x * 3U + 0] = (uint8_t)color;
                    row[x * 3U + 1] = (uint8_t)(color >> 8);
                    row[x * 3U + 2] = (uint8_t)(color >> 16);
                }
            }
        }
    }
}
