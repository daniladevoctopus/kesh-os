// тлс шифрование пакетов
#include "kesh_tls.h"
#include "../net_stack.h"
#include "timer.h"
#include "bearssl.h"

static void serial_print(const char *str) {
    if (!str) return;
    while (*str) {
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)*str++), "Nd"((uint16_t)0x3F8));
    }
}

const br_block_ctr_class *br_aes_x86ni_ctr_get_vtable(void) { return NULL; }
br_ghash br_ghash_pclmul_get(void) { return 0; }
const br_block_ctrcbc_class *br_aes_x86ni_ctrcbc_get_vtable(void) { return NULL; }
const br_block_cbcenc_class *br_aes_x86ni_cbcenc_get_vtable(void) { return NULL; }
const br_block_cbcdec_class *br_aes_x86ni_cbcdec_get_vtable(void) { return NULL; }
br_chacha20_run br_chacha20_sse2_get(void) { return 0; }
br_prng_seeder br_prng_seeder_system(const char **name) { (void)name; return 0; }

static struct {
    uint32_t resolved_ip;
    int bytes_sent_total;
    int bytes_read_total;
    int write_calls;
    int read_calls;
    int last_bearssl_err;
    int last_xdec_err;
    int first_cert_seen;
    int cert_bytes_pushed;
    int pkey_status;
} s_tls_diag;

void kesh_tls_get_diag(uint32_t *ip, int *sent, int *recv, int *bsl_err, int *xdec_err, int *cert_bytes, int *pkey_st) {
    if (ip) *ip = s_tls_diag.resolved_ip;
    if (sent) *sent = s_tls_diag.bytes_sent_total;
    if (recv) *recv = s_tls_diag.bytes_read_total;
    if (bsl_err) *bsl_err = s_tls_diag.last_bearssl_err;
    if (xdec_err) *xdec_err = s_tls_diag.last_xdec_err;
    if (cert_bytes) *cert_bytes = s_tls_diag.cert_bytes_pushed;
    if (pkey_st) *pkey_st = s_tls_diag.pkey_status;
}

static int tls_sock_read(void *ctx, unsigned char *buf, size_t len) {
    (void)ctx;
    s_tls_diag.read_calls++;
    int r = net_tcp_recv(buf, (uint16_t)len, 5000);
    if (r <= 0) return -1;
    s_tls_diag.bytes_read_total += r;
    return r;
}

static int tls_sock_write(void *ctx, const unsigned char *buf, size_t len) {
    (void)ctx;
    s_tls_diag.write_calls++;
    size_t sent = 0;
    while (sent < len) {
        size_t chunk = len - sent;
        if (chunk > 1400) chunk = 1400;
        int w = net_tcp_send(buf + sent, (uint16_t)chunk);
        if (w <= 0) return -1;
        sent += (size_t)w;
    }
    s_tls_diag.bytes_sent_total += (int)sent;
    return (int)sent;
}

static br_x509_decoder_context s_xdec;
static int s_first_cert = 0;

static void permissive_start_chain(const br_x509_class **ctx, const char *server_name) {
    (void)ctx;
    (void)server_name;
    s_first_cert = 1;
    s_tls_diag.first_cert_seen = 1;
}

static void permissive_start_cert(const br_x509_class **ctx, uint32_t length) {
    (void)ctx;
    (void)length;
    if (s_first_cert) {
        br_x509_decoder_init(&s_xdec, 0, 0);
    }
}

static void permissive_append(const br_x509_class **ctx, const unsigned char *buf, size_t len) {
    (void)ctx;
    if (s_first_cert) {
        s_tls_diag.cert_bytes_pushed += (int)len;
        br_x509_decoder_push(&s_xdec, buf, len);
    }
}

static void permissive_end_cert(const br_x509_class **ctx) {
    (void)ctx;
    s_first_cert = 0;
}

static unsigned permissive_end_chain(const br_x509_class **ctx) {
    (void)ctx;
    return 0;
}

static const br_x509_pkey *permissive_get_pkey(const br_x509_class *const *ctx, unsigned *usages) {
    (void)ctx;
    if (usages) {
        *usages = BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN;
    }
    const br_x509_pkey *pk = br_x509_decoder_get_pkey(&s_xdec);
    s_tls_diag.last_xdec_err = br_x509_decoder_last_error(&s_xdec);
    s_tls_diag.pkey_status = pk ? 1 : 2;
    return pk;
}

static const br_x509_class s_permissive_vtable = {
    sizeof(void*),
    permissive_start_chain,
    permissive_start_cert,
    permissive_append,
    permissive_end_cert,
    permissive_end_chain,
    permissive_get_pkey
};

static const br_x509_class *s_permissive_vtable_ptr = &s_permissive_vtable;

