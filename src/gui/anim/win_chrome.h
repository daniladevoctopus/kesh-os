// оформление окон
#ifndef WIN_CHROME_H
#define WIN_CHROME_H

#include <stdint.h>

void win_chrome_set_focused(int win_id);
int  win_chrome_is_focused(int win_id);

int win_chrome_traffic_lights(
    int win_id,
    int win_x, int win_y, int win_w,
    int header_h,
    int mx, int my, int click, int occluded,
    int *out_minimize_clicked,
    int *out_zoom_clicked
);

int win_chrome_dock_to_win_id(int dock_index);

#endif
