// сетевой стек tcp/ip, пока пилится
#include "net_stack.h"
#include "netdev.h"
#include "../../kernel/timer.h"
#include <stddef.h>

#define ETH_TYPE_ARP  0x0806
#define ETH_TYPE_IPV4 0x0800

#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

typedef struct {
    uint8_t  dst_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;
} __attribute__((packed)) eth_header_t;

typedef struct {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_len;
    uint8_t  proto_len;
    uint16_t opcode;
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
} __attribute__((packed)) arp_packet_t;

typedef struct {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_offset;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed)) ipv4_header_t;

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} __attribute__((packed)) icmp_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed)) dns_header_t;


typedef struct {
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint32_t cookie;
    uint8_t options[312];
} __attribute__((packed)) dhcp_packet_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;
    uint8_t  flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed)) tcp_header_t;

typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t tcp_len;
} __attribute__((packed)) tcp_pseudo_header_t;

static uint8_t s_my_mac[6] = {0};
static uint32_t s_my_ip = 0;
static uint32_t s_gateway_ip = 0;
static uint32_t s_dns_ip = 0;
static uint32_t s_my_mask = 0;
static uint8_t s_gateway_mac[6] = {0};
static bool s_gateway_mac_resolved = false;

typedef struct {
    uint32_t ip;
    uint8_t mac[6];
    uint64_t updated_ms;
    uint8_t valid;
} arp_cache_entry_t;

static arp_cache_entry_t s_arp_cache[8];

static uint16_t s_ping_seq = 0;
static volatile bool s_ping_got_reply = false;
static volatile uint16_t s_ping_reply_id = 0;
static volatile uint16_t s_ping_reply_seq = 0;

static volatile bool s_dns_resolved = false;
static volatile uint32_t s_dns_resolved_ip = 0;
static uint16_t s_dns_query_id = 0x5432;
static uint32_t s_dhcp_xid = 0x4B455348;
static volatile bool s_dhcp_received = false;
static uint32_t s_dhcp_offered_ip = 0;
static uint32_t s_dhcp_server_ip = 0;
static uint8_t s_dhcp_message_type = 0;

enum {
    TCP_STATE_CLOSED = 0,
    TCP_STATE_SYN_SENT,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT
};
static volatile int s_tcp_state = TCP_STATE_CLOSED;
static uint32_t s_tcp_dst_ip = 0;
static uint16_t s_tcp_dst_port = 0;
static uint16_t s_tcp_src_port = 45000;
static uint32_t s_tcp_our_seq = 0x10000000;
static uint32_t s_tcp_server_seq = 0;
static uint8_t  s_tcp_rx_buf[65536];
static uint8_t  s_tcp_peer_mac[6];
static uint8_t  s_tcp_peer_mac_valid = 0;
static volatile uint32_t s_tcp_rx_head = 0;
static volatile uint32_t s_tcp_rx_tail = 0;
static net_udp_handler_t s_udp_handler = NULL;

static void serial_print(const char *s) {
    while (s && *s) {
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)*s++), "Nd"((uint16_t)0x3F8));
    }
}

static inline uint16_t htons(uint16_t v) {
    return (v << 8) | (v >> 8);
}
#define ntohs htons

static inline uint32_t htonl(uint32_t v) {
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) |
           ((v & 0xFF0000) >> 8) | ((v >> 24) & 0xFF);
}
#define ntohl htonl

static uint16_t calc_checksum(const void *data, size_t len) {
    uint32_t sum = 0;
    const uint16_t *p = (const uint16_t*)data;
    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len > 0) {
        sum += *(const uint8_t*)p;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

uint32_t net_make_ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return ((uint32_t)a) | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
}

void net_ip_to_str(uint32_t ip, char *buf) {
    uint8_t a = (uint8_t)(ip & 0xFF);
    uint8_t b = (uint8_t)((ip >> 8) & 0xFF);
    uint8_t c = (uint8_t)((ip >> 16) & 0xFF);
    uint8_t d = (uint8_t)((ip >> 24) & 0xFF);

    int idx = 0;
    uint8_t parts[4] = {a, b, c, d};
    for (int p = 0; p < 4; p++) {
        uint8_t val = parts[p];
        if (val >= 100) buf[idx++] = '0' + (val / 100);
        if (val >= 10)  buf[idx++] = '0' + ((val / 10) % 10);
        buf[idx++] = '0' + (val % 10);
        if (p < 3) buf[idx++] = '.';
    }
    buf[idx] = '\0';
}

uint32_t net_str_to_ip(const char *str) {
    if (!str) return 0;
    uint8_t parts[4] = {0};
    int p = 0;
    while (*str && p < 4) {
        if (*str == '.') {
            p++;
            str++;
            continue;
        }
        if (*str >= '0' && *str <= '9') {
            parts[p] = parts[p] * 10 + (*str - '0');
        } else {
            return 0; 
        }
        str++;
    }
    if (p != 3) return 0;
    return net_make_ip(parts[0], parts[1], parts[2], parts[3]);
}

uint32_t net_get_my_ip(void) {
    return s_my_ip;
}

uint32_t net_get_netmask(void) {
    return s_my_mask;
}

uint32_t net_get_gateway(void) {
    return s_gateway_ip;
}

uint32_t net_get_dns_server(void) {
    return s_dns_ip;
}

const uint8_t* net_get_my_mac(void) {
    return s_my_mac;
}

