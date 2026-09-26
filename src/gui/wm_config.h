// конфиг оконного менеджера
#ifndef WM_CONFIG_H
#define WM_CONFIG_H

#define WINDOW_CONTROLS_LEFT  0
#define WINDOW_CONTROLS_RIGHT 1

#ifdef __cplusplus
extern "C" {
#endif

extern int g_window_controls_align;

void wm_config_init(void);
void wm_config_set_align(int align);
int  wm_config_get_align(void);

#ifdef __cplusplus
}
#endif

#endif
