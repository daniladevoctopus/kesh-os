// парсер эльфов чтоб бинарники запускать
#include "elf.h"
#include "memory.h"
#include <stddef.h>

static void serial_write(const char *s) {
    if (!s) return;
    while (*s) {
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)*s++), "Nd"((uint16_t)0x3F8));
    }
}

static void serial_write_hex(uint64_t val) {
    const char hex_chars[] = "0123456789ABCDEF";
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        buf[2 + i] = hex_chars[(val >> ((15 - i) * 4)) & 0xF];
    }
    buf[18] = '\0';
    serial_write(buf);
}

int elf_load(const void *elf_data, uint64_t size, uint64_t *out_pml4, uint64_t *out_entry) {
    if (!elf_data || size < sizeof(Elf64_Ehdr)) {
        serial_write("[ELF] Error: Binary too small or NULL\n");
        return -1;
    }

    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr*)elf_data;

    if (ehdr->e_ident[0] != ELFMAG0 ||
        ehdr->e_ident[1] != ELFMAG1 ||
        ehdr->e_ident[2] != ELFMAG2 ||
        ehdr->e_ident[3] != ELFMAG3) {
        serial_write("[ELF] Error: Invalid ELF magic signature\n");
        return -2;
    }

    if (ehdr->e_ident[4] != ELFCLASS64) {
        serial_write("[ELF] Error: Not a 64-bit ELF\n");
        return -3;
    }

    if (ehdr->e_machine != EM_X86_64) {
        serial_write("[ELF] Error: Architecture is not x86_64\n");
        return -4;
    }

    if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0) {
        serial_write("[ELF] Error: No program headers found\n");
        return -5;
    }

    serial_write("[ELF] Valid ELF64 binary. Creating isolated address space...\n");

    uint64_t user_pml4 = vmm_create_user_pml4();

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        uint64_t phdr_offset = ehdr->e_phoff + (uint64_t)i * ehdr->e_phentsize;
        if (phdr_offset + sizeof(Elf64_Phdr) > size) {
            serial_write("[ELF] Error: Program header out of file bounds\n");
            return -6;
        }

        const Elf64_Phdr *phdr = (const Elf64_Phdr*)((const uint8_t*)elf_data + phdr_offset);
        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        serial_write("[ELF] Loading segment at VA: ");
        serial_write_hex(phdr->p_vaddr);
        serial_write(", memsz: ");
        serial_write_hex(phdr->p_memsz);
        serial_write("\n");

        uint64_t vaddr_start = phdr->p_vaddr & ~0xFFFULL;
        uint64_t vaddr_end = (phdr->p_vaddr + phdr->p_memsz + 0xFFFULL) & ~0xFFFULL;

        for (uint64_t page_va = vaddr_start; page_va < vaddr_end; page_va += PAGE_SIZE) {
            uint64_t phys = vmm_get_page_phys(user_pml4, page_va);
            uint8_t *dst_page;

            if (!phys) {
                phys = pmm_alloc_page();
                vmm_map_page(user_pml4, page_va, phys, PTE_USER | PTE_WRITABLE);
                dst_page = (uint8_t*)(phys + g_hhdm_offset);

                for (size_t b = 0; b < PAGE_SIZE; b++) {
                    dst_page[b] = 0;
                }
            } else {
                dst_page = (uint8_t*)(phys + g_hhdm_offset);
            }

            uint64_t file_data_start = phdr->p_vaddr;
            uint64_t file_data_end = phdr->p_vaddr + phdr->p_filesz;

            uint64_t copy_start = page_va > file_data_start ? page_va : file_data_start;
            uint64_t page_next = page_va + PAGE_SIZE;
            uint64_t copy_end = page_next < file_data_end ? page_next : file_data_end;

            if (copy_start < copy_end) {
                uint64_t offset_in_file = phdr->p_offset + (copy_start - phdr->p_vaddr);
                uint64_t offset_in_page = copy_start - page_va;
                uint64_t count = copy_end - copy_start;

                if (offset_in_file + count <= size) {
                    const uint8_t *src = (const uint8_t*)elf_data + offset_in_file;
                    for (uint64_t c = 0; c < count; c++) {
                        dst_page[offset_in_page + c] = src[c];
                    }
                }
            }

            uint64_t bss_start = file_data_end;
            uint64_t bss_end = phdr->p_vaddr + phdr->p_memsz;
            if (bss_start < bss_end) {
                uint64_t zero_start = page_va > bss_start ? page_va : bss_start;
                uint64_t zero_end = page_next < bss_end ? page_next : bss_end;
                if (zero_start < zero_end) {
                    uint64_t offset_in_page = zero_start - page_va;
                    uint64_t count = zero_end - zero_start;
                    for (uint64_t c = 0; c < count; c++) {
                        dst_page[offset_in_page + c] = 0;
                    }
                }
            }
        }
    }

    serial_write("[ELF] Allocating user stack at 0x70000000...\n");
    for (uint64_t s_va = 0x6FFF0000ULL; s_va < 0x70000000ULL; s_va += PAGE_SIZE) {
        uint64_t s_phys = pmm_alloc_page();
        vmm_map_page(user_pml4, s_va, s_phys, PTE_USER | PTE_WRITABLE);
    }

    *out_pml4 = user_pml4;
    *out_entry = ehdr->e_entry;

    serial_write("[ELF] Program loaded successfully! Entry: ");
    serial_write_hex(ehdr->e_entry);
    serial_write("\n");

    return 0;
}
