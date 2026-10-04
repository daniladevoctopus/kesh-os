#include "elf.h"
#include "memory.h"
#include "serial.h"
#include <stddef.h>

static int add_overflow_u64(uint64_t a, uint64_t b, uint64_t *out) {
    *out = a + b;
    return *out < a;
}

static uint64_t segment_page_flags(uint32_t p_flags) {
    uint64_t flags = PTE_USER;
    if (p_flags & PF_W) flags |= PTE_WRITABLE;
    if (!(p_flags & PF_X)) flags |= PTE_NO_EXECUTE;
    return flags;
}

int elf_load(const void *elf_data, uint64_t size, uint64_t *out_pml4, uint64_t *out_entry) {
    if (!elf_data || !out_pml4 || !out_entry || size < sizeof(Elf64_Ehdr)) return -1;

    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)elf_data;
    if (ehdr->e_ident[0] != ELFMAG0 || ehdr->e_ident[1] != ELFMAG1 ||
        ehdr->e_ident[2] != ELFMAG2 || ehdr->e_ident[3] != ELFMAG3 ||
        ehdr->e_ident[4] != ELFCLASS64 || ehdr->e_ident[5] != ELFDATA2LSB ||
        ehdr->e_ident[6] != EV_CURRENT || ehdr->e_ident[7] != ELFOSABI_SYSV ||
        ehdr->e_machine != EM_X86_64 || ehdr->e_version != EV_CURRENT) {
        serial_print("[ELF] Invalid ELF64 x86_64 header.\n");
        return -2;
    }
    if (ehdr->e_type != ET_EXEC) return -3;
    if (ehdr->e_ehsize != sizeof(Elf64_Ehdr) || ehdr->e_phentsize != sizeof(Elf64_Phdr) ||
        ehdr->e_phnum == 0 || ehdr->e_phnum > 128) return -4;
    if (ehdr->e_phoff > size || (uint64_t)ehdr->e_phnum > (size - ehdr->e_phoff) / ehdr->e_phentsize) return -5;

    uint64_t user_pml4 = vmm_create_user_pml4();
    if (!user_pml4) return -6;

    uint64_t stack_high = 0x70000000ULL;
    uint64_t stack_low = 0x6FFF0000ULL;

    int load_segments = 0;
    int entry_covered = 0;
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        uint64_t phdr_offset = ehdr->e_phoff + (uint64_t)i * ehdr->e_phentsize;
        const Elf64_Phdr *phdr = (const Elf64_Phdr *)((const uint8_t *)elf_data + phdr_offset);
        if (phdr->p_type == PT_INTERP || phdr->p_type == PT_DYNAMIC) {
            vmm_destroy_user_pml4(user_pml4);
            return -7;
        }
        if (phdr->p_type != PT_LOAD) continue;
        load_segments++;
        if (phdr->p_filesz > phdr->p_memsz) {
            vmm_destroy_user_pml4(user_pml4);
            return -7;
        }
        if ((phdr->p_flags & ~(PF_R | PF_W | PF_X)) || !(phdr->p_flags & PF_R) ||
            ((phdr->p_flags & PF_W) && (phdr->p_flags & PF_X))) {
            serial_print("[ELF] Rejected writable+executable segment.\n");
            vmm_destroy_user_pml4(user_pml4);
            return -8;
        }
        if (phdr->p_align && ((phdr->p_align & (phdr->p_align - 1ULL)) != 0 ||
                              (phdr->p_offset & (phdr->p_align - 1ULL)) !=
                              (phdr->p_vaddr & (phdr->p_align - 1ULL)))) {
            vmm_destroy_user_pml4(user_pml4);
            return -9;
        }

        uint64_t file_end, mem_end;
        if (add_overflow_u64(phdr->p_offset, phdr->p_filesz, &file_end) || file_end > size ||
            add_overflow_u64(phdr->p_vaddr, phdr->p_memsz, &mem_end) ||
            phdr->p_vaddr < PAGE_SIZE || phdr->p_vaddr >= USER_VA_LIMIT || mem_end > USER_VA_LIMIT ||
            (phdr->p_vaddr < stack_high && mem_end > stack_low)) {
            vmm_destroy_user_pml4(user_pml4);
            return -9;
        }
        if (phdr->p_memsz == 0) continue;
        for (uint16_t previous = 0; previous < i; ++previous) {
            const Elf64_Phdr *other = (const Elf64_Phdr *)((const uint8_t *)elf_data +
                                      ehdr->e_phoff + (uint64_t)previous * ehdr->e_phentsize);
            uint64_t other_end = 0;
            if (other->p_type == PT_LOAD && other->p_memsz &&
                (!add_overflow_u64(other->p_vaddr, other->p_memsz, &other_end)) &&
                phdr->p_vaddr < other_end && other->p_vaddr < mem_end) {
                vmm_destroy_user_pml4(user_pml4);
                return -12;
            }
        }
        if ((phdr->p_flags & PF_X) && ehdr->e_entry >= phdr->p_vaddr && ehdr->e_entry < mem_end) entry_covered = 1;

        uint64_t vaddr_start = phdr->p_vaddr & ~(PAGE_SIZE - 1ULL);
        uint64_t vaddr_end = (mem_end + PAGE_SIZE - 1ULL) & ~(PAGE_SIZE - 1ULL);
        if (vaddr_end < mem_end) {
            vmm_destroy_user_pml4(user_pml4);
            return -10;
        }
        uint64_t final_flags = segment_page_flags(phdr->p_flags);

        for (uint64_t page_va = vaddr_start; page_va < vaddr_end; page_va += PAGE_SIZE) {
            uint64_t phys = vmm_get_page_phys(user_pml4, page_va);
            if (!phys) {
                phys = pmm_alloc_page();
                if (!phys || vmm_map_page(user_pml4, page_va, phys, final_flags) != 0) {
                    if (phys) pmm_free_page(phys);
                    vmm_destroy_user_pml4(user_pml4);
                    return -11;
                }
                uint8_t *page = (uint8_t *)(phys + g_hhdm_offset);
                for (size_t b = 0; b < PAGE_SIZE; b++) page[b] = 0;
            } else if ((vmm_get_page_flags(user_pml4, page_va) &
                        (PTE_USER | PTE_WRITABLE | PTE_NO_EXECUTE)) !=
                       (final_flags & (PTE_USER | PTE_WRITABLE | PTE_NO_EXECUTE))) {
                vmm_destroy_user_pml4(user_pml4);
                return -12;
            }

            uint64_t file_start = phdr->p_vaddr;
            uint64_t file_data_end = phdr->p_vaddr + phdr->p_filesz;
            uint64_t copy_start = page_va > file_start ? page_va : file_start;
            uint64_t page_next = page_va + PAGE_SIZE;
            uint64_t copy_end = page_next < file_data_end ? page_next : file_data_end;
            if (copy_start < copy_end) {
                uint64_t file_offset = phdr->p_offset + (copy_start - phdr->p_vaddr);
                uint64_t page_offset = copy_start - page_va;
                uint64_t count = copy_end - copy_start;
                const uint8_t *src = (const uint8_t *)elf_data + file_offset;
                uint8_t *dst = (uint8_t *)(phys + g_hhdm_offset) + page_offset;
                for (uint64_t b = 0; b < count; b++) dst[b] = src[b];
            }

        }
    }

    if (!load_segments || !entry_covered) {
        vmm_destroy_user_pml4(user_pml4);
        return -14;
    }
    uint64_t entry_flags = vmm_get_page_flags(user_pml4, ehdr->e_entry & ~(PAGE_SIZE - 1ULL));
    if (ehdr->e_entry >= USER_VA_LIMIT || !(entry_flags & PTE_PRESENT) ||
        !(entry_flags & PTE_USER) || (entry_flags & PTE_NO_EXECUTE)) {
        vmm_destroy_user_pml4(user_pml4);
        return -14;
    }

    for (uint64_t s_va = stack_low; s_va < stack_high; s_va += PAGE_SIZE) {
        uint64_t s_phys = pmm_alloc_page();
        if (!s_phys || vmm_map_page(user_pml4, s_va, s_phys, PTE_USER | PTE_WRITABLE | PTE_NO_EXECUTE) != 0) {
            if (s_phys) pmm_free_page(s_phys);
            vmm_destroy_user_pml4(user_pml4);
            return -13;
        }
    }

    *out_pml4 = user_pml4;
    *out_entry = ehdr->e_entry;
    serial_print("[ELF] Loaded with enforced segment permissions. Entry: ");
    serial_print_hex(ehdr->e_entry);
    serial_print("\n");
    return 0;
}
