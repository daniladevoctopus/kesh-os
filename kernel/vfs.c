// виртуальная файловая система, монтирование
#include "vfs.h"
#include "drivers/system/ata.h"
#include "drivers/system/fat32.h"
#include "drivers/system/iso9660.h"
#include "memory.h"
#include <stddef.h>

#define MAX_NODES 256
static vfs_node_t g_nodes[MAX_NODES];
static int g_node_count = 0;

static vfs_node_t *g_root = NULL;
static int g_fat32_available = 0;
static int g_iso9660_available = 0;
static vfs_node_t* find_node_by_path(const char *path);

static void str_copy(char *dst, const char *src, int max) {
    int i = 0;
    while (src && src[i] && i + 1 < max) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int str_eq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return (*a == *b);
}

static int str_len(const char *s) {
    if (!s) return 0;
    int len = 0;
    while (s[len]) len++;
    return len;
}

static vfs_node_t* alloc_node(const char *name, uint8_t is_dir, uint32_t size, const uint8_t *data, vfs_node_t *parent) {
    if (g_node_count >= MAX_NODES) return NULL;
    vfs_node_t *node = &g_nodes[g_node_count++];
    str_copy(node->name, name, VFS_NAME_MAX);
    node->is_dir = is_dir;
    node->size = size;
    node->data = data;
    node->parent = parent ? parent : node;
    node->child_count = 0;
    node->is_fat32 = 0;
    node->fat_cluster = 0;
    node->fat_populated = 0;
    node->is_iso9660 = 0;
    node->iso_lba = 0;
    node->iso_populated = 0;
    for (int i = 0; i < VFS_MAX_CHILDREN; i++) node->children[i] = NULL;

    if (parent && parent->child_count < VFS_MAX_CHILDREN) {
        parent->children[parent->child_count++] = node;
    }
    return node;
}

static void populate_fat32_node(vfs_node_t *dir) {
    if (!dir || !dir->is_fat32 || dir->fat_populated) return;
    dir->fat_populated = 1;
    dir->child_count = 0;

    fat32_entry_t entries[VFS_MAX_CHILDREN];
    int count = fat32_list_dir(dir->fat_cluster, entries, VFS_MAX_CHILDREN);
    for (int i = 0; i < count && dir->child_count < VFS_MAX_CHILDREN; i++) {
        vfs_node_t *child = alloc_node(entries[i].name, entries[i].is_dir ? 1 : 0, entries[i].size, NULL, dir);
        if (child) {
            child->is_fat32 = 1;
            child->fat_cluster = entries[i].first_cluster;
            child->fat_populated = 0;
        }
    }
}

static void populate_iso9660_node(vfs_node_t *dir) {
    if (!dir || !dir->is_iso9660 || dir->iso_populated) return;
    dir->iso_populated = 1;
    dir->child_count = 0;

    iso9660_entry_t entries[VFS_MAX_CHILDREN];
    int count = iso9660_list_dir(dir->iso_lba, dir->size, entries, VFS_MAX_CHILDREN);
    for (int i = 0; i < count && dir->child_count < VFS_MAX_CHILDREN; i++) {
        vfs_node_t *child = alloc_node(entries[i].name, entries[i].is_dir ? 1 : 0, entries[i].size, NULL, dir);
        if (child) {
            child->is_iso9660 = 1;
            child->iso_lba = entries[i].lba;
            child->iso_populated = 0;
        }
    }
}

extern const uint8_t notepad_kea_start[] __attribute__((weak));
extern const uint8_t notepad_kea_end[] __attribute__((weak));
extern const uint8_t explorer_kea_start[] __attribute__((weak));
extern const uint8_t explorer_kea_end[] __attribute__((weak));
extern const uint8_t taskmgr_kea_start[] __attribute__((weak));
extern const uint8_t taskmgr_kea_end[] __attribute__((weak));
extern const uint8_t paint_kea_start[] __attribute__((weak));
extern const uint8_t paint_kea_end[] __attribute__((weak));
extern const uint8_t shell_kea_start[] __attribute__((weak));
extern const uint8_t shell_kea_end[] __attribute__((weak));

vfs_node_t* vfs_get_node(const char *path) {
    return find_node_by_path(path);
}

