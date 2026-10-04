#include "kesh.h"

#define WIN_W 640
#define WIN_H 420
#define COPY_BUFFER_SIZE (128 * 1024)

static uint8_t copy_buffer[COPY_BUFFER_SIZE];

static void button(uint32_t *fb, int x, int y, int w, int h, const char *label, uint32_t color) {
    kesh_draw_rounded_rect(fb, WIN_W, x, y, w, h, 10, color);
    int tx = x + (w - font_text_width(label)) / 2;
    draw_string(label, tx, y + h / 2 - 5, 0xFFFFFFFF, fb, WIN_W);
}

/* The live image must never format a disk as a side effect of opening the
 * installer.  This is the first persistent step: it records an explicit
 * FAT32 installation plan which later installer stages can consume. */
static int save_fat32_plan(void) {
    static const char plan[] =
        "KeshOS installation plan\n"
        "filesystem=FAT32\n"
        "source=live-image\n"
        "state=prepared\n"
        "\n"
        "No partition table, filesystem or boot sector was changed.\n";

    kesh_vfs_mkdir("/hdd/KESHOS"); /* Existing directory is fine. */
    return kesh_vfs_write("/hdd/KESHOS/INSTALL.TXT", plan, (int)(sizeof(plan) - 1));
}

static int copy_live_apps(void) {
    static const char *sources[] = {
        "/apps/notepad.kea", "/apps/explorer.kea", "/apps/taskmgr.kea",
        "/apps/paint.kea", "/apps/shell.kea", "/apps/settings.kea",
        "/apps/about.kea", "/apps/browser.kea", "/apps/installer.kea"
    };
    static const char *targets[] = {
        "/hdd/KESHOS/APPS/NOTEPAD.KEA", "/hdd/KESHOS/APPS/EXPLORER.KEA", "/hdd/KESHOS/APPS/TASKMGR.KEA",
        "/hdd/KESHOS/APPS/PAINT.KEA", "/hdd/KESHOS/APPS/SHELL.KEA", "/hdd/KESHOS/APPS/SETTINGS.KEA",
        "/hdd/KESHOS/APPS/ABOUT.KEA", "/hdd/KESHOS/APPS/BROWSER.KEA", "/hdd/KESHOS/APPS/INSTALL.KEA"
    };
    kesh_vfs_mkdir("/hdd/KESHOS");
    kesh_vfs_mkdir("/hdd/KESHOS/APPS");
    for (int i = 0; i < (int)(sizeof(sources) / sizeof(sources[0])); ++i) {
        kesh_vfs_stat_t stat;
        if (kesh_vfs_stat(sources[i], &stat) != 0 || stat.is_dir || stat.size == 0 || stat.size > COPY_BUFFER_SIZE) return -1;
        int bytes = kesh_vfs_read(sources[i], copy_buffer, (int)stat.size);
        if (bytes != (int)stat.size || kesh_vfs_write(targets[i], copy_buffer, bytes) != bytes) return -1;
    }
    return 0;
}

