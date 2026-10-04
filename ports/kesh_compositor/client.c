#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <unistd.h>

#include <wayland-client.h>
#include "protocol/xdg-shell-client-protocol.h"

#define CLIENT_WIDTH  720
#define CLIENT_HEIGHT 480

struct demo_client {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *wm_base;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *toplevel;
    int configured;
};

static void fill_rect(uint32_t *pixels, int x, int y, int width, int height, uint32_t color)
{
    if (x < 0 || y < 0 || width <= 0 || height <= 0) return;
    if (x + width > CLIENT_WIDTH) width = CLIENT_WIDTH - x;
    if (y + height > CLIENT_HEIGHT) height = CLIENT_HEIGHT - y;
    for (int row = y; row < y + height; ++row) {
        for (int column = x; column < x + width; ++column)
            pixels[row * CLIENT_WIDTH + column] = color;
    }
}

static void draw_shell_surface(uint32_t *pixels)
{
    for (int y = 0; y < CLIENT_HEIGHT; ++y) {
        uint32_t blue = 0x18U + (uint32_t)y * 0x18U / CLIENT_HEIGHT;
        uint32_t green = 0x12U + (uint32_t)y * 0x0cU / CLIENT_HEIGHT;
        uint32_t color = 0xff000000U | (green << 8) | blue;
        for (int x = 0; x < CLIENT_WIDTH; ++x) pixels[y * CLIENT_WIDTH + x] = color;
    }

    /* A small first userspace shell surface: top bar, dock and app cards. */
    fill_rect(pixels, 0, 0, CLIENT_WIDTH, 42, 0xff172033U);
    fill_rect(pixels, 18, 13, 16, 16, 0xff49b8f2U);
    fill_rect(pixels, 39, 13, 16, 16, 0xff58d68dU);
    fill_rect(pixels, 60, 13, 16, 16, 0xffffc857U);

    fill_rect(pixels, 40, 78, 280, 276, 0xff182235U);
    fill_rect(pixels, 41, 79, 278, 48, 0xff223149U);
    fill_rect(pixels, 64, 98, 102, 10, 0xff75c9ffU);
    fill_rect(pixels, 64, 154, 224, 15, 0xff253550U);
    fill_rect(pixels, 64, 184, 224, 15, 0xff253550U);
    fill_rect(pixels, 64, 214, 174, 15, 0xff253550U);
    fill_rect(pixels, 64, 270, 112, 42, 0xff3daee9U);

    fill_rect(pixels, 346, 78, 334, 132, 0xff182235U);
    fill_rect(pixels, 370, 102, 56, 56, 0xff3daee9U);
    fill_rect(pixels, 446, 105, 184, 13, 0xffd8e6f3U);
    fill_rect(pixels, 446, 134, 142, 10, 0xff60758eU);
    fill_rect(pixels, 446, 158, 166, 10, 0xff60758eU);

    fill_rect(pixels, 346, 228, 158, 126, 0xff182235U);
    fill_rect(pixels, 522, 228, 158, 126, 0xff182235U);
    fill_rect(pixels, 370, 252, 80, 12, 0xff58d68dU);
    fill_rect(pixels, 546, 252, 80, 12, 0xffffc857U);
    fill_rect(pixels, 370, 282, 108, 9, 0xff60758eU);
    fill_rect(pixels, 546, 282, 108, 9, 0xff60758eU);

    fill_rect(pixels, 184, 404, 352, 56, 0xdd1b2638U);
    for (int i = 0; i < 7; ++i) {
        uint32_t colors[] = {0xff3daee9U, 0xff58d68dU, 0xffffc857U, 0xffff6b81U,
                             0xff9b8afbU, 0xff57d5d9U, 0xffd8e6f3U};
        fill_rect(pixels, 208 + i * 46, 416, 32, 32, colors[i]);
    }
}

static void wm_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
    (void)data;
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener wm_listener = {
    .ping = wm_ping,
};

static void surface_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
    struct demo_client *client = data;
    xdg_surface_ack_configure(surface, serial);
    client->configured = 1;
}

static const struct xdg_surface_listener surface_listener = {
    .configure = surface_configure,
};

