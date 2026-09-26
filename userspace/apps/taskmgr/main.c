// диспетчер задач, мониторим процессы
#include "kesh.h"

#define WIN_W 580
#define WIN_H 410

#define TOP_H 85
#define BOTTOM_H 45

#define MAX_PROCS 16

static kesh_proc_info_t g_procs[MAX_PROCS];
static int g_proc_count = 0;
static int g_selected_pid = -1;
static int g_selected_row = -1;

#define CPU_HIST_LEN 32
static int g_cpu_history[CPU_HIST_LEN];
static int g_hist_head = 0;

static void int_to_str(int val, char *buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[16];
    int p = 0;
    int is_neg = 0;
    if (val < 0) {
        is_neg = 1;
        val = -val;
    }
    while (val > 0) {
        tmp[p++] = '0' + (val % 10);
        val /= 10;
    }
    int dst = 0;
    if (is_neg) buf[dst++] = '-';
    while (p > 0) {
        buf[dst++] = tmp[--p];
    }
    buf[dst] = '\0';
}

int main(void) {
    kesh_print("[TASKMGR] Starting Task Manager (Ring 3)...\n");

    uint32_t *fb = kesh_create_window(WIN_W, WIN_H, "Task Manager (Ring 3)");
    if (!fb) {
        kesh_print("[TASKMGR] Failed to create window!\n");
        return 1;
    }

    for (int i = 0; i < CPU_HIST_LEN; i++) {
        g_cpu_history[i] = 10 + (i * 3) % 25;
    }

    kesh_sysinfo_t sysinfo;
    uint32_t last_refresh = 0;
    int tick = 0;

    while (1) {
        tick++;

        if (tick - last_refresh > 15 || last_refresh == 0) {
            last_refresh = tick;
            kesh_get_sysinfo(&sysinfo);
            g_proc_count = kesh_get_procs(g_procs, MAX_PROCS);

            int simulated_cpu = 6 + (g_proc_count * 4) + ((tick * 7) % 13);
            if (simulated_cpu > 95) simulated_cpu = 95;
            g_cpu_history[g_hist_head] = simulated_cpu;
            g_hist_head = (g_hist_head + 1) % CPU_HIST_LEN;
        }

        kesh_clear(fb, WIN_W, WIN_H, 0xFF16171B);

        kesh_draw_rounded_rect_alpha(fb, WIN_W, 12, 10, WIN_W - 24, TOP_H - 14, 8, 0xFF22242B, 240);
        kesh_draw_rect(fb, WIN_W, 12, 10 + TOP_H - 14, WIN_W - 24, 1, 0xFF2E313B);

        draw_string("CPU PERFORMANCE", 24, 18, 0xFF8E8E93, fb, WIN_W);
        int current_cpu = g_cpu_history[(g_hist_head - 1 + CPU_HIST_LEN) % CPU_HIST_LEN];
        char cpu_str[16];
        int_to_str(current_cpu, cpu_str);
        draw_string(cpu_str, 24, 34, 0xFF30D158, fb, WIN_W);
        draw_string("% @ 3.2 GHz", 48, 34, 0xFFE5E5EA, fb, WIN_W);

        int chart_x = 145;
        int chart_y = 20;
        int chart_w = 110;
        int chart_h = 36;
        kesh_draw_rect(fb, WIN_W, chart_x, chart_y, chart_w, chart_h, 0xFF121316);
        for (int i = 0; i < chart_w - 2; i += 12) {
            kesh_draw_rect(fb, WIN_W, chart_x + i, chart_y, 1, chart_h, 0xFF1C1E24);
        }
        for (int i = 0; i < chart_w - 2; i += 4) {
            int hist_idx = (g_hist_head - 1 - (i / 4) + CPU_HIST_LEN * 4) % CPU_HIST_LEN;
            int val = g_cpu_history[hist_idx];
            int bar_h = (val * (chart_h - 4)) / 100;
            if (bar_h < 2) bar_h = 2;
            int bx = chart_x + chart_w - 4 - i;
            int by = chart_y + chart_h - 2 - bar_h;
            kesh_draw_rect(fb, WIN_W, bx, by, 3, bar_h, 0xFF30D158);
        }

        int ram_x = 280;
        draw_string("MEMORY ALLOCATION", ram_x, 18, 0xFF8E8E93, fb, WIN_W);
        uint32_t total_mb = (uint32_t)(sysinfo.total_ram_bytes / (1024 * 1024));
        uint32_t used_mb = (uint32_t)((sysinfo.total_ram_bytes - sysinfo.free_ram_bytes) / (1024 * 1024));
        if (total_mb == 0) { total_mb = 256; used_mb = 38; }

        char u_str[16], t_str[16];
        int_to_str((int)used_mb, u_str);
        int_to_str((int)total_mb, t_str);
        draw_string(u_str, ram_x, 34, 0xFF0A84FF, fb, WIN_W);
        draw_string("MB / ", ram_x + 24, 34, 0xFF8E8E93, fb, WIN_W);
        draw_string(t_str, ram_x + 56, 34, 0xFFE5E5EA, fb, WIN_W);
        draw_string("MB", ram_x + 84, 34, 0xFF8E8E93, fb, WIN_W);

        int bar_w = 120;
        int bar_filled = (total_mb > 0) ? ((used_mb * bar_w) / total_mb) : 20;
        if (bar_filled > bar_w) bar_filled = bar_w;
        kesh_draw_rounded_rect(fb, WIN_W, ram_x, 50, bar_w, 8, 4, 0xFF2A2C36);
        if (bar_filled > 0) {
            kesh_draw_rounded_rect(fb, WIN_W, ram_x, 50, bar_filled, 8, 4, 0xFF0A84FF);
        }

        int meta_x = 430;
        draw_string("SYSTEM STATUS", meta_x, 18, 0xFF8E8E93, fb, WIN_W);
        draw_string("Active Tasks: ", meta_x, 34, 0xFF8E8E93, fb, WIN_W);
        char tasks_str[16];
        int_to_str(g_proc_count, tasks_str);
        draw_string(tasks_str, meta_x + 85, 34, 0xFF30D158, fb, WIN_W);

        draw_string("Uptime: ", meta_x, 48, 0xFF8E8E93, fb, WIN_W);
        char up_str[16];
        int_to_str((int)(sysinfo.uptime_ms / 1000), up_str);
        draw_string(up_str, meta_x + 50, 48, 0xFFE5E5EA, fb, WIN_W);
        draw_string("s", meta_x + 75, 48, 0xFF8E8E93, fb, WIN_W);

        int table_y = TOP_H + 8;
        int table_h = WIN_H - TOP_H - BOTTOM_H - 16;
        kesh_draw_rounded_rect_alpha(fb, WIN_W, 12, table_y, WIN_W - 24, table_h, 6, 0xFF1C1D24, 250);

        kesh_draw_rect(fb, WIN_W, 12, table_y, WIN_W - 24, 24, 0xFF252731);
        draw_string("PID", 24, table_y + 6, 0xFF8E8E93, fb, WIN_W);
        draw_string("PROCESS NAME", 70, table_y + 6, 0xFF8E8E93, fb, WIN_W);
        draw_string("STATUS", 320, table_y + 6, 0xFF8E8E93, fb, WIN_W);
        draw_string("MEMORY", 460, table_y + 6, 0xFF8E8E93, fb, WIN_W);
        kesh_draw_rect(fb, WIN_W, 12, table_y + 24, WIN_W - 24, 1, 0xFF2E313C);

        int row_y = table_y + 26;
        int row_h = 24;
        for (int i = 0; i < g_proc_count && i < 9; i++) {
            int is_selected = (g_selected_row == i);

            if (is_selected) {
                kesh_draw_rect(fb, WIN_W, 13, row_y, WIN_W - 26, row_h - 1, 0xFF0A4480);
            } else if (i % 2 == 1) {
                kesh_draw_rect(fb, WIN_W, 13, row_y, WIN_W - 26, row_h - 1, 0xFF20222B);
            }

            char pid_str[16];
            int_to_str(g_procs[i].pid, pid_str);
            draw_string(pid_str, 24, row_y + 5, 0xFF0A84FF, fb, WIN_W);

            draw_string(g_procs[i].name, 70, row_y + 5, 0xFFFFFFFF, fb, WIN_W);

            if (g_procs[i].state == 2) {
                draw_string("RUNNING", 320, row_y + 5, 0xFF30D158, fb, WIN_W);
            } else if (g_procs[i].state == 1) {
                draw_string("READY", 320, row_y + 5, 0xFF64D2FF, fb, WIN_W);
            } else {
                draw_string("EXITED", 320, row_y + 5, 0xFF8E8E93, fb, WIN_W);
            }

            char mem_str[16];
            int_to_str((int)(g_procs[i].memory_bytes / 1024), mem_str);
            draw_string(mem_str, 460, row_y + 5, 0xFFE5E5EA, fb, WIN_W);
            draw_string("KB", 495, row_y + 5, 0xFF8E8E93, fb, WIN_W);

            row_y += row_h;
        }

        int bot_y = WIN_H - BOTTOM_H;
        kesh_draw_rect(fb, WIN_W, 0, bot_y, WIN_W, BOTTOM_H, 0xFF191A20);
        kesh_draw_rect(fb, WIN_W, 0, bot_y, WIN_W, 1, 0xFF2A2C36);

        if (g_selected_pid >= 0) {
            draw_string("Target Selected: PID ", 20, bot_y + 16, 0xFF8E8E93, fb, WIN_W);
            char sel_str[16];
            int_to_str(g_selected_pid, sel_str);
            draw_string(sel_str, 160, bot_y + 16, 0xFFFF453A, fb, WIN_W);
        } else {
            draw_string("Click a process row to select and manage", 20, bot_y + 16, 0xFF636366, fb, WIN_W);
        }

        int end_btn_x = WIN_W - 145;
        int end_btn_y = bot_y + 8;
        int end_btn_w = 130;
        int end_btn_h = 28;
        uint32_t btn_col = (g_selected_pid >= 0) ? 0xFFFF453A : 0xFF48484A;
        kesh_draw_rounded_rect(fb, WIN_W, end_btn_x, end_btn_y, end_btn_w, end_btn_h, 6, btn_col);
        draw_string("End Process", end_btn_x + 22, end_btn_y + 7, 0xFFFFFFFF, fb, WIN_W);

        kesh_update_window(0);

        kesh_event_t ev;
        while (kesh_poll_event(0, &ev)) {
            if (ev.type == EVENT_CLOSE) {
                kesh_print("[TASKMGR] Close requested. Exiting...\n");
                kesh_exit(0);
            } else if (ev.type == EVENT_MOUSE_DOWN) {

                if (ev.mx >= 12 && ev.mx <= WIN_W - 12 && ev.my >= table_y + 26 && ev.my <= table_y + 26 + (9 * row_h)) {
                    int clicked_row = (ev.my - (table_y + 26)) / row_h;
                    if (clicked_row >= 0 && clicked_row < g_proc_count) {
                        g_selected_row = clicked_row;
                        g_selected_pid = g_procs[clicked_row].pid;
                    }
                }

                if (ev.mx >= end_btn_x && ev.mx <= end_btn_x + end_btn_w &&
                    ev.my >= end_btn_y && ev.my <= end_btn_y + end_btn_h) {
                    if (g_selected_pid >= 0) {
                        kesh_print("[TASKMGR] Terminating target process...\n");
                        kesh_kill(g_selected_pid);
                        g_selected_pid = -1;
                        g_selected_row = -1;

                        g_proc_count = kesh_get_procs(g_procs, MAX_PROCS);
                    }
                }
            }
        }

        kesh_yield();
    }

    return 0;
}
