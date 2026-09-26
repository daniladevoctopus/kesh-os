// хедера тлс
#ifndef KESH_TLS_H
#define KESH_TLS_H

#include <stdint.h>
#include <stddef.h>

#define KESH_HTTP_OK            0
#define KESH_HTTP_ERR_DNS       -1
#define KESH_HTTP_ERR_CONNECT   -2
#define KESH_HTTP_ERR_TLS       -3
#define KESH_HTTP_ERR_SEND      -4
#define KESH_HTTP_ERR_RECV      -5
#define KESH_HTTP_ERR_URL       -6

int kesh_http_get(const char *url, char *response_buf, size_t max_buf, int *out_status_code);
int kesh_https_get(const char *url, char *response_buf, size_t max_buf, int *out_status_code);

int kesh_fetch_url(const char *url, char *response_buf, size_t max_buf, int *out_status_code);
int kesh_http_fetch(const char *url, char *response_buf, size_t max_buf, int *out_status_code);

void kesh_tls_get_diag(uint32_t *ip, int *sent, int *recv, int *bsl_err, int *xdec_err, int *cert_bytes, int *pkey_st);

#endif
