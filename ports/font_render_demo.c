#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <linux/fb.h>
#include <linux/input.h>

#include <pixman.h>
#include <ft2build.h>
#include FT_FREETYPE_H

static void draw_glyph(uint32_t *fb, int fb_w, int fb_h, FT_Bitmap *bmp, int x, int y, uint32_t color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    for (int row = 0; row < (int)bmp->rows; ++row) {
        int py = y + row;
        if (py < 0 || py >= fb_h) continue;

        for (int col = 0; col < (int)bmp->width; ++col) {
            int px = x + col;
            if (px < 0 || px >= fb_w) continue;

            uint8_t alpha = bmp->buffer[row * bmp->pitch + col];
            if (alpha == 0) continue;

            if (alpha == 255) {
                fb[py * fb_w + px] = color;
            } else {
                uint32_t bg = fb[py * fb_w + px];
                uint8_t b_r = (bg >> 16) & 0xFF;
                uint8_t b_g = (bg >> 8) & 0xFF;
                uint8_t b_b = bg & 0xFF;

                uint32_t out_r = (r * alpha + b_r * (255 - alpha)) / 255;
                uint32_t out_g = (g * alpha + b_g * (255 - alpha)) / 255;
                uint32_t out_b = (b * alpha + b_b * (255 - alpha)) / 255;

                fb[py * fb_w + px] = (0xFF << 24) | (out_r << 16) | (out_g << 8) | out_b;
            }
        }
    }
}

static void render_text(FT_Face face, uint32_t *fb, int fb_w, int fb_h, const char *text, int x, int y, uint32_t color) {
    int pen_x = x;
    int pen_y = y;

    for (const char *p = text; *p; ++p) {
        if (FT_Load_Char(face, (unsigned char)*p, FT_LOAD_RENDER)) {
            continue;
        }

        FT_GlyphSlot slot = face->glyph;
        draw_glyph(fb, fb_w, fb_h, &slot->bitmap,
                   pen_x + slot->bitmap_left,
                   pen_y - slot->bitmap_top,
                   color);

        pen_x += slot->advance.x >> 6;
    }
}

