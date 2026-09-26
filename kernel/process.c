// шедулер процессов и переключение контекста
#include "process.h"
#include "memory.h"
#include "gdt.h"
#include "elf.h"
#include "vfs.h"
#include "kesh/kea.h"
#include "timer.h"
#include <stddef.h>

#define MAX_PROCESSES 16

extern int process_save_kernel_state(void);
extern void process_return_to_kernel(int exit_code);
extern void enter_user_mode(uint64_t rip, uint64_t rsp);
extern int process_switch_to_user(user_context_t *ctx);

extern const uint8_t user_mode_test_payload[];
extern const uint8_t user_mode_test_payload_end[];
extern uint64_t g_syscall_kernel_stack_top;

static process_t g_processes[MAX_PROCESSES];
int g_active_proc_idx = -1;

static void serial_write(const char *s) {
    if (!s) return;
    while (*s) {
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)*s++), "Nd"((uint16_t)0x3F8));
    }
}

void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        g_processes[i].id = i;
        g_processes[i].state = PROCESS_STATE_UNUSED;
        g_processes[i].name[0] = '\0';
    }
    serial_write("[PROCESS] Multi-process management initialized.\n");
}

process_t* process_spawn_elf(const char *name, const void *elf_data, uint64_t size) {
    int slot = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (g_processes[i].state == PROCESS_STATE_UNUSED || g_processes[i].state == PROCESS_STATE_EXITED) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        serial_write("[PROCESS] Max processes reached!\n");
        return NULL;
    }

    uint64_t user_pml4 = 0;
    uint64_t entry_point = 0;

    int res = elf_load(elf_data, size, &user_pml4, &entry_point);
    if (res != 0) {
        serial_write("[PROCESS] Failed to load ELF binary!\n");
        return NULL;
    }

    process_t *proc = &g_processes[slot];
    int n = 0;
    const char *src_name = name ? name : "user_app";
    while (src_name[n] && n < 31) {
        proc->name[n] = src_name[n];
        n++;
    }
    proc->name[n] = '\0';
    proc->pml4_phys = user_pml4;
    proc->entry_point = entry_point;
    proc->user_stack_top = 0x70000000ULL - 16;
    proc->state = PROCESS_STATE_READY;
    proc->wake_at_ms = 0;

    proc->ctx.rip = entry_point;
    proc->ctx.rsp = proc->user_stack_top;
    proc->ctx.rflags = 0x202; 
    proc->ctx.rbp = 0;
    proc->ctx.rbx = 0;
    proc->ctx.r12 = 0;
    proc->ctx.r13 = 0;
    proc->ctx.r14 = 0;
    proc->ctx.r15 = 0;
    proc->ctx.cr3 = user_pml4;
    proc->ctx.is_started = 0;
    proc->ctx.return_rax = 0;

    gdt_set_kernel_stack(g_syscall_kernel_stack_top);

    g_active_proc_idx = slot;
    serial_write("[PROCESS] Spawned ELF process: ");
    serial_write(proc->name);
    serial_write("\n");

    return proc;
}

process_t* process_spawn_kea(const char *name, const void *kea_data, uint64_t size) {
    if (!kea_data || size < sizeof(kea_header_t)) return NULL;
    const kea_header_t *hdr = (const kea_header_t*)kea_data;
    if (hdr->magic != KEA_MAGIC) {

        const uint8_t *raw = (const uint8_t*)kea_data;
        if (size >= 4 && raw[0] == 0x7F && raw[1] == 'E' && raw[2] == 'L' && raw[3] == 'F') {
            return process_spawn_elf(name, kea_data, size);
        }
        serial_write("[KEA] Not a valid KEA container: ");
        serial_write(name ? name : "unknown");
        serial_write("\n");
        return NULL;
    }

    if (hdr->elf_offset + hdr->elf_size > size) {
        serial_write("[KEA] Corrupted KEA: ELF payload out of bounds!\n");
        return NULL;
    }

    const uint8_t *elf_payload = (const uint8_t*)kea_data + hdr->elf_offset;
    const char *app_name = (hdr->name[0] != '\0') ? hdr->name : name;
    serial_write("[KEA] Unpacking KEA app: ");
    serial_write(app_name);
    serial_write(" (v");
    serial_write(hdr->app_version);
    serial_write(")\n");

    return process_spawn_elf(app_name, elf_payload, hdr->elf_size);
}

