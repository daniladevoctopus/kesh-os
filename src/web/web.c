#include "web.h"
#include <stddef.h>

static void copy_str(char *dst, const char *src, size_t cap) {
    size_t i = 0;
    if (!dst || !cap) return;
    while (src && src[i] && i + 1 < cap) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static size_t len_str(const char *s) { size_t n = 0; while (s && s[n]) n++; return n; }

int kesh_url_parse(const char *url, kesh_url_t *out) {
    if (!url || !out) return -1;
    out->scheme[0] = out->host[0] = out->path[0] = 0;
    out->port = 80;
    const char *sep = url;
    while (*sep && *sep != ':') sep++;
    if (*sep != ':') return -2;
    size_t scheme_len = (size_t)(sep - url);
    if (scheme_len >= sizeof(out->scheme)) return -3;
    for (size_t i = 0; i < scheme_len; i++) out->scheme[i] = url[i] >= 'A' && url[i] <= 'Z' ? (char)(url[i] + 32) : url[i];
    out->scheme[scheme_len] = 0;
    if (sep[1] != '/' || sep[2] != '/') return -4;
    const char *host = sep + 3;
    const char *p = host;
    while (*p && *p != '/' && *p != ':') p++;
    size_t host_len = (size_t)(p - host);
    if (!host_len || host_len >= sizeof(out->host)) return -5;
    for (size_t i = 0; i < host_len; i++) out->host[i] = host[i];
    out->host[host_len] = 0;
    if (*p == ':') {
        p++;
        uint32_t port = 0;
        while (*p >= '0' && *p <= '9') { port = port * 10u + (uint32_t)(*p - '0'); p++; if (port > 65535u) return -6; }
        out->port = (uint16_t)port;
    }
    if (out->port == 80 && out->scheme[0] == 'h' && out->scheme[1] == 't' && out->scheme[2] == 't' && out->scheme[3] == 'p' && out->scheme[4] == 's') out->port = 443;
    if (*p) copy_str(out->path, p, sizeof(out->path)); else copy_str(out->path, "/", sizeof(out->path));
    return 0;
}

int kesh_html_next(const uint8_t *html, size_t len, size_t *cursor, kesh_html_token_t *out) {
    if (!html || !cursor || !out || *cursor >= len) return 0;
    size_t i = *cursor;
    while (i < len && html[i] == ' ') i++;
    out->kind = 0; out->tag[0] = out->text[0] = 0;
    if (i < len && html[i] == '<') {
        i++;
        size_t j = i;
        while (j < len && html[j] != '>') j++;
        if (j == len) return 0;
        out->kind = 1;
        size_t n = 0;
        while (i < j && html[i] != ' ' && html[i] != '/' && n + 1 < sizeof(out->tag)) out->tag[n++] = (char)html[i++];
        out->tag[n] = 0;
        *cursor = j + 1;
        return 1;
    }
    size_t j = i;
    while (j < len && html[j] != '<') j++;
    size_t n = 0;
    while (i < j && n + 1 < sizeof(out->text)) out->text[n++] = (char)html[i++];
    out->text[n] = 0;
    out->kind = 2;
    *cursor = j;
    return 1;
}

static int find_header(uint8_t *buffer, uint32_t len, const char *name, char *out, size_t cap) {
    size_t nlen = len_str(name);
    for (uint32_t i = 0; i + nlen + 1 < len; i++) {
        if (i && buffer[i - 1] != '\n') continue;
        int match = 1;
        for (size_t j = 0; j < nlen; j++) if (buffer[i + j] != (uint8_t)name[j]) { match = 0; break; }
        if (!match || buffer[i + nlen] != ':') continue;
        uint32_t p = i + (uint32_t)nlen + 1u;
        while (p < len && (buffer[p] == ' ' || buffer[p] == '\t')) p++;
        size_t k = 0;
        while (p < len && buffer[p] != '\r' && buffer[p] != '\n' && k + 1 < cap) out[k++] = (char)buffer[p++];
        out[k] = 0;
        return 0;
    }
    return -1;
}

int kesh_http_parse_response(uint8_t *buffer, uint32_t len, kesh_http_response_t *out) {
    if (!buffer || !out || len < 12) return -1;
    out->status = 0; out->content_type[0] = 0; out->body = 0; out->body_len = 0;
    if (buffer[0] != 'H' || buffer[1] != 'T' || buffer[2] != 'T' || buffer[3] != 'P') return -2;
    uint32_t p = 9;
    while (p < len && buffer[p] >= '0' && buffer[p] <= '9') { out->status = out->status * 10 + (buffer[p] - '0'); p++; }
    for (uint32_t i = 0; i + 3 < len; i++) if (buffer[i] == '\r' && buffer[i+1] == '\n' && buffer[i+2] == '\r' && buffer[i+3] == '\n') { out->body = buffer + i + 4; out->body_len = len - i - 4; break; }
    find_header(buffer, len, "Content-Type", out->content_type, sizeof(out->content_type));
    return out->body ? 0 : -3;
}

int kesh_html_sanitize_text(const char *src, char *dst, size_t cap) {
    if (!src || !dst || !cap) return -1;
    size_t o = 0;
    for (size_t i = 0; src[i] && o + 1 < cap; i++) {
        char c = src[i];
        if (c == '&' && src[i+1] == 'l' && src[i+2] == 't' && src[i+3] == ';') { dst[o++] = '<'; i += 3; }
        else if (c == '&' && src[i+1] == 'g' && src[i+2] == 't' && src[i+3] == ';') { dst[o++] = '>'; i += 3; }
        else if (c == '&' && src[i+1] == 'a' && src[i+2] == 'm' && src[i+3] == 'p' && src[i+4] == ';') { dst[o++] = '&'; i += 4; }
        else if (c == '&' && src[i+1] == 'q' && src[i+2] == 'u' && src[i+3] == 'o' && src[i+4] == 't' && src[i+5] == ';') { dst[o++] = '"'; i += 5; }
        else if (c == '&' && src[i+1] == '#' && src[i+2] == '3' && src[i+3] == '9' && src[i+4] == ';') { dst[o++] = (char)39; i += 4; }
        else dst[o++] = c;
    }
    dst[o] = 0;
    return (int)o;
}
