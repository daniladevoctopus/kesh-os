#ifndef KESHOS_DISPLAY_H
#define KESHOS_DISPLAY_H

#include <stdint.h>
#include "include/limine.h"

#define DISPLAY_MAX_OUTPUTS 8

typedef struct {
    uint64_t address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint16_t bpp;
    uint16_t scale_milli;
} display_info_t;

int display_init(struct limine_framebuffer **framebuffers, uint64_t count);
uint32_t display_count(void);
const display_info_t *display_get(uint32_t index);
void display_clear(uint32_t color);

#endif
