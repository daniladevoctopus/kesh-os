#ifndef KESHOS_DRM_FB_H
#define KESHOS_DRM_FB_H

#include <stdint.h>
#include <stddef.h>

/* Linux Framebuffer ioctls */
#define FBIOGET_VSCREENINFO 0x4600
#define FBIOPUT_VSCREENINFO 0x4601
#define FBIOGET_FSCREENINFO 0x4602
#define FBIOBLANK           0x4611

struct fb_bitfield {
    uint32_t offset;
    uint32_t length;
    uint32_t msb_right;
};

struct fb_var_screeninfo {
    uint32_t xres;
    uint32_t yres;
    uint32_t xres_virtual;
    uint32_t yres_virtual;
    uint32_t xoffset;
    uint32_t yoffset;
    uint32_t bits_per_pixel;
    uint32_t grayscale;
    struct fb_bitfield red;
    struct fb_bitfield green;
    struct fb_bitfield blue;
    struct fb_bitfield transp;
    uint32_t nonstd;
    uint32_t activate;
    uint32_t height;
    uint32_t width;
    uint32_t accel_flags;
    uint32_t pixclock;
    uint32_t left_margin;
    uint32_t right_margin;
    uint32_t upper_margin;
    uint32_t lower_margin;
    uint32_t hsync_len;
    uint32_t vsync_len;
    uint32_t sync;
    uint32_t vmode;
    uint32_t rotate;
    uint32_t colorspace;
    uint32_t reserved[4];
};

struct fb_fix_screeninfo {
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
    uint64_t mmio_start;
    uint32_t mmio_len;
    uint32_t accel;
    uint16_t capabilities;
    uint16_t reserved[2];
};

/* DRM KMS ioctl base */
#define DRM_IOCTL_BASE 'd'
#define DRM_IOCTL_VERSION 0xc0406400U
#define DRM_IOCTL_GET_CAP 0xc010640cU

struct drm_version {
    int version_major;
    int version_minor;
    int version_patchlevel;
    uint64_t name_len;
    uint64_t name;
    uint64_t date_len;
    uint64_t date;
    uint64_t desc_len;
    uint64_t desc;
};

struct drm_get_cap {
    uint64_t capability;
    uint64_t value;
};

void drm_fb_init(void);
int drm_fb_open_fb0(int pid);
int drm_fb_open_card0(int pid);
int drm_fb_ioctl(void *custom_ptr, uint64_t req, uint64_t arg);
uint64_t drm_fb_mmap(int pid, size_t len, uint32_t prot);
void drm_fb_release(void *custom_ptr);

/* The legacy kernel desktop and a direct userspace compositor must never race
 * each other for the same physical framebuffer. Opening /dev/fb0 grants one
 * process temporary exclusive scanout ownership until its last fb0 FD closes.
 * Input/network/process scheduling continue in the kernel while rendering is
 * handed off. */
int drm_fb_userspace_owned(void);
int drm_fb_owner_pid(void);

#endif /* KESHOS_DRM_FB_H */