static void send_arp_request(uint32_t target_ip) {
    uint8_t frame[sizeof(eth_header_t) + sizeof(arp_packet_t)];
    eth_header_t *eth = (eth_header_t*)frame;
    arp_packet_t *arp = (arp_packet_t*)(frame + sizeof(eth_header_t));

    for (int i = 0; i < 6; i++) {
        eth->dst_mac[i] = 0xFF; 
        eth->src_mac[i] = s_my_mac[i];
    }
    eth->ethertype = htons(ETH_TYPE_ARP);

    arp->hw_type = htons(1);
    arp->proto_type = htons(ETH_TYPE_IPV4);
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->opcode = htons(ARP_OP_REQUEST);
    for (int i = 0; i < 6; i++) {
        arp->sender_mac[i] = s_my_mac[i];
        arp->target_mac[i] = 0x00;
    }
    arp->sender_ip = s_my_ip;
    arp->target_ip = target_ip;

    netdev_send(frame, sizeof(frame));
}

static void arp_cache_update(uint32_t ip, const uint8_t mac[6]) {
    if (!ip || !mac) return;
    int slot = -1;
    uint64_t oldest = (uint64_t)-1;
    for (int i = 0; i < 8; i++) {
        if (s_arp_cache[i].valid && s_arp_cache[i].ip == ip) { slot = i; break; }
        if (!s_arp_cache[i].valid) { slot = i; break; }
        if (s_arp_cache[i].updated_ms < oldest) { oldest = s_arp_cache[i].updated_ms; slot = i; }
    }
    if (slot < 0) return;
    s_arp_cache[slot].ip = ip;
    for (int i = 0; i < 6; i++) s_arp_cache[slot].mac[i] = mac[i];
    s_arp_cache[slot].updated_ms = timer_millis();
    s_arp_cache[slot].valid = 1;
    if (ip == s_gateway_ip) {
        for (int i = 0; i < 6; i++) s_gateway_mac[i] = mac[i];
        s_gateway_mac_resolved = true;
    }
}

static int arp_cache_lookup(uint32_t ip, uint8_t out_mac[6]) {
    if (!ip || !out_mac) return 0;
    uint64_t now = timer_millis();
    for (int i = 0; i < 8; i++) {
        if (s_arp_cache[i].valid && s_arp_cache[i].ip == ip) {
            if (now - s_arp_cache[i].updated_ms > 300000) {
                s_arp_cache[i].valid = 0;
                continue;
            }
            for (int k = 0; k < 6; k++) out_mac[k] = s_arp_cache[i].mac[k];
            return 1;
        }
    }
    return 0;
}

static uint32_t net_next_hop(uint32_t dst_ip) {
    if (!s_my_mask) return s_gateway_ip;
    if ((dst_ip & s_my_mask) == (s_my_ip & s_my_mask)) return dst_ip;
    return s_gateway_ip;
}

static int net_resolve_mac(uint32_t dst_ip, uint8_t out_mac[6]) {
    uint32_t next_hop = net_next_hop(dst_ip);
    if (arp_cache_lookup(next_hop, out_mac)) return 0;
    for (int attempt = 0; attempt < 2; attempt++) {
        send_arp_request(next_hop);
        uint64_t start = timer_millis();
        while (timer_millis() - start < 300) {
            net_poll();
            if (arp_cache_lookup(next_hop, out_mac)) return 0;
        }
    }
    return -1;
}

static void handle_arp(const uint8_t *packet, uint16_t len) {
    if (len < sizeof(eth_header_t) + sizeof(arp_packet_t)) return;
    const arp_packet_t *arp = (const arp_packet_t*)(packet + sizeof(eth_header_t));

    uint16_t op = ntohs(arp->opcode);
    if (arp->hw_len == 6 && arp->proto_len == 4) {
        arp_cache_update(arp->sender_ip, arp->sender_mac);
    }

    if (op == ARP_OP_REQUEST && arp->target_ip == s_my_ip) {
        uint8_t reply_frame[sizeof(eth_header_t) + sizeof(arp_packet_t)];
        eth_header_t *eth = (eth_header_t*)reply_frame;
        arp_packet_t *reply = (arp_packet_t*)(reply_frame + sizeof(eth_header_t));

        for (int i = 0; i < 6; i++) {
            eth->dst_mac[i] = arp->sender_mac[i];
            eth->src_mac[i] = s_my_mac[i];
        }
        eth->ethertype = htons(ETH_TYPE_ARP);

        reply->hw_type = htons(1);
        reply->proto_type = htons(ETH_TYPE_IPV4);
        reply->hw_len = 6;
        reply->proto_len = 4;
        reply->opcode = htons(ARP_OP_REPLY);
        for (int i = 0; i < 6; i++) {
            reply->sender_mac[i] = s_my_mac[i];
            reply->target_mac[i] = arp->sender_mac[i];
        }
        reply->sender_ip = s_my_ip;
        reply->target_ip = arp->sender_ip;

        netdev_send(reply_frame, sizeof(reply_frame));
    }
}

static void handle_dhcp_reply(const uint8_t *payload, uint16_t len) {
    if (len < 240) return;
    const dhcp_packet_t *p = (const dhcp_packet_t*)payload;
    if (ntohl(p->xid) != s_dhcp_xid || p->yiaddr == 0) return;
    uint32_t gateway = 0;
    uint32_t dns = 0;
    uint32_t mask = 0;
    uint8_t message_type = 0;
    const uint8_t *opt = p->options;
    uint32_t left = len - 240;
    while (left > 0) {
        uint8_t code = *opt++; left--;
        if (code == 0) continue;
        if (code == 255) break;
        if (left == 0) break;
        uint8_t olen = *opt++; left--;
        if (olen > left) break;
        if (code == 53 && olen >= 1) message_type = opt[0];
        if (code == 1 && olen >= 4) { mask = *(const uint32_t*)opt; }
        if (code == 3 && olen >= 4) { gateway = *(const uint32_t*)opt; }
        if (code == 6 && olen >= 4) { dns = *(const uint32_t*)opt; }
        if (code == 54 && olen >= 4) { s_dhcp_server_ip = *(const uint32_t*)opt; }
        opt += olen; left -= olen;
    }
    if (message_type != 2 && message_type != 5) return;
    s_dhcp_message_type = message_type;
    s_dhcp_offered_ip = p->yiaddr;
    if (mask) s_my_mask = mask;
    if (gateway) { s_gateway_ip = gateway; s_gateway_mac_resolved = false; }
    if (dns) s_dns_ip = dns;
    if (message_type == 5) s_my_ip = p->yiaddr;
    s_dhcp_received = true;
}

