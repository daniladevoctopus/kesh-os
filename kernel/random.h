#ifndef KESHOS_RANDOM_H
#define KESHOS_RANDOM_H

#include <stddef.h>
#include <stdint.h>

int random_init(void);
int random_is_ready(void);
int random_bytes(void *out, size_t size);
int random_load_persistent_seed(void);
int random_save_persistent_seed(void);
void random_add_entropy(const void *data, size_t size, uint32_t estimated_bits);
void random_get_status(uint64_t *generated_bytes, uint64_t *reseed_count,
                       uint32_t *entropy_bits, uint32_t *health_failures);

#endif