static br_ssl_client_context s_sc;
static br_x509_minimal_context s_xc;
static br_sslio_context s_ioc;
static unsigned char s_iobuf[BR_SSL_BUFSIZE_BIDI];

static int parse_url(const char *url, int *out_is_https, char *out_host, size_t max_host,
                     uint16_t *out_port, char *out_path, size_t max_path) {
    if (!url || !out_host || !out_path) return -1;

    const char *p = url;
    int is_https = 0;
    uint16_t port = 80;

    if (p[0] == 'h' && p[1] == 't' && p[2] == 't' && p[3] == 'p') {
        if (p[4] == 's' && p[5] == ':' && p[6] == '/' && p[7] == '/') {
            is_https = 1;
            port = 443;
            p += 8;
        } else if (p[4] == ':' && p[5] == '/' && p[6] == '/') {
            is_https = 0;
            port = 80;
            p += 7;
        }
    }

    if (out_is_https) *out_is_https = is_https;

    size_t hi = 0;
    while (*p && *p != '/' && *p != ':' && hi < max_host - 1) {
        out_host[hi++] = *p++;
    }
    out_host[hi] = '\0';

    if (*p == ':') {
        p++;
        uint32_t custom_port = 0;
        while (*p >= '0' && *p <= '9') {
            custom_port = custom_port * 10 + (*p - '0');
            p++;
        }
        if (custom_port > 0 && custom_port <= 65535) port = (uint16_t)custom_port;
    }

    if (out_port) *out_port = port;

    if (*p == '\0') {
        out_path[0] = '/';
        out_path[1] = '\0';
    } else {
        size_t pi = 0;
        while (*p && pi < max_path - 1) {
            out_path[pi++] = *p++;
        }
        out_path[pi] = '\0';
    }

    return 0;
}

static int parse_http_status(const char *resp) {
    if (!resp) return 0;

    const char *p = resp;
    while (*p && *p != ' ' && *p != '\r' && *p != '\n') p++;
    if (*p == ' ') {
        p++;
        int code = 0;
        while (*p >= '0' && *p <= '9') {
            code = code * 10 + (*p - '0');
            p++;
        }
        return code;
    }
    return 0;
}

int kesh_http_get(const char *url, char *response_buf, size_t max_buf, int *out_status_code) {
    if (!url || !response_buf || max_buf < 2) return KESH_HTTP_ERR_URL;
    char host[64];
    char path[128];
    uint16_t port = 80;
    int is_https = 0;

    if (parse_url(url, &is_https, host, sizeof(host), &port, path, sizeof(path)) != 0) {
        return KESH_HTTP_ERR_URL;
    }

    serial_print("[HTTP] Resolving host: ");
    serial_print(host);
    serial_print("\n");

    uint32_t ip = 0;
    if (net_dns_resolve(host, &ip, 3000) != 0) {
        serial_print("[HTTP] DNS resolution failed\n");
        return KESH_HTTP_ERR_DNS;
    }

    serial_print("[HTTP] Connecting TCP...\n");
    if (net_tcp_connect(ip, port, 4000) != 0) {
        serial_print("[HTTP] TCP connect failed\n");
        return KESH_HTTP_ERR_CONNECT;
    }

    char req[512];
    int rlen = 0;
    const char *p1 = "GET ";
    while (*p1) req[rlen++] = *p1++;
    const char *p2 = path;
    while (*p2) req[rlen++] = *p2++;
    const char *p3 = " HTTP/1.1\r\nHost: ";
    while (*p3) req[rlen++] = *p3++;
    const char *p4 = host;
    while (*p4) req[rlen++] = *p4++;
    const char *p5 = "\r\nUser-Agent: KeshOS/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n";
    while (*p5) req[rlen++] = *p5++;
    req[rlen] = '\0';

    if (net_tcp_send(req, (uint16_t)rlen) <= 0) {
        net_tcp_close();
        return KESH_HTTP_ERR_SEND;
    }

    size_t total = 0;
    while (total < max_buf - 1) {
        int r = net_tcp_recv(response_buf + total, (uint16_t)(max_buf - 1 - total), 3000);
        if (r <= 0) break;
        total += (size_t)r;
    }
    response_buf[total] = '\0';
    net_tcp_close();

    if (out_status_code) {
        *out_status_code = parse_http_status(response_buf);
    }

    return KESH_HTTP_OK;
}

