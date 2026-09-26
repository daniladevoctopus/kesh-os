#include "kesh3d.h"
#include <stddef.h>

static float f_abs(float v) { return v < 0.0f ? -v : v; }
static int i_floor(float v) { int i = (int)v; return (v < 0.0f && (float)i != v) ? i - 1 : i; }
static float edge(float ax, float ay, float bx, float by, float px, float py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}
static float f_sin(float x) {
    const float pi = 3.14159265f;
    const float two_pi = 6.28318530f;
    while (x > pi) x -= two_pi;
    while (x < -pi) x += two_pi;
    float x2 = x * x;
    return x * (1.0f - x2 / 6.0f + (x2 * x2) / 120.0f - (x2 * x2 * x2) / 5040.0f);
}
static float f_cos(float x) {
    const float pi = 3.14159265f;
    const float two_pi = 6.28318530f;
    while (x > pi) x -= two_pi;
    while (x < -pi) x += two_pi;
    float x2 = x * x;
    return 1.0f - x2 / 2.0f + (x2 * x2) / 24.0f - (x2 * x2 * x2) / 720.0f;
}

void kesh3d_clear(kesh3d_target_t *t, uint32_t color, float depth) {
    if (!t || !t->color || !t->depth || t->pitch < t->width) return;
    for (uint32_t y = 0; y < t->height; y++) {
        uint32_t *row = t->color + (size_t)y * t->pitch;
        float *zrow = t->depth + (size_t)y * t->width;
        for (uint32_t x = 0; x < t->width; x++) { row[x] = color; zrow[x] = depth; }
    }
}

void kesh3d_triangle(kesh3d_target_t *t, const kesh3d_triangle_t *tr) {
    if (!t || !tr || !t->color || !t->depth || t->pitch < t->width) return;
    float area = edge(tr->a.x, tr->a.y, tr->b.x, tr->b.y, tr->c.x, tr->c.y);
    if (f_abs(area) < 0.0001f) return;
    int minx = i_floor(tr->a.x);
    int maxx = i_floor(tr->a.x);
    int miny = i_floor(tr->a.y);
    int maxy = i_floor(tr->a.y);
    if (tr->b.x < minx) minx = i_floor(tr->b.x);
    if (tr->c.x < minx) minx = i_floor(tr->c.x);
    if (tr->b.x > maxx) maxx = i_floor(tr->b.x);
    if (tr->c.x > maxx) maxx = i_floor(tr->c.x);
    if (tr->b.y < miny) miny = i_floor(tr->b.y);
    if (tr->c.y < miny) miny = i_floor(tr->c.y);
    if (tr->b.y > maxy) maxy = i_floor(tr->b.y);
    if (tr->c.y > maxy) maxy = i_floor(tr->c.y);
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx >= (int)t->width) maxx = (int)t->width - 1;
    if (maxy >= (int)t->height) maxy = (int)t->height - 1;
    for (int y = miny; y <= maxy; y++) {
        for (int x = minx; x <= maxx; x++) {
            float px = (float)x + 0.5f;
            float py = (float)y + 0.5f;
            float w0 = edge(tr->b.x, tr->b.y, tr->c.x, tr->c.y, px, py);
            float w1 = edge(tr->c.x, tr->c.y, tr->a.x, tr->a.y, px, py);
            float w2 = edge(tr->a.x, tr->a.y, tr->b.x, tr->b.y, px, py);
            if (area > 0.0f) {
                if (w0 < -0.0001f || w1 < -0.0001f || w2 < -0.0001f) continue;
            } else {
                if (w0 > 0.0001f || w1 > 0.0001f || w2 > 0.0001f) continue;
            }
            float z = (w0 * tr->a.z + w1 * tr->b.z + w2 * tr->c.z) / area;
            size_t idx = (size_t)y * t->width + (size_t)x;
            if (z < t->depth[idx]) {
                t->depth[idx] = z;
                t->color[(size_t)y * t->pitch + (size_t)x] = tr->color;
            }
        }
    }
}

typedef struct { float x, y, z; } cube_vertex_t;
static int project_vertex(cube_vertex_t v, float angle, uint32_t w, uint32_t h, kesh_vec3_t *out) {
    float c = f_cos(angle), s = f_sin(angle);
    float x = v.x * c - v.z * s;
    float z = v.x * s + v.z * c + 4.0f;
    float y = v.y * f_cos(angle * 0.71f) - z * f_sin(angle * 0.71f) * 0.18f;
    if (z <= 0.15f) return 0;
    float inv = 1.0f / z;
    float scale = (float)(w < h ? w : h) * 0.8f;
    out->x = (float)w * 0.5f + x * inv * scale;
    out->y = (float)h * 0.5f + y * inv * scale;
    out->z = z;
    return 1;
}

void kesh3d_cube_demo(kesh3d_target_t *t, float angle) {
    if (!t || !t->color || !t->depth) return;
    static const cube_vertex_t v[8] = {
        {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
        {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}
    };
    static const uint8_t faces[12][3] = {
        {0,1,2},{0,2,3},{1,5,6},{1,6,2},{5,4,7},{5,7,6},
        {4,0,3},{4,3,7},{3,2,6},{3,6,7},{4,5,1},{4,1,0}
    };
    static const uint32_t colors[6] = {
        0x00FF8A1C,0x00FFB52E,0x0030D158,0x000A84FF,0x00AF52DE,0x00FF375F
    };
    kesh_vec3_t p[8];
    for (int i = 0; i < 8; i++) if (!project_vertex(v[i], angle, t->width, t->height, &p[i])) return;
    for (int i = 0; i < 12; i++) {
        int face = i / 2;
        kesh3d_triangle_t tr;
        tr.a = p[faces[i][0]];
        tr.b = p[faces[i][1]];
        tr.c = p[faces[i][2]];
        tr.color = colors[face];
        kesh3d_triangle(t, &tr);
    }
}
