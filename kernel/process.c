// шедулер процессов и переключение контекста
#include "process.h"
#include "memory.h"
#include "gdt.h"
#include "elf.h"
#include "socket.h"
#include "tty.h"
#include "fd.h"
#include "ipc.h"
#include "vfs.h"
#include "kesh/kea.h"
#include "../kpm/kea_verify.h"
#include "timer.h"
#include "serial.h"
#include "log.h"
#include <stddef.h>

#define PROCESS_VM_BASE 0x0000000100000000ULL
#define PROCESS_VM_MAX_SIZE (512ULL * 1024ULL * 1024ULL)
#define PROCESS_HEAP_BASE 0x0000000030000000ULL

extern int process_save_kernel_state(void);
extern void process_return_to_kernel(int exit_code) __attribute__((noreturn));
extern void enter_user_mode(uint64_t rip, uint64_t rsp);
extern int process_switch_to_user(user_context_t *ctx);

extern const uint8_t user_mode_test_payload[];
extern const uint8_t user_mode_test_payload_end[];
extern uint64_t g_syscall_kernel_stack_top;

static process_t g_processes[MAX_PROCESSES];
int g_active_proc_idx = -1;
static int g_active_thread_idx = 0;

static process_t* process_spawn_elf_with_permissions(const char *name, const void *elf_data,
                                                      uint64_t size, uint32_t permissions, int trusted_root);

void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        g_processes[i].id = i;
        g_processes[i].state = PROCESS_STATE_UNUSED;
        g_processes[i].controlling_pty = -1;
        g_processes[i].process_group = i;
        g_processes[i].name[0] = '\0';
        g_processes[i].credentials.uid = KESH_UID_USER;
        g_processes[i].credentials.gid = KESH_UID_USER;
        g_processes[i].credentials.groups = 0;
        g_processes[i].active_thread_idx = 0;
        g_processes[i].vm_next_base = PROCESS_VM_BASE;
        g_processes[i].heap_base = PROCESS_HEAP_BASE;
        g_processes[i].heap_break = PROCESS_HEAP_BASE;
        g_processes[i].heap_mapped_end = PROCESS_HEAP_BASE;
        for (int r = 0; r < MAX_VM_REGIONS_PER_PROCESS; ++r) g_processes[i].vm_regions[r].active = 0;
        for (int t = 0; t < MAX_THREADS_PER_PROCESS; ++t) {
            g_processes[i].threads[t].tid = t;
            g_processes[i].threads[t].owner_pid = i;
            g_processes[i].threads[t].state = THREAD_STATE_UNUSED;
            g_processes[i].threads[t].stack_base = 0;
            g_processes[i].threads[t].stack_pages = 0;
            g_processes[i].threads[t].wake_at_ms = 0;
            g_processes[i].threads[t].fs_base = 0;
            g_processes[i].threads[t].clear_child_tid = 0;
        }
    }
    g_active_proc_idx = -1;
    g_active_thread_idx = 0;
    serial_print("[PROCESS] Multi-process + thread management initialized.\n");
}

static inline uint64_t proc_rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void proc_wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = (uint32_t)val;
    uint32_t high = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

static void process_release_resources(process_t *proc) {
    if (!proc) return;
    if (proc->pml4_phys) {
        extern void uwindow_destroy_process(uint64_t pml4_phys);
        uwindow_destroy_process(proc->pml4_phys);
        for (int r = 0; r < MAX_VM_REGIONS_PER_PROCESS; ++r) {
            process_vm_region_t *region = &proc->vm_regions[r];
            if (!region->active) continue;
            for (uint64_t offset = 0; offset < region->size; offset += PAGE_SIZE)
                (void)vmm_unmap_page(proc->pml4_phys, region->base + offset, region->owns_physical);
            if (region->backing_kind == FD_KIND_MEMFD && region->backing_ptr) {
                extern void unix_memfd_release(void *custom_ptr);
                unix_memfd_release(region->backing_ptr);
            }
            region->active = 0;
        }
        vmm_destroy_user_pml4(proc->pml4_phys);
        proc->pml4_phys = 0;
    }
    proc->entry_point = 0;
    proc->user_stack_top = 0;
    proc->permissions = 0;
    proc->credentials.uid = KESH_UID_USER;
    proc->credentials.gid = KESH_UID_USER;
    proc->credentials.groups = 0;
    proc->is_linux_abi = 0;
    proc->fs_base = 0;
    proc->ctx.rip = 0;
    proc->ctx.rsp = 0;
    proc->ctx.cr3 = 0;
    proc->ctx.is_started = 0;
    for (int t = 0; t < MAX_THREADS_PER_PROCESS; ++t) {
        proc->threads[t].state = THREAD_STATE_UNUSED;
        proc->threads[t].stack_base = 0;
        proc->threads[t].stack_pages = 0;
        proc->threads[t].wake_at_ms = 0;
        proc->threads[t].fs_base = 0;
        proc->threads[t].clear_child_tid = 0;
        proc->threads[t].ctx.cr3 = 0;
        proc->threads[t].ctx.is_started = 0;
    }
    proc->active_thread_idx = 0;
    proc->vm_next_base = PROCESS_VM_BASE;
    proc->heap_base = PROCESS_HEAP_BASE;
    proc->heap_break = PROCESS_HEAP_BASE;
    proc->heap_mapped_end = PROCESS_HEAP_BASE;
    for (int r = 0; r < MAX_VM_REGIONS_PER_PROCESS; ++r) proc->vm_regions[r].active = 0;
}

