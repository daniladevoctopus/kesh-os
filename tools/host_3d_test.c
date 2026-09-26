#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "../src/gfx/kesh3d.h"

int main(void) {
    const uint32_t w = 320, h = 240;
    uint32_t *color = calloc((size_t)w * h, sizeof(uint32_t));
    float *depth = malloc((size_t)w * h * sizeof(float));
    if (!color || !depth) return 1;
    kesh3d_target_t s = { color, depth, w, h, w };
    kesh3d_clear(&s, 0x11223344u, 1.0e9f);
    kesh3d_cube_demo(&s, 0.75f);
    size_t pixels = 0;
    size_t depth_hits = 0;
    for (size_t i = 0; i < (size_t)w * h; ++i) {
        if (color[i] != 0x11223344u) pixels++;
        if (depth[i] < 1.0e9f) depth_hits++;
    }
    free(color);
    free(depth);
    if (pixels < 100 || depth_hits < 100) return 2;
    printf("host_3d_test: ok pixels=%zu depth=%zu\n", pixels, depth_hits);
    return 0;
}
