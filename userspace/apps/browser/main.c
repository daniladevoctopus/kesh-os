#include "kesh.h"

#define WIN_W 820
#define WIN_H 520
#define URL_CAP 256
#define RESP_CAP 32768
#define PAGE_CAP 24576
#define MAX_LINES 34

static char g_url[URL_CAP] = "https://example.com/";
static int g_url_len = 20;
static char g_response[RESP_CAP];
static char g_page[PAGE_CAP];
static int g_page_len = 0;
static int g_status = 0;
static int g_loaded = 0;
static int g_loading = 0;
static int g_scroll = 0;

static void append_page_char(char c) {
    if (g_page_len + 1 < PAGE_CAP) g_page[g_page_len++] = c;
}

static void append_page_text(const char *s) {
    while (*s) append_page_char(*s++);
}

static void decode_entity(const char *s, int *used) {
    if (!s || !used) return;
    *used = 0;
    if (s[0] == '&' && s[1] == 'a' && s[2] == 'm' && s[3] == 'p' && s[4] == ';') { append_page_char('&'); *used = 5; return; }
    if (s[0] == '&' && s[1] == 'l' && s[2] == 't' && s[3] == ';') { append_page_char('<'); *used = 4; return; }
    if (s[0] == '&' && s[1] == 'g' && s[2] == 't' && s[3] == ';') { append_page_char('>'); *used = 4; return; }
    if (s[0] == '&' && s[1] == 'q' && s[2] == 'u' && s[3] == 'o' && s[4] == 't' && s[5] == ';') { append_page_char('"'); *used = 6; return; }
    if (s[0] == '&' && s[1] == '#' && s[2] == '3' && s[3] == '9' && s[4] == ';') { append_page_char((char)39); *used = 5; return; }
}

