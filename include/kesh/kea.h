#ifndef KESH_KEA_H
#define KESH_KEA_H

#include <stdint.h>

#define KEA_MAGIC 0x0141454BU
#define KEA_VERSION_LEGACY 1
#define KEA_VERSION 2
#define KEA_FLAG_SIGNED (1U << 0)
#define KEA_SIGNATURE_ECDSA_P256_SHA256 1
#define KEA_SIGNATURE_SIZE 64
#define KEA_KEY_ID_SIZE 16
#define KEA_DIGEST_SIZE 32
#define KEA_MAX_DEPENDENCIES 16

#define KEA_PERM_NETWORK (1U << 0)
#define KEA_PERM_FS      (1U << 1)
#define KEA_PERM_SOUND   (1U << 2)
#define KEA_PERM_GUI     (1U << 3)
#define KEA_PERM_ROOT    (1U << 4)

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint32_t hdr_size;
    uint16_t version;
    uint16_t flags;
    char name[32];
    char app_version[16];
    char author[32];
    char category[16];
    char description[64];
    uint32_t permissions;
    uint32_t icon_offset;
    uint32_t icon_size;
    uint16_t icon_width;
    uint16_t icon_height;
    uint64_t elf_offset;
    uint64_t elf_size;
    uint64_t entry_point;
    uint32_t checksum;
    uint32_t reserved[8];
} kea_header_t;

typedef struct {
    char name[32];
    char min_version[16];
} kea_dependency_t;

typedef struct {
    kea_header_t base;
    uint32_t dependency_offset;
    uint16_t dependency_count;
    uint16_t dependency_stride;
    uint32_t signature_offset;
    uint16_t signature_size;
    uint16_t signature_algorithm;
    uint8_t key_id[KEA_KEY_ID_SIZE];
    uint8_t package_sha256[KEA_DIGEST_SIZE];
} kea_header_v2_t;
#pragma pack(pop)

#endif