static void send_dhcp(uint8_t message_type) {
    uint8_t frame[sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t) + sizeof(dhcp_packet_t)];
    eth_header_t *eth = (eth_header_t*)frame;
    ipv4_header_t *ip = (ipv4_header_t*)(frame + sizeof(eth_header_t));
    udp_header_t *udp = (udp_header_t*)(frame + sizeof(eth_header_t) + sizeof(ipv4_header_t));
    dhcp_packet_t *dhcp = (dhcp_packet_t*)((uint8_t*)udp + sizeof(udp_header_t));
    for (int i = 0; i < 6; i++) { eth->dst_mac[i] = 0xFF; eth->src_mac[i] = s_my_mac[i]; dhcp->chaddr[i] = s_my_mac[i]; }
    eth->ethertype = htons(ETH_TYPE_IPV4);
    dhcp->op = 1; dhcp->htype = 1; dhcp->hlen = 6; dhcp->xid = htonl(s_dhcp_xid); dhcp->flags = htons(0x8000);
    dhcp->cookie = htonl(0x63825363);
    uint8_t *o = dhcp->options;
    *o++ = 53; *o++ = 1; *o++ = message_type;
    *o++ = 61; *o++ = 7; *o++ = 1; for (int i = 0; i < 6; i++) *o++ = s_my_mac[i];
    if (message_type == 3 && s_dhcp_offered_ip) {
        *o++ = 50; *o++ = 4; for (int i = 0; i < 4; i++) *o++ = ((uint8_t*)&s_dhcp_offered_ip)[i];
        if (s_dhcp_server_ip) { *o++ = 54; *o++ = 4; for (int i = 0; i < 4; i++) *o++ = ((uint8_t*)&s_dhcp_server_ip)[i]; }
    }
    *o++ = 55; *o++ = 4; *o++ = 1; *o++ = 3; *o++ = 6; *o++ = 15;
    *o++ = 255;
    uint16_t dhcp_len = (uint16_t)((uint8_t*)o - (uint8_t*)dhcp);
    udp->src_port = htons(68); udp->dst_port = htons(67); udp->length = htons((uint16_t)(sizeof(udp_header_t) + dhcp_len)); udp->checksum = 0;
    ip->ver_ihl = 0x45; ip->tos = 0; ip->total_len = htons((uint16_t)(sizeof(ipv4_header_t) + sizeof(udp_header_t) + dhcp_len));
    ip->id = htons(0xD4C1); ip->flags_offset = 0; ip->ttl = 64; ip->protocol = IP_PROTO_UDP; ip->src_ip = 0; ip->dst_ip = 0xFFFFFFFFu; ip->checksum = 0;
    ip->checksum = calc_checksum(ip, sizeof(ipv4_header_t));
    netdev_send(frame, (uint16_t)(sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t) + dhcp_len));
}

static int net_dhcp_configure(void) {
    s_dhcp_received = false;
    s_dhcp_offered_ip = 0;
    s_dhcp_server_ip = 0;
    s_dhcp_xid++;
    send_dhcp(1);
    uint64_t start = timer_millis();
    while (timer_millis() - start < 1500) {
        net_poll();
        if (s_dhcp_received) break;
    }
    if (!s_dhcp_received) return -1;
    uint32_t offered_ip = s_dhcp_offered_ip ? s_dhcp_offered_ip : s_my_ip;
    s_my_ip = 0;
    send_dhcp(3);
    s_dhcp_received = false;
    start = timer_millis();
    while (timer_millis() - start < 1500) {
        net_poll();
        if (s_dhcp_received && s_dhcp_message_type == 5) return 0;
    }
    s_my_ip = offered_ip;
    return -1;
}

static void handle_dns_reply(const uint8_t *payload, uint16_t len) {
    if (len < sizeof(dns_header_t)) return;
    const dns_header_t *dns = (const dns_header_t*)payload;
    if (ntohs(dns->id) != s_dns_query_id) return;
    if (ntohs(dns->ancount) == 0) return;

    const uint8_t *p = payload + sizeof(dns_header_t);
    const uint8_t *end = payload + len;

    while (p < end && *p != 0) {
        p += (*p + 1);
    }
    p++; 
    p += 4; 

    for (int a = 0; a < ntohs(dns->ancount) && p < end; a++) {

        if (*p >= 0xC0) {
            p += 2; 
        } else {
            while (p < end && *p != 0) p += (*p + 1);
            p++;
        }
        if (p + 10 > end) break;
        uint16_t type = ntohs(*(const uint16_t*)p); p += 2;
        p += 2; 
        p += 4; 
        uint16_t rdlen = ntohs(*(const uint16_t*)p); p += 2;

        if (type == 1 && rdlen == 4 && p + 4 <= end) {
            s_dns_resolved_ip = *(const uint32_t*)p;
            s_dns_resolved = true;
            serial_print("[DNS] Successfully resolved domain name!\n");
            return;
        }
        p += rdlen;
    }
}

