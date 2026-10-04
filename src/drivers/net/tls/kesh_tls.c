// тлс шифрование пакетов
#include "kesh_tls.h"
#include "../net_stack.h"
#include "timer.h"
#include "random.h"
#include "bearssl.h"
#include "../../../../kernel/vfs.h"
#include "../../../../kpm/trusted_keys.h"

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

static const unsigned char s_isrg_root_x1_dn[] = {
    0x30, 0x4F, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
    0x02, 0x55, 0x53, 0x31, 0x29, 0x30, 0x27, 0x06, 0x03, 0x55, 0x04, 0x0A,
    0x13, 0x20, 0x49, 0x6E, 0x74, 0x65, 0x72, 0x6E, 0x65, 0x74, 0x20, 0x53,
    0x65, 0x63, 0x75, 0x72, 0x69, 0x74, 0x79, 0x20, 0x52, 0x65, 0x73, 0x65,
    0x61, 0x72, 0x63, 0x68, 0x20, 0x47, 0x72, 0x6F, 0x75, 0x70, 0x31, 0x15,
    0x30, 0x13, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0C, 0x49, 0x53, 0x52,
    0x47, 0x20, 0x52, 0x6F, 0x6F, 0x74, 0x20, 0x58, 0x31
};

static const unsigned char s_isrg_root_x1_n[] = {
    0xAD, 0xE8, 0x24, 0x73, 0xF4, 0x14, 0x37, 0xF3, 0x9B, 0x9E, 0x2B, 0x57,
    0x28, 0x1C, 0x87, 0xBE, 0xDC, 0xB7, 0xDF, 0x38, 0x90, 0x8C, 0x6E, 0x3C,
    0xE6, 0x57, 0xA0, 0x78, 0xF7, 0x75, 0xC2, 0xA2, 0xFE, 0xF5, 0x6A, 0x6E,
    0xF6, 0x00, 0x4F, 0x28, 0xDB, 0xDE, 0x68, 0x86, 0x6C, 0x44, 0x93, 0xB6,
    0xB1, 0x63, 0xFD, 0x14, 0x12, 0x6B, 0xBF, 0x1F, 0xD2, 0xEA, 0x31, 0x9B,
    0x21, 0x7E, 0xD1, 0x33, 0x3C, 0xBA, 0x48, 0xF5, 0xDD, 0x79, 0xDF, 0xB3,
    0xB8, 0xFF, 0x12, 0xF1, 0x21, 0x9A, 0x4B, 0xC1, 0x8A, 0x86, 0x71, 0x69,
    0x4A, 0x66, 0x66, 0x6C, 0x8F, 0x7E, 0x3C, 0x70, 0xBF, 0xAD, 0x29, 0x22,
    0x06, 0xF3, 0xE4, 0xC0, 0xE6, 0x80, 0xAE, 0xE2, 0x4B, 0x8F, 0xB7, 0x99,
    0x7E, 0x94, 0x03, 0x9F, 0xD3, 0x47, 0x97, 0x7C, 0x99, 0x48, 0x23, 0x53,
    0xE8, 0x38, 0xAE, 0x4F, 0x0A, 0x6F, 0x83, 0x2E, 0xD1, 0x49, 0x57, 0x8C,
    0x80, 0x74, 0xB6, 0xDA, 0x2F, 0xD0, 0x38, 0x8D, 0x7B, 0x03, 0x70, 0x21,
    0x1B, 0x75, 0xF2, 0x30, 0x3C, 0xFA, 0x8F, 0xAE, 0xDD, 0xDA, 0x63, 0xAB,
    0xEB, 0x16, 0x4F, 0xC2, 0x8E, 0x11, 0x4B, 0x7E, 0xCF, 0x0B, 0xE8, 0xFF,
    0xB5, 0x77, 0x2E, 0xF4, 0xB2, 0x7B, 0x4A, 0xE0, 0x4C, 0x12, 0x25, 0x0C,
    0x70, 0x8D, 0x03, 0x29, 0xA0, 0xE1, 0x53, 0x24, 0xEC, 0x13, 0xD9, 0xEE,
    0x19, 0xBF, 0x10, 0xB3, 0x4A, 0x8C, 0x3F, 0x89, 0xA3, 0x61, 0x51, 0xDE,
    0xAC, 0x87, 0x07, 0x94, 0xF4, 0x63, 0x71, 0xEC, 0x2E, 0xE2, 0x6F, 0x5B,
    0x98, 0x81, 0xE1, 0x89, 0x5C, 0x34, 0x79, 0x6C, 0x76, 0xEF, 0x3B, 0x90,
    0x62, 0x79, 0xE6, 0xDB, 0xA4, 0x9A, 0x2F, 0x26, 0xC5, 0xD0, 0x10, 0xE1,
    0x0E, 0xDE, 0xD9, 0x10, 0x8E, 0x16, 0xFB, 0xB7, 0xF7, 0xA8, 0xF7, 0xC7,
    0xE5, 0x02, 0x07, 0x98, 0x8F, 0x36, 0x08, 0x95, 0xE7, 0xE2, 0x37, 0x96,
    0x0D, 0x36, 0x75, 0x9E, 0xFB, 0x0E, 0x72, 0xB1, 0x1D, 0x9B, 0xBC, 0x03,
    0xF9, 0x49, 0x05, 0xD8, 0x81, 0xDD, 0x05, 0xB4, 0x2A, 0xD6, 0x41, 0xE9,
    0xAC, 0x01, 0x76, 0x95, 0x0A, 0x0F, 0xD8, 0xDF, 0xD5, 0xBD, 0x12, 0x1F,
    0x35, 0x2F, 0x28, 0x17, 0x6C, 0xD2, 0x98, 0xC1, 0xA8, 0x09, 0x64, 0x77,
    0x6E, 0x47, 0x37, 0xBA, 0xCE, 0xAC, 0x59, 0x5E, 0x68, 0x9D, 0x7F, 0x72,
    0xD6, 0x89, 0xC5, 0x06, 0x41, 0x29, 0x3E, 0x59, 0x3E, 0xDD, 0x26, 0xF5,
    0x24, 0xC9, 0x11, 0xA7, 0x5A, 0xA3, 0x4C, 0x40, 0x1F, 0x46, 0xA1, 0x99,
    0xB5, 0xA7, 0x3A, 0x51, 0x6E, 0x86, 0x3B, 0x9E, 0x7D, 0x72, 0xA7, 0x12,
    0x05, 0x78, 0x59, 0xED, 0x3E, 0x51, 0x78, 0x15, 0x0B, 0x03, 0x8F, 0x8D,
    0xD0, 0x2F, 0x05, 0xB2, 0x3E, 0x7B, 0x4A, 0x1C, 0x4B, 0x73, 0x05, 0x12,
    0xFC, 0xC6, 0xEA, 0xE0, 0x50, 0x13, 0x7C, 0x43, 0x93, 0x74, 0xB3, 0xCA,
    0x74, 0xE7, 0x8E, 0x1F, 0x01, 0x08, 0xD0, 0x30, 0xD4, 0x5B, 0x71, 0x36,
    0xB4, 0x07, 0xBA, 0xC1, 0x30, 0x30, 0x5C, 0x48, 0xB7, 0x82, 0x3B, 0x98,
    0xA6, 0x7D, 0x60, 0x8A, 0xA2, 0xA3, 0x29, 0x82, 0xCC, 0xBA, 0xBD, 0x83,
    0x04, 0x1B, 0xA2, 0x83, 0x03, 0x41, 0xA1, 0xD6, 0x05, 0xF1, 0x1B, 0xC2,
    0xB6, 0xF0, 0xA8, 0x7C, 0x86, 0x3B, 0x46, 0xA8, 0x48, 0x2A, 0x88, 0xDC,
    0x76, 0x9A, 0x76, 0xBF, 0x1F, 0x6A, 0xA5, 0x3D, 0x19, 0x8F, 0xEB, 0x38,
    0xF3, 0x64, 0xDE, 0xC8, 0x2B, 0x0D, 0x0A, 0x28, 0xFF, 0xF7, 0xDB, 0xE2,
    0x15, 0x42, 0xD4, 0x22, 0xD0, 0x27, 0x5D, 0xE1, 0x79, 0xFE, 0x18, 0xE7,
    0x70, 0x88, 0xAD, 0x4E, 0xE6, 0xD9, 0x8B, 0x3A, 0xC6, 0xDD, 0x27, 0x51,
    0x6E, 0xFF, 0xBC, 0x64, 0xF5, 0x33, 0x43, 0x4F
};