int main(int argc, char **argv) {
    printf("[font_demo] Starting FreeType + Pixman + DRM/evdev GUI demo...\n");

    // 1. Open framebuffer
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        perror("open /dev/fb0");
        return 1;
    }

    struct fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("ioctl FBIOGET_VSCREENINFO");
        close(fb_fd);
        return 1;
    }

    int width = vinfo.xres;
    int height = vinfo.yres;
    size_t screensize = width * height * (vinfo.bits_per_pixel / 8);
    printf("[font_demo] Screen resolution: %dx%d @ %d bpp (size %zu bytes)\n",
           width, height, vinfo.bits_per_pixel, screensize);

    uint32_t *fb = (uint32_t *)mmap(NULL, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb == MAP_FAILED) {
        perror("mmap fb");
        close(fb_fd);
        return 1;
    }

    // 2. Init FreeType
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "[font_demo] Failed to initialize FreeType\n");
        munmap(fb, screensize);
        close(fb_fd);
        return 1;
    }

    // Check font paths
    const char *font_path = "/boot/fonts/OpenSans.ttf";
    if (access(font_path, R_OK) != 0) {
        font_path = "ready/boot/fonts/OpenSans.ttf";
        if (access(font_path, R_OK) != 0) {
            font_path = "ports/freetype2/tests/data/varc-static-gvar.ttf";
        }
    }

    FT_Face face_title;
    FT_Face face_body;
    if (FT_New_Face(ft, font_path, 0, &face_title) != 0) {
        fprintf(stderr, "[font_demo] Failed to load font: %s\n", font_path);
        FT_Done_FreeType(ft);
        munmap(fb, screensize);
        close(fb_fd);
        return 1;
    }
    FT_New_Face(ft, font_path, 0, &face_body);

    FT_Set_Pixel_Sizes(face_title, 0, 32);
    FT_Set_Pixel_Sizes(face_body, 0, 18);

    // 3. Open evdev input
    int ev_kbd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    int ev_mouse = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
    printf("[font_demo] Input devices: kbd fd=%d, mouse fd=%d\n", ev_kbd, ev_mouse);

    // 4. Create Pixman backbuffer
    pixman_image_t *backbuffer = pixman_image_create_bits(
        PIXMAN_a8r8g8b8, width, height, NULL, width * 4);
    uint32_t *back_data = pixman_image_get_data(backbuffer);

    int mouse_x = width / 2;
    int mouse_y = height / 2;
    int running = 1;
    int frame = 0;

    while (running && frame < 300) {
        // Poll input
        struct pollfd pfd[2];
        pfd[0].fd = ev_kbd;
        pfd[0].events = POLLIN;
        pfd[1].fd = ev_mouse;
        pfd[1].events = POLLIN;

        int ret = poll(pfd, 2, 16); // ~60 fps
        if (ret > 0) {
            if (pfd[0].revents & POLLIN) {
                struct input_event ev;
                while (read(ev_kbd, &ev, sizeof(ev)) == sizeof(ev)) {
                    if (ev.type == EV_KEY && ev.value == 1) {
                        if (ev.code == KEY_ESC || ev.code == KEY_Q) {
                            running = 0;
                        }
                    }
                }
            }
            if (pfd[1].revents & POLLIN) {
                struct input_event ev;
                while (read(ev_mouse, &ev, sizeof(ev)) == sizeof(ev)) {
                    if (ev.type == EV_REL) {
                        if (ev.code == REL_X) mouse_x += ev.value;
                        if (ev.code == REL_Y) mouse_y += ev.value;
                        if (mouse_x < 0) mouse_x = 0;
                        if (mouse_x >= width) mouse_x = width - 1;
                        if (mouse_y < 0) mouse_y = 0;
                        if (mouse_y >= height) mouse_y = height - 1;
                    }
                }
            }
        }

        // Render Background (Deep KDE Plasma Breeze Slate Dark)
        pixman_color_t bg_color = { .red = 0x1616, .green = 0x1B1B, .blue = 0x2222, .alpha = 0xFFFF };
        pixman_image_t *bg_src = pixman_image_create_solid_fill(&bg_color);
        pixman_image_composite(PIXMAN_OP_SRC, bg_src, NULL, backbuffer, 0, 0, 0, 0, 0, 0, width, height);
        pixman_image_unref(bg_src);

        // Header Card (Glassmorphism blue accent)
        pixman_color_t card_color = { .red = 0x2020, .green = 0x3030, .blue = 0x5050, .alpha = 0xDDDD };
        pixman_image_t *card_src = pixman_image_create_solid_fill(&card_color);
        pixman_image_composite(PIXMAN_OP_OVER, card_src, NULL, backbuffer, 0, 0, 0, 0, 60, 50, width - 120, 200);
        pixman_image_unref(card_src);

        // Render Vector Typography with FreeType
        render_text(face_title, back_data, width, height, "KeshOS KDE Plasma Subsystem", 100, 110, 0xFFFFFFFF);
        render_text(face_body, back_data, width, height, "Userspace Wayland Compositor + Pixman + FreeType2 Vector Graphics Engine", 100, 155, 0xFF3DAEE9);
        render_text(face_body, back_data, width, height, "TrueType font rasterization & anti-aliased alpha blending active in Ring 3", 100, 190, 0xFFBDC3C7);

        // Stats Card
        pixman_color_t stats_color = { .red = 0x1E1E, .green = 0x2424, .blue = 0x2E2E, .alpha = 0xEEEE };
        pixman_image_t *stats_src = pixman_image_create_solid_fill(&stats_color);
        pixman_image_composite(PIXMAN_OP_OVER, stats_src, NULL, backbuffer, 0, 0, 0, 0, 60, 280, width - 120, 240);
        pixman_image_unref(stats_src);

        char stats_buf[128];
        snprintf(stats_buf, sizeof(stats_buf), "Frame: %d | Mouse Pointer: (%d, %d)", frame, mouse_x, mouse_y);
        render_text(face_body, back_data, width, height, stats_buf, 100, 330, 0xFF2ECC71);

        render_text(face_body, back_data, width, height, "Architecture: x86_64 Long Mode | Toolchain: musl libc 1.2.5 + LLVM Clang", 100, 370, 0xFFE0E0E0);
        render_text(face_body, back_data, width, height, "IPC: AF_UNIX + SCM_RIGHTS | Memory: memfd_create + zero-copy /dev/fb0 mmap", 100, 410, 0xFFE0E0E0);
        render_text(face_body, back_data, width, height, "Press ESC or Q to exit back to command prompt", 100, 470, 0xFFE74C3C);

        // Draw Mouse Cursor (arrow)
        for (int dy = 0; dy < 16; ++dy) {
            for (int dx = 0; dx <= dy && dx < 12; ++dx) {
                int cx = mouse_x + dx;
                int cy = mouse_y + dy;
                if (cx >= 0 && cx < width && cy >= 0 && cy < height) {
                    back_data[cy * width + cx] = (dx == 0 || dx == dy || dy == 15) ? 0xFF000000 : 0xFF3DAEE9;
                }
            }
        }

        // Blit backbuffer to linear VRAM framebuffer
        memcpy(fb, back_data, screensize);

        frame++;
    }

    pixman_image_unref(backbuffer);
    FT_Done_Face(face_title);
    FT_Done_Face(face_body);
    FT_Done_FreeType(ft);
    if (ev_kbd >= 0) close(ev_kbd);
    if (ev_mouse >= 0) close(ev_mouse);
    munmap(fb, screensize);
    close(fb_fd);

    printf("[font_demo] Demo finished cleanly after %d frames.\n", frame);
    return 0;
}
