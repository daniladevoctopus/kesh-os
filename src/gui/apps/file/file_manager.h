// стейт проводника
#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <stdint.h>

typedef enum {
    NODE_FILE,
    NODE_DIR
} node_type_t;

typedef struct vfs_node {
    char name[32];
    node_type_t type;
    uint32_t size;
    const uint8_t* data;
    struct vfs_node* parent;
    struct vfs_node* children[16];
    int child_count;

    uint32_t fat_cluster;  
    int is_real_disk;      
    int populated;         
} vfs_node_t;

void init_file_manager(void);
void toggle_file_manager(void);
void render_file_manager_window(uint32_t* buf, int scr_w, int scr_h, int mx, int my, int btn, int click);

#endif