static const unsigned char s_isrg_root_x1_e[] = { 0x01, 0x00, 0x01 };

static const br_x509_trust_anchor s_builtin_trust_anchors[] = {
    {
        { (unsigned char *)s_isrg_root_x1_dn, sizeof(s_isrg_root_x1_dn) },
        BR_X509_TA_CA,
        {
            BR_KEYTYPE_RSA,
            { .rsa = {
                (unsigned char *)s_isrg_root_x1_n, sizeof(s_isrg_root_x1_n),
                (unsigned char *)s_isrg_root_x1_e, sizeof(s_isrg_root_x1_e)
            } }
        }
    }
};

#define KESH_CA_STORE_MAGIC 0x5341434BU
#define KESH_CA_STORE_MAX 1024

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t generation;
    uint16_t dn_size;
    uint16_t modulus_size;
    uint16_t exponent_size;
    uint16_t flags;
    uint8_t key_id[16];
    uint8_t digest[32];
    uint8_t signature[64];
} kesh_ca_store_header_t;

static unsigned char s_update_dn[256];
static unsigned char s_update_modulus[512];
static unsigned char s_update_exponent[8];
static br_x509_trust_anchor s_active_trust_anchors[2];
static size_t s_active_trust_anchor_count;
static uint32_t s_trust_store_generation;
static uint8_t s_trust_store_loaded;

