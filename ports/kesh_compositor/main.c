#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <wayland-server.h>
#include "protocol/xdg-shell-server-protocol.h"

int kesh_demo_client_start(void);

struct compositor;

struct surface {
    struct compositor *compositor;
    struct wl_resource *resource;
    struct wl_resource *pending_buffer;
    struct wl_resource *frame_callback;
    struct wl_resource *xdg_surface;
    struct wl_resource *xdg_toplevel;
    int32_t attach_x;
    int32_t attach_y;
    int32_t scale;
};

struct compositor {
    struct wl_display *display;
    uint32_t *framebuffer;
    size_t framebuffer_size;
    int framebuffer_fd;
    int listen_fd;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
};

static void resource_destroy(struct wl_client *client, struct wl_resource *resource)
{
    (void)client;
    wl_resource_destroy(resource);
}

static void surface_resource_destroy(struct wl_resource *resource)
{
    struct surface *surface = wl_resource_get_user_data(resource);
    if (!surface) return;
    if (surface->frame_callback) wl_resource_destroy(surface->frame_callback);
    free(surface);
}

static void surface_attach(struct wl_client *client, struct wl_resource *resource,
                           struct wl_resource *buffer, int32_t x, int32_t y)
{
    (void)client;
    struct surface *surface = wl_resource_get_user_data(resource);
    surface->pending_buffer = buffer;
    surface->attach_x = x;
    surface->attach_y = y;
}

static void surface_noop_region(struct wl_client *client, struct wl_resource *resource,
                                struct wl_resource *region)
{
    (void)client; (void)resource; (void)region;
}

static void surface_damage(struct wl_client *client, struct wl_resource *resource,
                           int32_t x, int32_t y, int32_t width, int32_t height)
{
    (void)client; (void)resource; (void)x; (void)y; (void)width; (void)height;
}

static void surface_frame(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
    struct surface *surface = wl_resource_get_user_data(resource);
    if (surface->frame_callback) wl_resource_destroy(surface->frame_callback);
    surface->frame_callback = wl_resource_create(client, &wl_callback_interface, 1, id);
}

static void surface_commit(struct wl_client *client, struct wl_resource *resource)
{
    (void)client;
    struct surface *surface = wl_resource_get_user_data(resource);
    struct compositor *compositor = surface->compositor;
    if (surface->pending_buffer && compositor->framebuffer) {
        struct wl_shm_buffer *shm = wl_shm_buffer_get(surface->pending_buffer);
        if (!shm) {
            wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                                   "Kesh compositor supports wl_shm buffers");
            return;
        }
        int width = wl_shm_buffer_get_width(shm);
        int height = wl_shm_buffer_get_height(shm);
        int stride = wl_shm_buffer_get_stride(shm);
        uint32_t format = wl_shm_buffer_get_format(shm);
        if ((format != WL_SHM_FORMAT_XRGB8888 && format != WL_SHM_FORMAT_ARGB8888) ||
            width <= 0 || height <= 0 || stride < width * 4) {
            wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FORMAT,
                                   "unsupported SHM buffer format or geometry");
            return;
        }

        int dst_x = width < (int)compositor->width ? ((int)compositor->width - width) / 2 : 0;
        int dst_y = height < (int)compositor->height ? ((int)compositor->height - height) / 2 : 0;
        int copy_width = width;
        int copy_height = height;
        if (copy_width > (int)compositor->width) copy_width = (int)compositor->width;
        if (copy_height > (int)compositor->height) copy_height = (int)compositor->height;

        wl_shm_buffer_begin_access(shm);
        const uint8_t *pixels = wl_shm_buffer_get_data(shm);
        for (int y = 0; y < copy_height; ++y) {
            memcpy((uint8_t *)compositor->framebuffer + (size_t)(dst_y + y) * compositor->pitch +
                       (size_t)dst_x * 4,
                   pixels + (size_t)y * (size_t)stride, (size_t)copy_width * 4);
        }
        wl_shm_buffer_end_access(shm);
        wl_buffer_send_release(surface->pending_buffer);
        surface->pending_buffer = NULL;
    }

    if (surface->frame_callback) {
        wl_callback_send_done(surface->frame_callback, 0);
        wl_resource_destroy(surface->frame_callback);
        surface->frame_callback = NULL;
    }
}

static void surface_set_transform(struct wl_client *client, struct wl_resource *resource,
                                  int32_t transform)
{
    (void)client; (void)resource; (void)transform;
}