static uint16_t calc_tcp_checksum(const ipv4_header_t *ip, const tcp_header_t *tcp, const void *payload, uint16_t payload_len) {
    tcp_pseudo_header_t pseudo;
    pseudo.src_ip = ip->src_ip;
    pseudo.dst_ip = ip->dst_ip;
    pseudo.zero = 0;
    pseudo.protocol = IP_PROTO_TCP;
    pseudo.tcp_len = htons(sizeof(tcp_header_t) + payload_len);

    uint32_t sum = 0;
    const uint16_t *p = (const uint16_t*)&pseudo;
    for (size_t i = 0; i < sizeof(pseudo) / 2; i++) sum += *p++;

    p = (const uint16_t*)tcp;
    for (size_t i = 0; i < sizeof(tcp_header_t) / 2; i++) sum += *p++;

    p = (const uint16_t*)payload;
    size_t plen = payload_len;
    while (plen > 1) { sum += *p++; plen -= 2; }
    if (plen > 0) sum += *(const uint8_t*)p;

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

static void send_tcp_ack(void) {
    uint8_t dst_mac[6];
    if (s_tcp_peer_mac_valid) {
        for (int i = 0; i < 6; i++) dst_mac[i] = s_tcp_peer_mac[i];
    } else if (net_resolve_mac(s_tcp_dst_ip, dst_mac) != 0) {
        return;
    }

    uint8_t frame[sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(tcp_header_t)];
    eth_header_t *eth = (eth_header_t*)frame;
    ipv4_header_t *ip = (ipv4_header_t*)(frame + sizeof(eth_header_t));
    tcp_header_t *tcp = (tcp_header_t*)(frame + sizeof(eth_header_t) + sizeof(ipv4_header_t));

    for (int i = 0; i < 6; i++) {
        eth->dst_mac[i] = dst_mac[i];
        eth->src_mac[i] = s_my_mac[i];
    }
    eth->ethertype = htons(ETH_TYPE_IPV4);

    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_len = htons((uint16_t)(sizeof(ipv4_header_t) + sizeof(tcp_header_t)));
    ip->id = htons(0x3344);
    ip->flags_offset = 0;
    ip->ttl = 64;
    ip->protocol = IP_PROTO_TCP;
    ip->src_ip = s_my_ip;
    ip->dst_ip = s_tcp_dst_ip;
    ip->checksum = 0;
    ip->checksum = calc_checksum(ip, sizeof(ipv4_header_t));

    tcp->src_port = htons(s_tcp_src_port);
    tcp->dst_port = htons(s_tcp_dst_port);
    tcp->seq_num = htonl(s_tcp_our_seq);
    tcp->ack_num = htonl(s_tcp_server_seq);
    tcp->data_offset = (sizeof(tcp_header_t) / 4) << 4;
    tcp->flags = 0x10; 
    tcp->window_size = htons(32768);
    tcp->checksum = 0;
    tcp->urgent_ptr = 0;
    tcp->checksum = calc_tcp_checksum(ip, tcp, NULL, 0);

    netdev_send(frame, sizeof(frame));
}

static void handle_tcp(const uint8_t *packet, uint16_t len) {
    if (len < sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(tcp_header_t)) return;
    const eth_header_t *eth = (const eth_header_t*)packet;
    const tcp_header_t *tcp = (const tcp_header_t*)(packet + sizeof(eth_header_t) + sizeof(ipv4_header_t));

    if (ntohs(tcp->dst_port) != s_tcp_src_port) return;
    for (int i = 0; i < 6; i++) s_tcp_peer_mac[i] = eth->src_mac[i];
    s_tcp_peer_mac_valid = 1;

    uint8_t flags = tcp->flags;

    if (s_tcp_state == TCP_STATE_SYN_SENT && (flags & 0x12) == 0x12) {

        s_tcp_server_seq = ntohl(tcp->seq_num) + 1;
        s_tcp_our_seq++;
        s_tcp_state = TCP_STATE_ESTABLISHED;
        send_tcp_ack();
        serial_print("[TCP] Connection Established!\n");
    } else if (s_tcp_state == TCP_STATE_ESTABLISHED) {
        size_t tcp_hdr_len = (tcp->data_offset >> 4) * 4;
        const uint8_t *data = packet + sizeof(eth_header_t) + sizeof(ipv4_header_t) + tcp_hdr_len;
        int data_len = len - (sizeof(eth_header_t) + sizeof(ipv4_header_t) + tcp_hdr_len);

        if (data_len > 0) {
            for (int i = 0; i < data_len; i++) {
                uint32_t next_head = (s_tcp_rx_head + 1) % sizeof(s_tcp_rx_buf);
                if (next_head != s_tcp_rx_tail) {
                    s_tcp_rx_buf[s_tcp_rx_head] = data[i];
                    s_tcp_rx_head = next_head;
                }
            }
            s_tcp_server_seq += data_len;
            send_tcp_ack();
        }

        if (flags & 0x01) {

            s_tcp_server_seq++;
            send_tcp_ack();
            s_tcp_state = TCP_STATE_CLOSED;
        }
    }
}

static void handle_ipv4(const uint8_t *packet, uint16_t len) {
    if (len < sizeof(eth_header_t) + sizeof(ipv4_header_t)) return;
    const eth_header_t *eth = (const eth_header_t*)packet;
    const ipv4_header_t *ip = (const ipv4_header_t*)(packet + sizeof(eth_header_t));
    uint16_t ip_length = ntohs(ip->total_len);
    if (ip->ver_ihl != 0x45 || ip_length < sizeof(ipv4_header_t) ||
        sizeof(eth_header_t) + ip_length > len || (ntohs(ip->flags_offset) & 0x3FFFU)) return;
    if (calc_checksum(ip, sizeof(ipv4_header_t)) != 0) return;

    if (ip->dst_ip != s_my_ip && ip->dst_ip != 0xFFFFFFFF) return;

    if (ip->protocol == IP_PROTO_ICMP) {
        size_t ip_hdr_len = (ip->ver_ihl & 0x0F) * 4;
        if (ip_length < ip_hdr_len + sizeof(icmp_header_t)) return;
        const icmp_header_t *icmp = (const icmp_header_t*)(packet + sizeof(eth_header_t) + ip_hdr_len);
        size_t icmp_len = ip_length - ip_hdr_len;
        if (sizeof(eth_header_t) + sizeof(ipv4_header_t) + icmp_len > 1500) return;

        if (icmp->type == 8) {

            uint8_t reply_buf[1500];
            eth_header_t *r_eth = (eth_header_t*)reply_buf;
            ipv4_header_t *r_ip = (ipv4_header_t*)(reply_buf + sizeof(eth_header_t));
            icmp_header_t *r_icmp = (icmp_header_t*)(reply_buf + sizeof(eth_header_t) + sizeof(ipv4_header_t));

            for (int i = 0; i < 6; i++) {
                r_eth->dst_mac[i] = eth->src_mac[i];
                r_eth->src_mac[i] = s_my_mac[i];
            }
            r_eth->ethertype = htons(ETH_TYPE_IPV4);

            r_ip->ver_ihl = 0x45;
            r_ip->tos = 0;
            r_ip->total_len = htons((uint16_t)(sizeof(ipv4_header_t) + icmp_len));
            r_ip->id = htons(0x1000);
            r_ip->flags_offset = 0;
            r_ip->ttl = 64;
            r_ip->protocol = IP_PROTO_ICMP;
            r_ip->src_ip = s_my_ip;
            r_ip->dst_ip = ip->src_ip;
            r_ip->checksum = 0;
            r_ip->checksum = calc_checksum(r_ip, sizeof(ipv4_header_t));

            const uint8_t *src_payload = (const uint8_t*)icmp;
            uint8_t *dst_payload = (uint8_t*)r_icmp;
            for (size_t i = 0; i < icmp_len; i++) {
                dst_payload[i] = src_payload[i];
            }
            r_icmp->type = 0; 
            r_icmp->checksum = 0;
            r_icmp->checksum = calc_checksum(r_icmp, icmp_len);

            netdev_send(reply_buf, (uint16_t)(sizeof(eth_header_t) + sizeof(ipv4_header_t) + icmp_len));
        } else if (icmp->type == 0) {
            s_ping_got_reply = true;
            s_ping_reply_id = ntohs(icmp->id);
            s_ping_reply_seq = ntohs(icmp->seq);
        }
    } else if (ip->protocol == IP_PROTO_UDP) {
        size_t ip_hdr_len = (ip->ver_ihl & 0x0F) * 4;
        if (ip_hdr_len < sizeof(ipv4_header_t) || sizeof(eth_header_t) + ip_hdr_len + sizeof(udp_header_t) > len) return;
        const udp_header_t *udp = (const udp_header_t*)(packet + sizeof(eth_header_t) + ip_hdr_len);
        const uint8_t *udp_payload = (const uint8_t*)udp + sizeof(udp_header_t);
        uint16_t udp_len = ntohs(udp->length);
        if (udp_len < sizeof(udp_header_t) || sizeof(eth_header_t) + ip_hdr_len + udp_len > len) return;
        uint16_t payload_len = (uint16_t)(udp_len - sizeof(udp_header_t));
        if (ntohs(udp->src_port) == 67 && ntohs(udp->dst_port) == 68) handle_dhcp_reply(udp_payload, payload_len);
        if (ntohs(udp->src_port) == 53) handle_dns_reply(udp_payload, payload_len);
        if (s_udp_handler) s_udp_handler(ip->src_ip, ntohs(udp->src_port), ntohs(udp->dst_port), udp_payload, payload_len);
    } else if (ip->protocol == IP_PROTO_TCP) {
        handle_tcp(packet, len);
    }
}

void net_poll(void) {
    uint8_t buf[2048];
    int len;
    while ((len = netdev_poll(buf, sizeof(buf))) > 0) {
        if (len < (int)sizeof(eth_header_t)) continue;
        const eth_header_t *eth = (const eth_header_t*)buf;
        uint16_t type = ntohs(eth->ethertype);

        if (type == ETH_TYPE_ARP) {
            handle_arp(buf, (uint16_t)len);
        } else if (type == ETH_TYPE_IPV4) {
            handle_ipv4(buf, (uint16_t)len);
        }
    }
}

void net_udp_set_handler(net_udp_handler_t handler) {
    s_udp_handler = handler;
}

int net_udp_send(uint32_t destination_ip, uint16_t source_port, uint16_t destination_port,
                 const void *data, uint16_t length) {
    if (!destination_ip || !source_port || !destination_port || (!data && length) || length > 1400) return -1;
    uint8_t destination_mac[6];
    if (net_resolve_mac(destination_ip, destination_mac) != 0) return -2;
    uint8_t frame[sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t) + 1400];
    eth_header_t *eth = (eth_header_t *)frame;
    ipv4_header_t *ip = (ipv4_header_t *)(frame + sizeof(*eth));
    udp_header_t *udp = (udp_header_t *)((uint8_t *)ip + sizeof(*ip));
    uint8_t *payload = (uint8_t *)udp + sizeof(*udp);
    for (int i = 0; i < 6; ++i) { eth->dst_mac[i] = destination_mac[i]; eth->src_mac[i] = s_my_mac[i]; }
    eth->ethertype = htons(ETH_TYPE_IPV4);
    uint16_t udp_length = (uint16_t)(sizeof(*udp) + length);
    ip->ver_ihl = 0x45; ip->tos = 0; ip->total_len = htons((uint16_t)(sizeof(*ip) + udp_length));
    ip->id = htons((uint16_t)(timer_millis() & 0xFFFFU)); ip->flags_offset = 0; ip->ttl = 64;
    ip->protocol = IP_PROTO_UDP; ip->src_ip = s_my_ip; ip->dst_ip = destination_ip; ip->checksum = 0;
    ip->checksum = calc_checksum(ip, sizeof(*ip));
    udp->src_port = htons(source_port); udp->dst_port = htons(destination_port);
    udp->length = htons(udp_length); udp->checksum = 0;
    for (uint16_t i = 0; i < length; ++i) payload[i] = ((const uint8_t *)data)[i];
    return netdev_send(frame, (uint16_t)(sizeof(*eth) + sizeof(*ip) + udp_length));
}

