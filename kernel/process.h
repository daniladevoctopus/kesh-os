// структуры процессов и потоков
#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>

#define PROCESS_STATE_UNUSED  0
#define PROCESS_STATE_READY   1
#define PROCESS_STATE_RUNNING 2
#define PROCESS_STATE_EXITED  3
#define PROCESS_STATE_STOPPED 4

#define THREAD_STATE_UNUSED   0
#define THREAD_STATE_READY    1
#define THREAD_STATE_RUNNING  2
#define THREAD_STATE_EXITED   3
#define MAX_PROCESSES 16
#define MAX_THREADS_PER_PROCESS 32
#define MAX_VM_REGIONS_PER_PROCESS 2048
#define KESH_UID_ROOT 0U
#define KESH_UID_USER 1000U
#define KESH_SIGINT  2
#define KESH_SIGKILL 9
#define KESH_SIGTERM 15
#define KESH_SIGCONT 18
#define KESH_SIGSTOP 19

typedef struct {
    uint32_t uid;
    uint32_t gid;
    uint32_t groups;
} credentials_t;

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

typedef struct thread {
    int tid;
    int owner_pid;
    int state;
    uint64_t stack_base;
    uint32_t stack_pages;
    uint64_t wake_at_ms;
    uint64_t fs_base;
    uint64_t clear_child_tid;
    user_context_t ctx;
} thread_t;

typedef struct {
    uint64_t base;
    uint64_t size;
    uint32_t protection;
    uint8_t owns_physical;
    uint8_t backing_kind;
    void *backing_ptr;
    uint8_t active;
} process_vm_region_t;

typedef struct process {
    int id;
    char name[32];
    uint64_t pml4_phys;
    uint64_t entry_point;
    uint64_t user_stack_top;
    uint32_t permissions;
    credentials_t credentials;
    int state;
    user_context_t ctx;
    thread_t threads[MAX_THREADS_PER_PROCESS];
    int active_thread_idx;
    process_vm_region_t vm_regions[MAX_VM_REGIONS_PER_PROCESS];
    uint64_t vm_next_base;
    uint64_t heap_base;
    uint64_t heap_break;
    uint64_t heap_mapped_end;
    int controlling_pty;
    int process_group;
    int is_linux_abi;
    uint64_t fs_base;
} process_t;

void process_init(void);
void process_run_test_ring3(void);
int process_current_is_linux(void);
void process_set_current_linux(int is_linux);
void process_set_current_fs_base(uint64_t fs_base);
uint64_t process_get_current_fs_base(void);

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
int process_exists(int pid);
int process_get_credentials(int pid, credentials_t *out);
int process_set_controlling_pty(int pid, int handle);
int process_current_pty(void);
int process_get_group(int pid);
int process_set_group(int caller_pid, int pid, int process_group);
int process_send_signal(int caller_pid, int pid, int signal);
int process_signal_group(int caller_pid, int process_group, int signal);
int process_current_id(void);
int process_current_thread_id(void);
const char *process_current_name(void);
uint32_t process_current_permissions(void);
int process_has_permission(uint32_t permission);
int process_revoke_permissions(int caller_pid, int target_pid, uint32_t mask);
const credentials_t *process_current_credentials(void);
int process_current_is_root(void);
int process_create_thread(uint64_t entry_point);
int process_create_linux_thread(uint64_t rip, uint64_t rsp, uint64_t fs_base, uint64_t fn);
int process_exit_current_thread(void);
void process_set_thread_clear_child_tid(int tidx, uint64_t ctid_ptr);
void process_set_current_clear_child_tid(uint64_t ctid_ptr);
uint64_t process_get_current_clear_child_tid(void);
int process_has_other_live_threads(void);
uint64_t process_vm_map(uint64_t size, uint32_t protection);
uint64_t process_vm_map_fixed(uint64_t address, uint64_t size, uint32_t protection);
uint64_t process_vm_map_phys(const uint64_t *phys_pages, uint64_t num_pages, uint32_t protection);
int process_vm_attach_backing(uint64_t address, uint8_t backing_kind, void *backing_ptr);
int process_vm_protect(uint64_t address, uint64_t size, uint32_t protection);
int process_vm_unmap(uint64_t address, uint64_t size);
uint64_t process_vm_brk(uint64_t address);

#endif