static int secure_equal(const uint8_t *a, const uint8_t *b, size_t size) {
    uint8_t difference = 0;
    for (size_t i = 0; i < size; ++i) difference |= a[i] ^ b[i];
    return difference == 0;
}

static const kpm_trusted_key_t *trusted_key_find(const uint8_t key_id[16]) {
    for (size_t i = 0; i < g_kpm_trusted_key_count; ++i)
        if (secure_equal(key_id, g_kpm_trusted_keys[i].id, 16)) return &g_kpm_trusted_keys[i];
    return NULL;
}

int kesh_tls_reload_trust_store(void) {
    uint8_t image[KESH_CA_STORE_MAX];
    if (!s_active_trust_anchor_count) {
        s_active_trust_anchors[0] = s_builtin_trust_anchors[0];
        s_active_trust_anchor_count = 1;
    }
    s_trust_store_loaded = 1;
    int size = vfs_read("/hdd/etc/CASTORE.BIN", image, sizeof(image));
    if (size < (int)sizeof(kesh_ca_store_header_t)) return 0;
    const kesh_ca_store_header_t *header = (const kesh_ca_store_header_t *)image;
    uint32_t payload_size = (uint32_t)header->dn_size + header->modulus_size + header->exponent_size;
    if (header->magic != KESH_CA_STORE_MAGIC || header->version != 1 || header->header_size != sizeof(*header) ||
        !header->generation || header->generation < s_trust_store_generation ||
        !header->dn_size || header->dn_size > sizeof(s_update_dn) ||
        !header->modulus_size || header->modulus_size > sizeof(s_update_modulus) ||
        !header->exponent_size || header->exponent_size > sizeof(s_update_exponent) || header->flags != BR_X509_TA_CA ||
        payload_size > KESH_CA_STORE_MAX - sizeof(*header) || size != (int)(sizeof(*header) + payload_size)) return -1;
    const kpm_trusted_key_t *trusted = trusted_key_find(header->key_id);
    if (!trusted) return -1;
    br_sha256_context sha;
    uint8_t digest[32], zeros[96] = {0};
    size_t digest_offset = offsetof(kesh_ca_store_header_t, digest);
    br_sha256_init(&sha);
    br_sha256_update(&sha, image, digest_offset);
    br_sha256_update(&sha, zeros, sizeof(zeros));
    br_sha256_update(&sha, image + sizeof(*header), payload_size);
    br_sha256_out(&sha, digest);
    if (!secure_equal(digest, header->digest, sizeof(digest))) return -1;
    br_ec_public_key public_key = { BR_EC_secp256r1, (unsigned char *)trusted->public_key, sizeof(trusted->public_key) };
    if (!br_ecdsa_i31_vrfy_raw(&br_ec_all_m15, digest, sizeof(digest), &public_key,
                               header->signature, sizeof(header->signature))) return -1;
    const uint8_t *payload = image + sizeof(*header);
    for (uint16_t i = 0; i < header->dn_size; ++i) s_update_dn[i] = payload[i];
    payload += header->dn_size;
    for (uint16_t i = 0; i < header->modulus_size; ++i) s_update_modulus[i] = payload[i];
    payload += header->modulus_size;
    for (uint16_t i = 0; i < header->exponent_size; ++i) s_update_exponent[i] = payload[i];
    s_active_trust_anchors[1].dn.data = s_update_dn;
    s_active_trust_anchors[1].dn.len = header->dn_size;
    s_active_trust_anchors[1].flags = BR_X509_TA_CA;
    s_active_trust_anchors[1].pkey.key_type = BR_KEYTYPE_RSA;
    s_active_trust_anchors[1].pkey.key.rsa.n = s_update_modulus;
    s_active_trust_anchors[1].pkey.key.rsa.nlen = header->modulus_size;
    s_active_trust_anchors[1].pkey.key.rsa.e = s_update_exponent;
    s_active_trust_anchors[1].pkey.key.rsa.elen = header->exponent_size;
    s_active_trust_anchor_count = 2;
    s_trust_store_generation = header->generation;
    return 1;
}