void net_init(void) {
    for (int i = 0; i < 8; i++) s_arp_cache[i].valid = 0;
    if (!netdev_init()) {
        serial_print("[NET] No supported network device found.\n");
        return;
    }
    const netdev_info_t *info = netdev_get_info();
    for (int i = 0; i < 6; i++) s_my_mac[i] = info->mac[i];
    serial_print("[NET] Device: ");
    serial_print(info->name ? info->name : "unknown");
    serial_print("\n");
    s_my_ip = net_make_ip(10, 0, 2, 15);
    s_my_mask = net_make_ip(255, 255, 255, 0);
    s_gateway_ip = net_make_ip(10, 0, 2, 2);
    s_dns_ip = net_make_ip(10, 0, 2, 3);
    if (net_dhcp_configure() != 0) {
        s_my_ip = net_make_ip(10, 0, 2, 15);
        s_gateway_ip = net_make_ip(10, 0, 2, 2);
        s_dns_ip = net_make_ip(10, 0, 2, 3);
    }
    send_arp_request(s_gateway_ip);
}

int net_ping_send(uint32_t target_ip, uint16_t seq) {
    if (s_my_mac[0] == 0 && s_my_mac[1] == 0 && s_my_mac[2] == 0) {
        return -2;
    }

    uint8_t frame[sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(icmp_header_t) + 32];
    eth_header_t *eth = (eth_header_t*)frame;
    ipv4_header_t *ip = (ipv4_header_t*)(frame + sizeof(eth_header_t));
    icmp_header_t *icmp = (icmp_header_t*)(frame + sizeof(eth_header_t) + sizeof(ipv4_header_t));
    uint8_t *payload = frame + sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(icmp_header_t);

    uint8_t dst_mac[6];
    if (net_resolve_mac(target_ip, dst_mac) != 0) return -3;
    for (int i = 0; i < 6; i++) {
        eth->dst_mac[i] = dst_mac[i];
        eth->src_mac[i] = s_my_mac[i];
    }
    eth->ethertype = htons(ETH_TYPE_IPV4);

    uint16_t icmp_size = sizeof(icmp_header_t) + 32;
    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_len = htons((uint16_t)(sizeof(ipv4_header_t) + icmp_size));
    ip->id = htons(0x4321);
    ip->flags_offset = 0;
    ip->ttl = 64;
    ip->protocol = IP_PROTO_ICMP;
    ip->src_ip = s_my_ip;
    ip->dst_ip = target_ip;
    ip->checksum = 0;
    ip->checksum = calc_checksum(ip, sizeof(ipv4_header_t));

    s_ping_seq = seq;
    icmp->type = 8;
    icmp->code = 0;
    icmp->id = htons(0x7777);
    icmp->seq = htons(seq);
    icmp->checksum = 0;

    const char pattern[] = "KeshOS 1.0 Drop Network ICMPPing";
    for (int i = 0; i < 32; i++) {
        payload[i] = pattern[i];
    }
    icmp->checksum = calc_checksum(icmp, icmp_size);

    s_ping_got_reply = false;
    netdev_send(frame, sizeof(frame));
    return 0;
}