process_t* process_spawn_path(const char *path) {
    if (!path) return NULL;
    serial_write("[PROCESS] Spawning executable from VFS: ");
    serial_write(path);
    serial_write("\n");

    kesh_vfs_stat_t st;
    if (vfs_stat(path, &st) != 0 || st.is_dir || st.size == 0) {
        serial_write("[PROCESS] File not found or is dir: ");
        serial_write(path);
        serial_write("\n");
        return NULL;
    }

    vfs_node_t *node = vfs_get_node(path);
    if (node && node->data && !node->is_fat32) {
        return process_spawn_kea(node->name, node->data, node->size);
    }

    size_t pages_needed = (st.size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t phys = pmm_alloc_pages(pages_needed);
    if (!phys) {
        serial_write("[PROCESS] Out of memory loading binary!\n");
        return NULL;
    }
    uint8_t *buf = (uint8_t*)(phys + g_hhdm_offset);

    int read_bytes = vfs_read(path, buf, (int)st.size);
    if (read_bytes <= 0) {
        serial_write("[PROCESS] Failed to read from VFS!\n");
        return NULL;
    }

    return process_spawn_kea(path, buf, (uint64_t)read_bytes);
}

void process_step_active(void) {

    for (int step = 0; step < MAX_PROCESSES; step++) {
        g_active_proc_idx = (g_active_proc_idx + 1) % MAX_PROCESSES;
        process_t *proc = &g_processes[g_active_proc_idx];
        if (proc->state == PROCESS_STATE_READY &&
            (proc->wake_at_ms == 0 || timer_millis() >= proc->wake_at_ms)) {
            proc->state = PROCESS_STATE_RUNNING;
            int res = process_switch_to_user(&proc->ctx);
            if (res == 2) {
                proc->state = PROCESS_STATE_EXITED;
                serial_write("[PROCESS] Process terminated via SYS_EXIT: ");
                serial_write(proc->name);
                serial_write("\n");
            } else {
                proc->state = PROCESS_STATE_READY;
            }
            return;
        }
    }
}

void process_sleep_current(uint32_t ms) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES) return;
    process_t *proc = &g_processes[g_active_proc_idx];
    if (proc->state == PROCESS_STATE_RUNNING || proc->state == PROCESS_STATE_READY) {
        proc->wake_at_ms = timer_millis() + ms;
    }
}

int process_has_active(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (g_processes[i].state == PROCESS_STATE_READY || g_processes[i].state == PROCESS_STATE_RUNNING) {
            return 1;
        }
    }
    return 0;
}

int process_get_table(kesh_proc_info_t *out_table, int max_entries) {
    if (!out_table || max_entries <= 0) return 0;
    int count = 0;
    for (int i = 0; i < MAX_PROCESSES && count < max_entries; i++) {
        if (g_processes[i].state != PROCESS_STATE_UNUSED) {
            out_table[count].pid = g_processes[i].id;
            out_table[count].state = g_processes[i].state;
            out_table[count].memory_bytes = 64 * 1024; 
            const char *src = g_processes[i].name[0] ? g_processes[i].name : "proc";
            int j = 0;
            while (src[j] && j < 31) {
                out_table[count].name[j] = src[j];
                j++;
            }
            out_table[count].name[j] = '\0';
            count++;
        }
    }
    return count;
}

int process_kill(int pid) {
    if (pid < 0 || pid >= MAX_PROCESSES) return -1;
    if (g_processes[pid].state == PROCESS_STATE_READY || g_processes[pid].state == PROCESS_STATE_RUNNING) {
        g_processes[pid].state = PROCESS_STATE_EXITED;
        serial_write("[PROCESS] Process terminated via process_kill: ");
        serial_write(g_processes[pid].name);
        serial_write("\n");
        extern void uwindow_destroy(int win_id);
        uwindow_destroy(pid);
        return 0;
    }
    return -1;
}

void process_run_test_ring3(void) {
    serial_write("[PROCESS] Running baseline Ring 3 test...\n");

    if (process_save_kernel_state() != 0) {
        serial_write("[PROCESS] Returned back to Kernel Mode from Ring 3 test successfully!\n");
        return;
    }

    uint64_t user_pml4 = vmm_create_user_pml4();
    uint64_t code_phys = pmm_alloc_page();
    vmm_map_page(user_pml4, 0x40000000ULL, code_phys, PTE_USER | PTE_WRITABLE);

    uint8_t *dest_code = (uint8_t*)(code_phys + g_hhdm_offset);
    size_t payload_len = (size_t)(user_mode_test_payload_end - user_mode_test_payload);
    if (payload_len > PAGE_SIZE) payload_len = PAGE_SIZE;

    for (size_t i = 0; i < payload_len; i++) {
        dest_code[i] = user_mode_test_payload[i];
    }

    uint64_t stack_phys = pmm_alloc_page();
    vmm_map_page(user_pml4, 0x70000000ULL, stack_phys, PTE_USER | PTE_WRITABLE);
    uint64_t user_stack_top = 0x70000000ULL + PAGE_SIZE - 16;

    gdt_set_kernel_stack(g_syscall_kernel_stack_top);
    vmm_switch_pml4(user_pml4);
    enter_user_mode(0x40000000ULL, user_stack_top);
}
