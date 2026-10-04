#include "kpm_os.h"
#include "kea_verify.h"
#include "system_update.h"
#include "drivers/net/tls/kesh_tls.h"
#include "vfs.h"
#include "memory.h"

#define KPM_VER "2.0.0"
#define KPM_REPO "https://kesh-kpm.vercel.app"
#define KPM_BUFFER_SIZE (1024U * 1024U)
#define KPM_DB_PATH "/hdd/apps/.kpm.db"
#define KPM_TXN_PATH "/hdd/apps/.kpm.txn"

static uint8_t s_buffer[KPM_BUFFER_SIZE];
static char s_db[8192];

static uint32_t s_len(const char *s) {
    uint32_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static void s_copy(char *dst, const char *src, uint32_t cap) {
    uint32_t i = 0;
    if (!cap) return;
    while (src && src[i] && i + 1 < cap) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void s_cat(char *dst, const char *src, uint32_t cap) {
    uint32_t n = s_len(dst), i = 0;
    while (src && src[i] && n + i + 1 < cap) { dst[n + i] = src[i]; i++; }
    dst[n + i] = 0;
}

static int s_eq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

static void split(const char *line, char *cmd, uint32_t cmd_cap, char *arg, uint32_t arg_cap) {
    uint32_t i = 0, n = 0;
    while (line && line[i] == ' ') i++;
    while (line && line[i] && line[i] != ' ' && n + 1 < cmd_cap) cmd[n++] = line[i++];
    cmd[n] = 0;
    while (line && line[i] == ' ') i++;
    n = 0;
    while (line && line[i] && n + 1 < arg_cap) arg[n++] = line[i++];
    arg[n] = 0;
}

static int normalize_id(const char *input, char *out, uint32_t cap) {
    uint32_t n = 0;
    while (input && input[n] && n + 1 < cap) {
        char c = input[n];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0;
        out[n++] = c;
    }
    out[n] = 0;
    return n > 0 && (!input[n]);
}

static void package_path(char *out, uint32_t cap, const char *id, const char *suffix) {
    s_copy(out, "/hdd/apps/", cap);
    s_cat(out, id, cap);
    s_cat(out, suffix && suffix[0] ? suffix : ".kea", cap);
}

static const uint8_t *http_body(size_t response_size, size_t *body_size) {
    for (size_t i = 0; i + 3 < response_size; ++i) {
        if (s_buffer[i] == '\r' && s_buffer[i + 1] == '\n' && s_buffer[i + 2] == '\r' && s_buffer[i + 3] == '\n') {
            *body_size = response_size - i - 4;
            return s_buffer + i + 4;
        }
    }
    return NULL;
}

static const uint8_t *http_body_from(const uint8_t *buffer, size_t response_size, size_t *body_size) {
    for (size_t i = 0; i + 3 < response_size; ++i) {
        if (buffer[i] == '\r' && buffer[i + 1] == '\n' && buffer[i + 2] == '\r' && buffer[i + 3] == '\n') {
            *body_size = response_size - i - 4;
            return buffer + i + 4;
        }
    }
    return NULL;
}

static int download_url(const char *url, const uint8_t **body, size_t *body_size) {
    int status = 0;
    int result = kesh_https_get(url, (char *)s_buffer, sizeof(s_buffer), &status);
    if (result != KESH_HTTP_OK || status != 200) return -1;
    *body = http_body(kesh_tls_last_response_len(), body_size);
    return *body ? 0 : -2;
}

static int download(const char *id, const uint8_t **body, size_t *body_size) {
    char url[160];
    s_copy(url, KPM_REPO "/api/download?pkg=", sizeof(url));
    s_cat(url, id, sizeof(url));
    s_cat(url, ".kea", sizeof(url));
    return download_url(url, body, body_size);
}

static int version_at_least(const char *have, const char *need) {
    uint32_t hi = 0, ni = 0;
    for (int part = 0; part < 4; ++part) {
        uint32_t hv = 0, nv = 0;
        while (have[hi] >= '0' && have[hi] <= '9') hv = hv * 10 + (uint32_t)(have[hi++] - '0');
        while (need[ni] >= '0' && need[ni] <= '9') nv = nv * 10 + (uint32_t)(need[ni++] - '0');
        if (hv != nv) return hv > nv;
        if (have[hi] == '.') hi++;
        if (need[ni] == '.') ni++;
    }
    return 1;
}

static int installed_version(const char *id, const char *minimum) {
    char path[96];
    package_path(path, sizeof(path), id, "");
    kesh_vfs_stat_t st;
    if (vfs_stat(path, &st) != 0 || st.size > sizeof(s_buffer)) return 0;
    int got = vfs_read(path, s_buffer, (int)sizeof(s_buffer));
    if (got <= 0 || kea_verify_package(s_buffer, (size_t)got, 1) != KEA_VERIFY_OK) return 0;
    const kea_header_t *h = (const kea_header_t *)s_buffer;
    return version_at_least(h->app_version, minimum);
}

static int rebuild_db(void) {
    kesh_vfs_entry_t entries[VFS_MAX_CHILDREN];
    int count = vfs_list("/hdd/apps", entries, VFS_MAX_CHILDREN);
    uint32_t used = 0;
    if (count < 0) return -1;
    for (int i = 0; i < count; ++i) {
        uint32_t n = s_len(entries[i].name);
        if (entries[i].is_dir || n < 5 || !s_eq(entries[i].name + n - 4, ".kea")) continue;
        char path[96];
        s_copy(path, "/hdd/apps/", sizeof(path));
        s_cat(path, entries[i].name, sizeof(path));
        int got = vfs_read(path, s_buffer, (int)sizeof(s_buffer));
        const kea_header_t *h = got > 0 ? kea_base_header(s_buffer, (size_t)got) : NULL;
        if (!h) continue;
        for (uint32_t j = 0; j < n - 4 && used + 1 < sizeof(s_db); ++j) s_db[used++] = entries[i].name[j];
        if (used + 2 < sizeof(s_db)) s_db[used++] = '|';
        for (uint32_t j = 0; h->app_version[j] && j < sizeof(h->app_version) && used + 1 < sizeof(s_db); ++j) s_db[used++] = h->app_version[j];
        if (used + 1 < sizeof(s_db)) s_db[used++] = '\n';
    }
    s_db[used] = 0;
    return vfs_write(KPM_DB_PATH, s_db, (int)used) == (int)used ? 0 : -2;
}

static void recover_transaction(void) {
    int got = vfs_read(KPM_TXN_PATH, s_db, 63);
    if (got <= 0) return;
    s_db[got] = 0;
    char dst[96], pending[96], backup[96];
    package_path(dst, sizeof(dst), s_db, "");
    package_path(pending, sizeof(pending), s_db, ".new");
    package_path(backup, sizeof(backup), s_db, ".bak");
    kesh_vfs_stat_t st;
    if (vfs_stat(backup, &st) == 0 && st.size <= sizeof(s_buffer)) {
        int old_size = vfs_read(backup, s_buffer, (int)sizeof(s_buffer));
        if (old_size > 0) vfs_write(dst, s_buffer, old_size);
    } else {
        vfs_delete(dst);
    }
    vfs_delete(pending);
    vfs_delete(backup);
    vfs_delete(KPM_TXN_PATH);
    rebuild_db();
}

static int commit_package(const char *id, const uint8_t *data, size_t size) {
    char dst[96], pending[96], backup[96];
    package_path(dst, sizeof(dst), id, "");
    package_path(pending, sizeof(pending), id, ".new");
    package_path(backup, sizeof(backup), id, ".bak");
    if (vfs_write(pending, data, (int)size) != (int)size) return -1;
    if (vfs_write(KPM_TXN_PATH, id, (int)s_len(id)) != (int)s_len(id)) return -2;
    kesh_vfs_stat_t st;
    if (vfs_stat(dst, &st) == 0 && st.size <= sizeof(s_buffer)) {
        int old_size = vfs_read(dst, s_buffer, (int)sizeof(s_buffer));
        if (old_size <= 0 || vfs_write(backup, s_buffer, old_size) != old_size) {
            recover_transaction();
            return -3;
        }
    }
    int pending_size = vfs_read(pending, s_buffer, (int)sizeof(s_buffer));
    if (pending_size <= 0 || kea_verify_package(s_buffer, (size_t)pending_size, 1) != KEA_VERIFY_OK ||
        vfs_write(dst, s_buffer, pending_size) != pending_size || rebuild_db() != 0) {
        recover_transaction();
        return -4;
    }
    vfs_delete(pending);
    vfs_delete(backup);
    vfs_delete(KPM_TXN_PATH);
    return 0;
}

static int install_package(const char *id, int depth, void (*print_fn)(const char *)) {
    if (depth > 8) return -10;
    const uint8_t *body;
    size_t body_size;
    if (download(id, &body, &body_size) != 0) return -1;
    int verified = kea_verify_package(body, body_size, 1);
    if (verified != KEA_VERIFY_OK) {
        print_fn(kea_verify_error(verified));
        return -2;
    }
    uint16_t dep_count = 0;
    const kea_dependency_t *deps = kea_dependencies(body, body_size, &dep_count);
    kea_dependency_t saved[KEA_MAX_DEPENDENCIES];
    for (uint16_t i = 0; i < dep_count; ++i) saved[i] = deps[i];
    for (uint16_t i = 0; i < dep_count; ++i) {
        char dep_id[33], dep_version[17];
        s_copy(dep_id, saved[i].name, sizeof(dep_id));
        s_copy(dep_version, saved[i].min_version, sizeof(dep_version));
        if (!normalize_id(dep_id, dep_id, sizeof(dep_id))) return -3;
        if (!installed_version(dep_id, dep_version) && install_package(dep_id, depth + 1, print_fn) != 0) return -4;
    }
    if (download(id, &body, &body_size) != 0 || kea_verify_package(body, body_size, 1) != KEA_VERIFY_OK) return -5;
    return commit_package(id, body, body_size);
}

static int package_is_required(const char *id) {
    kesh_vfs_entry_t entries[VFS_MAX_CHILDREN];
    int count = vfs_list("/hdd/apps", entries, VFS_MAX_CHILDREN);
    for (int i = 0; i < count; ++i) {
        uint32_t n = s_len(entries[i].name);
        if (n < 5 || !s_eq(entries[i].name + n - 4, ".kea")) continue;
        char path[96];
        s_copy(path, "/hdd/apps/", sizeof(path)); s_cat(path, entries[i].name, sizeof(path));
        int got = vfs_read(path, s_buffer, (int)sizeof(s_buffer));
        uint16_t dep_count = 0;
        const kea_dependency_t *deps = got > 0 ? kea_dependencies(s_buffer, (size_t)got, &dep_count) : NULL;
        for (uint16_t d = 0; deps && d < dep_count; ++d) {
            char dep_id[33]; s_copy(dep_id, deps[d].name, sizeof(dep_id));
            if (s_eq(dep_id, id)) return 1;
        }
    }
    return 0;
}

void kpm_cmd_exec(const char *args, void (*print_fn)(const char *line)) {
    if (!print_fn) return;
    if (vfs_mkdir("/hdd/apps") != 0 && !vfs_get_node("/hdd/apps")) { print_fn("[KPM] /hdd/apps is unavailable"); return; }
    recover_transaction();
    char command[16], argument[48], id[48];
    split(args, command, sizeof(command), argument, sizeof(argument));
    uint32_t arg_len = s_len(argument);
    if (arg_len > 4 && s_eq(argument + arg_len - 4, ".kea")) argument[arg_len - 4] = 0;

    if (!command[0] || s_eq(command, "help")) {
        print_fn("Kesh Package Manager v" KPM_VER);
        print_fn("kpm install <id> | remove <id> | list | info <id> | update");
        print_fn("kpm os-update | os-confirm | os-rollback");
        return;
    }
    if (s_eq(command, "install") || s_eq(command, "i")) {
        if (!normalize_id(argument, id, sizeof(id))) { print_fn("[KPM] Invalid package id"); return; }
        print_fn("[KPM] Downloading and verifying signed dependency graph...");
        if (install_package(id, 0, print_fn) == 0) print_fn("[KPM] Transaction committed");
        else print_fn("[KPM] Installation aborted and rolled back");
        return;
    }
    if (s_eq(command, "remove") || s_eq(command, "rm")) {
        if (!normalize_id(argument, id, sizeof(id))) { print_fn("[KPM] Invalid package id"); return; }
        if (package_is_required(id)) { print_fn("[KPM] Package is required by an installed application"); return; }
        char path[96]; package_path(path, sizeof(path), id, "");
        if (vfs_delete(path) == 0 && rebuild_db() == 0) print_fn("[KPM] Package removed");
        else print_fn("[KPM] Package not found or database update failed");
        return;
    }
    if (s_eq(command, "list") || s_eq(command, "ls")) {
        if (rebuild_db() != 0) { print_fn("[KPM] Cannot read package database"); return; }
        print_fn("Installed packages:");
        uint32_t start = 0;
        for (uint32_t i = 0; s_db[i]; ++i) if (s_db[i] == '\n') { s_db[i] = 0; print_fn(s_db + start); start = i + 1; }
        return;
    }
    if (s_eq(command, "info")) {
        if (!normalize_id(argument, id, sizeof(id))) { print_fn("[KPM] Invalid package id"); return; }
        char path[96]; package_path(path, sizeof(path), id, "");
        int got = vfs_read(path, s_buffer, (int)sizeof(s_buffer));
        const kea_header_t *h = got > 0 ? kea_base_header(s_buffer, (size_t)got) : NULL;
        if (!h) { print_fn("[KPM] Package not installed"); return; }
        print_fn(h->name); print_fn(h->app_version); print_fn(h->description);
        print_fn(kea_verify_package(s_buffer, (size_t)got, 1) == 0 ? "Signature: trusted" : "Signature: invalid");
        return;
    }
    if (s_eq(command, "update")) {
        int status = 0;
        int result = kesh_https_get(KPM_REPO "/api/catalog", (char *)s_buffer, sizeof(s_buffer), &status);
        size_t body_size = 0;
        const uint8_t *body = http_body(kesh_tls_last_response_len(), &body_size);
        if (result == KESH_HTTP_OK && status == 200 && body && vfs_write("/hdd/apps/.kpm-index.json", body, (int)body_size) == (int)body_size)
            print_fn("[KPM] Repository index updated");
        else print_fn("[KPM] Repository update failed");
        return;
    }
    if (s_eq(command, "os-update")) {
        size_t capacity = 64U * 1024U * 1024U;
        size_t pages = capacity / PAGE_SIZE;
        uint64_t physical = pmm_alloc_pages(pages);
        if (!physical) { print_fn("[KPM] Not enough memory for update download"); return; }
        uint8_t *response = (uint8_t *)(physical + g_hhdm_offset);
        int http_status = 0;
        int request = kesh_https_get(KPM_REPO "/api/system-update", (char *)response, capacity, &http_status);
        size_t body_size = 0;
        const uint8_t *body = http_body_from(response, kesh_tls_last_response_len(), &body_size);
        if (request != KESH_HTTP_OK || http_status != 200 || !body) {
            pmm_free_pages(physical, pages);
            print_fn("[KPM] System update download failed");
            return;
        }
        int status = kpm_system_update_stage(body, body_size);
        pmm_free_pages(physical, pages);
        print_fn(status == KPM_UPDATE_OK ? "[KPM] Signed update staged in inactive slot" : kpm_system_update_error(status));
        return;
    }
    if (s_eq(command, "os-confirm")) {
        int status = kpm_system_update_confirm();
        print_fn(status == KPM_UPDATE_OK ? "[KPM] Pending system slot confirmed" : kpm_system_update_error(status));
        return;
    }
    if (s_eq(command, "os-rollback")) {
        int status = kpm_system_update_rollback();
        print_fn(status == KPM_UPDATE_OK ? "[KPM] Pending system slot cancelled" : kpm_system_update_error(status));
        return;
    }
    print_fn("[KPM] Unknown command");
}