bool net_ping_check_reply(uint16_t seq) {
    if (s_ping_got_reply && s_ping_reply_id == 0x7777 && s_ping_reply_seq == seq) {
        return true;
    }
    return false;
}

int net_ping(uint32_t target_ip, uint32_t timeout_ms, int *out_rtt_ms) {
    static uint16_t seq = 0;
    seq++;
    if (net_ping_send(target_ip, seq) != 0) return -2;

    uint64_t t_start = timer_millis();
    while (timer_millis() - t_start < timeout_ms) {
        net_poll();
        if (net_ping_check_reply(seq)) {
            if (out_rtt_ms) {
                *out_rtt_ms = (int)(timer_millis() - t_start);
            }
            return 0;
        }
    }
    return -1;
}

static int dns_encode_labels(const char *name, uint8_t *out) {
    int idx = 0;
    const char *p = name;
    while (*p) {
        const char *dot = p;
        while (*dot && *dot != '.') dot++;
        int len = (int)(dot - p);
        if (len > 63) len = 63;
        out[idx++] = (uint8_t)len;
        for (int i = 0; i < len; i++) out[idx++] = p[i];
        p = dot;
        if (*p == '.') p++;
    }
    out[idx++] = 0;
    return idx;
}

int net_dns_send(const char *domain) {
    if (!domain) return -1;

    uint32_t direct_ip = net_str_to_ip(domain);
    if (direct_ip != 0) {
        s_dns_resolved_ip = direct_ip;
        s_dns_resolved = true;
        return 0;
    }

    if (s_my_mac[0] == 0 && s_my_mac[1] == 0 && s_my_mac[2] == 0) {
        return -2;
    }

    uint8_t frame[512];
    eth_header_t *eth = (eth_header_t*)frame;
    ipv4_header_t *ip = (ipv4_header_t*)(frame + sizeof(eth_header_t));
    udp_header_t *udp = (udp_header_t*)(frame + sizeof(eth_header_t) + sizeof(ipv4_header_t));
    dns_header_t *dns = (dns_header_t*)((uint8_t*)udp + sizeof(udp_header_t));
    uint8_t *q_ptr = (uint8_t*)dns + sizeof(dns_header_t);

    uint8_t dst_mac[6];
    if (net_resolve_mac(s_dns_ip, dst_mac) != 0) return -3;
    for (int i = 0; i < 6; i++) {
        eth->dst_mac[i] = dst_mac[i];
        eth->src_mac[i] = s_my_mac[i];
    }
    eth->ethertype = htons(ETH_TYPE_IPV4);

    s_dns_query_id++;
    dns->id = htons(s_dns_query_id);
    dns->flags = htons(0x0100); 
    dns->qdcount = htons(1);
    dns->ancount = 0;
    dns->nscount = 0;
    dns->arcount = 0;

    int q_len = dns_encode_labels(domain, q_ptr);
    q_ptr += q_len;

    *(uint16_t*)q_ptr = htons(1); q_ptr += 2;
    *(uint16_t*)q_ptr = htons(1); q_ptr += 2;

    uint16_t dns_payload_len = (uint16_t)(q_ptr - (uint8_t*)dns);
    uint16_t udp_len = sizeof(udp_header_t) + dns_payload_len;

    udp->src_port = htons(53535);
    udp->dst_port = htons(53);
    udp->length = htons(udp_len);
    udp->checksum = 0;

    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_len = htons((uint16_t)(sizeof(ipv4_header_t) + udp_len));
    ip->id = htons(0x9876);
    ip->flags_offset = 0;
    ip->ttl = 64;
    ip->protocol = IP_PROTO_UDP;
    ip->src_ip = s_my_ip;
    ip->dst_ip = s_dns_ip; 
    ip->checksum = 0;
    ip->checksum = calc_checksum(ip, sizeof(ipv4_header_t));

    uint16_t total_frame_len = sizeof(eth_header_t) + sizeof(ipv4_header_t) + udp_len;

    s_dns_resolved = false;
    netdev_send(frame, total_frame_len);

    return 0;
}

