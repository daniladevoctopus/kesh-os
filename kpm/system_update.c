#include "system_update.h"
#include "trusted_keys.h"
#include "bearssl.h"
#include "vfs.h"
#include "memory.h"
#include <stdint.h>

#define KSU_MAGIC 0x3155534BU
#define KSU_VERSION 1
#define KSU_SIGNATURE_SIZE 64
#define KSU_ALGORITHM 1

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t header_size;
    uint16_t version;
    char release[32];
    uint64_t payload_offset;
    uint64_t payload_size;
    uint32_t signature_offset;
    uint16_t signature_size;
    uint16_t signature_algorithm;
    uint8_t key_id[16];
    uint8_t digest[32];
    uint8_t reserved[16];
} ksu_header_t;

static char s_state[128];

static int equal_bytes(const uint8_t *a, const uint8_t *b, size_t size) {
    uint8_t diff = 0;
    for (size_t i = 0; i < size; ++i) diff |= a[i] ^ b[i];
    return diff == 0;
}

static const kpm_trusted_key_t *trusted_key(const uint8_t id[16]) {
    for (size_t i = 0; i < g_kpm_trusted_key_count; ++i) if (equal_bytes(id, g_kpm_trusted_keys[i].id, 16)) return &g_kpm_trusted_keys[i];
    return 0;
}

static int verify_bundle(const void *bundle, size_t size) {
    if (!bundle || size < sizeof(ksu_header_t) + KSU_SIGNATURE_SIZE) return KPM_UPDATE_FORMAT;
    const ksu_header_t *h = bundle;
    if (h->magic != KSU_MAGIC || h->header_size != sizeof(*h) || h->version != KSU_VERSION ||
        h->signature_size != KSU_SIGNATURE_SIZE || h->signature_algorithm != KSU_ALGORITHM ||
        h->payload_offset < sizeof(*h) || h->payload_offset > size || h->payload_size > size - h->payload_offset ||
        h->signature_offset != h->payload_offset + h->payload_size || h->signature_offset + h->signature_size != size) return KPM_UPDATE_FORMAT;
    const kpm_trusted_key_t *key = trusted_key(h->key_id);
    if (!key) return KPM_UPDATE_UNTRUSTED;
    br_sha256_context sha;
    uint8_t digest[32], zeros[32] = {0};
    size_t digest_offset = (size_t)((const uint8_t *)h->digest - (const uint8_t *)bundle);
    br_sha256_init(&sha);
    br_sha256_update(&sha, bundle, digest_offset);
    br_sha256_update(&sha, zeros, sizeof(zeros));
    br_sha256_update(&sha, (const uint8_t *)bundle + digest_offset + sizeof(zeros), h->signature_offset - digest_offset - sizeof(zeros));
    br_sha256_out(&sha, digest);
    if (!equal_bytes(digest, h->digest, sizeof(digest))) return KPM_UPDATE_DIGEST;
    br_ec_public_key public_key = { BR_EC_secp256r1, (unsigned char *)key->public_key, sizeof(key->public_key) };
    return br_ecdsa_i31_vrfy_raw(&br_ec_all_m15, digest, sizeof(digest), &public_key,
                                 (const uint8_t *)bundle + h->signature_offset, h->signature_size) ?
           KPM_UPDATE_OK : KPM_UPDATE_SIGNATURE;
}

static char active_slot(void) {
    int count = vfs_read("/hdd/KUPD.STA", s_state, sizeof(s_state) - 1);
    if (count <= 0) return 'A';
    s_state[count] = 0;
    for (int i = 0; i + 7 < count; ++i) if (s_state[i] == 'a' && s_state[i + 6] == '=') return s_state[i + 7] == 'B' ? 'B' : 'A';
    return 'A';
}

