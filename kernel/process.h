// структуры процессов и потоков
#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>

#define PROCESS_STATE_UNUSED  0
#define PROCESS_STATE_READY   1
#define PROCESS_STATE_RUNNING 2
#define PROCESS_STATE_EXITED  3

typedef struct user_context {
    uint64_t rip;
    uint64_t rsp;
    uint64_t rflags;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t cr3;
    uint64_t is_started;
    uint64_t return_rax;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
} __attribute__((packed)) user_context_t;

typedef struct process {
    int id;
    char name[32];
    uint64_t pml4_phys;
    uint64_t entry_point;
    uint64_t user_stack_top;
    int state;
    user_context_t ctx;
    uint64_t wake_at_ms;
} process_t;

void process_init(void);
void process_run_test_ring3(void);

typedef struct {
    int pid;
    char name[32];
    int state;
    uint64_t memory_bytes;
} kesh_proc_info_t;

typedef struct {
    uint64_t total_ram_bytes;
    uint64_t free_ram_bytes;
    uint64_t uptime_ms;
    uint32_t active_procs;
    uint32_t screen_w;
    uint32_t screen_h;
    uint64_t disk_total_bytes;
    uint64_t disk_free_bytes;
} kesh_sysinfo_t;

int process_get_table(kesh_proc_info_t *out_table, int max_entries);
int process_kill(int pid);

process_t* process_spawn_elf(const char *name, const void *elf_data, uint64_t size);
process_t* process_spawn_kea(const char *name, const void *kea_data, uint64_t size);
process_t* process_spawn_path(const char *path);
void process_step_active(void);
void process_sleep_current(uint32_t ms);
int process_has_active(void);

#endif
