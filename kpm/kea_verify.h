#ifndef KPM_KEA_VERIFY_H
#define KPM_KEA_VERIFY_H

#include <stddef.h>
#include <stdint.h>
#include "kesh/kea.h"

enum {
    KEA_VERIFY_OK = 0,
    KEA_VERIFY_FORMAT = -1,
    KEA_VERIFY_BOUNDS = -2,
    KEA_VERIFY_CHECKSUM = -3,
    KEA_VERIFY_UNSIGNED = -4,
    KEA_VERIFY_UNTRUSTED = -5,
    KEA_VERIFY_DIGEST = -6,
    KEA_VERIFY_SIGNATURE = -7
};

int kea_verify_package(const void *data, size_t size, int require_signature);
const char *kea_verify_error(int status);
const kea_header_t *kea_base_header(const void *data, size_t size);
const kea_dependency_t *kea_dependencies(const void *data, size_t size, uint16_t *count);

#endif
