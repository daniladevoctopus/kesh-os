// протоколы сети
#ifndef NET_STACK_H
#define NET_STACK_H

#include <stdint.h>
#include <stdbool.h>

void net_init(void);
void net_poll(void);

uint32_t net_make_ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
void net_ip_to_str(uint32_t ip, char *buf);
uint32_t net_str_to_ip(const char *str);

int net_ping(uint32_t target_ip, uint32_t timeout_ms, int *out_rtt_ms);
int net_ping_send(uint32_t target_ip, uint16_t seq);
bool net_ping_check_reply(uint16_t seq);

int net_dns_resolve(const char *domain, uint32_t *out_ip, uint32_t timeout_ms);
int net_dns_send(const char *domain);
bool net_dns_check_reply(uint32_t *out_ip);

int net_tcp_connect(uint32_t dst_ip, uint16_t dst_port, uint32_t timeout_ms);
int net_tcp_send(const void *data, uint16_t len);
int net_tcp_recv(void *buf, uint16_t max_len, uint32_t timeout_ms);
void net_tcp_close(void);

uint32_t net_get_my_ip(void);
uint32_t net_get_netmask(void);
uint32_t net_get_gateway(void);
uint32_t net_get_dns_server(void);
const uint8_t* net_get_my_mac(void);

#endif
