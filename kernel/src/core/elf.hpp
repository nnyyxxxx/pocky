#pragma once

#include <cstdint>

namespace kernel {

struct Elf64_Ehdr {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

class ElfLoader {
public:
    static bool load_elf(const char* path, uint64_t& entry_point);

private:
    static bool validate_elf_header(const Elf64_Ehdr* header);
    static bool load_program_headers(const Elf64_Ehdr* header, const char* base);
    static bool map_segment(const Elf64_Phdr* phdr, const char* base);
};
}