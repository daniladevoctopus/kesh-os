// формат кеа пакетов
#ifndef KESH_KEA_H
#define KESH_KEA_H

#include <stdint.h>

#define KEA_MAGIC 0x0141454B 
#define KEA_VERSION 1

#define KEA_PERM_NETWORK (1 << 0)
#define KEA_PERM_FS      (1 << 1)
#define KEA_PERM_SOUND   (1 << 2)
#define KEA_PERM_GUI     (1 << 3)
#define KEA_PERM_ROOT    (1 << 4)

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;            
    uint32_t hdr_size;         
    uint16_t version;          
    uint16_t flags;            
    char     name[32];         
    char     app_version[16];  
    char     author[32];       
    char     category[16];     
    char     description[64];  
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
#pragma pack(pop)

#endif