void vfs_init(void) {
    g_node_count = 0;

    g_root = alloc_node("/", 1, 0, NULL, NULL);

    vfs_node_t *apps_dir = alloc_node("apps", 1, 0, NULL, g_root);
    if (notepad_kea_start && notepad_kea_end) {
        uint32_t sz = (uint32_t)(notepad_kea_end - notepad_kea_start);
        alloc_node("notepad.kea", 0, sz, notepad_kea_start, apps_dir);
    }
    if (explorer_kea_start && explorer_kea_end) {
        uint32_t sz = (uint32_t)(explorer_kea_end - explorer_kea_start);
        alloc_node("explorer.kea", 0, sz, explorer_kea_start, apps_dir);
    }
    if (taskmgr_kea_start && taskmgr_kea_end) {
        uint32_t sz = (uint32_t)(taskmgr_kea_end - taskmgr_kea_start);
        alloc_node("taskmgr.kea", 0, sz, taskmgr_kea_start, apps_dir);
    }
    if (paint_kea_start && paint_kea_end) {
        uint32_t sz = (uint32_t)(paint_kea_end - paint_kea_start);
        alloc_node("paint.kea", 0, sz, paint_kea_start, apps_dir);
    }
    if (shell_kea_start && shell_kea_end) {
        uint32_t sz = (uint32_t)(shell_kea_end - shell_kea_start);
        alloc_node("shell.kea", 0, sz, shell_kea_start, apps_dir);
    }

    vfs_node_t *bin_dir = alloc_node("bin", 1, 0, NULL, g_root);
    alloc_node("notepad.elf", 0, 16384, NULL, bin_dir);
    alloc_node("explorer.elf", 0, 32768, NULL, bin_dir);
    alloc_node("calc.elf", 0, 12288, NULL, bin_dir);

    vfs_rescan_drives();
}

void vfs_rescan_drives(void) {
    if (!g_root) return;

    ata_init();

    int hdd_idx = ata_get_primary_hdd();
    vfs_node_t *existing_hdd = NULL;
    for (int i = 0; i < g_root->child_count; i++) {
        if (str_eq(g_root->children[i]->name, "hdd") || str_eq(g_root->children[i]->name, "c")) {
            existing_hdd = g_root->children[i];
            break;
        }
    }

    if (hdd_idx >= 0) {
        if (!existing_hdd) {
            existing_hdd = alloc_node("hdd", 1, 0, NULL, g_root);
        }
        if (existing_hdd) {
            str_copy(existing_hdd->name, "hdd", VFS_NAME_MAX);
            existing_hdd->child_count = 0;
            g_fat32_available = fat32_init();
            if (g_fat32_available) {
                existing_hdd->is_fat32 = 1;
                existing_hdd->fat_cluster = fat32_root_cluster();
                existing_hdd->fat_populated = 0;
                populate_fat32_node(existing_hdd);
            } else {
                existing_hdd->is_fat32 = 0;
                existing_hdd->fat_populated = 1;
            }
        }
    }

    g_iso9660_available = iso9660_init();
    vfs_node_t *existing_cd = NULL;
    for (int i = 0; i < g_root->child_count; i++) {
        if (str_eq(g_root->children[i]->name, "cdrom")) {
            existing_cd = g_root->children[i];
            break;
        }
    }

    if (g_iso9660_available) {
        const iso9660_info_t *iso = iso9660_get_primary_info();
        if (!existing_cd) {
            existing_cd = alloc_node("cdrom", 1, 0, NULL, g_root);
        }
        if (existing_cd) {
            existing_cd->child_count = 0;
            existing_cd->is_iso9660 = 1;
            existing_cd->iso_lba = iso->root_lba;
            existing_cd->size = iso->root_size;
            existing_cd->iso_populated = 0;
            populate_iso9660_node(existing_cd);
        }
    } else {

        if (existing_cd) {
            existing_cd->child_count = 0;
            existing_cd->iso_populated = 1;
        }
    }
}

static vfs_node_t* find_node_by_path(const char *path) {
    if (!path || !g_root) return NULL;
    if (path[0] == '\0' || str_eq(path, "/") || str_eq(path, "\\")) {
        return g_root;
    }

    const char *p = path;
    if (*p == '/' || *p == '\\') p++;
    if ((p[0] == 'C' || p[0] == 'c') && p[1] == ':') {
        p += 2;
        if (*p == '/' || *p == '\\') p++;
    }

    if (*p == '\0') {

        for (int i = 0; i < g_root->child_count; i++) {
            if (str_eq(g_root->children[i]->name, "c")) return g_root->children[i];
        }
        return g_root;
    }

    vfs_node_t *curr = g_root;
    char comp[VFS_NAME_MAX];

    while (*p) {
        int idx = 0;
        while (*p && *p != '/' && *p != '\\' && idx + 1 < VFS_NAME_MAX) {
            comp[idx++] = *p++;
        }
        comp[idx] = '\0';
        if (*p == '/' || *p == '\\') p++;

        if (str_eq(comp, ".")) continue;
        if (str_eq(comp, "..")) {
            curr = curr->parent;
            continue;
        }

        if (curr->is_fat32 && !curr->fat_populated) {
            populate_fat32_node(curr);
        }

        if (curr->is_iso9660 && !curr->iso_populated) {
            populate_iso9660_node(curr);
        }

        vfs_node_t *found = NULL;
        for (int i = 0; i < curr->child_count; i++) {
            if (str_eq(curr->children[i]->name, comp) ||
                (str_eq(comp, "c") && str_eq(curr->children[i]->name, "hdd")) ||
                (str_eq(comp, "hdd") && str_eq(curr->children[i]->name, "c"))) {
                found = curr->children[i];
                break;
            }
        }
        if (!found) return NULL;
        curr = found;
    }
    return curr;
}

