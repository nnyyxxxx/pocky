#include "elf.hpp"

#include <cstring>

#include "heap.hpp"
#include "physical_memory.hpp"
#include "printf.hpp"
#include "virtual_memory.hpp"

namespace kernel {

namespace {

Elf64_Shdr* find_section_by_type(const Elf64_Ehdr* header, uint32_t type) {
    auto* sections = reinterpret_cast<const Elf64_Shdr*>(reinterpret_cast<const char*>(header) +
                                                         header->e_shoff);

    for (uint16_t i = 0; i < header->e_shnum; i++) {
        if (sections[i].sh_type == type) return const_cast<Elf64_Shdr*>(&sections[i]);
    }
    return nullptr;
}

Elf64_Shdr* find_section_by_name(const Elf64_Ehdr* header, const char* name) {
    auto* sections = reinterpret_cast<const Elf64_Shdr*>(reinterpret_cast<const char*>(header) +
                                                         header->e_shoff);

    auto* shstrtab = &sections[header->e_shstrndx];
    const char* strtab = reinterpret_cast<const char*>(header) + shstrtab->sh_offset;

    for (uint16_t i = 0; i < header->e_shnum; i++) {
        const char* section_name = strtab + sections[i].sh_name;
        if (strcmp(section_name, name) == 0) return const_cast<Elf64_Shdr*>(&sections[i]);
    }
    return nullptr;
}

}  // namespace

bool ElfLoader::validate_elf_header(const Elf64_Ehdr* header) {
    if (memcmp(header->e_ident, &ELFMAG, SELFMAG) != 0) return false;
    if (header->e_ident[4] != ELFCLASS64) return false;
    if (header->e_ident[5] != ELFDATA2LSB) return false;
    if (header->e_type != ET_EXEC && header->e_type != ET_DYN) return false;
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

        vmm.map_page(addr, phys_page, (phdr->p_flags & PF_W) != 0);
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

bool ElfLoader::process_relocations(const Elf64_Ehdr* header, const char* base) {
    auto* rela_plt = find_section_by_name(header, ".rela.plt");
    if (!rela_plt) return true;

    auto* dynsym = find_section_by_type(header, SHT_DYNSYM);
    if (!dynsym) return false;

    auto* dynstr = find_section_by_type(header, SHT_STRTAB);
    if (!dynstr) return false;

    auto* symbols = reinterpret_cast<const Elf64_Sym*>(base + dynsym->sh_offset);
    const char* strings = base + dynstr->sh_offset;
    auto* relocations = reinterpret_cast<const Elf64_Rela*>(base + rela_plt->sh_offset);

    size_t relcount = rela_plt->sh_size / sizeof(Elf64_Rela);

    for (size_t i = 0; i < relcount; i++) {
        const auto* rel = &relocations[i];

        if (ELF64_R_TYPE(rel->r_info) != R_X86_64_JUMP_SLOT) continue;

        uint64_t sym_idx = ELF64_R_SYM(rel->r_info);
        const char* symbol_name = strings + symbols[sym_idx].st_name;

        printf("Unresolved symbol: %s\n", symbol_name);
    }

    return true;
}

bool ElfLoader::process_dynamic_section(const Elf64_Ehdr* header, const char* base) {
    auto* dyn_section = find_section_by_type(header, SHT_DYNAMIC);
    if (!dyn_section) return true;

    auto* dyn_entries = reinterpret_cast<const Elf64_Dyn*>(base + dyn_section->sh_offset);
    auto* strtab_section = find_section_by_type(header, SHT_STRTAB);
    if (!strtab_section) return false;

    const char* strtab = base + strtab_section->sh_offset;

    for (size_t i = 0; dyn_entries[i].d_tag != DT_NULL; i++) {
        if (dyn_entries[i].d_tag == DT_NEEDED) {
            const char* lib_name = strtab + dyn_entries[i].d_val;
            printf("Required shared library: %s\n", lib_name);
        }
    }

    return true;
}

bool ElfLoader::load_elf(const char* path, uint64_t& entry_point) {
    const Elf64_Ehdr* header = reinterpret_cast<const Elf64_Ehdr*>(path);
    if (!validate_elf_header(header)) return false;

    if (!load_program_headers(header, path)) return false;

    if (!process_dynamic_section(header, path)) return false;
    if (!process_relocations(header, path)) return false;

    entry_point = header->e_entry;
    return true;
}

}  // namespace kernel