#include "drm_fb.h"
#include "fd.h"
#include "process.h"
#include "memory.h"
#include "serial.h"

extern uint8_t *g_fb_vram;
extern uint32_t g_screen_w, g_screen_h, g_screen_pitch, g_screen_bpp;

typedef struct {
    int is_drm;
    int pid;
    uint32_t open_count;
} drm_fb_device_t;

static drm_fb_device_t g_fb0_dev = { .is_drm = 0, .pid = -1, .open_count = 0 };
static drm_fb_device_t g_card0_dev = { .is_drm = 1, .pid = -1, .open_count = 0 };

void drm_fb_init(void) {
    g_fb0_dev.pid = -1;
    g_fb0_dev.open_count = 0;
    g_card0_dev.pid = -1;
    g_card0_dev.open_count = 0;
}

int drm_fb_userspace_owned(void) {
    return g_fb0_dev.open_count != 0;
}

int drm_fb_owner_pid(void) {
    return g_fb0_dev.open_count ? g_fb0_dev.pid : -1;
}

int drm_fb_open_fb0(int pid) {
    if (pid < 0) return -1;

    /* A direct scanout process owns fb0 until its last independently opened
     * fb0 FD is released. Duplicating one FD does not increment open_count,
     * because fd_close() calls drm_fb_release() only when the shared FD object
     * loses its last reference. */
    if (g_fb0_dev.open_count && g_fb0_dev.pid != pid) return -1;

    int fd = fd_create_custom(pid, FD_KIND_DRM_FB, &g_fb0_dev,
                              FD_OPEN_READ | FD_OPEN_WRITE);
    if (fd < 0) return fd;

    if (g_fb0_dev.open_count == 0) g_fb0_dev.pid = pid;
    g_fb0_dev.open_count++;
    return fd;
}

int drm_fb_open_card0(int pid) {
    return fd_create_custom(pid, FD_KIND_DRM_FB, &g_card0_dev,
                            FD_OPEN_READ | FD_OPEN_WRITE);
}

int drm_fb_ioctl(void *custom_ptr, uint64_t req, uint64_t arg) {
    (void)custom_ptr;
    if (req == FBIOBLANK) return 0;
    if (!arg) return -1;

    /* FBIOGET_VSCREENINFO */
    if (req == FBIOGET_VSCREENINFO) {
        struct fb_var_screeninfo vi;
        for (size_t i = 0; i < sizeof(vi); i++) ((uint8_t *)&vi)[i] = 0;
        vi.xres = g_screen_w;
        vi.yres = g_screen_h;
        vi.xres_virtual = g_screen_w;
        vi.yres_virtual = g_screen_h;
        vi.bits_per_pixel = 32;

        vi.red.offset = 16;   vi.red.length = 8;
        vi.green.offset = 8;  vi.green.length = 8;
        vi.blue.offset = 0;   vi.blue.length = 8;
        vi.transp.offset = 24; vi.transp.length = 8;

        return copy_to_user((void *)arg, &vi, sizeof(vi));
    }

    /* FBIOGET_FSCREENINFO */
    if (req == FBIOGET_FSCREENINFO) {
        struct fb_fix_screeninfo fi;
        for (size_t i = 0; i < sizeof(fi); i++) ((uint8_t *)&fi)[i] = 0;
        const char name[] = "keshos-fb";
        for (size_t i = 0; i < sizeof(name); i++) fi.id[i] = name[i];

        extern uint64_t g_hhdm_offset;
        uint64_t phys_start = (uint64_t)g_fb_vram;
        if (phys_start >= g_hhdm_offset) phys_start -= g_hhdm_offset;
        fi.smem_start = phys_start;
        fi.smem_len = g_screen_pitch * g_screen_h;
        fi.line_length = g_screen_pitch;
        fi.visual = 2; /* FB_VISUAL_TRUECOLOR */

        return copy_to_user((void *)arg, &fi, sizeof(fi));
    }

    /* DRM_IOCTL_VERSION */
    if (req == DRM_IOCTL_VERSION) {
        struct drm_version v;
        if (copy_from_user(&v, (const void *)arg, sizeof(v)) != 0) return -1;
        v.version_major = 1;
        v.version_minor = 0;
        v.version_patchlevel = 0;

        const char dname[] = "keshos-drm";
        const char ddate[] = "20261004";
        const char ddesc[] = "KeshOS KMS/DRM Driver";

        if (v.name && v.name_len > 0) copy_to_user((void *)v.name, dname, sizeof(dname));
        if (v.date && v.date_len > 0) copy_to_user((void *)v.date, ddate, sizeof(ddate));
        if (v.desc && v.desc_len > 0) copy_to_user((void *)v.desc, ddesc, sizeof(ddesc));

        return copy_to_user((void *)arg, &v, sizeof(v));
    }

    /* DRM_IOCTL_GET_CAP */
    if (req == DRM_IOCTL_GET_CAP) {
        struct drm_get_cap cap;
        if (copy_from_user(&cap, (const void *)arg, sizeof(cap)) != 0) return -1;
        cap.value = 1; /* Supported */
        return copy_to_user((void *)arg, &cap, sizeof(cap));
    }

    return 0;
}

uint64_t drm_fb_mmap(int pid, size_t len, uint32_t prot) {
    if (!g_fb_vram || len == 0) return 0;

    /* fb0 mappings are direct scanout mappings. Refuse a stale/unowned mmap or
     * a mapping from a different process instead of exposing VRAM broadly. */
    if (!g_fb0_dev.open_count || g_fb0_dev.pid != pid) return 0;

    uint64_t total_size = (uint64_t)g_screen_pitch * (uint64_t)g_screen_h;
    if ((uint64_t)len > total_size) return 0;
    uint64_t num_pages = ((uint64_t)len + 4095ULL) / 4096ULL;

    /* 64 MiB covers 4K 32-bpp framebuffers with headroom. Never silently
     * return a shorter mapping than userspace requested. */
    #define MAX_FB_PAGES 16384
    if (num_pages == 0 || num_pages > MAX_FB_PAGES) return 0;

    static uint64_t s_fb_phys[MAX_FB_PAGES];
    extern uint64_t g_hhdm_offset;
    uint64_t base_phys = (uint64_t)g_fb_vram;
    if (base_phys >= g_hhdm_offset) base_phys -= g_hhdm_offset;
    for (uint64_t i = 0; i < num_pages; i++) {
        s_fb_phys[i] = base_phys + i * 4096ULL;
    }

    return process_vm_map_phys(s_fb_phys, num_pages, prot);
}

void drm_fb_release(void *custom_ptr) {
    drm_fb_device_t *dev = (drm_fb_device_t *)custom_ptr;
    if (!dev || dev->is_drm) return;

    if (dev->open_count) dev->open_count--;
    if (dev->open_count == 0) dev->pid = -1;
}