uint32_t kesh_tls_trust_store_generation(void) { return s_trust_store_generation; }

static br_ssl_client_context s_sc;
static br_x509_minimal_context s_xc;
static br_sslio_context s_ioc;
static size_t s_tls_last_response_len;

size_t kesh_tls_last_response_len(void) { return s_tls_last_response_len; }
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
    if (is_https || !host[0]) return KESH_HTTP_ERR_URL;

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
    s_tls_last_response_len = 0;
    if (!url || !response_buf || max_buf < 2) return KESH_HTTP_ERR_URL;
    char host[64];
    char path[128];
    uint16_t port = 443;
    int is_https = 1;

    if (parse_url(url, &is_https, host, sizeof(host), &port, path, sizeof(path)) != 0) {
        return KESH_HTTP_ERR_URL;
    }
    if (!is_https || !host[0]) return KESH_HTTP_ERR_URL;

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
    if (!s_trust_store_loaded) (void)kesh_tls_reload_trust_store();
    br_ssl_client_init_full(&s_sc, &s_xc, s_active_trust_anchors, s_active_trust_anchor_count);

    uint64_t realtime_seconds = timer_realtime_ns() / 1000000000ULL;
    if (realtime_seconds < 1577836800ULL) {
        serial_print("[HTTPS] Reliable wall clock unavailable; refusing certificate validation\n");
        net_tcp_close();
        return KESH_HTTP_ERR_TLS;
    }
    br_x509_minimal_set_time(&s_xc,
                             (uint32_t)(719528ULL + realtime_seconds / 86400ULL),
                             (uint32_t)(realtime_seconds % 86400ULL));

    br_ssl_engine_set_buffer(&s_sc.eng, s_iobuf, sizeof(s_iobuf), 1);

    uint8_t seed[32];
    if (random_bytes(seed, sizeof(seed)) != 0) {
        serial_print("[HTTPS] CSPRNG unavailable; refusing TLS session\n");
        net_tcp_close();
        return KESH_HTTP_ERR_TLS;
    }
    br_ssl_engine_inject_entropy(&s_sc.eng, seed, sizeof(seed));
    for (size_t i = 0; i < sizeof(seed); i++) seed[i] = 0;

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
        s_tls_diag.last_xdec_err = s_tls_diag.last_bearssl_err;
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
    s_tls_last_response_len = total;

    s_tls_diag.last_bearssl_err = br_ssl_engine_last_error(&s_sc.eng);
    s_tls_diag.last_xdec_err = s_tls_diag.last_bearssl_err;
    s_tls_diag.pkey_status = s_tls_diag.last_bearssl_err == 0 ? 1 : 2;

    int close_status = br_sslio_close(&s_ioc);
    net_tcp_close();

    if (close_status < 0 || (total == 0 && s_tls_diag.last_bearssl_err != 0)) {
        return KESH_HTTP_ERR_TLS;
    }

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