int vfs_list(const char *path, kesh_vfs_entry_t *out_entries, int max_entries) {
    if (!out_entries || max_entries <= 0) return 0;
    vfs_node_t *node = find_node_by_path(path);
    if (!node || !node->is_dir) return -1;

    if (node->is_fat32 && !node->fat_populated) {
        populate_fat32_node(node);
    }
    if (node->is_iso9660 && !node->iso_populated) {
        populate_iso9660_node(node);
    }

    int count = node->child_count;
    if (count > max_entries) count = max_entries;

    for (int i = 0; i < count; i++) {
        vfs_node_t *c = node->children[i];
        str_copy(out_entries[i].name, c->name, VFS_NAME_MAX);
        out_entries[i].size = c->size;
        out_entries[i].is_dir = c->is_dir;
    }
    return count;
}

int vfs_read(const char *path, void *buffer, int max_bytes) {
    if (!buffer || max_bytes <= 0) return -1;
    vfs_node_t *node = find_node_by_path(path);
    if (!node || node->is_dir) return -2;

    if (node->is_fat32) {
        return (int)fat32_read_file(node->fat_cluster, node->size, (uint8_t*)buffer, (uint32_t)max_bytes);
    }

    if (node->is_iso9660) {
        return (int)iso9660_read_file(node->iso_lba, node->size, buffer, max_bytes);
    }

    if (!node->data) return 0; 

    int to_copy = (int)node->size;
    if (to_copy > max_bytes) to_copy = max_bytes;

    uint8_t *dst = (uint8_t*)buffer;
    for (int i = 0; i < to_copy; i++) {
        dst[i] = node->data[i];
    }
    return to_copy;
}

static void basename_copy(const char *path, char *out, int cap) {
    int len = str_len(path);
    int end = len;
    while (end > 0 && (path[end - 1] == '/' || path[end - 1] == '\\')) end--;
    int start = end - 1;
    while (start >= 0 && path[start] != '/' && path[start] != '\\') start--;
    start++;
    int n = 0;
    while (start < end && n + 1 < cap) out[n++] = path[start++];
    out[n] = '\0';
}

int vfs_write(const char *path, const void *buffer, int bytes) {
    if (!path || !buffer || bytes < 0) return -1;
    vfs_node_t *node = find_node_by_path(path);
    if (node && node->is_dir) return -2;

    if (node && node->is_fat32) {
        vfs_node_t *parent = node->parent;
        if (!parent || !parent->is_fat32) return -3;
        char name[VFS_NAME_MAX];
        basename_copy(path, name, VFS_NAME_MAX);
        if (!fat32_write_file(parent->fat_cluster, name, (const uint8_t*)buffer, (uint32_t)bytes)) return -4;
        parent->fat_populated = 0;
        populate_fat32_node(parent);
        return bytes;
    }

    if (node && !node->is_fat32 && !node->is_iso9660) {
        if (!node->data || node->size < (uint64_t)bytes) {
            uint64_t pages = ((uint64_t)bytes + PAGE_SIZE - 1) / PAGE_SIZE;
            uint64_t phys = pmm_alloc_pages(pages ? pages : 1);
            if (!phys) return -5;
            node->data = (uint8_t*)(phys + g_hhdm_offset);
        }
        uint8_t *dst = (uint8_t*)node->data;
        for (int i = 0; i < bytes; i++) {
            dst[i] = ((const uint8_t*)buffer)[i];
        }
        node->size = bytes;
        return bytes;
    }

    char parent_path[VFS_NAME_MAX];
    int len = str_len(path);
    int slash = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (path[i] == '/' || path[i] == '\\') { slash = i; break; }
    }
    if (slash < 1) return -6;
    if (slash >= VFS_NAME_MAX) return -6;
    for (int i = 0; i < slash; i++) parent_path[i] = path[i];
    parent_path[slash] = '\0';
    if (!parent_path[0]) { parent_path[0] = '/'; parent_path[1] = '\0'; }

    vfs_node_t *parent = find_node_by_path(parent_path);
    if (!parent) return -7;
    char name[VFS_NAME_MAX];
    basename_copy(path, name, VFS_NAME_MAX);

    if (parent->is_fat32) {
        if (!fat32_write_file(parent->fat_cluster, name, (const uint8_t*)buffer, (uint32_t)bytes)) return -8;
        parent->fat_populated = 0;
        populate_fat32_node(parent);
        return bytes;
    }

    uint64_t pages = ((uint64_t)bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t phys = pmm_alloc_pages(pages ? pages : 1);
    if (!phys) return -8;
    uint8_t *data = (uint8_t*)(phys + g_hhdm_offset);
    for (int i = 0; i < bytes; i++) data[i] = ((const uint8_t*)buffer)[i];
    vfs_node_t *new_node = alloc_node(name, 0, bytes, data, parent);
    if (!new_node) return -9;
    return bytes;
}

