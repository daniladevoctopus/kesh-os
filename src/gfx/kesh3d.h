#ifndef KESH_3D_H
#define KESH_3D_H

#include <stdint.h>

typedef struct { float x, y, z; } kesh_vec3_t;
typedef struct { uint32_t *color; float *depth; uint32_t width; uint32_t height; uint32_t pitch; } kesh3d_target_t;
typedef struct { kesh_vec3_t a, b, c; uint32_t color; } kesh3d_triangle_t;

void kesh3d_clear(kesh3d_target_t *target, uint32_t color, float depth);
void kesh3d_triangle(kesh3d_target_t *target, const kesh3d_triangle_t *triangle);
void kesh3d_cube_demo(kesh3d_target_t *target, float angle);

#endif