bool net_dns_check_reply(uint32_t *out_ip) {
    if (s_dns_resolved) {
        if (out_ip) *out_ip = s_dns_resolved_ip;
        return true;
    }
    return false;
}

int net_dns_resolve(const char *domain, uint32_t *out_ip, uint32_t timeout_ms) {
    if (!domain || !out_ip) return -1;

    int err = net_dns_send(domain);
    if (err != 0) return err;

    uint64_t start = timer_millis();
    while (timer_millis() - start < timeout_ms) {
        net_poll();
        if (net_dns_check_reply(out_ip)) {
            return 0;
        }
    }
    return -1; 
}

int net_tcp_connect(uint32_t dst_ip, uint16_t dst_port, uint32_t timeout_ms) {
    s_tcp_dst_ip = dst_ip;
    s_tcp_dst_port = dst_port;
    s_tcp_src_port++;
    if (s_tcp_src_port > 60000) s_tcp_src_port = 45000;
    s_tcp_our_seq = 0x12345000;
    s_tcp_peer_mac_valid = 0;
    s_tcp_rx_head = 0;
    s_tcp_rx_tail = 0;

    uint8_t frame[sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(tcp_header_t)];
    eth_header_t *eth = (eth_header_t*)frame;
    ipv4_header_t *ip = (ipv4_header_t*)(frame + sizeof(eth_header_t));
    tcp_header_t *tcp = (tcp_header_t*)(frame + sizeof(eth_header_t) + sizeof(ipv4_header_t));

    uint8_t dst_mac[6];
    if (net_resolve_mac(dst_ip, dst_mac) != 0) return -2;
    for (int i = 0; i < 6; i++) {
        eth->dst_mac[i] = dst_mac[i];
        eth->src_mac[i] = s_my_mac[i];
    }
    eth->ethertype = htons(ETH_TYPE_IPV4);

    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_len = htons((uint16_t)(sizeof(ipv4_header_t) + sizeof(tcp_header_t)));
    ip->id = htons(0x7788);
    ip->flags_offset = 0;
    ip->ttl = 64;
    ip->protocol = IP_PROTO_TCP;
    ip->src_ip = s_my_ip;
    ip->dst_ip = dst_ip;
    ip->checksum = 0;
    ip->checksum = calc_checksum(ip, sizeof(ipv4_header_t));

    tcp->src_port = htons(s_tcp_src_port);
    tcp->dst_port = htons(dst_port);
    tcp->seq_num = htonl(s_tcp_our_seq);
    tcp->ack_num = 0;
    tcp->data_offset = (sizeof(tcp_header_t) / 4) << 4;
    tcp->flags = 0x02; 
    tcp->window_size = htons(32768);
    tcp->checksum = 0;
    tcp->urgent_ptr = 0;
    tcp->checksum = calc_tcp_checksum(ip, tcp, NULL, 0);

    s_tcp_state = TCP_STATE_SYN_SENT;
    netdev_send(frame, sizeof(frame));

    uint64_t start = timer_millis();
    while (timer_millis() - start < timeout_ms) {
        net_poll();
        if (s_tcp_state == TCP_STATE_ESTABLISHED) {
            return 0;
        }
    }
    s_tcp_state = TCP_STATE_CLOSED;
    return -1;
}

