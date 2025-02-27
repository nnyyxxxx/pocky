#include "elf.hpp"

#include <cstring>

#include "heap.hpp"
#include "physical_memory.hpp"
#include "virtual_memory.hpp"

namespace kernel {

bool ElfLoader::validate_elf_header(const Elf64_Ehdr* header) {
    if (memcmp(header->e_ident, &ELFMAG, SELFMAG) != 0) return false;
    if (header->e_ident[4] != ELFCLASS64) return false;
    if (header->e_ident[5] != ELFDATA2LSB) return false;
    if (header->e_type != ET_EXEC) return false;
    if (header->e_shnum == 0) return false;

    return true;
}

bool ElfLoader::map_segment(const Elf64_Phdr* phdr, const char* base) {
    if (phdr->p_type != PT_LOAD) return true;

    auto& vmm = VirtualMemoryManager::instance();
    auto& pmm = PhysicalMemoryManager::instance();

    uint64_t vaddr_start = phdr->p_vaddr & ~0xFFF;
    uint64_t vaddr_end = (phdr->p_vaddr + phdr->p_memsz + 0xFFF) & ~0xFFF;

    for (uint64_t addr = vaddr_start; addr < vaddr_end; addr += 4096) {
        uintptr_t phys_page = reinterpret_cast<uintptr_t>(pmm.allocate_frame());
        if (phys_page == 0) return false;

        vmm.map_page(addr, phys_page, true);
    }

    memcpy(reinterpret_cast<void*>(phdr->p_vaddr), base + phdr->p_offset, phdr->p_filesz);

    if (phdr->p_memsz > phdr->p_filesz) {
        memset(reinterpret_cast<void*>(phdr->p_vaddr + phdr->p_filesz), 0,
               phdr->p_memsz - phdr->p_filesz);
    }

    return true;
}

bool ElfLoader::load_program_headers(const Elf64_Ehdr* header, const char* base) {
    const Elf64_Phdr* phdr = reinterpret_cast<const Elf64_Phdr*>(base + header->e_phoff);

    for (uint16_t i = 0; i < header->e_phnum; i++) {
        if (!map_segment(&phdr[i], base)) return false;
    }

    return true;
}

bool ElfLoader::load_elf(const char* path, uint64_t& entry_point) {
    const Elf64_Ehdr* header = reinterpret_cast<const Elf64_Ehdr*>(path);

    if (!validate_elf_header(header)) return false;

    if (!load_program_headers(header, path)) return false;

    entry_point = header->e_entry;
    return true;
}

}  // namespace kernel