static process_t* process_spawn_elf_with_permissions(const char *name, const void *elf_data,
                                                      uint64_t size, uint32_t permissions, int trusted_root) {
    int slot = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (g_processes[i].state == PROCESS_STATE_UNUSED || g_processes[i].state == PROCESS_STATE_EXITED) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        serial_print("[PROCESS] Max processes reached!\n");
        return NULL;
    }

    uint64_t user_pml4 = 0;
    uint64_t entry_point = 0;

    int res = elf_load(elf_data, size, &user_pml4, &entry_point);
    if (res != 0) {
        serial_print("[PROCESS] Failed to load ELF binary!\n");
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
    proc->is_linux_abi = 0;
    proc->fs_base = 0;
    if (src_name) {
        int slen = 0;
        while (src_name[slen]) slen++;
        if (slen >= 4 && src_name[slen-4] == '.' && src_name[slen-3] == 'e' && src_name[slen-2] == 'l' && src_name[slen-1] == 'f') {
            proc->is_linux_abi = 1;
        }
    }

    proc->user_stack_top = 0x70000000ULL - 16;
    uint64_t stack_phys = vmm_get_page_phys(user_pml4, 0x6FFFF000ULL);
    if (stack_phys) {
        const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)elf_data;
        uint64_t phdr_user_va = 0;
        for (uint16_t p = 0; p < ehdr->e_phnum; ++p) {
            const Elf64_Phdr *phdr = (const Elf64_Phdr *)((const uint8_t *)elf_data +
                                      ehdr->e_phoff + (uint64_t)p * ehdr->e_phentsize);
            if (phdr->p_type == PT_PHDR) {
                phdr_user_va = phdr->p_vaddr;
                break;
            }
            if (phdr->p_type == PT_LOAD && ehdr->e_phoff >= phdr->p_offset &&
                ehdr->e_phoff + (uint64_t)ehdr->e_phnum * ehdr->e_phentsize <=
                    phdr->p_offset + phdr->p_filesz) {
                phdr_user_va = phdr->p_vaddr + (ehdr->e_phoff - phdr->p_offset);
            }
        }

        uint8_t *page = (uint8_t *)(stack_phys + g_hhdm_offset);
        for (int i = 0; i < 4096; i++) page[i] = 0;

        char *str_name = (char *)(page + 3800);
        int si = 0;
        while (src_name[si] && si < 63) {
            str_name[si] = src_name[si];
            si++;
        }
        str_name[si] = '\0';
        uint64_t va_str_name = 0x6FFFF000ULL + 3800;

        uint8_t *rand_bytes = page + 3900;
        for (int r = 0; r < 16; r++) rand_bytes[r] = (uint8_t)(0x42 + r * 7);
        uint64_t va_rand_bytes = 0x6FFFF000ULL + 3900;

        uint64_t *sp = (uint64_t *)(page + 3520);
        int idx = 0;
        sp[idx++] = 1;                  /* argc = 1 */
        sp[idx++] = va_str_name;        /* argv[0] */
        sp[idx++] = 0;                  /* argv[1] = NULL */
        sp[idx++] = 0;                  /* envp[0] = NULL */

        sp[idx++] = 3;  sp[idx++] = phdr_user_va;     /* AT_PHDR */
        sp[idx++] = 4;  sp[idx++] = ehdr->e_phentsize;/* AT_PHENT */
        sp[idx++] = 5;  sp[idx++] = ehdr->e_phnum;    /* AT_PHNUM */
        sp[idx++] = 6;  sp[idx++] = 4096;          /* AT_PAGESZ = 4096 */
        sp[idx++] = 25; sp[idx++] = va_rand_bytes; /* AT_RANDOM */
        sp[idx++] = 9;  sp[idx++] = entry_point;   /* AT_ENTRY */
        sp[idx++] = 11; sp[idx++] = 0;             /* AT_UID = 0 */
        sp[idx++] = 12; sp[idx++] = 0;             /* AT_EUID = 0 */
        sp[idx++] = 13; sp[idx++] = 0;             /* AT_GID = 0 */
        sp[idx++] = 14; sp[idx++] = 0;             /* AT_EGID = 0 */
        sp[idx++] = 23; sp[idx++] = 0;             /* AT_SECURE = 0 */
        sp[idx++] = 31; sp[idx++] = va_str_name;   /* AT_EXECFN */
        sp[idx++] = 0;  sp[idx++] = 0;             /* AT_NULL = 0 */

        proc->user_stack_top = 0x6FFFF000ULL + 3520;
    }

    proc->permissions = permissions;
    proc->credentials.uid = trusted_root ? KESH_UID_ROOT : KESH_UID_USER;
    proc->credentials.gid = trusted_root ? KESH_UID_ROOT : KESH_UID_USER;
    proc->credentials.groups = 0;
    proc->state = PROCESS_STATE_READY;
    proc->controlling_pty = -1;
    proc->process_group = proc->id;

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

    for (int t = 0; t < MAX_THREADS_PER_PROCESS; ++t) {
        proc->threads[t].state = THREAD_STATE_UNUSED;
        proc->threads[t].stack_base = 0;
        proc->threads[t].stack_pages = 0;
        proc->threads[t].wake_at_ms = 0;
        proc->threads[t].fs_base = 0;
        proc->threads[t].clear_child_tid = 0;
    }
    proc->threads[0].tid = 0;
    proc->threads[0].owner_pid = proc->id;
    proc->threads[0].state = THREAD_STATE_READY;
    proc->threads[0].ctx = proc->ctx;
    proc->active_thread_idx = 0;
    proc->vm_next_base = PROCESS_VM_BASE;
    proc->heap_base = PROCESS_HEAP_BASE;
    proc->heap_break = PROCESS_HEAP_BASE;
    proc->heap_mapped_end = PROCESS_HEAP_BASE;
    for (int r = 0; r < MAX_VM_REGIONS_PER_PROCESS; ++r) proc->vm_regions[r].active = 0;

    gdt_set_kernel_stack(g_syscall_kernel_stack_top);

    serial_print("[PROCESS] Spawned ELF process: ");
    serial_print(proc->name);
    serial_print("\n");

    return proc;
}

process_t* process_spawn_elf(const char *name, const void *elf_data, uint64_t size) {
    return process_spawn_elf_with_permissions(name, elf_data, size, KEA_PERM_GUI | KEA_PERM_FS, 0);
}

