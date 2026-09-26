#ifndef KESH_GPU_H
#define KESH_GPU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    bool sse2;
    bool avx;
    bool avx2;
    bool gpu_device;
} kesh_gpu_caps_t;

void kesh_gpu_init(kesh_gpu_caps_t *out_caps);
const kesh_gpu_caps_t *kesh_gpu_caps(void);
void kesh_gpu_copy(void *dst, const void *src, size_t bytes);
void kesh_gpu_fill32(uint32_t *dst, size_t pixels, uint32_t color);
void kesh_gpu_blend32(uint32_t *dst, const uint32_t *src, size_t pixels, uint8_t alpha);
void kesh_gpu_set_target(uint32_t *pixels, uint32_t width, uint32_t height, uint32_t pitch);

#endif
