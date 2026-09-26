// регистры e1000
#ifndef E1000_H
#define E1000_H

#include <stdint.h>
#include <stdbool.h>

#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 32
#define E1000_RX_BUFFER_SIZE 2048
#define E1000_TX_BUFFER_SIZE 2048

typedef struct {
    uint64_t buffer_addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed)) e1000_rx_desc_t;

typedef struct {
    uint64_t buffer_addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

bool e1000_init(void);
const uint8_t* e1000_get_mac(void);
int e1000_send_packet(const void *data, uint16_t len);
int e1000_poll_packet(void *out_buf, uint16_t max_len);

#endif