process_t* process_spawn_kea(const char *name, const void *kea_data, uint64_t size) {
    if (!kea_data || size < sizeof(kea_header_t)) return NULL;
    const kea_header_t *hdr = (const kea_header_t*)kea_data;
    if (hdr->magic != KEA_MAGIC) {

        const uint8_t *raw = (const uint8_t*)kea_data;
        if (size >= 4 && raw[0] == 0x7F && raw[1] == 'E' && raw[2] == 'L' && raw[3] == 'F') {
            return process_spawn_elf(name, kea_data, size);
        }
        serial_print("[KEA] Not a valid KEA container: ");
        serial_print(name ? name : "unknown");
        serial_print("\n");
        return NULL;
    }

    int signed_package = hdr->version == KEA_VERSION;
    int verify_status = kea_verify_package(kea_data, size, signed_package);
    if (verify_status != KEA_VERIFY_OK) {
        serial_print("[KEA] Verification failed: ");
        serial_print(kea_verify_error(verify_status));
        serial_print("\n");
        return NULL;
    }

    if ((hdr->permissions & KEA_PERM_ROOT) && !signed_package) {
        serial_print("[KEA] ROOT permission rejected for legacy package.\n");
        return NULL;
    }

    const uint8_t *elf_payload = (const uint8_t*)kea_data + hdr->elf_offset;
    const char *app_name = (hdr->name[0] != '\0') ? hdr->name : name;
    serial_print("[KEA] Unpacking KEA app: ");
    serial_print(app_name);
    serial_print(" (v");
    serial_print(hdr->app_version);
    serial_print(")\n");

    int trusted_root = signed_package && (hdr->permissions & KEA_PERM_ROOT);
    if (trusted_root) KLOG_NOTICE("security", "starting signed privileged package name=%s", app_name);
    return process_spawn_elf_with_permissions(app_name, elf_payload, hdr->elf_size, hdr->permissions, trusted_root);
}

process_t* process_spawn_path(const char *path) {
    if (!path) return NULL;
    serial_print("[PROCESS] Spawning executable from VFS: ");
    serial_print(path);
    serial_print("\n");

    kesh_vfs_stat_t st;
    if (vfs_stat(path, &st) != 0 || st.is_dir || st.size == 0) {
        serial_print("[PROCESS] File not found or is dir: ");
        serial_print(path);
        serial_print("\n");
        return NULL;
    }

    vfs_node_t *node = vfs_get_node(path);
    if (node && node->data && !node->is_fat32) {
        return process_spawn_kea(node->name, node->data, node->size);
    }

    if (st.size > 0x7FFFFFFFULL) {
        serial_print("[PROCESS] Executable is too large.\n");
        return NULL;
    }

    size_t pages_needed = (st.size + PAGE_SIZE - 1ULL) / PAGE_SIZE;
    uint64_t phys = pmm_alloc_pages(pages_needed);
    if (!phys) {
        serial_print("[PROCESS] Out of memory loading binary!\n");
        return NULL;
    }
    uint8_t *buf = (uint8_t*)(phys + g_hhdm_offset);

    int read_bytes = vfs_read(path, buf, (int)st.size);
    if (read_bytes <= 0) {
        pmm_free_pages(phys, pages_needed);
        serial_print("[PROCESS] Failed to read from VFS! (read ");
        char num[16]; int n = 0, v = read_bytes;
        if (v <= 0) num[n++] = '0';
        while (v > 0 && n < 15) { num[n++] = (char)('0' + (v % 10)); v /= 10; }
        while (n) { char c = num[--n]; __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)c), "Nd"((uint16_t)0x3F8)); }
        serial_print(" of ");
        v = (int)st.size; n = 0;
        if (v <= 0) num[n++] = '0';
        while (v > 0 && n < 15) { num[n++] = (char)('0' + (v % 10)); v /= 10; }
        while (n) { char c = num[--n]; __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)c), "Nd"((uint16_t)0x3F8)); }
        serial_print(" bytes)\n");
        return NULL;
    }

    process_t *proc = process_spawn_kea(path, buf, (uint64_t)read_bytes);
    pmm_free_pages(phys, pages_needed);
    return proc;
}

void process_step_active(void) {
    for (int step = 0; step < MAX_PROCESSES; step++) {
        g_active_proc_idx = (g_active_proc_idx + 1) % MAX_PROCESSES;
        process_t *proc = &g_processes[g_active_proc_idx];
        if (proc->state != PROCESS_STATE_READY) continue;

        for (int tstep = 0; tstep < MAX_THREADS_PER_PROCESS; ++tstep) {
            int tidx = (proc->active_thread_idx + tstep) % MAX_THREADS_PER_PROCESS;
            thread_t *thread = &proc->threads[tidx];
            if (thread->state != THREAD_STATE_READY) continue;
            if (thread->ctx.cr3 == 0) continue;
            if (proc->threads[0].ctx.cr3 != thread->ctx.cr3) continue;
            if (tidx != 0 && (proc->threads[0].state == THREAD_STATE_UNUSED)) continue;

            if (thread->wake_at_ms != 0 && timer_millis() < thread->wake_at_ms) continue;
            thread->wake_at_ms = 0;

            proc->active_thread_idx = tidx;
            g_active_thread_idx = tidx;
            proc->state = PROCESS_STATE_RUNNING;
            thread->state = THREAD_STATE_RUNNING;

            if (proc->is_linux_abi) {
                uint64_t target_fs = thread->fs_base ? thread->fs_base : proc->fs_base;
                if (target_fs) proc_wrmsr(0xC0000100, target_fs);
            }

            int res = process_switch_to_user(&thread->ctx);
            proc->ctx = thread->ctx;

            if (proc->is_linux_abi) {
                thread->fs_base = proc_rdmsr(0xC0000100);
                if (tidx == 0) proc->fs_base = thread->fs_base;
            }

            if (res == 2) {
                process_exit_current_thread();
            } else if (thread->state == THREAD_STATE_RUNNING) {
                thread->state = THREAD_STATE_READY;
                proc->state = PROCESS_STATE_READY;
            }
            proc->active_thread_idx = (tidx + 1) % MAX_THREADS_PER_PROCESS;
            return;
        }
    }
}

int process_current_is_linux(void) {
    if (g_active_proc_idx >= 0 && g_active_proc_idx < MAX_PROCESSES) {
        return g_processes[g_active_proc_idx].is_linux_abi;
    }
    return 0;
}

void process_set_current_linux(int is_linux) {
    if (g_active_proc_idx >= 0 && g_active_proc_idx < MAX_PROCESSES) {
        g_processes[g_active_proc_idx].is_linux_abi = is_linux ? 1 : 0;
    }
}