static int write_state(char active, char pending, const char *release, int attempts) {
    int pos = 0;
    const char *a = "active=";
    while (*a) s_state[pos++] = *a++;
    s_state[pos++] = active; s_state[pos++] = '\n';
    const char *p = "pending=";
    while (*p) s_state[pos++] = *p++;
    s_state[pos++] = pending; s_state[pos++] = '\n';
    const char *t = "attempts=";
    while (*t) s_state[pos++] = *t++;
    s_state[pos++] = (char)('0' + (attempts < 0 ? 0 : attempts > 9 ? 9 : attempts)); s_state[pos++] = '\n';
    const char *r = "release=";
    while (*r) s_state[pos++] = *r++;
    for (int i = 0; release && release[i] && i < 31; ++i) s_state[pos++] = release[i];
    s_state[pos++] = '\n';
    return vfs_write("/hdd/KUPD.STA", s_state, pos) == pos ? 0 : -1;
}

int kpm_system_update_stage(const void *bundle, size_t size) {
    int status = verify_bundle(bundle, size);
    if (status != KPM_UPDATE_OK) return status;
    if (size > 64U * 1024U * 1024U) return KPM_UPDATE_STORAGE;
    char active = active_slot(), pending = active == 'A' ? 'B' : 'A';
    const char *directory = pending == 'A' ? "/hdd/KSA" : "/hdd/KSB";
    const char *temporary = pending == 'A' ? "/hdd/KSA/NEW.KSU" : "/hdd/KSB/NEW.KSU";
    const char *destination = pending == 'A' ? "/hdd/KSA/SYSTEM.KSU" : "/hdd/KSB/SYSTEM.KSU";
    if (vfs_mkdir(directory) != 0 && !vfs_get_node(directory)) return KPM_UPDATE_STORAGE;
    if (vfs_write("/hdd/KUPD.TXN", &pending, 1) != 1 || vfs_write(temporary, bundle, (int)size) != (int)size) return KPM_UPDATE_STORAGE;
    size_t pages = (size + PAGE_SIZE - 1U) / PAGE_SIZE;
    uint64_t physical = pmm_alloc_pages(pages);
    if (!physical) { vfs_delete(temporary); vfs_delete("/hdd/KUPD.TXN"); return KPM_UPDATE_STORAGE; }
    uint8_t *verify_buffer = (uint8_t *)(physical + g_hhdm_offset);
    int stored = vfs_read(temporary, verify_buffer, (int)size);
    if (stored != (int)size || verify_bundle(verify_buffer, (size_t)stored) != KPM_UPDATE_OK ||
        vfs_write(destination, verify_buffer, stored) != stored) {
        pmm_free_pages(physical, pages);
        vfs_delete(temporary); vfs_delete("/hdd/KUPD.TXN");
        return KPM_UPDATE_STORAGE;
    }
    pmm_free_pages(physical, pages);
    const ksu_header_t *header = bundle;
    if (write_state(active, pending, header->release, 3) != 0) return KPM_UPDATE_STORAGE;
    vfs_delete(temporary); vfs_delete("/hdd/KUPD.TXN");
    return KPM_UPDATE_OK;
}

int kpm_system_update_confirm(void) {
    int count = vfs_read("/hdd/KUPD.STA", s_state, sizeof(s_state) - 1);
    if (count <= 0) return KPM_UPDATE_STORAGE;
    char pending = 0;
    for (int i = 0; i + 8 < count; ++i) if (s_state[i] == 'p' && s_state[i + 7] == '=') pending = s_state[i + 8];
    if (pending != 'A' && pending != 'B') return KPM_UPDATE_FORMAT;
    return write_state(pending, '-', "confirmed", 0) == 0 ? KPM_UPDATE_OK : KPM_UPDATE_STORAGE;
}

int kpm_system_update_rollback(void) {
    char active = active_slot();
    return write_state(active, '-', "rollback", 0) == 0 ? KPM_UPDATE_OK : KPM_UPDATE_STORAGE;
}

const char *kpm_system_update_error(int status) {
    if (status == KPM_UPDATE_OK) return "ok";
    if (status == KPM_UPDATE_FORMAT) return "invalid update bundle";
    if (status == KPM_UPDATE_UNTRUSTED) return "untrusted update key";
    if (status == KPM_UPDATE_DIGEST) return "update digest mismatch";
    if (status == KPM_UPDATE_SIGNATURE) return "update signature mismatch";
    return "update storage transaction failed";
}