static void surface_set_scale(struct wl_client *client, struct wl_resource *resource, int32_t scale)
{
    (void)client;
    struct surface *surface = wl_resource_get_user_data(resource);
    surface->scale = scale > 0 ? scale : 1;
}

static void surface_offset(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y)
{
    (void)client;
    struct surface *surface = wl_resource_get_user_data(resource);
    surface->attach_x = x;
    surface->attach_y = y;
}

static const struct wl_surface_interface surface_implementation = {
    .destroy = resource_destroy,
    .attach = surface_attach,
    .damage = surface_damage,
    .frame = surface_frame,
    .set_opaque_region = surface_noop_region,
    .set_input_region = surface_noop_region,
    .commit = surface_commit,
    .set_buffer_transform = surface_set_transform,
    .set_buffer_scale = surface_set_scale,
    .damage_buffer = surface_damage,
    .offset = surface_offset,
};

static void region_noop(struct wl_client *client, struct wl_resource *resource,
                        int32_t x, int32_t y, int32_t width, int32_t height)
{
    (void)client; (void)resource; (void)x; (void)y; (void)width; (void)height;
}

static const struct wl_region_interface region_implementation = {
    .destroy = resource_destroy,
    .add = region_noop,
    .subtract = region_noop,
};

static void compositor_create_surface(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
    struct compositor *compositor = wl_resource_get_user_data(resource);
    struct surface *surface = calloc(1, sizeof(*surface));
    if (!surface) {
        wl_client_post_no_memory(client);
        return;
    }
    surface->compositor = compositor;
    surface->scale = 1;
    surface->resource = wl_resource_create(client, &wl_surface_interface,
                                           wl_resource_get_version(resource) < 6 ?
                                           wl_resource_get_version(resource) : 6, id);
    if (!surface->resource) {
        free(surface);
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(surface->resource, &surface_implementation,
                                   surface, surface_resource_destroy);
}

static void compositor_create_region(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
    struct wl_resource *region = wl_resource_create(client, &wl_region_interface, 1, id);
    if (!region) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(region, &region_implementation, NULL, NULL);
    (void)resource;
}

static const struct wl_compositor_interface compositor_implementation = {
    .create_surface = compositor_create_surface,
    .create_region = compositor_create_region,
};

static void bind_compositor(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
    if (version > 6) version = 6;
    struct wl_resource *resource = wl_resource_create(client, &wl_compositor_interface, version, id);
    if (!resource) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(resource, &compositor_implementation, data, NULL);
}

static void output_release(struct wl_client *client, struct wl_resource *resource)
{
    resource_destroy(client, resource);
}

static const struct wl_output_interface output_implementation = { .release = output_release };

static void bind_output(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
    struct compositor *compositor = data;
    if (version > 4) version = 4;
    struct wl_resource *resource = wl_resource_create(client, &wl_output_interface, version, id);
    if (!resource) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(resource, &output_implementation, compositor, NULL);
    wl_output_send_geometry(resource, 0, 0, 270, 203, WL_OUTPUT_SUBPIXEL_UNKNOWN,
                            "KeshOS", "Virtual Display", WL_OUTPUT_TRANSFORM_NORMAL);
    wl_output_send_mode(resource, WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
                        (int32_t)compositor->width, (int32_t)compositor->height, 60000);
    if (version >= 2) { wl_output_send_scale(resource, 1); wl_output_send_done(resource); }
    if (version >= 4) {
        wl_output_send_name(resource, "KESH-1");
        wl_output_send_description(resource, "KeshOS framebuffer output");
    }
}

static void xdg_toplevel_destroy(struct wl_client *client, struct wl_resource *resource)
{
    (void)client;
    struct surface *surface = wl_resource_get_user_data(resource);
    if (surface) surface->xdg_toplevel = NULL;
    wl_resource_destroy(resource);
}

static void xdg_toplevel_noop(struct wl_client *client, struct wl_resource *resource)
{ (void)client; (void)resource; }
static void xdg_toplevel_parent(struct wl_client *client, struct wl_resource *resource, struct wl_resource *parent)
{ (void)client; (void)resource; (void)parent; }
static void xdg_toplevel_string(struct wl_client *client, struct wl_resource *resource, const char *text)
{ (void)client; (void)resource; (void)text; }
static void xdg_toplevel_menu(struct wl_client *client, struct wl_resource *resource,
                              struct wl_resource *seat, uint32_t serial, int32_t x, int32_t y)
{ (void)client; (void)resource; (void)seat; (void)serial; (void)x; (void)y; }
static void xdg_toplevel_move(struct wl_client *client, struct wl_resource *resource,
                              struct wl_resource *seat, uint32_t serial)
{ (void)client; (void)resource; (void)seat; (void)serial; }
static void xdg_toplevel_resize(struct wl_client *client, struct wl_resource *resource,
                                struct wl_resource *seat, uint32_t serial, uint32_t edges)
{ (void)client; (void)resource; (void)seat; (void)serial; (void)edges; }
static void xdg_toplevel_size(struct wl_client *client, struct wl_resource *resource, int32_t w, int32_t h)
{ (void)client; (void)resource; (void)w; (void)h; }
static void xdg_toplevel_fullscreen(struct wl_client *client, struct wl_resource *resource,
                                    struct wl_resource *output)
{ (void)client; (void)resource; (void)output; }

static const struct xdg_toplevel_interface xdg_toplevel_implementation = {
    .destroy = xdg_toplevel_destroy,
    .set_parent = xdg_toplevel_parent,
    .set_title = xdg_toplevel_string,
    .set_app_id = xdg_toplevel_string,
    .show_window_menu = xdg_toplevel_menu,
    .move = xdg_toplevel_move,
    .resize = xdg_toplevel_resize,
    .set_max_size = xdg_toplevel_size,
    .set_min_size = xdg_toplevel_size,
    .set_maximized = xdg_toplevel_noop,
    .unset_maximized = xdg_toplevel_noop,
    .set_fullscreen = xdg_toplevel_fullscreen,
    .unset_fullscreen = xdg_toplevel_noop,
    .set_minimized = xdg_toplevel_noop,
};

static void xdg_surface_destroy(struct wl_client *client, struct wl_resource *resource)
{
    (void)client;
    struct surface *surface = wl_resource_get_user_data(resource);
    if (surface) surface->xdg_surface = NULL;
    wl_resource_destroy(resource);
}

static void xdg_surface_get_toplevel(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
    struct surface *surface = wl_resource_get_user_data(resource);
    surface->xdg_toplevel = wl_resource_create(client, &xdg_toplevel_interface,
                                               wl_resource_get_version(resource), id);
    if (!surface->xdg_toplevel) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(surface->xdg_toplevel, &xdg_toplevel_implementation,
                                   surface, NULL);
    struct wl_array states;
    wl_array_init(&states);
    xdg_toplevel_send_configure(surface->xdg_toplevel,
                                (int32_t)surface->compositor->width,
                                (int32_t)surface->compositor->height, &states);
    wl_array_release(&states);
    xdg_surface_send_configure(resource, wl_display_next_serial(surface->compositor->display));
}

static void xdg_surface_get_popup(struct wl_client *client, struct wl_resource *resource,
                                  uint32_t id, struct wl_resource *parent, struct wl_resource *positioner)
{
    (void)id; (void)parent; (void)positioner;
    wl_resource_post_error(resource, XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE,
                           "xdg_popup is not implemented yet");
    (void)client;
}
static void xdg_surface_geometry(struct wl_client *client, struct wl_resource *resource,
                                 int32_t x, int32_t y, int32_t w, int32_t h)
{ (void)client; (void)resource; (void)x; (void)y; (void)w; (void)h; }
static void xdg_surface_ack(struct wl_client *client, struct wl_resource *resource, uint32_t serial)
{ (void)client; (void)resource; (void)serial; }

static const struct xdg_surface_interface xdg_surface_implementation = {
    .destroy = xdg_surface_destroy,
    .get_toplevel = xdg_surface_get_toplevel,
    .get_popup = xdg_surface_get_popup,
    .set_window_geometry = xdg_surface_geometry,
    .ack_configure = xdg_surface_ack,
};

static void xdg_wm_destroy(struct wl_client *client, struct wl_resource *resource)
{ resource_destroy(client, resource); }
static void xdg_wm_positioner(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
    (void)id;
    wl_resource_post_error(resource, XDG_WM_BASE_ERROR_INVALID_POSITIONER,
                           "xdg_positioner is not implemented yet");
    (void)client;
}
static void xdg_wm_get_surface(struct wl_client *client, struct wl_resource *resource,
                               uint32_t id, struct wl_resource *wl_surface_resource)
{
    struct surface *surface = wl_resource_get_user_data(wl_surface_resource);
    if (!surface || surface->xdg_surface) {
        wl_resource_post_error(resource, XDG_WM_BASE_ERROR_ROLE, "wl_surface already has a role");
        return;
    }
    surface->xdg_surface = wl_resource_create(client, &xdg_surface_interface,
                                              wl_resource_get_version(resource), id);
    if (!surface->xdg_surface) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(surface->xdg_surface, &xdg_surface_implementation,
                                   surface, NULL);
}
static void xdg_wm_pong(struct wl_client *client, struct wl_resource *resource, uint32_t serial)
{ (void)client; (void)resource; (void)serial; }

static const struct xdg_wm_base_interface xdg_wm_implementation = {
    .destroy = xdg_wm_destroy,
    .create_positioner = xdg_wm_positioner,
    .get_xdg_surface = xdg_wm_get_surface,
    .pong = xdg_wm_pong,
};

static void bind_xdg_wm(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
    if (version > 6) version = 6;
    struct wl_resource *resource = wl_resource_create(client, &xdg_wm_base_interface, version, id);
    if (!resource) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(resource, &xdg_wm_implementation, data, NULL);
}

static int open_framebuffer(struct compositor *compositor)
{
    struct fb_var_screeninfo variable;
    struct fb_fix_screeninfo fixed;
    compositor->framebuffer_fd = open("/dev/fb0", O_RDWR);
    if (compositor->framebuffer_fd < 0) return -1;
    if (ioctl(compositor->framebuffer_fd, FBIOGET_VSCREENINFO, &variable) != 0 ||
        ioctl(compositor->framebuffer_fd, FBIOGET_FSCREENINFO, &fixed) != 0 ||
        variable.bits_per_pixel != 32) return -1;
    compositor->width = variable.xres;
    compositor->height = variable.yres;
    compositor->pitch = fixed.line_length;
    compositor->framebuffer_size = fixed.smem_len;
    compositor->framebuffer = mmap(NULL, compositor->framebuffer_size,
                                   PROT_READ | PROT_WRITE, MAP_SHARED,
                                   compositor->framebuffer_fd, 0);
    if (compositor->framebuffer == MAP_FAILED) return -1;
    for (uint32_t y = 0; y < compositor->height; ++y) {
        uint32_t *row = (uint32_t *)((uint8_t *)compositor->framebuffer + (size_t)y * compositor->pitch);
        for (uint32_t x = 0; x < compositor->width; ++x) {
            uint32_t blue = 20U + (x * 34U / (compositor->width ? compositor->width : 1));
            row[x] = 0xFF080E18U | blue;
        }
    }
    return 0;
}

static int create_wayland_socket(struct compositor *compositor)
{
    compositor->listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (compositor->listen_fd < 0) return -1;
    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, "/tmp/wayland-0");
    if (bind(compositor->listen_fd, (struct sockaddr *)&address, sizeof(address)) != 0) return -1;
    if (listen(compositor->listen_fd, 16) != 0) return -1;
    return wl_display_add_socket_fd(compositor->display, compositor->listen_fd);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    struct compositor compositor;
    memset(&compositor, 0, sizeof(compositor));
    compositor.framebuffer_fd = compositor.listen_fd = -1;
    printf("[kesh-compositor] starting official libwayland server in Ring 3\n");
    if (open_framebuffer(&compositor) != 0) {
        printf("[kesh-compositor] failed to open 32-bit /dev/fb0\n");
        return 1;
    }
    compositor.display = wl_display_create();
    if (!compositor.display || wl_display_init_shm(compositor.display) != 0) {
        printf("[kesh-compositor] failed to initialize wl_display/wl_shm\n");
        return 1;
    }
    if (!wl_global_create(compositor.display, &wl_compositor_interface, 6,
                          &compositor, bind_compositor) ||
        !wl_global_create(compositor.display, &wl_output_interface, 4,
                          &compositor, bind_output) ||
        !wl_global_create(compositor.display, &xdg_wm_base_interface, 6,
                          &compositor, bind_xdg_wm)) {
        printf("[kesh-compositor] failed to publish Wayland globals\n");
        return 1;
    }
    if (create_wayland_socket(&compositor) != 0) {
        printf("[kesh-compositor] failed to publish /tmp/wayland-0 errno=%d\n", errno);
        return 1;
    }
    printf("[kesh-compositor] READY %ux%u: wl_compositor + wl_shm + wl_output + xdg_wm_base\n",
           compositor.width, compositor.height);
    printf("[kesh-compositor] listening on /tmp/wayland-0\n");
    if (kesh_demo_client_start() != 0)
        printf("[kesh-compositor] failed to start first Wayland shell client\n");
    else
        printf("[kesh-compositor] first Wayland shell client thread started\n");
    wl_display_run(compositor.display);
    wl_display_destroy_clients(compositor.display);
    wl_display_destroy(compositor.display);
    return 0;
}
