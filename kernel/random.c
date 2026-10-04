#include "random.h"
#include "serial.h"
#include "vfs.h"
#include <stdint.h>
#include <bearssl.h>

static br_hmac_drbg_context g_drbg;
static br_sha256_context g_entropy_pool;
static volatile uint32_t g_random_lock = 0;
static int g_random_ready = 0;
static int g_has_rdseed = 0;
static int g_has_rdrand = 0;
static uint64_t g_bytes_since_reseed = 0;
static uint64_t g_generated_bytes = 0;
static uint64_t g_reseed_count = 0;
static uint64_t g_last_hw_word = 0;
static uint32_t g_entropy_bits = 0;
static uint32_t g_health_failures = 0;
static int g_last_hw_valid = 0;
static int g_pool_ready = 0;

#define RANDOM_RESEED_INTERVAL (1024ULL * 1024ULL)
#define RANDOM_SEED_PATH "/hdd/RNGSEED.BIN"

typedef struct {
    uint8_t magic[8];
    uint64_t generation;
    uint8_t seed[32];
    uint8_t digest[32];
} __attribute__((packed)) random_seed_file_t;

static uint64_t g_seed_generation;

static void random_seed_digest(const random_seed_file_t *record, uint8_t digest[32]) {
    br_sha256_context context;
    br_sha256_init(&context);
    br_sha256_update(&context, record, offsetof(random_seed_file_t, digest));
    br_sha256_out(&context, digest);
}

static int bytes_equal(const uint8_t *a, const uint8_t *b, size_t size) {
    uint8_t difference = 0;
    for (size_t i = 0; i < size; ++i) difference |= a[i] ^ b[i];
    return difference == 0;
}

static int cpu_feature(uint32_t leaf, uint32_t subleaf, uint32_t reg, uint32_t bit) {
    uint32_t max_leaf, ebx, ecx, edx;
    uint32_t eax = 0;
    __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    max_leaf = eax;
    if (max_leaf < leaf) return 0;
    eax = leaf;
    ecx = subleaf;
    __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx));
    uint32_t value = reg == 1 ? ebx : reg == 2 ? ecx : edx;
    return (value & (1U << bit)) != 0;
}

static int random_hw_word(uint64_t *value) {
    for (int attempt = 0; attempt < 16; ++attempt) {
        unsigned char ok;
        if (g_has_rdseed) {
            __asm__ volatile("rdseed %0; setc %1" : "=r"(*value), "=qm"(ok));
        } else if (g_has_rdrand) {
            __asm__ volatile("rdrand %0; setc %1" : "=r"(*value), "=qm"(ok));
        } else {
            return 0;
        }
        if (ok) {
            if (g_last_hw_valid && *value == g_last_hw_word) {
                g_health_failures++;
                return 0;
            }
            g_last_hw_word = *value;
            g_last_hw_valid = 1;
            return 1;
        }
    }
    return 0;
}

static uint64_t random_irq_save(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0" : "=r"(flags));
    __asm__ volatile("cli" : : : "memory");
    return flags;
}

static void random_irq_restore(uint64_t flags) {
    if (flags & (1ULL << 9)) __asm__ volatile("sti" : : : "memory");
}

static void random_mix_pool_locked(void) {
    uint8_t digest[32];
    br_sha256_out(&g_entropy_pool, digest);
    br_hmac_drbg_update(&g_drbg, digest, sizeof(digest));
    br_sha256_init(&g_entropy_pool);
    br_sha256_update(&g_entropy_pool, digest, sizeof(digest));
    for (size_t i = 0; i < sizeof(digest); ++i) digest[i] = 0;
    g_entropy_bits = 0;
}

static int random_reseed_locked(void) {
    uint8_t seed[48];
    int have_hardware = 1;
    for (size_t offset = 0; offset < sizeof(seed); offset += sizeof(uint64_t)) {
        uint64_t word;
        if (!random_hw_word(&word)) { have_hardware = 0; break; }
        for (size_t i = 0; i < sizeof(word); ++i) seed[offset + i] = (uint8_t)(word >> (i * 8U));
    }
    if (have_hardware) br_hmac_drbg_update(&g_drbg, seed, sizeof(seed));
    for (size_t i = 0; i < sizeof(seed); ++i) seed[i] = 0;
    int have_pool = g_entropy_bits >= 256;
    if (have_pool) random_mix_pool_locked();
    if (!have_hardware && !have_pool) return -1;
    g_bytes_since_reseed = 0;
    g_reseed_count++;
    return 0;
}

static int random_hardware_health_test(void) {
    uint32_t ones[64];
    for (int bit = 0; bit < 64; ++bit) ones[bit] = 0;
    for (int sample = 0; sample < 64; ++sample) {
        uint64_t word = 0;
        if (!random_hw_word(&word)) return -1;
        br_sha256_update(&g_entropy_pool, &word, sizeof(word));
        for (int bit = 0; bit < 64; ++bit) ones[bit] += (uint32_t)((word >> bit) & 1ULL);
    }
    for (int bit = 0; bit < 64; ++bit) {
        if (ones[bit] < 8 || ones[bit] > 56) { g_health_failures++; return -1; }
    }
    return 0;
}

