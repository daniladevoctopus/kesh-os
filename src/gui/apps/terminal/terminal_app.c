// встроенный терминал
#include "terminal_app.h"
#include "gui/desktop.h"
#include "gui/font.h"
#include "gui/anim/genie_anim.h"
#include "gui/anim/win_chrome.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

extern void draw_rounded_rect_buf(int x, int y, int w, int h, int r, uint32_t color);
extern void draw_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha);
extern void draw_rect_buf(int x, int y, int w, int h, uint32_t color);

#define TERM_ROWS      14
#define TERM_LINE_MAX  80
#define TERM_INPUT_MAX 64
#define TERM_DOCK_INDEX 1

static int is_open = 0;
static int minimized = 0;
static int win_x = 0, win_y = 0;
static int win_w = 560, win_h = 360;
static int positioned = 0;
static int dragging = 0;
static int drag_ox = 0, drag_oy = 0;
static genie_state_t genie;

static char lines[TERM_ROWS][TERM_LINE_MAX];
static int line_count = 0;
static char input[TERM_INPUT_MAX];
static uint32_t input_len = 0;
static int booted = 0;

static uint32_t t_strlen(const char *s) { uint32_t n = 0; while (s[n]) n++; return n; }

static void t_strcpy(char *dst, const char *src, uint32_t max) {
    uint32_t i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void t_strcat(char *dst, const char *src, uint32_t max) {
    uint32_t d = t_strlen(dst);
    uint32_t s = 0;
    while (src[s] && d + s < max - 1) {
        dst[d + s] = src[s];
        s++;
    }
    dst[d + s] = 0;
}

static int t_streq(const char *a, const char *b) {
    uint32_t i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static void t_split_first_word(const char *line, char *cmd, uint32_t cmd_max,
                                char *rest, uint32_t rest_max) {
    uint32_t i = 0, ci = 0;
    while (line[i] == ' ') i++;
    while (line[i] && line[i] != ' ' && ci < cmd_max - 1) cmd[ci++] = line[i++];
    cmd[ci] = 0;
    while (line[i] == ' ') i++;
    uint32_t ri = 0;
    while (line[i] && ri < rest_max - 1) rest[ri++] = line[i++];
    rest[ri] = 0;
}

static void term_push_line(const char *line) {
    if (line_count < TERM_ROWS) {
        t_strcpy(lines[line_count], line, TERM_LINE_MAX);
        line_count++;
    } else {
        for (int i = 1; i < TERM_ROWS; i++) t_strcpy(lines[i - 1], lines[i], TERM_LINE_MAX);
        t_strcpy(lines[TERM_ROWS - 1], line, TERM_LINE_MAX);
    }
}

typedef struct {
    int active;
    uint32_t target;
    char target_name[48];
    int is_dns;
    int cur_seq;
    int max_seq;
    int received;
    uint64_t send_time;
    int state; 
} ping_job_t;

static ping_job_t g_ping_job = {0};

static void terminal_ping_tick(void) {
    if (!g_ping_job.active) return;
    extern int net_ping_send(uint32_t target_ip, uint16_t seq);
    extern int net_ping_check_reply(uint16_t seq);
    extern bool net_dns_check_reply(uint32_t *out_ip);
    extern void net_ip_to_str(uint32_t ip, char *buf);
    extern uint64_t timer_millis(void);

    if (g_ping_job.is_dns) {
        uint32_t resolved_ip = 0;
        if (net_dns_check_reply(&resolved_ip)) {
            g_ping_job.target = resolved_ip;
            g_ping_job.is_dns = 0;
            char ip_str[32];
            net_ip_to_str(resolved_ip, ip_str);

            char msg[80];
            t_strcpy(msg, "PING ", 80);
            t_strcat(msg, g_ping_job.target_name, 80);
            t_strcat(msg, " (", 80);
            t_strcat(msg, ip_str, 80);
            t_strcat(msg, "): 32 data bytes", 80);
            term_push_line(msg);
            g_ping_job.state = 0;
        } else if (timer_millis() - g_ping_job.send_time >= 3000) {
            term_push_line("Ping: cannot resolve host (DNS timeout)");
            g_ping_job.active = 0;
        }
        return;
    }

    if (g_ping_job.state == 0) {
        g_ping_job.cur_seq++;
        int err = net_ping_send(g_ping_job.target, (uint16_t)g_ping_job.cur_seq);
        if (err != 0) {
            term_push_line("Ping error: Network interface not ready.");
            g_ping_job.active = 0;
            return;
        }
        g_ping_job.send_time = timer_millis();
        g_ping_job.state = 1;
    } else if (g_ping_job.state == 1) {
        if (net_ping_check_reply((uint16_t)g_ping_job.cur_seq)) {
            int rtt = (int)(timer_millis() - g_ping_job.send_time);
            g_ping_job.received++;
            char reply_msg[80];
            char ip_str[32];
            net_ip_to_str(g_ping_job.target, ip_str);

            t_strcpy(reply_msg, "64 bytes from ", 80);
            t_strcat(reply_msg, ip_str, 80);
            t_strcat(reply_msg, ": icmp_seq=", 80);
            char seq_str[4];
            seq_str[0] = '0' + g_ping_job.cur_seq; seq_str[1] = '\0';
            t_strcat(reply_msg, seq_str, 80);
            t_strcat(reply_msg, " time=", 80);

            char rtt_str[12];
            int ri = 0;
            if (rtt >= 100) rtt_str[ri++] = '0' + ((rtt / 100) % 10);
            if (rtt >= 10) rtt_str[ri++] = '0' + ((rtt / 10) % 10);
            rtt_str[ri++] = '0' + (rtt % 10);
            rtt_str[ri++] = 'm'; rtt_str[ri++] = 's'; rtt_str[ri] = '\0';
            t_strcat(reply_msg, rtt_str, 80);
            term_push_line(reply_msg);

            if (g_ping_job.cur_seq >= g_ping_job.max_seq) {
                char stat[80];
                t_strcpy(stat, "--- ping statistics: 3 sent, 3 received, 0% loss ---", 80);
                term_push_line(stat);
                g_ping_job.active = 0;
            } else {
                g_ping_job.state = 0;
            }
        } else if (timer_millis() - g_ping_job.send_time >= 1000) {
            term_push_line("Request timed out.");
            if (g_ping_job.cur_seq >= g_ping_job.max_seq) {
                term_push_line("--- ping finished: packet loss detected ---");
                g_ping_job.active = 0;
            } else {
                g_ping_job.state = 0;
            }
        }
    }
}

static void term_exec(const char *cmdline) {
    char prompt_line[TERM_LINE_MAX];
    t_strcpy(prompt_line, "~ $ ", TERM_LINE_MAX);
    uint32_t base = t_strlen(prompt_line);
    uint32_t i = 0;
    while (cmdline[i] && base + i < TERM_LINE_MAX - 1) { prompt_line[base + i] = cmdline[i]; i++; }
    prompt_line[base + i] = 0;
    term_push_line(prompt_line);

    char cmd[16], rest[TERM_INPUT_MAX];
    t_split_first_word(cmdline, cmd, sizeof(cmd), rest, sizeof(rest));

    if (t_strlen(cmd) == 0) {
        return;
    } else if (t_streq(cmd, "help")) {
        term_push_line("COMMANDS: HELP KPM PING NSLOOKUP CURL IFCONFIG NOTEPAD EXPLORER PANIC CLEAR");
    } else if (t_streq(cmd, "kpm")) {
        extern void kpm_cmd_exec(const char *args, void (*print_fn)(const char *line));
        kpm_cmd_exec(rest, term_push_line);
    } else if (t_streq(cmd, "clear")) {
        line_count = 0;
    } else if (t_streq(cmd, "pwd")) {
        term_push_line("/");
    } else if (t_streq(cmd, "whoami")) {
        term_push_line("root@keshos-drop");
    } else if (t_streq(cmd, "echo")) {
        term_push_line(rest);
    } else if (t_streq(cmd, "date")) {
        term_push_line("KESHOS DROP DOES NOT TRACK REAL TIME YET");
    } else if (t_streq(cmd, "notepad") || t_streq(cmd, "ring3")) {
        extern void toggle_notepad_user_app(void);
        toggle_notepad_user_app();
        term_push_line("LAUNCHED RING 3 PROCESS: NOTEPAD.ELF");
    } else if (t_streq(cmd, "explorer") || t_streq(cmd, "files")) {
        extern void toggle_explorer_user_app(void);
        toggle_explorer_user_app();
        term_push_line("LAUNCHED RING 3 PROCESS: EXPLORER.ELF");
    } else if (t_streq(cmd, "ping")) {
        extern uint32_t net_str_to_ip(const char *str);
        extern uint32_t net_make_ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
        extern int net_dns_send(const char *domain);
        extern uint64_t timer_millis(void);

        if (t_strlen(rest) == 0) {
            t_strcpy(rest, "10.0.2.2", sizeof(rest));
        }

        uint32_t direct_ip = net_str_to_ip(rest);
        t_strcpy(g_ping_job.target_name, rest, sizeof(g_ping_job.target_name));
        g_ping_job.cur_seq = 0;
        g_ping_job.max_seq = 3;
        g_ping_job.received = 0;

        if (direct_ip != 0) {
            g_ping_job.target = direct_ip;
            g_ping_job.is_dns = 0;
            g_ping_job.state = 0;
            char msg[80];
            t_strcpy(msg, "PING ", 80);
            t_strcat(msg, rest, 80);
            t_strcat(msg, ": 32 bytes of ICMP data...", 80);
            term_push_line(msg);
        } else {
            g_ping_job.target = 0;
            g_ping_job.is_dns = 1;
            g_ping_job.send_time = timer_millis();
            char msg[80];
            t_strcpy(msg, "DNS: Resolving ", 80);
            t_strcat(msg, rest, 80);
            t_strcat(msg, " via 10.0.2.3:53...", 80);
            term_push_line(msg);
            net_dns_send(rest);
        }
        g_ping_job.active = 1;
    } else if (t_streq(cmd, "nslookup") || t_streq(cmd, "dns")) {
        if (t_strlen(rest) == 0) {
            term_push_line("Usage: nslookup <domain>");
            return;
        }
        extern int net_dns_resolve(const char *domain, uint32_t *out_ip, uint32_t timeout_ms);
        extern void net_ip_to_str(uint32_t ip, char *buf);

        term_push_line("Server:   10.0.2.3");
        term_push_line("Address:  10.0.2.3#53");

        uint32_t ip = 0;
        int err = net_dns_resolve(rest, &ip, 2500);
        if (err == 0) {
            char ip_str[32];
            net_ip_to_str(ip, ip_str);
            char line1[80], line2[80];
            t_strcpy(line1, "Name:     ", 80);
            t_strcat(line1, rest, 80);
            term_push_line(line1);

            t_strcpy(line2, "Address:  ", 80);
            t_strcat(line2, ip_str, 80);
            term_push_line(line2);
        } else {
            char err_line[80];
            t_strcpy(err_line, "** server can't find ", 80);
            t_strcat(err_line, rest, 80);
            t_strcat(err_line, ": NXDOMAIN", 80);
            term_push_line(err_line);
        }
    } else if (t_streq(cmd, "curl") || t_streq(cmd, "http") || t_streq(cmd, "https")) {
        if (t_strlen(rest) == 0) {
            term_push_line("Usage: curl <http:// or https:// url>");
            return;
        }
        extern int kesh_fetch_url(const char *url, char *response_buf, size_t max_buf, int *out_status_code);

        char info[80];
        t_strcpy(info, "Connecting to ", 80);
        t_strcat(info, rest, 80);
        t_strcat(info, "...", 80);
        term_push_line(info);

        static char http_resp[2048];
        int status = 0;
        int res = kesh_fetch_url(rest, http_resp, sizeof(http_resp), &status);
        if (res == 0) {
            char status_line[80];
            t_strcpy(status_line, "< HTTP Response Status: ", 80);
            char scode[8];
            int s = status;
            scode[0] = '0' + ((s / 100) % 10);
            scode[1] = '0' + ((s / 10) % 10);
            scode[2] = '0' + (s % 10);
            scode[3] = '\0';
            t_strcat(status_line, scode, 80);
            term_push_line(status_line);

            int p = 0;
            for (int l = 0; l < 4 && http_resp[p]; l++) {
                char one_line[80];
                int oli = 0;
                while (http_resp[p] && http_resp[p] != '\r' && http_resp[p] != '\n' && oli < 78) {
                    one_line[oli++] = http_resp[p++];
                }
                one_line[oli] = '\0';
                while (http_resp[p] == '\r' || http_resp[p] == '\n') p++;
                if (oli > 0) term_push_line(one_line);
            }
        } else {
            char err_line[80];
            t_strcpy(err_line, "curl: Connection or TLS handshake failed (err=", 80);
            int code = -res;
            char num[4] = {'0' + code, ')', '\0'};
            t_strcat(err_line, num, 80);
            term_push_line(err_line);
        }
    } else if (t_streq(cmd, "kea") || t_streq(cmd, "kea-info")) {
        term_push_line("[KEA] Format: Kesh Executable Application");
        term_push_line("Specs: 64-bit ELF container + RGBA Icon + Metadata");
        term_push_line("Available .kea packages in /apps/:");
        term_push_line("  - /apps/notepad.kea (42 KB, Text Editor)");
        term_push_line("  - /apps/explorer.kea (47 KB, File Manager)");
    } else if (t_streq(cmd, "ifconfig") || t_streq(cmd, "ip")) {
        extern const uint8_t* net_get_my_mac(void);
        extern uint32_t net_get_my_ip(void);
        extern void net_ip_to_str(uint32_t ip, char *buf);

        const uint8_t *mac = net_get_my_mac();
        char ip_str[32];
        net_ip_to_str(net_get_my_ip(), ip_str);

        char mac_line[64];
        t_strcpy(mac_line, "eth0: Intel 82540EM e1000 Gigabit NIC", 64);
        term_push_line(mac_line);

        const char hex[] = "0123456789ABCDEF";
        char ether_line[64];
        t_strcpy(ether_line, "      ether ", 64);
        int ep = t_strlen(ether_line);
        for (int i = 0; i < 6; i++) {
            ether_line[ep++] = hex[(mac[i] >> 4) & 0xF];
            ether_line[ep++] = hex[mac[i] & 0xF];
            if (i < 5) ether_line[ep++] = ':';
        }
        ether_line[ep] = '\0';
        term_push_line(ether_line);

        char inet_line[64];
        t_strcpy(inet_line, "      inet ", 64);
        t_strcat(inet_line, ip_str, 64);
        t_strcat(inet_line, "  netmask 255.255.255.0", 64);
        term_push_line(inet_line);
    } else if (t_streq(cmd, "panic") || t_streq(cmd, "crash")) {
        extern void kernel_panic(const char *msg);
        kernel_panic("Kernel panic requested via Terminal command");
    } else {
        char line[TERM_LINE_MAX];
        t_strcpy(line, "UNKNOWN COMMAND: ", TERM_LINE_MAX);
        uint32_t b2 = t_strlen(line);
        uint32_t j = 0;
        while (cmd[j] && b2 + j < TERM_LINE_MAX - 1) { line[b2 + j] = cmd[j]; j++; }
        line[b2 + j] = 0;
        term_push_line(line);
    }
}

void toggle_terminal_app(void)
{
    int dx, dy;
    genie_dock_icon_point(TERM_DOCK_INDEX, &dx, &dy);

    if (genie_is_animating(&genie))
        genie_cancel(&genie);

    if (is_open && minimized) {
        genie_start_open(&genie, dx, dy);
        minimized = 0;
        dragging = 0;
        return;
    }

    if (is_open) {
        genie_start_close(&genie, dx, dy);
        is_open = 0;
        minimized = 0;
        dragging = 0;
        return;
    }

    is_open = 1;
    minimized = 0;
    dragging = 0;
    if (!booted) {
        term_push_line("KeshOS Terminal (x86_64)");
        term_push_line("Type 'help' for a list of built-in commands.");
        term_push_line("");
        booted = 1;
    }
    genie_start_open(&genie, dx, dy);
}

void terminal_app_feed_key(char key)
{
    if (!is_open) return;

    if (key == '\r' || key == '\n') {
        input[input_len] = 0;
        char echo[TERM_LINE_MAX];
        t_strcpy(echo, "root@keshos:~$ ", sizeof(echo));
        t_strcat(echo, input, sizeof(echo));
        term_push_line(echo);
        term_exec(input);
        input_len = 0;
        input[0] = 0;
    } else if (key == 8 ) {
        if (input_len > 0) input[--input_len] = 0;
    } else if (key >= 32 && key < 127) {
        if (input_len < TERM_INPUT_MAX - 1) {
            input[input_len++] = key;
            input[input_len] = 0;
        }
    }
}

void render_terminal_app_window(
    uint32_t* buf,
    int scr_w,
    int scr_h,
    int mx,
    int my,
    int btn,
    int click
) {
    genie_tick(&genie);

    terminal_ping_tick();

    if (!is_open && !genie_is_animating(&genie))
        return;

    if (minimized && !genie_is_animating(&genie))
        return;

    if (!positioned) {
        win_x = (scr_w - win_w) / 2 - 140;
        win_y = (scr_h - win_h) / 2 - 40;
        positioned = 1;
    }

    int mid_genie = genie_is_animating(&genie);

    int header_h = 34;
    int traffic_zone_w = 84;

    extern int g_window_controls_align;
    int drag_start_x = (g_window_controls_align == 1) ? win_x : (win_x + traffic_zone_w);
    int drag_end_x   = (g_window_controls_align == 1) ? (win_x + win_w - traffic_zone_w) : (win_x + win_w);

    if (!mid_genie && btn && !dragging && win_drag_available() &&
        mx >= drag_start_x && mx <= drag_end_x &&
        my >= win_y && my <= win_y + header_h)
    {
        dragging = 1;
        drag_ox = mx - win_x;
        drag_oy = my - win_y;
        win_drag_claim();
        win_set_focused(WIN_ID_TERMINAL_);
    }
    if (!btn) dragging = 0;
    if (dragging) { win_x = mx - drag_ox; win_y = my - drag_oy; }

    win_report_rect(WIN_ID_TERMINAL_, win_x, win_y, win_w, win_h, is_open && !mid_genie);
    int occluded = win_click_occluded(WIN_ID_TERMINAL_, mx, my);

    int draw_x, draw_y, draw_w, draw_h;
    genie_get_rect(&genie, win_x, win_y, win_w, win_h, &draw_x, &draw_y, &draw_w, &draw_h);

    int tl_size = 12, tl_gap = 8;
    int tl_y = win_y + (header_h - tl_size) / 2;
    int close_x, minimize_x, zoom_x;
    if (g_window_controls_align == 1) {
        close_x = win_x + win_w - 14 - tl_size;
        minimize_x = close_x - tl_gap - tl_size;
        zoom_x = minimize_x - tl_gap - tl_size;
    } else {
        close_x = win_x + 14;
        minimize_x = close_x + tl_size + tl_gap;
        zoom_x = minimize_x + tl_size + tl_gap;
    }
    int hit_padding = 8;
    int hover_close = !mid_genie &&
        mx >= (close_x - hit_padding) && mx <= (close_x + tl_size + hit_padding) &&
        my >= (tl_y - hit_padding) && my <= (tl_y + tl_size + hit_padding);
    int hover_minimize = !mid_genie &&
        mx >= (minimize_x - hit_padding) && mx <= (minimize_x + tl_size + hit_padding) &&
        my >= (tl_y - hit_padding) && my <= (tl_y + tl_size + hit_padding);
    int hover_zoom = !mid_genie &&
        mx >= (zoom_x - hit_padding) && mx <= (zoom_x + tl_size + hit_padding) &&
        my >= (tl_y - hit_padding) && my <= (tl_y + tl_size + hit_padding);

    static const struct { int spread; int drop; uint8_t alpha; } shadows[] = {
        {14, 18, 8}, {11, 15, 12}, {8, 12, 16}, {5, 9, 22}, {2, 6, 30},
    };
    for (unsigned i = 0; i < sizeof(shadows) / sizeof(shadows[0]); i++) {
        int sp = shadows[i].spread;
        draw_rounded_rect_alpha(
            draw_x - sp / 2, draw_y - sp / 2 + shadows[i].drop,
            draw_w + sp, draw_h + sp, 18 + sp / 2, 0x00000000, shadows[i].alpha
        );
    }

    draw_rounded_rect_buf(draw_x, draw_y, draw_w, draw_h, 16, 0x0014161B);

    if (mid_genie) return; 

    draw_rounded_rect_buf(win_x, win_y, win_w, header_h, 16, 0x001E2028);
    draw_rect_buf(win_x, win_y + header_h / 2, win_w, header_h / 2, 0x001E2028);
    draw_rect_buf(win_x, win_y + header_h, win_w, 1, 0x002B2E38);

    const char *title_str = "Terminal (bash)";
    int tw = font_text_width(title_str);
    int tx = win_x + (win_w - tw) / 2;
    draw_rounded_rect_alpha(tx - 12, win_y + 6, tw + 24, 20, 4, 0x00FFFFFF, 12);
    draw_string(title_str, tx, win_y + 10, 0x00C7CBD6, buf, (uint32_t)scr_w);

    draw_rounded_rect_buf(zoom_x, tl_y, tl_size, tl_size, 4,
        hover_zoom ? 0x0028C93F : 0x003A3A3C);
    draw_rounded_rect_buf(minimize_x, tl_y, tl_size, tl_size, 4,
        hover_minimize ? 0x00FFBD2E : 0x003A3A3C);
    draw_rounded_rect_buf(close_x, tl_y, tl_size, tl_size, 4,
        hover_close ? 0x00FF5F57 : 0x003A3A3C);

    if (click && hover_close && !occluded) {
        int dx, dy;
        genie_dock_icon_point(TERM_DOCK_INDEX, &dx, &dy);
        genie_start_close(&genie, dx, dy);
        is_open = 0;
        dragging = 0;
        return;
    }

    if (click && hover_minimize && !occluded) {
        int dx, dy;
        genie_dock_icon_point(TERM_DOCK_INDEX, &dx, &dy);
        genie_start_minimize(&genie, dx, dy);
        minimized = 1;
        dragging = 0;
        return;
    }

    if (click && hover_zoom && !occluded) {
        static int pre_x, pre_y, pre_w, pre_h, is_zoomed = 0;
        if (!is_zoomed) {
            pre_x = win_x; pre_y = win_y; pre_w = win_w; pre_h = win_h;
            win_x = 40; win_y = 44; win_w = scr_w - 80; win_h = scr_h - 84;
            is_zoomed = 1;
        } else {
            win_x = pre_x; win_y = pre_y; win_w = pre_w; win_h = pre_h;
            is_zoomed = 0;
        }
        return;
    }

    int content_x = win_x + 16;
    int content_y = win_y + header_h + 12;
    int line_h = 18;

    for (int i = 0; i < line_count; i++) {
        const char *l = lines[i];
        if (l[0] == 'r' && l[1] == 'o' && l[2] == 'o' && l[3] == 't' && l[4] == '@') {
            int p1_w = font_text_width("root@keshos:");
            draw_string("root@keshos:", content_x, content_y + i * line_h, 0x0030D158, buf, (uint32_t)scr_w);
            int p2_w = font_text_width("~$ ");
            draw_string("~$ ", content_x + p1_w, content_y + i * line_h, 0x0064D2FF, buf, (uint32_t)scr_w);
            draw_string(l + 15, content_x + p1_w + p2_w, content_y + i * line_h, 0x00F2F2F7, buf, (uint32_t)scr_w);
        } else {
            uint32_t color = 0x00E5E7EB;
            if (l[0] == 'U' && l[1] == 'N') color = 0x00FF453A;
            else if (l[0] == 'P' && l[1] == 'I' && l[2] == 'N') color = 0x0064D2FF;
            else if (l[0] == '[' && l[1] == 'K') color = 0x00FFD60A;
            draw_string(l, content_x, content_y + i * line_h, color, buf, (uint32_t)scr_w);
        }
    }

    int prompt_y = content_y + line_count * line_h;
    int p1_w = font_text_width("root@keshos:");
    draw_string("root@keshos:", content_x, prompt_y, 0x0030D158, buf, (uint32_t)scr_w);
    int p2_w = font_text_width("~$ ");
    draw_string("~$ ", content_x + p1_w, prompt_y, 0x0064D2FF, buf, (uint32_t)scr_w);
    draw_string(input, content_x + p1_w + p2_w, prompt_y, 0x00F2F2F7, buf, (uint32_t)scr_w);

    extern uint64_t timer_millis(void);
    if ((timer_millis() / 500) % 2 == 0) {
        int caret_x = content_x + p1_w + p2_w + font_text_width(input) + 2;
        draw_rounded_rect_buf(caret_x, prompt_y + 1, 7, 13, 2, 0x000A84FF);
    }
}
