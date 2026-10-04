// ноды и дескрипторы вфс
#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define VFS_NAME_MAX 64
#define VFS_MAX_CHILDREN 32

typedef struct {
    char name[VFS_NAME_MAX];
    uint32_t size;
    uint8_t is_dir;
} kesh_vfs_entry_t;

typedef struct {
    uint32_t size;
    uint8_t is_dir;
} kesh_vfs_stat_t;

typedef struct vfs_node {
    char name[VFS_NAME_MAX];
    uint8_t is_dir;
    uint32_t size;
    const uint8_t *data;
    struct vfs_node *parent;
    struct vfs_node *children[VFS_MAX_CHILDREN];
    int child_count;
    uint8_t is_fat32;
    uint32_t fat_cluster;
    uint8_t fat_populated;
    uint8_t is_iso9660;
    uint32_t iso_lba;
    uint8_t iso_populated;
} vfs_node_t;

void vfs_init(void);
int vfs_list(const char *path, kesh_vfs_entry_t *out_entries, int max_entries);
int vfs_read(const char *path, void *buffer, int max_bytes);
int vfs_read_at(const char *path, uint64_t offset, void *buffer, int max_bytes);
int vfs_write(const char *path, const void *buffer, int bytes);
int vfs_stat(const char *path, kesh_vfs_stat_t *out_stat);
vfs_node_t* vfs_get_node(const char *path);
int vfs_mkdir(const char *path);
int vfs_delete(const char *path);
int vfs_rename(const char *old_path, const char *new_path);
int vfs_get_disk_stats(uint64_t *total_bytes, uint64_t *free_bytes);
void vfs_rescan_drives(void);

#endif