int random_init(void) {
    uint8_t seed[48];
    br_sha256_init(&g_entropy_pool);
    g_pool_ready = 1;
    g_entropy_bits = 0;
    g_health_failures = 0;
    g_last_hw_valid = 0;
    g_generated_bytes = 0;
    g_reseed_count = 0;
    g_seed_generation = 0;
    g_has_rdseed = cpu_feature(7, 0, 1, 18);
    g_has_rdrand = cpu_feature(1, 0, 2, 30);
    if (!g_has_rdseed && !g_has_rdrand) {
        serial_print("[RNG] Hardware entropy source unavailable.\n");
        return -1;
    }
    if (random_hardware_health_test() != 0) {
        serial_print("[RNG] Hardware entropy health test failed.\n");
        return -1;
    }

    for (size_t offset = 0; offset < sizeof(seed); offset += sizeof(uint64_t)) {
        uint64_t word;
        if (!random_hw_word(&word)) {
            serial_print("[RNG] Hardware entropy source did not return data.\n");
            return -1;
        }
        for (size_t i = 0; i < sizeof(word); ++i) {
            seed[offset + i] = (uint8_t)(word >> (i * 8U));
        }
    }

    br_hmac_drbg_init(&g_drbg, &br_sha256_vtable, seed, sizeof(seed));
    br_sha256_update(&g_entropy_pool, seed, sizeof(seed));
    for (size_t i = 0; i < sizeof(seed); ++i) seed[i] = 0;
    g_random_ready = 1;
    g_bytes_since_reseed = 0;
    serial_print(g_has_rdseed ? "[RNG] HMAC-DRBG ready; seed source=RDSEED.\n"
                               : "[RNG] HMAC-DRBG ready; seed source=RDRAND.\n");
    return 0;
}

int random_is_ready(void) {
    return g_random_ready;
}

int random_bytes(void *out, size_t size) {
    if (!out || size == 0 || !g_random_ready) return -1;
    uint64_t flags = random_irq_save();
    while (__atomic_test_and_set(&g_random_lock, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("pause");
    }
    if ((g_bytes_since_reseed >= RANDOM_RESEED_INTERVAL || g_entropy_bits >= 256) && random_reseed_locked() != 0) {
        __atomic_clear(&g_random_lock, __ATOMIC_RELEASE);
        random_irq_restore(flags);
        return -1;
    }
    br_hmac_drbg_generate(&g_drbg, out, size);
    g_bytes_since_reseed += size;
    g_generated_bytes += size;
    __atomic_clear(&g_random_lock, __ATOMIC_RELEASE);
    random_irq_restore(flags);
    return 0;
}

int random_load_persistent_seed(void) {
    if (!g_random_ready) return -1;
    random_seed_file_t record;
    int size = vfs_read(RANDOM_SEED_PATH, &record, sizeof(record));
    static const uint8_t magic[8] = {'K','R','N','G','S','E','E','D'};
    uint8_t digest[32];
    if (size != (int)sizeof(record)) return random_save_persistent_seed();
    random_seed_digest(&record, digest);
    if (!bytes_equal(record.magic, magic, sizeof(magic)) || !bytes_equal(record.digest, digest, sizeof(digest))) return -1;
    uint64_t flags = random_irq_save();
    while (__atomic_test_and_set(&g_random_lock, __ATOMIC_ACQUIRE)) __asm__ volatile("pause");
    br_hmac_drbg_update(&g_drbg, record.seed, sizeof(record.seed));
    br_sha256_update(&g_entropy_pool, &record.generation, sizeof(record.generation));
    br_sha256_update(&g_entropy_pool, record.seed, sizeof(record.seed));
    g_seed_generation = record.generation;
    g_reseed_count++;
    __atomic_clear(&g_random_lock, __ATOMIC_RELEASE);
    random_irq_restore(flags);
    for (size_t i = 0; i < sizeof(record); ++i) ((uint8_t *)&record)[i] = 0;
    return random_save_persistent_seed();
}

int random_save_persistent_seed(void) {
    if (!g_random_ready) return -1;
    random_seed_file_t record;
    static const uint8_t magic[8] = {'K','R','N','G','S','E','E','D'};
    for (size_t i = 0; i < sizeof(record); ++i) ((uint8_t *)&record)[i] = 0;
    for (size_t i = 0; i < sizeof(magic); ++i) record.magic[i] = magic[i];
    record.generation = ++g_seed_generation;
    if (random_bytes(record.seed, sizeof(record.seed)) != 0) return -1;
    random_seed_digest(&record, record.digest);
    int written = vfs_write(RANDOM_SEED_PATH, &record, sizeof(record));
    for (size_t i = 0; i < sizeof(record); ++i) ((uint8_t *)&record)[i] = 0;
    return written == (int)sizeof(record) ? 0 : -1;
}

void random_add_entropy(const void *data, size_t size, uint32_t estimated_bits) {
    if (!data || !size || !g_pool_ready) return;
    uint64_t flags = random_irq_save();
    while (__atomic_test_and_set(&g_random_lock, __ATOMIC_ACQUIRE)) __asm__ volatile("pause");
    uint64_t tsc;
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    tsc = ((uint64_t)hi << 32) | lo;
    br_sha256_update(&g_entropy_pool, &tsc, sizeof(tsc));
    br_sha256_update(&g_entropy_pool, data, size);
    if (estimated_bits > 32) estimated_bits = 32;
    g_entropy_bits = g_entropy_bits + estimated_bits > 512 ? 512 : g_entropy_bits + estimated_bits;
    __atomic_clear(&g_random_lock, __ATOMIC_RELEASE);
    random_irq_restore(flags);
}

void random_get_status(uint64_t *generated_bytes, uint64_t *reseed_count,
                       uint32_t *entropy_bits, uint32_t *health_failures) {
    uint64_t flags = random_irq_save();
    while (__atomic_test_and_set(&g_random_lock, __ATOMIC_ACQUIRE)) __asm__ volatile("pause");
    if (generated_bytes) *generated_bytes = g_generated_bytes;
    if (reseed_count) *reseed_count = g_reseed_count;
    if (entropy_bits) *entropy_bits = g_entropy_bits;
    if (health_failures) *health_failures = g_health_failures;
    __atomic_clear(&g_random_lock, __ATOMIC_RELEASE);
    random_irq_restore(flags);
}