int vfs_stat(const char *path, kesh_vfs_stat_t *out_stat) {
    if (!out_stat) return -1;
    vfs_node_t *node = find_node_by_path(path);
    if (!node) return -2;

    out_stat->size = node->size;
    out_stat->is_dir = node->is_dir;
    return 0;
}

int vfs_mkdir(const char *path) {
    if (!path || !path[0]) return -1;
    int len = str_len(path);
    if (len <= 1) return -1;

    int last_slash = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (path[i] == '/' || path[i] == '\\') {
            last_slash = i;
            break;
        }
    }
    if (last_slash < 0) return -1;

    char parent_path[VFS_NAME_MAX];
    const char *new_name = path + last_slash + 1;
    if (!new_name[0]) return -1;

    if (last_slash == 0) {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    } else {
        str_copy(parent_path, path, last_slash + 1);
        parent_path[last_slash] = '\0';
    }

    vfs_node_t *parent = find_node_by_path(parent_path);
    if (!parent || !parent->is_dir) return -2;

    for (int i = 0; i < parent->child_count; i++) {
        if (str_eq(parent->children[i]->name, new_name)) return -3;
    }

    if (parent->is_fat32) {
        if (!fat32_make_dir(parent->fat_cluster, new_name)) return -4;
    }

    vfs_node_t *node = alloc_node(new_name, 1, 0, NULL, parent);
    if (!node) return -5;
    node->is_fat32 = parent->is_fat32;
    if (node->is_fat32) {
        fat32_entry_t entries[VFS_MAX_CHILDREN];
        int count = fat32_list_dir(parent->fat_cluster, entries, VFS_MAX_CHILDREN);
        for (int i = 0; i < count; i++) {
            if (str_eq(entries[i].name, new_name)) {
                node->fat_cluster = entries[i].first_cluster;
                break;
            }
        }
    }
    return 0;
}

int vfs_delete(const char *path) {
    if (!path || str_eq(path, "/") || str_eq(path, "\\")) return -1;

    vfs_node_t *node = find_node_by_path(path);
    if (!node || node == g_root) return -2;

    vfs_node_t *parent = node->parent;
    if (!parent) return -3;

    if (node->is_fat32) {
        if (!fat32_delete_file(parent->fat_cluster, node->name)) return -6;
    }

    int found_idx = -1;
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == node) {
            found_idx = i;
            break;
        }
    }
    if (found_idx < 0) return -4;

    for (int i = found_idx; i < parent->child_count - 1; i++) {
        parent->children[i] = parent->children[i + 1];
    }
    parent->child_count--;
    return 0;
}

int vfs_get_disk_stats(uint64_t *total_bytes, uint64_t *free_bytes) {
    if (g_fat32_available) {
        return fat32_get_stats(total_bytes, free_bytes);
    }
    int hdd_idx = ata_get_primary_hdd();
    if (hdd_idx >= 0) {
        const ata_device_t *d = ata_get_drive(hdd_idx);
        if (d && d->present) {
            if (total_bytes) *total_bytes = d->total_bytes;
            if (free_bytes) *free_bytes = d->total_bytes;
            return 1;
        }
    }
    if (g_iso9660_available) {
        const iso9660_info_t *iso = iso9660_get_primary_info();
        if (total_bytes) *total_bytes = iso->total_bytes;
        if (free_bytes) *free_bytes = 0;
        return 1;
    }
    if (total_bytes) *total_bytes = 0;
    if (free_bytes) *free_bytes = 0;
    return 0;
}
