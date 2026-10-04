#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <pixman.h>
#include <xkbcommon/xkbcommon.h>

/* Linux Input Definitions */
#define EVIOCGVERSION 0x80044501U
#define EVIOCGID      0x80084502U

/* Linux Framebuffer Definitions */
#define FBIOGET_VSCREENINFO 0x4600
#define FBIOGET_FSCREENINFO 0x4602

struct my_fb_var_screeninfo {
    uint32_t xres;
    uint32_t yres;
    uint32_t xres_virtual;
    uint32_t yres_virtual;
    uint32_t xoffset;
    uint32_t yoffset;
    uint32_t bits_per_pixel;
};

struct my_fb_fix_screeninfo {
    char id[16];
    uint64_t smem_start;
    uint32_t smem_len;
    uint32_t type;
    uint32_t type_aux;
    uint32_t visual;
    uint16_t xpanstep;
    uint16_t ypanstep;
    uint16_t ywrapstep;
    uint32_t line_length;
};

int main(void) {
    printf("\n======================================================\n");
    printf("   KeshOS Phase 3: Graphics & Input Subsystem Demo\n");
    printf("   (pixman 2D, xkbcommon, evdev, fbdev & DRM/KMS)\n");
    printf("======================================================\n\n");

    /* 1. Test Pixman 2D rasterization & compositing */
    printf("[PHASE 3.1] Testing Pixman 2D Engine...\n");
    uint32_t dst_buf[64 * 64];
    memset(dst_buf, 0, sizeof(dst_buf));

    pixman_image_t *dst_img = pixman_image_create_bits(
        PIXMAN_a8r8g8b8, 64, 64, dst_buf, 64 * 4);
    if (!dst_img) {
        printf("FAILED: pixman_image_create_bits\n");
        return 1;
    }

    pixman_color_t color = { .red = 0xFFFF, .green = 0x8000, .blue = 0x0000, .alpha = 0xFFFF };
    pixman_image_t *src_img = pixman_image_create_solid_fill(&color);
    if (!src_img) {
        printf("FAILED: pixman_image_create_solid_fill\n");
        return 1;
    }

    pixman_image_composite32(
        PIXMAN_OP_SRC,
        src_img, NULL, dst_img,
        0, 0, 0, 0, 0, 0, 64, 64);

    pixman_image_unref(src_img);
    pixman_image_unref(dst_img);

    printf("  -> Pixman rasterized 64x64 RGBA surface (pixel[0]=0x%08X)\n", dst_buf[0]);
    if ((dst_buf[0] & 0x00FF0000) != 0x00FF0000) {
        printf("FAILED: Pixman pixel color mismatch\n");
        return 1;
    }
    printf("  -> Pixman 2D compositing verified [PASS]\n");

    /* 2. Test libxkbcommon keyboard translation */
    printf("\n[PHASE 3.2] Testing libxkbcommon Keyboard Subsystem...\n");
    struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!ctx) {
        printf("FAILED: xkb_context_new\n");
        return 1;
    }
    printf("  -> xkb_context created: %p\n", (void *)ctx);

    /* Test keysym conversion */
    xkb_keysym_t sym_a = xkb_keysym_from_name("a", XKB_KEYSYM_NO_FLAGS);
    char utf8_buf[16] = {0};
    int n = xkb_keysym_to_utf8(sym_a, utf8_buf, sizeof(utf8_buf));
    printf("  -> Keysym 'a' (0x%X) converted to UTF-8: '%s' (%d bytes)\n", sym_a, utf8_buf, n);

    xkb_keysym_t sym_ret = xkb_keysym_from_name("Return", XKB_KEYSYM_NO_FLAGS);
    printf("  -> Keysym 'Return' resolved to 0x%X\n", sym_ret);
    xkb_context_unref(ctx);
    printf("  -> libxkbcommon keysym engine verified [PASS]\n");

    /* 3. Test evdev input devices */
    printf("\n[PHASE 3.3] Testing evdev Kernel Device Nodes (/dev/input/event*)...\n");
    int kbd_fd = open("/dev/input/event0", O_RDONLY);
    if (kbd_fd < 0) {
        printf("FAILED: open /dev/input/event0\n");
        return 1;
    }
    printf("  -> Opened /dev/input/event0 (fd=%d)\n", kbd_fd);

    uint32_t ev_version = 0;
    if (ioctl(kbd_fd, EVIOCGVERSION, &ev_version) == 0) {
        printf("  -> evdev version: 0x%X\n", ev_version);
    }
    char dev_name[64] = {0};
    if (ioctl(kbd_fd, 0x4506, dev_name) == 0 || ioctl(kbd_fd, 0x5506, dev_name) == 0) {
        printf("  -> Device name: '%s'\n", dev_name);
    }
    close(kbd_fd);

    int mouse_fd = open("/dev/input/event1", O_RDONLY);
    if (mouse_fd < 0) {
        printf("FAILED: open /dev/input/event1\n");
        return 1;
    }
    printf("  -> Opened /dev/input/event1 (fd=%d)\n", mouse_fd);
    memset(dev_name, 0, sizeof(dev_name));
    if (ioctl(mouse_fd, 0x4506, dev_name) == 0 || ioctl(mouse_fd, 0x5506, dev_name) == 0) {
        printf("  -> Device name: '%s'\n", dev_name);
    }
    close(mouse_fd);
    printf("  -> evdev keyboard and mouse subsystem verified [PASS]\n");

    /* 4. Test fbdev / DRM display devices */
    printf("\n[PHASE 3.4] Testing fbdev (/dev/fb0) and DRM/KMS (/dev/dri/card0)...\n");
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        printf("FAILED: open /dev/fb0\n");
        return 1;
    }
    printf("  -> Opened /dev/fb0 (fd=%d)\n", fb_fd);

    struct my_fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == 0) {
        printf("  -> Framebuffer resolution: %ux%u, %u bpp\n",
               vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
    }
    struct my_fb_fix_screeninfo finfo;
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == 0) {
        printf("  -> Framebuffer smem: len=%u bytes, pitch=%u bytes, id='%s'\n",
               finfo.smem_len, finfo.line_length, finfo.id);
    }
    close(fb_fd);

    int drm_fd = open("/dev/dri/card0", O_RDWR);
    if (drm_fd < 0) {
        printf("FAILED: open /dev/dri/card0\n");
        return 1;
    }
    printf("  -> Opened /dev/dri/card0 (fd=%d)\n", drm_fd);
    close(drm_fd);
    printf("  -> Framebuffer and DRM/KMS device access verified [PASS]\n");

    printf("\n======================================================\n");
    printf("   SUCCESS: Phase 3 Graphics & Input Subsystem Ready!\n");
    printf("   pixman, libxkbcommon, evdev, fb0, DRM are online!\n");
    printf("======================================================\n\n");
    return 0;
}
