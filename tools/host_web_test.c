#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../src/web/web.h"

int main(void) {
    kesh_url_t u;
    char out[64];
    const char *html = "<html><body><h1>KeshOS</h1><p>Hello</p></body></html>";
    uint8_t buf[160];
    size_t cursor = 0;
    kesh_html_token_t tok;
    int seen = 0;
    kesh_http_response_t r;
    const char *response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 5\r\n\r\nhello";
    size_t response_len = strlen(response);

    if (kesh_url_parse("https://example.com:8443/path?q=1", &u) != 0) return 1;
    if (u.port != 8443) return 2;
    if (strcmp(u.scheme, "https") != 0 || strcmp(u.host, "example.com") != 0) return 3;
    if (strcmp(u.path, "/path?q=1") != 0) return 4;
    if (!kesh_html_sanitize_text("a&amp;b &lt;c&gt; &#39; &quot;", out, sizeof(out))) return 5;
    if (strcmp(out, "a&b <c> ' \"") != 0) return 6;
    while (kesh_html_next((const uint8_t *)html, strlen(html), &cursor, &tok)) {
        if (tok.kind == 2 && (strcmp(tok.text, "KeshOS") == 0 || strcmp(tok.text, "Hello") == 0)) seen++;
    }
    if (seen != 2) return 7;
    memcpy(buf, response, response_len);
    if (kesh_http_parse_response(buf, (uint32_t)response_len, &r) != 0) return 8;
    if (r.status != 200 || r.body_len != 5) return 9;
    if (memcmp(r.body, "hello", 5) != 0) return 10;
    printf("host_web_test: ok\n");
    return 0;
}
