#include <cstring>

#include "elf.hpp"
#include "heap.hpp"
#include "physical_memory.hpp"
#include "terminal.hpp"
#include "virtual_memory.hpp"

namespace kernel {

struct LoadedLibrary {
    char* name = nullptr;
    Elf64_Ehdr* header = nullptr;
    void* base = nullptr;
    uint64_t got_base = 0;
};

constexpr size_t MAX_LOADED_LIBRARIES = 16;
static LoadedLibrary loaded_libraries[MAX_LOADED_LIBRARIES];
static size_t num_loaded_libraries = 0;

static LoadedLibrary main_executable;

static Elf64_Shdr* find_section_by_type(Elf64_Ehdr* ehdr, uint32_t type) {
    Elf64_Shdr* sections =
        reinterpret_cast<Elf64_Shdr*>(reinterpret_cast<char*>(ehdr) + ehdr->e_shoff);

    for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
        if (sections[i].sh_type == type) return &sections[i];
    }

    return nullptr;
}

static Elf64_Shdr* find_section_by_name(Elf64_Ehdr* ehdr, const char* name) {
    Elf64_Shdr* sections =
        reinterpret_cast<Elf64_Shdr*>(reinterpret_cast<char*>(ehdr) + ehdr->e_shoff);

    Elf64_Shdr* shstrtab = &sections[ehdr->e_shstrndx];
    const char* strtab = reinterpret_cast<char*>(ehdr) + shstrtab->sh_offset;

    for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
        const char* section_name = strtab + sections[i].sh_name;
        if (strcmp(section_name, name) == 0) return &sections[i];
    }

    return nullptr;
}

bool DynamicLinker::locate_got(const Elf64_Ehdr* ehdr, uint64_t& got_base) {
    Elf64_Shdr* got_plt = find_section_by_name(const_cast<Elf64_Ehdr*>(ehdr), ".got.plt");
    if (!got_plt) {
        terminal_writestring("ERROR: Could not find .got.plt section\n");
        return false;
    }

    got_base = got_plt->sh_addr;
    return true;
}

bool DynamicLinker::process_dynamic_section(const Elf64_Ehdr* ehdr, const char* base) {
    Elf64_Shdr* dyn_section = find_section_by_type(const_cast<Elf64_Ehdr*>(ehdr), SHT_DYNAMIC);
    if (!dyn_section) {
        terminal_writestring("ERROR: Could not find dynamic section\n");
        return false;
    }

    Elf64_Dyn* dyn_entries =
        reinterpret_cast<Elf64_Dyn*>(const_cast<char*>(base) + dyn_section->sh_offset);

    Elf64_Shdr* strtab_section = find_section_by_type(const_cast<Elf64_Ehdr*>(ehdr), SHT_STRTAB);
    if (!strtab_section) {
        terminal_writestring("ERROR: Could not find string table section\n");
        return false;
    }

    const char* strtab = base + strtab_section->sh_offset;

    for (size_t i = 0; dyn_entries[i].d_tag != DT_NULL; i++) {
        if (dyn_entries[i].d_tag == DT_NEEDED) {
            const char* lib_name = strtab + dyn_entries[i].d_val;
            terminal_writestring("Loading shared library: ");
            terminal_writestring(lib_name);
            terminal_writestring("\n");

            if (!load_shared_library(lib_name)) {
                terminal_writestring("ERROR: Failed to load shared library\n");
                return false;
            }
        }
    }

    return true;
}

void* DynamicLinker::lookup_symbol(const char* name, const Elf64_Ehdr* lib_ehdr) {
    const char* base = reinterpret_cast<const char*>(lib_ehdr);

    Elf64_Shdr* symtab = find_section_by_type(const_cast<Elf64_Ehdr*>(lib_ehdr), SHT_DYNSYM);
    if (!symtab) return nullptr;

    Elf64_Shdr* strtab = find_section_by_type(const_cast<Elf64_Ehdr*>(lib_ehdr), SHT_STRTAB);
    if (!strtab) return nullptr;

    Elf64_Sym* symbols = reinterpret_cast<Elf64_Sym*>(const_cast<char*>(base) + symtab->sh_offset);
    const char* strings = base + strtab->sh_offset;

    size_t symcount = symtab->sh_size / sizeof(Elf64_Sym);

    for (size_t i = 0; i < symcount; i++) {
        if (symbols[i].st_name != 0) {
            const char* symbol_name = strings + symbols[i].st_name;
            if (strcmp(symbol_name, name) == 0) {
                return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(base) +
                                               symbols[i].st_value);
            }
        }
    }

    return nullptr;
}

