#pragma once

#include <cstdint>

namespace kernel {

constexpr uint32_t ELFMAG = 0x464C457F;
constexpr int SELFMAG = 4;
constexpr uint8_t ELFCLASS64 = 2;
constexpr uint8_t ELFDATA2LSB = 1;
constexpr uint16_t ET_EXEC = 2;
constexpr uint16_t ET_DYN = 3;
constexpr uint32_t PT_LOAD = 1;
constexpr uint32_t PT_DYNAMIC = 2;
constexpr uint32_t SHT_DYNSYM = 11;
constexpr uint32_t SHT_STRTAB = 3;
constexpr uint32_t SHT_DYNAMIC = 6;
constexpr uint32_t SHT_RELA = 4;
constexpr uint64_t DT_NULL = 0;
constexpr uint64_t DT_NEEDED = 1;
constexpr uint64_t DT_RELA = 7;
constexpr uint64_t DT_RELASZ = 8;
constexpr uint64_t DT_STRTAB = 5;
constexpr uint64_t DT_SYMTAB = 6;
constexpr uint64_t R_X86_64_JUMP_SLOT = 7;

constexpr uint32_t PF_X = 0x1;
constexpr uint32_t PF_W = 0x2;
constexpr uint32_t PF_R = 0x4;

constexpr uint64_t SHF_WRITE = 0x1;
constexpr uint64_t SHF_ALLOC = 0x2;
constexpr uint64_t SHF_EXECINSTR = 0x4;

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

struct Elf64_Shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
};

struct Elf64_Sym {
    uint32_t st_name;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
};

struct Elf64_Rela {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t r_addend;
};

struct Elf64_Dyn {
    uint64_t d_tag;
    uint64_t d_val;
};

#define ELF64_R_SYM(i) ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffffL)
#define ELF64_R_INFO(s, t) (((s) << 32) + ((t) & 0xffffffffL))

class ElfLoader {
public:
    static bool validate_elf_header(const Elf64_Ehdr* header);
    static bool load_program_headers(const Elf64_Ehdr* header, const char* base);
    static bool map_segment(const Elf64_Phdr* phdr, const char* base);
    static bool process_relocations(const Elf64_Ehdr* header, const char* base);
    static bool process_dynamic_section(const Elf64_Ehdr* header, const char* base);
    static bool load_elf(const char* path, uint64_t& entry_point);
};

class DynamicLinker {
public:
    static bool initialize();

    static bool load_shared_library(const char* path);

    static void* resolve_symbol(const char* symbol_name);

    static void* runtime_resolver(uint64_t got_base, uint64_t rel_index);

private:
    static bool locate_got(const Elf64_Ehdr* ehdr, uint64_t& got_base);

    static bool process_dynamic_section(const Elf64_Ehdr* ehdr, const char* base);

    static void* lookup_symbol(const char* name, const Elf64_Ehdr* lib_ehdr);

    static bool process_relocations(const Elf64_Ehdr* ehdr, const char* base);
};

}