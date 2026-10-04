#ifndef KESH_ICO_H
#define KESH_ICO_H

/* Small ICO engine for native applications.  It accepts the common ICO form
   with a 32-bit uncompressed DIB image and preserves per-pixel alpha. */
#include "kesh.h"

static inline uint16_t kesh_ico_u16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t kesh_ico_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

typedef struct {
    const uint8_t *pixels;
    int width;
    int height;
} kesh_ico_image_t;

static inline int kesh_ico_decode(const uint8_t *data, uint32_t size, kesh_ico_image_t *out) {
    if (!data || !out || size < 22 || kesh_ico_u16(data) != 0 || kesh_ico_u16(data + 2) != 1) return -1;
    uint16_t count = kesh_ico_u16(data + 4);
    uint32_t best_offset = 0, best_size = 0;
    int best_delta = 0x7fffffff;
    for (uint16_t i = 0; i < count && 6U + (uint32_t)(i + 1U) * 16U <= size; ++i) {
        const uint8_t *entry = data + 6U + (uint32_t)i * 16U;
        int side = entry[0] ? entry[0] : 256;
        uint16_t bpp = kesh_ico_u16(entry + 6);
        uint32_t image_size = kesh_ico_u32(entry + 8), offset = kesh_ico_u32(entry + 12);
        int delta = side > 48 ? side - 48 : 48 - side;
        if (bpp == 32 && offset <= size && image_size <= size - offset && delta < best_delta) {
            best_delta = delta; best_offset = offset; best_size = image_size;
        }
    }
    if (!best_offset || best_size < 40 || kesh_ico_u32(data + best_offset) < 40) return -1;
    const uint8_t *dib = data + best_offset;
    int width = (int)kesh_ico_u32(dib + 4);
    int full_height = (int)kesh_ico_u32(dib + 8);
    if (width < 1 || width > 256 || full_height < 2 || kesh_ico_u16(dib + 12) != 1 || kesh_ico_u16(dib + 14) != 32 || kesh_ico_u32(dib + 16) != 0) return -1;
    int height = full_height / 2;
    uint32_t bytes = (uint32_t)width * (uint32_t)height * 4U;
    if (bytes > best_size - 40U) return -1;
    out->pixels = dib + 40;
    out->width = width;
    out->height = height;
    return 0;
}

static inline int kesh_ico_load_vfs(const char *path, uint8_t *storage, uint32_t cap, kesh_ico_image_t *out) {
    int bytes = kesh_vfs_read(path, storage, (int)cap);
    return bytes > 0 ? kesh_ico_decode(storage, (uint32_t)bytes, out) : -1;
}

static inline void kesh_ico_draw(uint32_t *fb, int fb_w, int x, int y, int size, const kesh_ico_image_t *image) {
    if (!fb || !image || !image->pixels || size < 1) return;
    for (int dy = 0; dy < size; ++dy) {
        int sy = image->height - 1 - (dy * image->height) / size;
        for (int dx = 0; dx < size; ++dx) {
            int sx = (dx * image->width) / size;
            const uint8_t *src = image->pixels + ((uint32_t)sy * (uint32_t)image->width + (uint32_t)sx) * 4U;
            uint32_t alpha = src[3];
            if (!alpha) continue;
            uint32_t *dst = &fb[(y + dy) * fb_w + x + dx];
            if (alpha == 255U) *dst = 0xFF000000U | ((uint32_t)src[2] << 16) | ((uint32_t)src[1] << 8) | src[0];
            else {
                uint32_t bg = *dst, inv = 255U - alpha;
                uint32_t r = ((uint32_t)src[2] * alpha + ((bg >> 16) & 0xFFU) * inv) / 255U;
                uint32_t g = ((uint32_t)src[1] * alpha + ((bg >> 8) & 0xFFU) * inv) / 255U;
                uint32_t b = ((uint32_t)src[0] * alpha + (bg & 0xFFU) * inv) / 255U;
                *dst = 0xFF000000U | (r << 16) | (g << 8) | b;
            }
        }
    }
}

#endif
