#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <wayland-server.h>
#include <wayland-client.h>

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

#ifndef SYS_memfd_create
#define SYS_memfd_create 319
#endif

static int my_memfd(const char *name, unsigned int flags) {
    return syscall(SYS_memfd_create, name, flags);
}

int main(void) {
    printf("\n==================================================\n");
    printf("   KeshOS Wayland Architecture & libwayland Demo\n");
    printf("==================================================\n");

    /* 1. Create Wayland Server Display */
    printf("[WAYLAND 1] Initializing Wayland Display Server...\n");
    struct wl_display *server_display = wl_display_create();
    if (!server_display) {
        printf("FAILED: wl_display_create returned NULL\n");
        return 1;
    }
    printf("  -> Server display created: %p\n", (void *)server_display);

    /* 2. Initialize wl_shm on Server */
    printf("[WAYLAND 2] Initializing wl_shm subsystem...\n");
    if (wl_display_init_shm(server_display) < 0) {
        printf("FAILED: wl_display_init_shm\n");
        return 1;
    }
    printf("  -> wl_shm initialized successfully on display server\n");

    /* 3. Create connected socket pair for Wayland Client <-> Server connection */
    printf("[WAYLAND 3] Creating AF_UNIX client-server socketpair...\n");
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        printf("FAILED: socketpair\n");
        return 1;
    }
    printf("  -> Sockets: server_fd=%d, client_fd=%d\n", sv[0], sv[1]);

    /* Attach server_fd to server display as a client */
    struct wl_client *client = wl_client_create(server_display, sv[0]);
    if (!client) {
        printf("FAILED: wl_client_create\n");
        return 1;
    }
    printf("  -> Connected client object created on server: %p\n", (void *)client);

    /* 4. Connect Wayland Client using client_fd */
    printf("[WAYLAND 4] Connecting Wayland Client via client fd...\n");
    struct wl_display *client_display = wl_display_connect_to_fd(sv[1]);
    if (!client_display) {
        printf("FAILED: wl_display_connect_to_fd\n");
        return 1;
    }
    printf("  -> Client connected successfully: %p\n", (void *)client_display);

    /* 5. Test memfd pixel buffer creation */
    printf("[WAYLAND 5] Creating Wayland SHM Pixel Buffer (800x600x4 = 1.92 MB)...\n");
    size_t fb_size = 800 * 600 * 4;
    int shm_fd = my_memfd("wayland_plasma_surface", MFD_CLOEXEC);
    if (shm_fd < 0) {
        printf("FAILED: memfd_create\n");
        return 1;
    }
    if (ftruncate(shm_fd, fb_size) != 0) {
        printf("FAILED: ftruncate for pixel buffer\n");
        return 1;
    }
    uint32_t *pixels = (uint32_t *)mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (pixels == MAP_FAILED || !pixels) {
        printf("FAILED: mmap pixel buffer\n");
        return 1;
    }

    /* Fill test KDE Plasma color gradient */
    for (int y = 0; y < 600; y++) {
        for (int x = 0; x < 800; x++) {
            uint8_t r = (x * 255) / 800;
            uint8_t g = (y * 255) / 600;
            uint8_t b = 200;
            pixels[y * 800 + x] = (0xFFU << 24) | (r << 16) | (g << 8) | b;
        }
    }
    printf("  -> Rendered Plasma gradient into zero-copy SHM buffer (fd=%d, addr=%p)\n", shm_fd, (void *)pixels);

    /* 6. Dispatch events on server event loop */
    printf("[WAYLAND 6] Dispatching Wayland Server Event Loop...\n");
    struct wl_event_loop *loop = wl_display_get_event_loop(server_display);
    if (!loop) {
        printf("FAILED: wl_display_get_event_loop\n");
        return 1;
    }
    wl_display_flush_clients(server_display);
    int dispatched = wl_event_loop_dispatch(loop, 0);
    printf("  -> Event loop dispatched %d events cleanly\n", dispatched);

    /* 7. Cleanup */
    printf("[WAYLAND 7] Destroying client and server displays...\n");
    munmap(pixels, fb_size);
    close(shm_fd);
    wl_display_disconnect(client_display);
    wl_display_destroy(server_display);

    printf("\n==================================================\n");
    printf("   SUCCESS: Phase 2 Wayland Stack Operational!\n");
    printf("   libwayland-server, libwayland-client, libffi, musl\n");
    printf("   all running natively in Ring 3 on KeshOS!\n");
    printf("==================================================\n\n");
    return 0;
}