bool DynamicLinker::process_relocations(const Elf64_Ehdr* ehdr, const char* base) {
    Elf64_Shdr* rela_plt = find_section_by_name(const_cast<Elf64_Ehdr*>(ehdr), ".rela.plt");
    if (!rela_plt) {
        terminal_writestring("ERROR: Could not find relocation table\n");
        return false;
    }

    Elf64_Shdr* dynsym = find_section_by_type(const_cast<Elf64_Ehdr*>(ehdr), SHT_DYNSYM);
    if (!dynsym) {
        terminal_writestring("ERROR: Could not find dynamic symbol table\n");
        return false;
    }

    Elf64_Shdr* dynstr = find_section_by_type(const_cast<Elf64_Ehdr*>(ehdr), SHT_STRTAB);
    if (!dynstr) {
        terminal_writestring("ERROR: Could not find dynamic string table\n");
        return false;
    }

    Elf64_Sym* symbols = reinterpret_cast<Elf64_Sym*>(const_cast<char*>(base) + dynsym->sh_offset);
    const char* strings = base + dynstr->sh_offset;
    Elf64_Rela* relocations =
        reinterpret_cast<Elf64_Rela*>(const_cast<char*>(base) + rela_plt->sh_offset);

    uint64_t got_base = 0;
    if (!locate_got(ehdr, got_base)) return false;

    size_t relcount = rela_plt->sh_size / sizeof(Elf64_Rela);

    for (size_t i = 0; i < relcount; i++) {
        Elf64_Rela* rel = &relocations[i];

        if (ELF64_R_TYPE(rel->r_info) != R_X86_64_JUMP_SLOT) continue;

        uint64_t sym_idx = ELF64_R_SYM(rel->r_info);
        const char* symbol_name = strings + symbols[sym_idx].st_name;

        void* symbol_addr = resolve_symbol(symbol_name);
        if (symbol_addr)
            *reinterpret_cast<uint64_t*>(rel->r_offset) = reinterpret_cast<uint64_t>(symbol_addr);
        else {
            terminal_writestring("ERROR: Could not resolve symbol: ");
            terminal_writestring(symbol_name);
            terminal_writestring("\n");
            return false;
        }
    }

    return true;
}

bool DynamicLinker::initialize() {
    num_loaded_libraries = 0;

    main_executable.name = nullptr;
    main_executable.header = nullptr;
    main_executable.base = nullptr;
    main_executable.got_base = 0;

    return true;
}

bool DynamicLinker::load_shared_library(const char* path) {
    if (num_loaded_libraries >= MAX_LOADED_LIBRARIES) {
        terminal_writestring("ERROR: Too many loaded libraries\n");
        return false;
    }

    LoadedLibrary& lib = loaded_libraries[num_loaded_libraries];

    size_t name_len = strlen(path) + 1;
    lib.name = static_cast<char*>(HeapAllocator::instance().allocate(name_len));
    if (!lib.name) {
        terminal_writestring("ERROR: Failed to allocate memory for library name\n");
        return false;
    }
    strcpy(lib.name, path);

    num_loaded_libraries++;

    return true;
}

void* DynamicLinker::resolve_symbol(const char* symbol_name) {
    if (main_executable.header) {
        void* addr = lookup_symbol(symbol_name, main_executable.header);
        if (addr) return addr;
    }

    for (size_t i = 0; i < num_loaded_libraries; i++) {
        const LoadedLibrary& lib = loaded_libraries[i];
        if (lib.header) {
            void* addr = lookup_symbol(symbol_name, lib.header);
            if (addr) return addr;
        }
    }

    return nullptr;
}

void* DynamicLinker::runtime_resolver(uint64_t got_base, uint64_t) {
    terminal_writestring("Dynamic symbol resolution called\n");
    return reinterpret_cast<void*>(0);
}

}  // namespace kernel