int net_tcp_send(const void *data, uint16_t len) {
    if (s_tcp_state != TCP_STATE_ESTABLISHED) return -1;

    uint8_t frame[1500];
    eth_header_t *eth = (eth_header_t*)frame;
    ipv4_header_t *ip = (ipv4_header_t*)(frame + sizeof(eth_header_t));
    tcp_header_t *tcp = (tcp_header_t*)(frame + sizeof(eth_header_t) + sizeof(ipv4_header_t));
    uint8_t *payload = frame + sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(tcp_header_t);

    uint8_t dst_mac[6];
    if (net_resolve_mac(s_tcp_dst_ip, dst_mac) != 0) return -2;
    for (int i = 0; i < 6; i++) {
        eth->dst_mac[i] = dst_mac[i];
        eth->src_mac[i] = s_my_mac[i];
    }
    eth->ethertype = htons(ETH_TYPE_IPV4);

    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_len = htons((uint16_t)(sizeof(ipv4_header_t) + sizeof(tcp_header_t) + len));
    ip->id = htons(0x7789);
    ip->flags_offset = 0;
    ip->ttl = 64;
    ip->protocol = IP_PROTO_TCP;
    ip->src_ip = s_my_ip;
    ip->dst_ip = s_tcp_dst_ip;
    ip->checksum = 0;
    ip->checksum = calc_checksum(ip, sizeof(ipv4_header_t));

    tcp->src_port = htons(s_tcp_src_port);
    tcp->dst_port = htons(s_tcp_dst_port);
    tcp->seq_num = htonl(s_tcp_our_seq);
    tcp->ack_num = htonl(s_tcp_server_seq);
    tcp->data_offset = (sizeof(tcp_header_t) / 4) << 4;
    tcp->flags = 0x18; 
    tcp->window_size = htons(32768);
    tcp->checksum = 0;
    tcp->urgent_ptr = 0;

    const uint8_t *src_d = (const uint8_t*)data;
    for (uint16_t i = 0; i < len; i++) payload[i] = src_d[i];

    tcp->checksum = calc_tcp_checksum(ip, tcp, payload, len);

    s_tcp_our_seq += len;
    netdev_send(frame, (uint16_t)(sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(tcp_header_t) + len));
    return len;
}

int net_tcp_recv(void *buf, uint16_t max_len, uint32_t timeout_ms) {
    uint64_t start = timer_millis();
    while (s_tcp_rx_head == s_tcp_rx_tail) {
        net_poll();
        if (timer_millis() - start >= timeout_ms) return 0;
        if (s_tcp_state == TCP_STATE_CLOSED) return 0;
    }

    uint8_t *dst = (uint8_t*)buf;
    uint16_t copied = 0;
    while (s_tcp_rx_tail != s_tcp_rx_head && copied < max_len) {
        dst[copied++] = s_tcp_rx_buf[s_tcp_rx_tail];
        s_tcp_rx_tail = (s_tcp_rx_tail + 1) % sizeof(s_tcp_rx_buf);
    }
    return copied;
}

void net_tcp_close(void) {
    if (s_tcp_state == TCP_STATE_ESTABLISHED) {

        uint8_t frame[sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(tcp_header_t)];
        eth_header_t *eth = (eth_header_t*)frame;
        ipv4_header_t *ip = (ipv4_header_t*)(frame + sizeof(eth_header_t));
        tcp_header_t *tcp = (tcp_header_t*)(frame + sizeof(eth_header_t) + sizeof(ipv4_header_t));

        uint8_t dst_mac[6];
        if (net_resolve_mac(s_tcp_dst_ip, dst_mac) == 0) {
            for (int i = 0; i < 6; i++) {
                eth->dst_mac[i] = dst_mac[i];
                eth->src_mac[i] = s_my_mac[i];
            }
        }
        eth->ethertype = htons(ETH_TYPE_IPV4);

        ip->ver_ihl = 0x45;
        ip->tos = 0;
        ip->total_len = htons((uint16_t)(sizeof(ipv4_header_t) + sizeof(tcp_header_t)));
        ip->id = htons(0x7790);
        ip->flags_offset = 0;
        ip->ttl = 64;
        ip->protocol = IP_PROTO_TCP;
        ip->src_ip = s_my_ip;
        ip->dst_ip = s_tcp_dst_ip;
        ip->checksum = 0;
        ip->checksum = calc_checksum(ip, sizeof(ipv4_header_t));

        tcp->src_port = htons(s_tcp_src_port);
        tcp->dst_port = htons(s_tcp_dst_port);
        tcp->seq_num = htonl(s_tcp_our_seq);
        tcp->ack_num = htonl(s_tcp_server_seq);
        tcp->data_offset = (sizeof(tcp_header_t) / 4) << 4;
        tcp->flags = 0x11; 
        tcp->window_size = htons(4096);
        tcp->checksum = 0;
        tcp->urgent_ptr = 0;
        tcp->checksum = calc_tcp_checksum(ip, tcp, NULL, 0);

        netdev_send(frame, sizeof(frame));
    }
    s_tcp_state = TCP_STATE_CLOSED;
}

int net_tcp_is_connected(void) {
    return s_tcp_state == TCP_STATE_ESTABLISHED;
}

int net_tcp_available(void) {
    if (s_tcp_rx_head >= s_tcp_rx_tail) return (int)(s_tcp_rx_head - s_tcp_rx_tail);
    return (int)(sizeof(s_tcp_rx_buf) - s_tcp_rx_tail + s_tcp_rx_head);
}
