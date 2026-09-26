// читаем конфиг оконного менеджера
#include "wm_config.h"
#include "vfs.h"

int g_window_controls_align = WINDOW_CONTROLS_LEFT;

void wm_config_init(void) {
    char buf[512];
    int n = vfs_read("/wm.conf", buf, sizeof(buf) - 1);
    if (n <= 0) {
        n = vfs_read("/system/wm.conf", buf, sizeof(buf) - 1);
    }
    if (n > 0) {
        buf[n] = '\0';
        for (int i = 0; i < n - 4; i++) {
            if ((buf[i] == 'R' || buf[i] == 'r') &&
                (buf[i+1] == 'I' || buf[i+1] == 'i') &&
                (buf[i+2] == 'G' || buf[i+2] == 'g') &&
                (buf[i+3] == 'H' || buf[i+3] == 'h') &&
                (buf[i+4] == 'T' || buf[i+4] == 't')) {
                g_window_controls_align = WINDOW_CONTROLS_RIGHT;
                return;
            }
            if ((buf[i] == 'L' || buf[i] == 'l') &&
                (buf[i+1] == 'E' || buf[i+1] == 'e') &&
                (buf[i+2] == 'F' || buf[i+2] == 'f') &&
                (buf[i+3] == 'T' || buf[i+3] == 't')) {
                g_window_controls_align = WINDOW_CONTROLS_LEFT;
                return;
            }
        }
    }
}

void wm_config_set_align(int align) {
    if (align == WINDOW_CONTROLS_LEFT || align == WINDOW_CONTROLS_RIGHT) {
        g_window_controls_align = align;
    }
}

int wm_config_get_align(void) {
    return g_window_controls_align;
}
