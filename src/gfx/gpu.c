#include "gpu.h"
#include <stdint.h>
#include <stddef.h>

static kesh_gpu_caps_t s_caps;
static uint32_t *s_target;
static uint32_t s_width;
static uint32_t s_height;
static uint32_t s_pitch;

static uint64_t cpuid_leaf(uint32_t leaf, uint32_t subleaf, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(leaf), "c"(subleaf));
    if (ebx) *ebx = b;
    if (ecx) *ecx = c;
    if (edx) *edx = d;
    return ((uint64_t)a << 32) | b;
}

void kesh_gpu_init(kesh_gpu_caps_t *out_caps) {
    uint32_t ebx, ecx, edx;
    cpuid_leaf(1, 0, &ebx, &ecx, &edx);
    s_caps.sse2 = (edx & (1u << 26)) != 0;
    s_caps.avx = (ecx & (1u << 28)) != 0;
    s_caps.avx2 = false;
    if (s_caps.avx) {
        cpuid_leaf(7, 0, &ebx, &ecx, &edx);
        s_caps.avx2 = (ebx & (1u << 5)) != 0;
    }
    s_caps.gpu_device = false;
    s_caps.width = 0;
    s_caps.height = 0;
    s_caps.pitch = 0;
    s_caps.bpp = 0;
    if (out_caps) *out_caps = s_caps;
}

const kesh_gpu_caps_t *kesh_gpu_caps(void) { return &s_caps; }

void kesh_gpu_copy(void *dst, const void *src, size_t bytes) {
    if (!dst || !src || !bytes) return;
    uint8_t *d = (uint8_t*)dst;
    const uint8_t *s = (const uint8_t*)src;
    if (s_caps.sse2) {
        while (bytes >= 16) {
            __asm__ volatile("movdqu (%1), %%xmm0\n\t" "movdqu %%xmm0, (%0)\n\t" : : "r"(d), "r"(s) : "xmm0", "memory");
            d += 16;
            s += 16;
            bytes -= 16;
        }
    }
    while (bytes--) *d++ = *s++;
}

void kesh_gpu_fill32(uint32_t *dst, size_t pixels, uint32_t color) {
    if (!dst) return;
    if (s_caps.sse2) {
        uint64_t pair = ((uint64_t)color << 32) | color;
        while (pixels >= 4) {
            __asm__ volatile("movq %1, %%xmm0\n\t" "pshufd $0, %%xmm0, %%xmm0\n\t" "movdqu %%xmm0, (%0)\n\t" : : "r"(dst), "m"(pair) : "xmm0", "memory");
            dst += 4;
            pixels -= 4;
        }
    }
    while (pixels--) *dst++ = color;
}

void kesh_gpu_blend32(uint32_t *dst, const uint32_t *src, size_t pixels, uint8_t alpha) {
    if (!dst || !src) return;
    uint32_t ia = 255u - alpha;
    for (size_t i = 0; i < pixels; i++) {
        uint32_t d = dst[i], s = src[i];
        uint32_t r = (((d >> 16) & 255u) * ia + ((s >> 16) & 255u) * alpha) / 255u;
        uint32_t g = (((d >> 8) & 255u) * ia + ((s >> 8) & 255u) * alpha) / 255u;
        uint32_t b = ((d & 255u) * ia + (s & 255u) * alpha) / 255u;
        dst[i] = (r << 16) | (g << 8) | b;
    }
}

void kesh_gpu_set_target(uint32_t *pixels, uint32_t width, uint32_t height, uint32_t pitch) {
    s_target = pixels;
    s_width = width;
    s_height = height;
    s_pitch = pitch;
    s_caps.width = width;
    s_caps.height = height;
    s_caps.pitch = pitch;
    s_caps.bpp = 32;
}