void process_set_current_fs_base(uint64_t fs_base) {
    if (g_active_proc_idx >= 0 && g_active_proc_idx < MAX_PROCESSES) {
        g_processes[g_active_proc_idx].fs_base = fs_base;
        if (g_active_thread_idx >= 0 && g_active_thread_idx < MAX_THREADS_PER_PROCESS) {
            g_processes[g_active_proc_idx].threads[g_active_thread_idx].fs_base = fs_base;
        }
    }
}

uint64_t process_get_current_fs_base(void) {
    if (g_active_proc_idx >= 0 && g_active_proc_idx < MAX_PROCESSES) {
        if (g_active_thread_idx >= 0 && g_active_thread_idx < MAX_THREADS_PER_PROCESS) {
            uint64_t t_fs = g_processes[g_active_proc_idx].threads[g_active_thread_idx].fs_base;
            if (t_fs) return t_fs;
        }
        return g_processes[g_active_proc_idx].fs_base;
    }
    return 0;
}

int process_current_id(void) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES) return -1;
    if (g_processes[g_active_proc_idx].state == PROCESS_STATE_UNUSED || g_processes[g_active_proc_idx].state == PROCESS_STATE_EXITED) return -1;
    return g_processes[g_active_proc_idx].id;
}

void process_sleep_current(uint32_t ms) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES) return;
    process_t *proc = &g_processes[g_active_proc_idx];
    if (g_active_thread_idx >= 0 && g_active_thread_idx < MAX_THREADS_PER_PROCESS &&
        (proc->state == PROCESS_STATE_RUNNING || proc->state == PROCESS_STATE_READY)) {
        proc->threads[g_active_thread_idx].wake_at_ms = timer_millis() + ms;
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

int process_exists(int pid) {
    return pid >= 0 && pid < MAX_PROCESSES && g_processes[pid].state != PROCESS_STATE_UNUSED && g_processes[pid].state != PROCESS_STATE_EXITED;
}

int process_set_controlling_pty(int pid, int handle) {
    if (!process_exists(pid)) return -1;
    g_processes[pid].controlling_pty = handle;
    return 0;
}

int process_current_pty(void) {
    return g_active_proc_idx >= 0 && g_active_proc_idx < MAX_PROCESSES ? g_processes[g_active_proc_idx].controlling_pty : -1;
}

int process_get_group(int pid) {
    return process_exists(pid) ? g_processes[pid].process_group : -1;
}

int process_get_credentials(int pid, credentials_t *out) {
    if (!out || !process_exists(pid)) return -1;
    *out = g_processes[pid].credentials;
    return 0;
}

int process_set_group(int caller_pid, int pid, int process_group) {
    if (!process_exists(caller_pid) || !process_exists(pid) || process_group < 0) return -1;
    if (caller_pid != pid && g_processes[caller_pid].credentials.uid != KESH_UID_ROOT) return -1;
    g_processes[pid].process_group = process_group ? process_group : pid;
    return 0;
}

int process_send_signal(int caller_pid, int pid, int signal) {
    if (!process_exists(pid)) return -1;
    if (caller_pid >= 0) {
        if (!process_exists(caller_pid)) return -1;
        credentials_t *sender = &g_processes[caller_pid].credentials;
        credentials_t *target = &g_processes[pid].credentials;
        if (sender->uid != KESH_UID_ROOT && sender->uid != target->uid) return -1;
        if (pid == caller_pid && signal == KESH_SIGSTOP) return -1;
    }
    process_t *proc = &g_processes[pid];
    if (signal == KESH_SIGKILL || signal == KESH_SIGTERM || signal == KESH_SIGINT) return process_kill(pid);
    if (signal == KESH_SIGSTOP) {
        if (proc->state == PROCESS_STATE_STOPPED) return 0;
        if (proc->state != PROCESS_STATE_READY && proc->state != PROCESS_STATE_RUNNING) return -1;
        for (int i = 0; i < MAX_THREADS_PER_PROCESS; ++i) if (proc->threads[i].state == THREAD_STATE_RUNNING) proc->threads[i].state = THREAD_STATE_READY;
        proc->state = PROCESS_STATE_STOPPED;
        return 0;
    }
    if (signal == KESH_SIGCONT) {
        if (proc->state != PROCESS_STATE_STOPPED) return -1;
        proc->state = PROCESS_STATE_READY;
        return 0;
    }
    return -1;
}

int process_signal_group(int caller_pid, int process_group, int signal) {
    if (process_group < 0) return -1;
    int delivered = 0;
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        if (!process_exists(i) || g_processes[i].process_group != process_group) continue;
        if (process_send_signal(caller_pid, i, signal) == 0) delivered++;
    }
    return delivered ? delivered : -1;
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
    process_t *proc = &g_processes[pid];
    if (proc->state != PROCESS_STATE_READY && proc->state != PROCESS_STATE_RUNNING && proc->state != PROCESS_STATE_STOPPED) return -1;

    proc->state = PROCESS_STATE_EXITED;
    socket_close_owner(proc->id);
    tty_close_owner(proc->id);
    fd_close_owner(proc->id);
    ipc_close_owner(proc->id);
    process_release_resources(proc);
    serial_print("[PROCESS] Process terminated and address space released: ");
    serial_print(proc->name);
    serial_print("\n");
    return 0;
}

int process_current_thread_id(void) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES) return -1;
    return g_active_thread_idx;
}

const char *process_current_name(void) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES) return NULL;
    process_t *proc = &g_processes[g_active_proc_idx];
    if (proc->state == PROCESS_STATE_UNUSED || proc->state == PROCESS_STATE_EXITED) return NULL;
    return proc->name;
}

uint32_t process_current_permissions(void) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES) return 0;
    process_t *proc = &g_processes[g_active_proc_idx];
    if (proc->state == PROCESS_STATE_UNUSED || proc->state == PROCESS_STATE_EXITED) return 0;
    return proc->permissions;
}

int process_has_permission(uint32_t permission) {
    return (process_current_permissions() & permission) == permission;
}