static void registry_global(void *data, struct wl_registry *registry, uint32_t name,
                            const char *interface, uint32_t version)
{
    struct demo_client *client = data;
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        uint32_t bind_version = version < 6 ? version : 6;
        client->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, bind_version);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        client->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        uint32_t bind_version = version < 6 ? version : 6;
        client->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, bind_version);
        xdg_wm_base_add_listener(client->wm_base, &wm_listener, client);
    }
}

static void registry_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)data; (void)registry; (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_remove,
};

static int connect_socket(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, "/tmp/wayland-0");
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void *demo_client_main(void *unused)
{
    (void)unused;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    struct demo_client client;
    memset(&client, 0, sizeof(client));

    printf("[kesh-shell] connecting through AF_UNIX /tmp/wayland-0\n");
    int socket_fd = connect_socket();
    if (socket_fd < 0) {
        printf("[kesh-shell] socket connect failed errno=%d\n", errno);
        return NULL;
    }
    client.display = wl_display_connect_to_fd(socket_fd);
    if (!client.display) {
        printf("[kesh-shell] wl_display_connect_to_fd failed errno=%d\n", errno);
        close(socket_fd);
        return NULL;
    }

    client.registry = wl_display_get_registry(client.display);
    wl_registry_add_listener(client.registry, &registry_listener, &client);
    if (wl_display_roundtrip(client.display) < 0 || !client.compositor ||
        !client.shm || !client.wm_base) {
        printf("[kesh-shell] required globals were not advertised\n");
        return NULL;
    }
    printf("[kesh-shell] registry: wl_compositor + wl_shm + xdg_wm_base\n");

    client.surface = wl_compositor_create_surface(client.compositor);
    client.xdg_surface = xdg_wm_base_get_xdg_surface(client.wm_base, client.surface);
    xdg_surface_add_listener(client.xdg_surface, &surface_listener, &client);
    client.toplevel = xdg_surface_get_toplevel(client.xdg_surface);
    xdg_toplevel_set_title(client.toplevel, "KeshOS Wayland Shell");
    xdg_toplevel_set_app_id(client.toplevel, "tech.sneakdeak.keshshell");
    wl_surface_commit(client.surface);
    if (wl_display_roundtrip(client.display) < 0 || !client.configured) {
        printf("[kesh-shell] xdg_surface configure failed\n");
        return NULL;
    }

    size_t size = (size_t)CLIENT_WIDTH * CLIENT_HEIGHT * sizeof(uint32_t);
    int shm_fd = (int)syscall(SYS_memfd_create, "kesh-shell", 1U);
    if (shm_fd < 0 || ftruncate(shm_fd, (off_t)size) != 0) {
        printf("[kesh-shell] memfd/ftruncate failed errno=%d\n", errno);
        return NULL;
    }
    uint32_t *pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (pixels == MAP_FAILED) {
        printf("[kesh-shell] mmap failed errno=%d\n", errno);
        return NULL;
    }
    draw_shell_surface(pixels);

    struct wl_shm_pool *pool = wl_shm_create_pool(client.shm, shm_fd, (int32_t)size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, CLIENT_WIDTH, CLIENT_HEIGHT,
                                                         CLIENT_WIDTH * 4, WL_SHM_FORMAT_XRGB8888);
    wl_surface_attach(client.surface, buffer, 0, 0);
    wl_surface_damage_buffer(client.surface, 0, 0, CLIENT_WIDTH, CLIENT_HEIGHT);
    wl_surface_commit(client.surface);
    if (wl_display_flush(client.display) < 0) {
        printf("[kesh-shell] surface flush failed errno=%d\n", errno);
        return NULL;
    }
    printf("[kesh-shell] COMMITTED %dx%d wl_shm xdg_toplevel surface\n",
           CLIENT_WIDTH, CLIENT_HEIGHT);

    /* Keep the real client connection alive and process compositor events. */
    while (wl_display_dispatch(client.display) >= 0) { }
    printf("[kesh-shell] display connection ended errno=%d\n", errno);
    return NULL;
}

int kesh_demo_client_start(void)
{
    pthread_t thread;
    int result = pthread_create(&thread, NULL, demo_client_main, NULL);
    if (result != 0) return -1;
    pthread_detach(thread);
    return 0;
}
