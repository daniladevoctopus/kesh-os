#ifndef KESH_WEB_H
#define KESH_WEB_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    char scheme[8];
    char host[128];
    char path[256];
    uint16_t port;
} kesh_url_t;

typedef struct {
    char tag[24];
    char text[256];
    uint8_t kind;
} kesh_html_token_t;

typedef struct {
    int status;
    char content_type[64];
    const uint8_t *body;
    uint32_t body_len;
} kesh_http_response_t;

int kesh_url_parse(const char *url, kesh_url_t *out);
int kesh_html_next(const uint8_t *html, size_t len, size_t *cursor, kesh_html_token_t *out);
int kesh_http_parse_response(uint8_t *buffer, uint32_t len, kesh_http_response_t *out);
int kesh_html_sanitize_text(const char *src, char *dst, size_t cap);

#endif
