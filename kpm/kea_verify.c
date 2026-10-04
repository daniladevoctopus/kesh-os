#include "kea_verify.h"
#include "trusted_keys.h"
#include "bearssl.h"

static uint32_t crc32(const uint8_t *data, uint64_t size) {
    uint32_t crc = 0xFFFFFFFFU;
    for (uint64_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320U & (uint32_t)-(int32_t)(crc & 1U));
    }
    return ~crc;
}

static int bytes_equal(const uint8_t *a, const uint8_t *b, size_t size) {
    uint8_t diff = 0;
    for (size_t i = 0; i < size; ++i) diff |= a[i] ^ b[i];
    return diff == 0;
}

static int base_valid(const kea_header_t *h, size_t size) {
    uint32_t allowed = KEA_PERM_NETWORK | KEA_PERM_FS | KEA_PERM_SOUND | KEA_PERM_GUI | KEA_PERM_ROOT;
    if (!h || h->magic != KEA_MAGIC || (h->permissions & ~allowed) != 0) return 0;
    if (h->icon_offset > size || h->icon_size > size - h->icon_offset) return 0;
    if (h->elf_offset > size || h->elf_size > size - h->elf_offset || h->elf_size < 4) return 0;
    return 1;
}

const kea_header_t *kea_base_header(const void *data, size_t size) {
    if (!data || size < sizeof(kea_header_t)) return NULL;
    const kea_header_t *h = (const kea_header_t *)data;
    return base_valid(h, size) ? h : NULL;
}

const kea_dependency_t *kea_dependencies(const void *data, size_t size, uint16_t *count) {
    if (count) *count = 0;
    const kea_header_t *base = kea_base_header(data, size);
    if (!base || base->version != KEA_VERSION || base->hdr_size != sizeof(kea_header_v2_t)) return NULL;
    const kea_header_v2_t *h = (const kea_header_v2_t *)data;
    uint64_t bytes = (uint64_t)h->dependency_count * h->dependency_stride;
    if (h->dependency_count > KEA_MAX_DEPENDENCIES || h->dependency_stride != sizeof(kea_dependency_t) ||
        h->dependency_offset > size || bytes > size - h->dependency_offset) return NULL;
    if (count) *count = h->dependency_count;
    return h->dependency_count ? (const kea_dependency_t *)((const uint8_t *)data + h->dependency_offset) : NULL;
}

int kea_verify_package(const void *data, size_t size, int require_signature) {
    const kea_header_t *base = kea_base_header(data, size);
    if (!base) return KEA_VERIFY_FORMAT;
    if (crc32((const uint8_t *)data + base->elf_offset, base->elf_size) != base->checksum) return KEA_VERIFY_CHECKSUM;
    if (base->version == KEA_VERSION_LEGACY && base->hdr_size == sizeof(kea_header_t)) {
        return require_signature ? KEA_VERIFY_UNSIGNED : KEA_VERIFY_OK;
    }
    if (base->version != KEA_VERSION || base->hdr_size != sizeof(kea_header_v2_t) || size < sizeof(kea_header_v2_t)) return KEA_VERIFY_FORMAT;
    const kea_header_v2_t *h = (const kea_header_v2_t *)data;
    if (!(base->flags & KEA_FLAG_SIGNED) || h->signature_algorithm != KEA_SIGNATURE_ECDSA_P256_SHA256 ||
        h->signature_size != KEA_SIGNATURE_SIZE) return KEA_VERIFY_UNSIGNED;
    if (h->signature_offset < sizeof(*h) || h->signature_offset > size || h->signature_size > size - h->signature_offset ||
        h->signature_offset + h->signature_size != size) return KEA_VERIFY_BOUNDS;
    uint16_t dependency_count = 0;
    if (h->dependency_count && !kea_dependencies(data, size, &dependency_count)) return KEA_VERIFY_BOUNDS;

    const kpm_trusted_key_t *trusted = NULL;
    for (size_t i = 0; i < g_kpm_trusted_key_count; ++i) {
        if (bytes_equal(h->key_id, g_kpm_trusted_keys[i].id, KEA_KEY_ID_SIZE)) {
            trusted = &g_kpm_trusted_keys[i];
            break;
        }
    }
    if (!trusted) return KEA_VERIFY_UNTRUSTED;

    br_sha256_context sha;
    uint8_t digest[KEA_DIGEST_SIZE];
    uint8_t zeros[KEA_DIGEST_SIZE] = {0};
    size_t digest_offset = (size_t)((const uint8_t *)h->package_sha256 - (const uint8_t *)data);
    br_sha256_init(&sha);
    br_sha256_update(&sha, data, digest_offset);
    br_sha256_update(&sha, zeros, sizeof(zeros));
    br_sha256_update(&sha, (const uint8_t *)data + digest_offset + sizeof(zeros),
                     h->signature_offset - digest_offset - sizeof(zeros));
    br_sha256_out(&sha, digest);
    if (!bytes_equal(digest, h->package_sha256, sizeof(digest))) return KEA_VERIFY_DIGEST;

    br_ec_public_key pk;
    pk.curve = BR_EC_secp256r1;
    pk.q = (unsigned char *)trusted->public_key;
    pk.qlen = sizeof(trusted->public_key);
    if (!br_ecdsa_i31_vrfy_raw(&br_ec_all_m15, digest, sizeof(digest), &pk,
                               (const uint8_t *)data + h->signature_offset, h->signature_size)) {
        return KEA_VERIFY_SIGNATURE;
    }
    return KEA_VERIFY_OK;
}

const char *kea_verify_error(int status) {
    switch (status) {
        case KEA_VERIFY_OK: return "ok";
        case KEA_VERIFY_FORMAT: return "invalid KEA format";
        case KEA_VERIFY_BOUNDS: return "invalid package bounds";
        case KEA_VERIFY_CHECKSUM: return "ELF checksum mismatch";
        case KEA_VERIFY_UNSIGNED: return "signature is required";
        case KEA_VERIFY_UNTRUSTED: return "untrusted repository key";
        case KEA_VERIFY_DIGEST: return "SHA-256 mismatch";
        case KEA_VERIFY_SIGNATURE: return "ECDSA signature mismatch";
        default: return "unknown verification error";
    }
}