int main(void) {
    uint32_t *fb = kesh_create_window(WIN_W, WIN_H, "Install KeshOS");
    if (!fb) return 1;
    int page = 0;
    int filesystem = 0;
    int plan_status = 0;
    int copy_status = 0;
    while (1) {
        kesh_event_t event;
        while (kesh_poll_event(0, &event)) {
            if (event.type == EVENT_CLOSE) { kesh_exit(0); return 0; }
            if (event.type == EVENT_MOUSE_UP) {
                if (page == 0 && event.mx >= 390 && event.mx < 590 && event.my >= 326 && event.my < 372) page = 1;
                else if (page == 1 && event.mx >= 390 && event.mx < 590 && event.my >= 326 && event.my < 372) page = 2;
                else if (page == 1 && event.mx >= 40 && event.mx < 330 && event.my >= 252 && event.my < 300) filesystem = 0;
                else if (page == 1 && event.mx >= 40 && event.mx < 330 && event.my >= 304 && event.my < 352) filesystem = 1;
                else if (page == 1 && event.mx >= 40 && event.mx < 330 && event.my >= 356 && event.my < 404) filesystem = 2;
                else if (page == 2 && event.mx >= 390 && event.mx < 590 && event.my >= 326 && event.my < 372) {
                    if (filesystem == 0) plan_status = save_fat32_plan() >= 0 ? 1 : -1;
                    else plan_status = -2;
                }
                else if (page == 2 && plan_status == 1 && event.mx >= 390 && event.mx < 590 && event.my >= 278 && event.my < 318) page = 3;
                else if (page == 3 && event.mx >= 390 && event.mx < 590 && event.my >= 326 && event.my < 372) copy_status = copy_live_apps() == 0 ? 1 : -1;
                else if ((page == 2 || page == 3) && event.mx >= 390 && event.mx < 590 && event.my >= 374 && event.my < 410) {
                    kesh_exit(0);
                    return 0;
                }
            }
        }
        kesh_clear(fb, WIN_W, WIN_H, 0xFF101827);
        kesh_draw_rect(fb, WIN_W, 0, 0, WIN_W, 92, 0xFF2463EB);
        draw_string("KeshOS", 38, 26, 0xFFFFFFFF, fb, WIN_W);
        draw_string("Live installer", 38, 52, 0xFFDCE8FF, fb, WIN_W);
        if (page == 0) {
            draw_string("Welcome to KeshOS", 40, 132, 0xFFFFFFFF, fb, WIN_W);
            draw_string("You are running the live image. No disk is changed", 40, 168, 0xFFB7C4D9, fb, WIN_W);
            draw_string("until you confirm installation in a later step.", 40, 190, 0xFFB7C4D9, fb, WIN_W);
            draw_string("1  Disk and partition check", 54, 250, 0xFF9EB8FF, fb, WIN_W);
            draw_string("2  Copy KeshOS system files", 54, 278, 0xFF9EB8FF, fb, WIN_W);
            button(fb, 390, 326, 200, 46, "Check disk", 0xFF2563EB);
        } else if (page == 1) {
            kesh_sysinfo_t info;
            kesh_get_sysinfo(&info);
            draw_string("Installation preflight", 40, 132, 0xFFFFFFFF, fb, WIN_W);
            draw_string("Detected persistent storage is available to KeshOS.", 40, 170, 0xFFB7C4D9, fb, WIN_W);
            draw_string("Choose the target filesystem", 40, 210, 0xFFFFFFFF, fb, WIN_W);
            button(fb, 40, 252, 290, 40, "FAT32  - supported live storage", filesystem == 0 ? 0xFF2563EB : 0xFF334155);
            button(fb, 40, 304, 290, 40, "ext4   - driver in progress", filesystem == 1 ? 0xFF7C3AED : 0xFF334155);
            button(fb, 40, 356, 290, 40, "Btrfs  - driver in progress", filesystem == 2 ? 0xFF7C3AED : 0xFF334155);
            button(fb, 390, 326, 200, 46, "Review plan", 0xFF2563EB);
        } else if (page == 2) {
            draw_string("Review installation plan", 40, 132, 0xFFFFFFFF, fb, WIN_W);
            draw_string(filesystem == 0 ? "Selected filesystem: FAT32" : filesystem == 1 ? "Selected filesystem: ext4" : "Selected filesystem: Btrfs", 40, 170, 0xFFB7C4D9, fb, WIN_W);
            if (filesystem == 0) {
                draw_string("Save a persistent plan to /hdd/KESHOS/INSTALL.TXT.", 40, 202, 0xFF93C5FD, fb, WIN_W);
                draw_string("It does not format a disk or overwrite boot data.", 40, 224, 0xFFB7C4D9, fb, WIN_W);
                if (plan_status == 1) draw_string("Plan saved. It is ready for the copy and boot stages.", 40, 260, 0xFF86EFAC, fb, WIN_W);
                if (plan_status == -1) draw_string("Could not write plan. Check that /hdd is FAT32 and writable.", 40, 260, 0xFFFFA0A0, fb, WIN_W);
                button(fb, 390, 326, 200, 46, "Save plan", 0xFF2563EB);
                if (plan_status == 1) button(fb, 390, 278, 200, 36, "Copy live apps", 0xFF2563EB);
            } else {
                draw_string("This choice needs a real formatter and mount driver.", 40, 202, 0xFFFFCC80, fb, WIN_W);
                draw_string("No disk changes are available for it in this build.", 40, 224, 0xFFB7C4D9, fb, WIN_W);
                button(fb, 390, 326, 200, 46, "Unavailable", 0xFF475569);
            }
            button(fb, 390, 374, 200, 36, "Close", 0xFF3B82F6);
        } else {
            draw_string("Copy live applications", 40, 132, 0xFFFFFFFF, fb, WIN_W);
            draw_string("Nine KeshOS KEA packages will be copied to FAT32.", 40, 170, 0xFFB7C4D9, fb, WIN_W);
            draw_string("Destination: /hdd/KESHOS/APPS", 40, 194, 0xFF93C5FD, fb, WIN_W);
            draw_string("Bootloader and partition data are not changed.", 40, 218, 0xFFFFCC80, fb, WIN_W);
            if (copy_status == 1) draw_string("Live applications copied successfully.", 40, 260, 0xFF86EFAC, fb, WIN_W);
            if (copy_status == -1) draw_string("Copy failed. Verify a writable FAT32 disk and free space.", 40, 260, 0xFFFFA0A0, fb, WIN_W);
            button(fb, 390, 326, 200, 46, "Copy live apps", 0xFF2563EB);
            button(fb, 390, 374, 200, 36, "Close", 0xFF3B82F6);
        }
        kesh_update_window(0);
        kesh_sleep(20);
    }
}