int kesh_https_get(const char *url, char *response_buf, size_t max_buf, int *out_status_code) {
    if (!url || !response_buf || max_buf < 2) return KESH_HTTP_ERR_URL;
    char host[64];
    char path[128];
    uint16_t port = 443;
    int is_https = 1;

    if (parse_url(url, &is_https, host, sizeof(host), &port, path, sizeof(path)) != 0) {
        return KESH_HTTP_ERR_URL;
    }

    uint8_t *dptr = (uint8_t*)&s_tls_diag;
    for (size_t i = 0; i < sizeof(s_tls_diag); i++) dptr[i] = 0;

    serial_print("[HTTPS] Resolving host: ");
    serial_print(host);
    serial_print("\n");

    uint32_t ip = 0;
    if (net_dns_resolve(host, &ip, 3000) != 0) {
        serial_print("[HTTPS] DNS resolution failed\n");
        return KESH_HTTP_ERR_DNS;
    }
    s_tls_diag.resolved_ip = ip;

    serial_print("[HTTPS] Connecting TCP port 443...\n");
    if (net_tcp_connect(ip, port, 5000) != 0) {
        serial_print("[HTTPS] TCP connect failed\n");
        return KESH_HTTP_ERR_CONNECT;
    }

    serial_print("[HTTPS] Initializing BearSSL TLS 1.2 engine...\n");
    br_ssl_client_init_full(&s_sc, &s_xc, NULL, 0);

    br_ssl_engine_set_x509(&s_sc.eng, &s_permissive_vtable_ptr);

    br_ssl_engine_set_buffer(&s_sc.eng, s_iobuf, sizeof(s_iobuf), 1);

    uint64_t t = timer_millis();
    uint64_t r = 0;
    __asm__ volatile ("rdtsc" : "=A"(r));
    uint8_t seed[32];
    for (int i = 0; i < 8; i++) {
        seed[i] = (uint8_t)(t >> (i * 8));
        seed[i + 8] = (uint8_t)(r >> (i * 8));
        seed[i + 16] = (uint8_t)(t ^ (r >> (i * 4)));
        seed[i + 24] = (uint8_t)(0xAA ^ (uint8_t)i);
    }
    br_ssl_engine_inject_entropy(&s_sc.eng, seed, sizeof(seed));

    if (!br_ssl_client_reset(&s_sc, host, 0)) {
        s_tls_diag.last_bearssl_err = br_ssl_engine_last_error(&s_sc.eng);
        serial_print("[HTTPS] TLS reset failed\n");
        net_tcp_close();
        return KESH_HTTP_ERR_TLS;
    }

    br_sslio_init(&s_ioc, &s_sc.eng, tls_sock_read, NULL, tls_sock_write, NULL);

    char req[512];
    int rlen = 0;
    const char *p1 = "GET ";
    while (*p1) req[rlen++] = *p1++;
    const char *p2 = path;
    while (*p2) req[rlen++] = *p2++;
    const char *p3 = " HTTP/1.1\r\nHost: ";
    while (*p3) req[rlen++] = *p3++;
    const char *p4 = host;
    while (*p4) req[rlen++] = *p4++;
    const char *p5 = "\r\nUser-Agent: KeshOS/1.0 (TLS BearSSL)\r\nAccept: */*\r\nConnection: close\r\n\r\n";
    while (*p5) req[rlen++] = *p5++;
    req[rlen] = '\0';

    serial_print("[HTTPS] Sending encrypted request over TLS...\n");
    if (br_sslio_write_all(&s_ioc, req, (size_t)rlen) < 0) {
        s_tls_diag.last_bearssl_err = br_ssl_engine_last_error(&s_sc.eng);
        s_tls_diag.last_xdec_err = br_x509_decoder_last_error(&s_xdec);
        serial_print("[HTTPS] TLS write failed\n");
        net_tcp_close();
        return KESH_HTTP_ERR_TLS;
    }
    br_sslio_flush(&s_ioc);

    serial_print("[HTTPS] Reading decrypted response...\n");
    size_t total = 0;
    while (total < max_buf - 1) {
        int res = br_sslio_read(&s_ioc, (unsigned char*)response_buf + total, max_buf - 1 - total);
        if (res <= 0) break;
        total += (size_t)res;
    }
    response_buf[total] = '\0';

    br_sslio_close(&s_ioc);
    net_tcp_close();

    if (out_status_code) {
        *out_status_code = parse_http_status(response_buf);
    }

    return KESH_HTTP_OK;
}

int kesh_fetch_url(const char *url, char *response_buf, size_t max_buf, int *out_status_code) {
    if (!url) return -1;
    if (url[0] == 'h' && url[1] == 't' && url[2] == 't' && url[3] == 'p' && url[4] == 's') {
        return kesh_https_get(url, response_buf, max_buf, out_status_code);
    }
    return kesh_http_get(url, response_buf, max_buf, out_status_code);
}

int kesh_http_fetch(const char *url, char *response_buf, size_t max_buf, int *out_status_code) {
    if (!url || !response_buf || max_buf < 2) return -1;
    int r = kesh_fetch_url(url, response_buf, max_buf, out_status_code);
    if (r != KESH_HTTP_OK) return r;
    size_t n = 0;
    while (n + 1 < max_buf && response_buf[n]) n++;
    return (int)n;
}
