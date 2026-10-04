#ifndef KPM_TRUSTED_KEYS_H
#define KPM_TRUSTED_KEYS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t id[16];
    uint8_t public_key[65];
} kpm_trusted_key_t;

extern const kpm_trusted_key_t g_kpm_trusted_keys[];
extern const size_t g_kpm_trusted_key_count;

#endif