int process_revoke_permissions(int caller_pid, int target_pid, uint32_t mask) {
    if (!process_exists(caller_pid) || !process_exists(target_pid) || !mask) return -1;
    if (g_processes[caller_pid].credentials.uid != KESH_UID_ROOT) return -1;
    uint32_t before = g_processes[target_pid].permissions;
    g_processes[target_pid].permissions &= ~mask;
    KLOG_NOTICE("security", "permission revoke caller=%d target=%d before=%x after=%x",
                caller_pid, target_pid, before, g_processes[target_pid].permissions);
    return before != g_processes[target_pid].permissions ? 0 : -1;
}

const credentials_t *process_current_credentials(void) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES) return NULL;
    process_t *proc = &g_processes[g_active_proc_idx];
    return (proc->state == PROCESS_STATE_UNUSED || proc->state == PROCESS_STATE_EXITED) ? NULL : &proc->credentials;
}

int process_current_is_root(void) {
    const credentials_t *credentials = process_current_credentials();
    return credentials && credentials->uid == KESH_UID_ROOT;
}

static int thread_entry_is_executable(process_t *proc, uint64_t entry) {
    if (!proc || !entry || entry >= USER_VA_LIMIT) return 0;
    uint64_t flags = vmm_get_page_flags(proc->pml4_phys, entry & ~(PAGE_SIZE - 1ULL));
    return (flags & PTE_PRESENT) && (flags & PTE_USER) && !(flags & PTE_NO_EXECUTE);
}

int process_create_thread(uint64_t entry_point) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES) return -1;
    process_t *proc = &g_processes[g_active_proc_idx];
    if (proc->state == PROCESS_STATE_UNUSED || !thread_entry_is_executable(proc, entry_point)) return -1;

    int tidx = -1;
    for (int i = 1; i < MAX_THREADS_PER_PROCESS; ++i) {
        if (proc->threads[i].state == THREAD_STATE_UNUSED || proc->threads[i].state == THREAD_STATE_EXITED) {
            tidx = i;
            break;
        }
    }
    if (tidx < 0) return -1;

    /* 8 pages (32 KiB) per extra thread, outside the primary process stack. */
    const uint32_t stack_pages = 8;
    uint64_t stack_base = 0x6E000000ULL + (uint64_t)(tidx - 1) * 0x00020000ULL;
    for (uint32_t p = 0; p < stack_pages; ++p) {
        uint64_t phys = pmm_alloc_page();
        if (!phys || vmm_map_page(proc->pml4_phys, stack_base + (uint64_t)p * PAGE_SIZE,
                                   phys, PTE_USER | PTE_WRITABLE | PTE_NO_EXECUTE) != 0) {
            if (phys) pmm_free_page(phys);
            for (uint32_t rollback = 0; rollback < p; ++rollback) {
                vmm_unmap_page(proc->pml4_phys, stack_base + (uint64_t)rollback * PAGE_SIZE, 1);
            }
            return -1;
        }
        uint8_t *page = (uint8_t *)(phys + g_hhdm_offset);
        for (size_t b = 0; b < PAGE_SIZE; ++b) page[b] = 0;
    }

    thread_t *thread = &proc->threads[tidx];
    thread->tid = tidx;
    thread->owner_pid = proc->id;
    thread->state = THREAD_STATE_READY;
    thread->stack_base = stack_base;
    thread->stack_pages = stack_pages;
    thread->ctx = proc->threads[0].ctx;
    thread->ctx.rip = entry_point;
    thread->ctx.rsp = stack_base + (uint64_t)stack_pages * PAGE_SIZE - 16ULL;
    thread->ctx.rbp = 0;
    thread->ctx.rbx = 0;
    thread->ctx.r12 = 0;
    thread->ctx.r13 = 0;
    thread->ctx.r14 = 0;
    thread->ctx.r15 = 0;
    thread->ctx.rdi = 0;
    thread->ctx.rsi = 0;
    thread->ctx.rdx = 0;
    thread->ctx.r8 = 0;
    thread->ctx.r9 = 0;
    thread->ctx.r10 = 0;
    thread->ctx.rflags = 0x202;
    thread->ctx.is_started = 0;
    thread->ctx.return_rax = 0;

    serial_print("[THREAD] Created user thread TID=");
    char num[12];
    int n = 0, v = tidx;
    if (v == 0) num[n++] = '0';
    while (v > 0 && n < 11) { num[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n) { char c = num[--n]; __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)c), "Nd"((uint16_t)0x3F8)); }
    serial_print(" in PID=");
    v = proc->id; n = 0;
    if (v == 0) num[n++] = '0';
    while (v > 0 && n < 11) { num[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n) { char c = num[--n]; __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)c), "Nd"((uint16_t)0x3F8)); }
    serial_print("\n");
    return tidx;
}

int process_create_linux_thread(uint64_t rip, uint64_t rsp, uint64_t fs_base, uint64_t fn) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES) return -1;
    process_t *proc = &g_processes[g_active_proc_idx];
    if (proc->state == PROCESS_STATE_UNUSED) return -1;

    int tidx = -1;
    for (int i = 1; i < MAX_THREADS_PER_PROCESS; ++i) {
        if (proc->threads[i].state == THREAD_STATE_UNUSED || proc->threads[i].state == THREAD_STATE_EXITED) {
            tidx = i;
            break;
        }
    }
    if (tidx < 0) return -1;

    thread_t *thread = &proc->threads[tidx];
    thread->tid = tidx;
    thread->owner_pid = proc->id;
    thread->state = THREAD_STATE_READY;
    thread->stack_base = 0;
    thread->stack_pages = 0;
    thread->wake_at_ms = 0;
    thread->fs_base = fs_base;
    thread->clear_child_tid = 0;

    thread->ctx = proc->threads[0].ctx;
    thread->ctx.cr3 = proc->pml4_phys;
    thread->ctx.rip = rip;
    thread->ctx.rsp = rsp;
    thread->ctx.rflags = 0x202;
    thread->ctx.is_started = 1;
    thread->ctx.return_rax = 0;
    thread->ctx.r9 = fn;
    thread->ctx.r10 = 0;
    thread->ctx.r8 = 0;
    thread->ctx.rdx = 0;
    thread->ctx.rsi = 0;
    thread->ctx.rdi = 0;
    thread->ctx.rbp = 0;
    thread->ctx.rbx = 0;
    thread->ctx.r12 = 0;
    thread->ctx.r13 = 0;
    thread->ctx.r14 = 0;
    thread->ctx.r15 = 0;

    KLOG_INFO("linux_sys", "created thread tid=%d in pid=%d rip=%llx rsp=%llx tls=%llx",
              tidx, proc->id, (unsigned long long)rip, (unsigned long long)rsp, (unsigned long long)fs_base);
    return tidx;
}