static int tag_is(const char *start, const char *name) {
    int i = 0;
    while (name[i]) {
        char c = start[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if (c != name[i]) return 0;
        i++;
    }
    return start[i] == '>' || start[i] == ' ' || start[i] == '/';
}

static void make_page_text(const char *body, int len) {
    g_page_len = 0;
    int in_tag = 0;
    int in_script = 0;
    int line_has_text = 0;
    for (int i = 0; i < len && g_page_len + 1 < PAGE_CAP; i++) {
        char c = body[i];
        if (!in_tag && c == '<') {
            if (i + 7 < len && tag_is(body + i + 1, "script")) in_script = 1;
            else if (i + 8 < len && tag_is(body + i + 1, "/script")) in_script = 0;
            if ((i + 3 < len && tag_is(body + i + 1, "br")) || (i + 4 < len && tag_is(body + i + 1, "p")) || (i + 5 < len && tag_is(body + i + 1, "/p")) || (i + 7 < len && tag_is(body + i + 1, "/h1")) || (i + 7 < len && tag_is(body + i + 1, "/h2")) || (i + 5 < len && tag_is(body + i + 1, "li")) || (i + 6 < len && tag_is(body + i + 1, "/li"))) {
                if (line_has_text) append_page_char('\n');
                line_has_text = 0;
            }
            in_tag = 1;
            continue;
        }
        if (in_tag) {
            if (c == '>') in_tag = 0;
            continue;
        }
        if (in_script) continue;
        if (c == '&') {
            int used = 0;
            decode_entity(body + i, &used);
            if (used) { i += used - 1; line_has_text = 1; continue; }
        }
        if (c == '\r') continue;
        if (c == '\n' || c == '\t') c = ' ';
        append_page_char(c);
        if (c != ' ') line_has_text = 1;
    }
    while (g_page_len > 0 && g_page[g_page_len - 1] == ' ') g_page_len--;
    g_page[g_page_len] = '\0';
}

static int find_body(const char *response, int len, const char **body) {
    for (int i = 0; i + 3 < len; i++) {
        if (response[i] == '\r' && response[i + 1] == '\n' && response[i + 2] == '\r' && response[i + 3] == '\n') {
            *body = response + i + 4;
            return len - i - 4;
        }
    }
    return -1;
}

static void load_page(void) {
    if (g_url_len == 0) {
        const char *prefix = "https://";
        int n = 0;
        while (prefix[n] && n < URL_CAP - 1) { g_url[n] = prefix[n]; n++; }
        g_url[n] = '\0';
        g_url_len = n;
    }
    if (!(g_url[0] == 'h' && g_url[1] == 't' && g_url[2] == 't' && g_url[3] == 'p' && (g_url[4] == ':' || g_url[4] == 's'))) {
        if (g_url_len + 8 < URL_CAP) {
            for (int i = g_url_len; i >= 0; i--) g_url[i + 8] = g_url[i];
            const char *prefix = "https://";
            for (int i = 0; i < 8; i++) g_url[i] = prefix[i];
            g_url_len += 8;
        }
    }
    g_loading = 1;
    g_loaded = 0;
    g_status = 0;
    g_scroll = 0;
    g_page_len = 0;
    int n = kesh_net_fetch(g_url, g_response, RESP_CAP - 1);
    if (n < 0) {
        append_page_text("Network request failed.\n\nThe address may be unreachable, DNS may be unavailable, or TLS may have failed.");
        g_loading = 0;
        return;
    }
    int body_len = 0;
    const char *body = NULL;
    for (int i = 0; i + 1 < n; i++) {
        if (g_response[i] >= '0' && g_response[i] <= '9' && g_response[i - 1 < 0 ? 0 : i - 1] == ' ') {
            int j = i;
            g_status = 0;
            while (j < n && g_response[j] >= '0' && g_response[j] <= '9') { g_status = g_status * 10 + g_response[j] - '0'; j++; }
            break;
        }
    }
    body_len = find_body(g_response, n, &body);
    if (body_len < 0) {
        body = g_response;
        body_len = n;
    }
    if (body_len > PAGE_CAP - 1) body_len = PAGE_CAP - 1;
    make_page_text(body, body_len);
    if (g_page_len == 0) append_page_text("(empty response)");
    g_loaded = 1;
    g_loading = 0;
}

static void draw_page(uint32_t *fb) {
    int y = 78 - g_scroll;
    int line = 0;
    int start = 0;
    while (start < g_page_len && line < 120) {
        int end = start;
        int width = 0;
        while (end < g_page_len && g_page[end] != '\n' && width < 94) { end++; width++; }
        int draw_end = end;
        if (draw_end > start && y >= 70 && y < WIN_H - 8) {
            char saved = g_page[draw_end];
            g_page[draw_end] = '\0';
            draw_string(g_page + start, 18, y, 0xFFE5E5EA, fb, WIN_W);
            g_page[draw_end] = saved;
        }
        if (end == g_page_len) break;
        if (g_page[end] == '\n') end++;
        else {
            while (end < g_page_len && g_page[end] != '\n') end++;
            if (end < g_page_len) end++;
        }
        start = end;
        y += 14;
        line++;
    }
}

int main(void) {
    uint32_t *fb = kesh_create_window(WIN_W, WIN_H, "Kesh Browser");
    if (!fb) return 1;
    kesh_net_info_t net = {0};
    kesh_net_info(&net);
    while (1) {
        kesh_clear(fb, WIN_W, WIN_H, 0xFF111318);
        kesh_draw_rounded_rect(fb, WIN_W, 10, 10, WIN_W - 20, 44, 10, 0xFF1F232C);
        kesh_draw_rounded_rect(fb, WIN_W, 20, 19, 54, 26, 7, 0xFF2D333D);
        draw_string("WEB", 29, 27, 0xFF64D2FF, fb, WIN_W);
        kesh_draw_rounded_rect(fb, WIN_W, 84, 17, WIN_W - 176, 30, 7, 0xFF0F1115);
        draw_string(g_url, 96, 27, 0xFFFFFFFF, fb, WIN_W);
        kesh_draw_rounded_rect(fb, WIN_W, WIN_W - 82, 17, 58, 30, 7, 0xFF0A84FF);
        draw_string("GO", WIN_W - 65, 27, 0xFFFFFFFF, fb, WIN_W);
        kesh_draw_rect(fb, WIN_W, 10, 62, WIN_W - 20, 1, 0xFF2A2F38);
        if (g_loading) {
            draw_string("Loading...", 18, 68, 0xFF64D2FF, fb, WIN_W);
        } else if (!g_loaded) {
            draw_string("Enter a URL and press Enter", 18, 68, 0xFF8E8E93, fb, WIN_W);
        } else {
            char status[48];
            int n = 0;
            const char *a = "HTTP ";
            while (*a) status[n++] = *a++;
            status[n++] = (char)('0' + (g_status / 100) % 10);
            status[n++] = (char)('0' + (g_status / 10) % 10);
            status[n++] = (char)('0' + g_status % 10);
            const char *b = "  |  ";
            while (*b) status[n++] = *b++;
            a = net.link_up ? "online" : "offline";
            while (*a && n < 47) status[n++] = *a++;
            status[n] = '\0';
            draw_string(status, 18, 68, 0xFF30D158, fb, WIN_W);
        }
        draw_page(fb);
        kesh_update_window(0);
        kesh_event_t ev;
        while (kesh_poll_event(0, &ev)) {
            if (ev.type == EVENT_CLOSE) kesh_exit(0);
            if (ev.type == EVENT_KEY_DOWN) {
                if (ev.key == '\n' || ev.key == '\r') {
                    load_page();
                } else if (ev.key == '\b' || ev.key == 127) {
                    if (g_url_len > 0) g_url[--g_url_len] = '\0';
                } else if (ev.key >= 32 && ev.key <= 126) {
                    if (g_url_len < URL_CAP - 1) {
                        g_url[g_url_len++] = (char)ev.key;
                        g_url[g_url_len] = '\0';
                    }
                }
            } else if (ev.type == EVENT_MOUSE_DOWN) {
                if (ev.btn == 4 && g_scroll > 0) g_scroll -= 42;
                else if (ev.btn == 5) g_scroll += 42;
                if (ev.mx >= WIN_W - 82 && ev.my >= 17 && ev.my < 47) load_page();
            }
        }
        kesh_sleep(16);
    }
    return 0;
}