void process_set_thread_clear_child_tid(int tidx, uint64_t ctid_ptr) {
    if (g_active_proc_idx >= 0 && g_active_proc_idx < MAX_PROCESSES &&
        tidx >= 0 && tidx < MAX_THREADS_PER_PROCESS) {
        g_processes[g_active_proc_idx].threads[tidx].clear_child_tid = ctid_ptr;
    }
}

void process_set_current_clear_child_tid(uint64_t ctid_ptr) {
    if (g_active_proc_idx >= 0 && g_active_proc_idx < MAX_PROCESSES &&
        g_active_thread_idx >= 0 && g_active_thread_idx < MAX_THREADS_PER_PROCESS) {
        g_processes[g_active_proc_idx].threads[g_active_thread_idx].clear_child_tid = ctid_ptr;
    }
}

uint64_t process_get_current_clear_child_tid(void) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES ||
        g_active_thread_idx < 0 || g_active_thread_idx >= MAX_THREADS_PER_PROCESS) {
        return 0;
    }
    return g_processes[g_active_proc_idx].threads[g_active_thread_idx].clear_child_tid;
}

int process_has_other_live_threads(void) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES) return 0;
    process_t *proc = &g_processes[g_active_proc_idx];
    for (int i = 0; i < MAX_THREADS_PER_PROCESS; ++i) {
        if (i == g_active_thread_idx) continue;
        if (proc->threads[i].state == THREAD_STATE_READY ||
            proc->threads[i].state == THREAD_STATE_RUNNING) {
            return 1;
        }
    }
    return 0;
}

int process_exit_current_thread(void) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES) return -1;
    process_t *proc = &g_processes[g_active_proc_idx];
    int tidx = g_active_thread_idx;
    if (tidx < 0 || tidx >= MAX_THREADS_PER_PROCESS) return -1;
    thread_t *thread = &proc->threads[tidx];
    if (thread->state == THREAD_STATE_UNUSED || thread->state == THREAD_STATE_EXITED) return -1;

    if (thread->clear_child_tid) {
        uint32_t zero = 0;
        (void)copy_to_user((void*)thread->clear_child_tid, &zero, sizeof(zero));
        thread->clear_child_tid = 0;
    }

    if (tidx != 0 && thread->stack_base) {
        for (uint32_t p = 0; p < thread->stack_pages; ++p) {
            vmm_unmap_page(proc->pml4_phys, thread->stack_base + (uint64_t)p * PAGE_SIZE, 1);
        }
    }
    thread->state = THREAD_STATE_EXITED;
    thread->stack_base = 0;
    thread->stack_pages = 0;
    thread->wake_at_ms = 0;

    int alive = 0;
    for (int i = 0; i < MAX_THREADS_PER_PROCESS; ++i) {
        if (proc->threads[i].state == THREAD_STATE_READY || proc->threads[i].state == THREAD_STATE_RUNNING) {
            alive = 1;
            break;
        }
    }
    if (!alive) {
        socket_close_owner(proc->id);
        tty_close_owner(proc->id);
        fd_close_owner(proc->id);
        ipc_close_owner(proc->id);
        process_release_resources(proc);
        proc->state = PROCESS_STATE_EXITED;
        return 1;
    }
    proc->state = PROCESS_STATE_READY;
    return 0;
}

uint64_t process_vm_map(uint64_t size, uint32_t protection) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES || !size || size > PROCESS_VM_MAX_SIZE) return 0;
    if ((protection & ~7U) || ((protection & 2U) && (protection & 4U))) return 0;
    process_t *proc = &g_processes[g_active_proc_idx];
    uint64_t pages = (size + PAGE_SIZE - 1ULL) / PAGE_SIZE;
    uint64_t mapped_size = pages * PAGE_SIZE;
    if (mapped_size < size) return 0;
    int region_index = -1;
    for (int r = 0; r < MAX_VM_REGIONS_PER_PROCESS; ++r) if (!proc->vm_regions[r].active) { region_index = r; break; }
    if (region_index < 0) return 0;
    uint64_t base = (proc->vm_next_base + PAGE_SIZE - 1ULL) & ~(PAGE_SIZE - 1ULL);
    while (base < USER_VA_LIMIT && mapped_size <= USER_VA_LIMIT - base) {
        int clear = 1;
        for (uint64_t p = 0; p < pages; ++p) if (vmm_get_page_flags(proc->pml4_phys, base + p * PAGE_SIZE) & PTE_PRESENT) { clear = 0; break; }
        if (clear) break;
        base += mapped_size;
    }
    if (base >= USER_VA_LIMIT || mapped_size > USER_VA_LIMIT - base) return 0;
    uint64_t page_flags = PTE_USER;
    if (protection & 2U) page_flags |= PTE_WRITABLE;
    if (!(protection & 4U)) page_flags |= PTE_NO_EXECUTE;
    for (uint64_t p = 0; p < pages; ++p) {
        uint64_t phys = pmm_alloc_page();
        if (!phys || vmm_map_page(proc->pml4_phys, base + p * PAGE_SIZE, phys, page_flags) != 0) {
            if (phys) pmm_free_page(phys);
            for (uint64_t rollback = 0; rollback < p; ++rollback) (void)vmm_unmap_page(proc->pml4_phys, base + rollback * PAGE_SIZE, 1);
            return 0;
        }
        uint8_t *page = (uint8_t *)(phys + g_hhdm_offset);
        for (size_t b = 0; b < PAGE_SIZE; ++b) page[b] = 0;
    }
    process_vm_region_t *region = &proc->vm_regions[region_index];
    region->base = base; region->size = mapped_size; region->protection = protection;
    region->owns_physical = 1; region->backing_kind = 0; region->backing_ptr = 0; region->active = 1;
    proc->vm_next_base = base + mapped_size;
    return base;
}

/* MAP_FIXED is required by musl's mallocng for the guard page preceding its
 * metadata arena.  The initial program break is not represented by a normal
 * VM-region descriptor, so silently ignoring the requested address corrupts
 * allocator invariants and eventually turns small calloc() calls into NULL. */
uint64_t process_vm_map_fixed(uint64_t address, uint64_t size, uint32_t protection) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES ||
        !address || !size || (address & (PAGE_SIZE - 1ULL)) ||
        size > PROCESS_VM_MAX_SIZE || (protection & ~7U) ||
        ((protection & 2U) && (protection & 4U))) return 0;
    uint64_t mapped_size = (size + PAGE_SIZE - 1ULL) & ~(PAGE_SIZE - 1ULL);
    if (mapped_size < size || address >= USER_VA_LIMIT ||
        mapped_size > USER_VA_LIMIT - address) return 0;

    process_t *proc = &g_processes[g_active_proc_idx];
    int region_index = -1;
    for (int r = 0; r < MAX_VM_REGIONS_PER_PROCESS; ++r) {
        process_vm_region_t *region = &proc->vm_regions[r];
        if (!region->active && region_index < 0) region_index = r;
        if (region->active && address < region->base + region->size &&
            region->base < address + mapped_size) {
            /* Whole-region replacement is safe.  Reject partial replacement
             * rather than leaving stale ownership metadata behind. */
            if (region->base != address || region->size != mapped_size) return 0;
            if (process_vm_unmap(address, mapped_size) != 0) return 0;
            region_index = r;
            break;
        }
    }
    if (region_index < 0) return 0;

    uint64_t page_flags = PTE_USER;
    if (protection & 2U) page_flags |= PTE_WRITABLE;
    if (!(protection & 4U)) page_flags |= PTE_NO_EXECUTE;
    uint64_t pages = mapped_size / PAGE_SIZE;
    for (uint64_t p = 0; p < pages; ++p) {
        uint64_t va = address + p * PAGE_SIZE;
        if (vmm_get_page_flags(proc->pml4_phys, va) & PTE_PRESENT)
            (void)vmm_unmap_page(proc->pml4_phys, va, 1);
        uint64_t phys = pmm_alloc_page();
        if (!phys || vmm_map_page(proc->pml4_phys, va, phys, page_flags) != 0) {
            if (phys) pmm_free_page(phys);
            for (uint64_t rollback = 0; rollback < p; ++rollback)
                (void)vmm_unmap_page(proc->pml4_phys, address + rollback * PAGE_SIZE, 1);
            return 0;
        }
    }
    process_vm_region_t *region = &proc->vm_regions[region_index];
    region->base = address; region->size = mapped_size; region->protection = protection;
    region->owns_physical = 1; region->backing_kind = 0; region->backing_ptr = 0; region->active = 1;
    return address;
}

uint64_t process_vm_map_phys(const uint64_t *phys_pages, uint64_t num_pages, uint32_t protection) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES || !phys_pages || !num_pages) return 0;
    if ((protection & ~7U) || ((protection & 2U) && (protection & 4U)) || num_pages > PROCESS_VM_MAX_SIZE / PAGE_SIZE) return 0;
    process_t *proc = &g_processes[g_active_proc_idx];
    uint64_t mapped_size = num_pages * PAGE_SIZE;
    int region_index = -1;
    for (int r = 0; r < MAX_VM_REGIONS_PER_PROCESS; ++r) if (!proc->vm_regions[r].active) { region_index = r; break; }
    if (region_index < 0) return 0;
    uint64_t base = (proc->vm_next_base + PAGE_SIZE - 1ULL) & ~(PAGE_SIZE - 1ULL);
    while (base < USER_VA_LIMIT && mapped_size <= USER_VA_LIMIT - base) {
        int clear = 1;
        for (uint64_t p = 0; p < num_pages; ++p) if (vmm_get_page_flags(proc->pml4_phys, base + p * PAGE_SIZE) & PTE_PRESENT) { clear = 0; break; }
        if (clear) break;
        base += mapped_size;
    }
    if (base >= USER_VA_LIMIT || mapped_size > USER_VA_LIMIT - base) return 0;
    uint64_t page_flags = PTE_USER;
    if (protection & 2U) page_flags |= PTE_WRITABLE;
    if (!(protection & 4U)) page_flags |= PTE_NO_EXECUTE;
    for (uint64_t p = 0; p < num_pages; ++p) {
        if (vmm_map_page(proc->pml4_phys, base + p * PAGE_SIZE, phys_pages[p], page_flags) != 0) {
            for (uint64_t rollback = 0; rollback < p; ++rollback) (void)vmm_unmap_page(proc->pml4_phys, base + rollback * PAGE_SIZE, 0);
            return 0;
        }
    }
    process_vm_region_t *region = &proc->vm_regions[region_index];
    region->base = base; region->size = mapped_size; region->protection = protection;
    region->owns_physical = 0; region->backing_kind = 0; region->backing_ptr = 0; region->active = 1;
    proc->vm_next_base = base + mapped_size;
    return base;
}

int process_vm_attach_backing(uint64_t address, uint8_t backing_kind, void *backing_ptr) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES || !address || !backing_ptr) return -1;
    process_t *proc = &g_processes[g_active_proc_idx];
    for (int r = 0; r < MAX_VM_REGIONS_PER_PROCESS; ++r) {
        process_vm_region_t *region = &proc->vm_regions[r];
        if (region->active && region->base == address) {
            region->backing_kind = backing_kind;
            region->backing_ptr = backing_ptr;
            return 0;
        }
    }
    return -1;
}

int process_vm_protect(uint64_t address, uint64_t size, uint32_t protection) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES || !address || !size) return -1;
    if ((address & (PAGE_SIZE - 1ULL)) || (protection & ~7U) || ((protection & 2U) && (protection & 4U))) return -1;
    uint64_t mapped_size = (size + PAGE_SIZE - 1ULL) & ~(PAGE_SIZE - 1ULL);
    if (mapped_size < size || address >= USER_VA_LIMIT || mapped_size > USER_VA_LIMIT - address) return -1;
    process_t *proc = &g_processes[g_active_proc_idx];
    uint64_t end = address + mapped_size;
    for (uint64_t page = address; page < end; page += PAGE_SIZE) {
        int covered = 0;
        for (int r = 0; r < MAX_VM_REGIONS_PER_PROCESS; ++r) {
            process_vm_region_t *region = &proc->vm_regions[r];
            if (region->active && page >= region->base && page < region->base + region->size) { covered = 1; break; }
        }
        if (!covered || !(vmm_get_page_flags(proc->pml4_phys, page) & PTE_PRESENT)) return -1;
    }
    uint64_t flags = PTE_USER;
    if (protection & 2U) flags |= PTE_WRITABLE;
    if (!(protection & 4U)) flags |= PTE_NO_EXECUTE;
    for (uint64_t page = address; page < end; page += PAGE_SIZE)
        if (vmm_set_page_flags(proc->pml4_phys, page, flags) != 0) return -1;
    return 0;
}

int process_vm_unmap(uint64_t address, uint64_t size) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES || !address || !size) return -1;
    process_t *proc = &g_processes[g_active_proc_idx];
    uint64_t mapped_size = (size + PAGE_SIZE - 1ULL) & ~(PAGE_SIZE - 1ULL);
    if (mapped_size < size) return -1;
    for (int r = 0; r < MAX_VM_REGIONS_PER_PROCESS; ++r) {
        process_vm_region_t *region = &proc->vm_regions[r];
        if (!region->active || region->base != address || region->size != mapped_size) continue;
        for (uint64_t offset = 0; offset < region->size; offset += PAGE_SIZE) {
            if (vmm_unmap_page(proc->pml4_phys, region->base + offset, region->owns_physical) != 0) return -1;
        }
        if (region->backing_kind == FD_KIND_MEMFD && region->backing_ptr) {
            extern void unix_memfd_release(void *custom_ptr);
            unix_memfd_release(region->backing_ptr);
        }
        region->active = 0;
        return 0;
    }
    return -1;
}

uint64_t process_vm_brk(uint64_t address) {
    if (g_active_proc_idx < 0 || g_active_proc_idx >= MAX_PROCESSES) return 0;
    process_t *proc = &g_processes[g_active_proc_idx];
    if (address == 0) return proc->heap_break;
    if (address < proc->heap_base || address > proc->heap_base + PROCESS_VM_MAX_SIZE) return proc->heap_break;

    uint64_t target_end = (address + PAGE_SIZE - 1ULL) & ~(PAGE_SIZE - 1ULL);
    if (target_end < address) return proc->heap_break;
    if (target_end > proc->heap_mapped_end) {
        uint64_t mapped = proc->heap_mapped_end;
        while (mapped < target_end) {
            if (vmm_get_page_flags(proc->pml4_phys, mapped) & PTE_PRESENT) break;
            uint64_t phys = pmm_alloc_page();
            if (!phys || vmm_map_page(proc->pml4_phys, mapped, phys,
                                      PTE_USER | PTE_WRITABLE | PTE_NO_EXECUTE) != 0) {
                if (phys) pmm_free_page(phys);
                break;
            }
            uint8_t *page = (uint8_t *)(phys + g_hhdm_offset);
            for (size_t b = 0; b < PAGE_SIZE; ++b) page[b] = 0;
            mapped += PAGE_SIZE;
        }
        if (mapped != target_end) {
            while (mapped > proc->heap_mapped_end) {
                mapped -= PAGE_SIZE;
                (void)vmm_unmap_page(proc->pml4_phys, mapped, 1);
            }
            return proc->heap_break;
        }
        proc->heap_mapped_end = target_end;
    } else if (target_end < proc->heap_mapped_end) {
        for (uint64_t mapped = target_end; mapped < proc->heap_mapped_end; mapped += PAGE_SIZE) {
            (void)vmm_unmap_page(proc->pml4_phys, mapped, 1);
        }
        proc->heap_mapped_end = target_end;
    }
    proc->heap_break = address;
    return address;
}

void process_run_test_ring3(void) {
    serial_print("[PROCESS] Running baseline Ring 3 test...\n");

    if (process_save_kernel_state() != 0) {
        serial_print("[PROCESS] Returned back to Kernel Mode from Ring 3 test successfully!\n");
        return;
    }

    uint64_t user_pml4 = vmm_create_user_pml4();
    uint64_t code_phys = user_pml4 ? pmm_alloc_page() : 0;
    if (!user_pml4 || !code_phys ||
        vmm_map_page(user_pml4, 0x40000000ULL, code_phys, PTE_USER) != 0) {
        if (code_phys) pmm_free_page(code_phys);
        if (user_pml4) vmm_destroy_user_pml4(user_pml4);
        serial_print("[PROCESS] Ring 3 test setup failed.\n");
        process_return_to_kernel(-1);
    }

    uint8_t *dest_code = (uint8_t*)(code_phys + g_hhdm_offset);
    size_t payload_len = (size_t)(user_mode_test_payload_end - user_mode_test_payload);
    if (payload_len > PAGE_SIZE) payload_len = PAGE_SIZE;

    for (size_t i = 0; i < payload_len; i++) {
        dest_code[i] = user_mode_test_payload[i];
    }

    uint64_t stack_phys = pmm_alloc_page();
    if (!stack_phys || vmm_map_page(user_pml4, 0x70000000ULL, stack_phys,
                                    PTE_USER | PTE_WRITABLE | PTE_NO_EXECUTE) != 0) {
        if (stack_phys) pmm_free_page(stack_phys);
        vmm_destroy_user_pml4(user_pml4);
        serial_print("[PROCESS] Ring 3 test stack setup failed.\n");
        process_return_to_kernel(-1);
    }
    uint64_t user_stack_top = 0x70000000ULL + PAGE_SIZE - 16;

    gdt_set_kernel_stack(g_syscall_kernel_stack_top);
    vmm_switch_pml4(user_pml4);
    enter_user_mode(0x40000000ULL, user_stack_top);
}
