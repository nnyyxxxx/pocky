#include "virtual_memory.hpp"

#include <cstdio>
#include <cstring>

#include "physical_memory.hpp"
#include "printf.hpp"

namespace {
constexpr size_t PAGE_SIZE = 4096;
constexpr size_t ENTRIES_PER_TABLE = 512;

constexpr size_t PML4_SHIFT = 39;
constexpr size_t PDPT_SHIFT = 30;
constexpr size_t PD_SHIFT = 21;
constexpr size_t PT_SHIFT = 12;

constexpr size_t PAGE_MASK = 0xFFFFFFFFFFFFF000;
}  // namespace

VirtualMemoryManager& VirtualMemoryManager::instance() {
    static VirtualMemoryManager instance;
    return instance;
}

void VirtualMemoryManager::initialize() {
    volatile uint16_t* vga = reinterpret_cast<uint16_t*>(0xB8000);
    auto write_debug = [&](const char* msg, int line, int offset = 0) {
        for (int i = 0; msg[i] != '\0'; i++) {
            vga[i + offset + (line * 80)] = 0x0200 | msg[i];
        }
    };

    auto write_hex = [&](uint64_t value, int line, int offset) {
        char hex[17];
        for (int i = 15; i >= 0; i--) {
            int digit = (value >> (i * 4)) & 0xF;
            hex[15 - i] = digit < 10 ? '0' + digit : 'A' + digit - 10;
        }
        hex[16] = '\0';
        for (int i = 0; i < 16; i++) {
            vga[i + offset + (line * 80)] = 0x0200 | hex[i];
        }
    };

    write_debug("VMM: Starting init", 15);

    auto& pmm = PhysicalMemoryManager::instance();
    m_pml4 = reinterpret_cast<PageTableEntry*>(pmm.allocate_frame());
    if (!m_pml4) {
        write_debug("VMM: Failed to allocate PML4!", 16);
        return;
    }
    write_debug("VMM: PML4 at: ", 16);
    write_hex(reinterpret_cast<uint64_t>(m_pml4), 16, 12);

    memset(m_pml4, 0, PAGE_SIZE);
    write_debug("VMM: PML4 cleared", 17);

    auto pdpt = create_page_table();
    if (!pdpt) {
        write_debug("VMM: Failed to allocate PDPT!", 18);
        return;
    }
    write_debug("VMM: PDPT at: ", 18);
    write_hex(reinterpret_cast<uint64_t>(pdpt), 18, 12);

    m_pml4[0].value = 0;
    m_pml4[0].set_address(reinterpret_cast<uint64_t>(pdpt));
    m_pml4[0].set_present(true);
    m_pml4[0].set_writable(true);
    m_pml4[0].set_user(false);
    write_debug("VMM: PML4[0] value: ", 19);
    write_hex(m_pml4[0].value, 19, 16);

    auto pd = create_page_table();
    if (!pd) {
        write_debug("VMM: Failed to allocate PD!", 20);
        return;
    }
    write_debug("VMM: PD at: ", 20);
    write_hex(reinterpret_cast<uint64_t>(pd), 20, 12);

    pdpt[0].value = 0;
    pdpt[0].set_present(true);
    pdpt[0].set_writable(true);
    pdpt[0].set_user(false);
    pdpt[0].set_pwt(false);
    pdpt[0].set_pcd(false);
    pdpt[0].set_accessed(false);
    pdpt[0].set_address(reinterpret_cast<uint64_t>(pd));

    write_debug("VMM: PDPT[0] value: ", 21);
    write_hex(pdpt[0].value, 21, 16);
    write_debug(" Flags: ", 21, 35);
    write_debug(pdpt[0].present() ? "P " : "- ", 21, 43);
    write_debug(pdpt[0].writable() ? "W " : "- ", 21, 45);

    for (size_t i = 0; i < 8; i++) {
        pd[i].value = 0;
        uint64_t phys_addr = i * 0x200000;

        pd[i].set_huge_page(true);

        pd[i].set_address(phys_addr);

        pd[i].set_present(true);
        pd[i].set_writable(true);
        pd[i].set_user(false);

        write_debug("VMM: PD[", 22 + i);
        write_hex(i, 22 + i, 8);
        write_debug("] PA: ", 22 + i, 10);
        write_hex(pd[i].address(), 22 + i, 15);
        write_debug(" Flags: ", 22 + i, 32);
        write_debug(pd[i].present() ? "P" : "-", 22 + i, 40);
        write_debug(pd[i].writable() ? "W" : "-", 22 + i, 41);
        write_debug(pd[i].huge_page() ? "H" : "-", 22 + i, 42);
    }

    write_debug("VMM: Verifying page mappings", 31);
    for (size_t i = 0; i < 8; i++) {
        if (!pd[i].present()) {
            write_debug("VMM: ERROR - PD entry not present: ", 32 + i);
            write_hex(i, 32 + i, 30);
            continue;
        }
        if (!pd[i].huge_page()) {
            write_debug("VMM: ERROR - PD entry not huge page: ", 32 + i);
            write_hex(i, 32 + i, 35);
            continue;
        }
        write_debug("VMM: PD[", 32 + i);
        write_hex(i, 32 + i, 8);
        write_debug("] OK - PA: ", 32 + i, 10);
        write_hex(pd[i].address(), 32 + i, 20);
    }

    write_debug("VMM: First 16MB mapped with 2MB pages", 30);

    auto kernel_pdpt = create_page_table();
    if (!kernel_pdpt) {
        write_debug("VMM: Failed to allocate kernel PDPT!", 31);
        return;
    }

    m_pml4[511].set_address(reinterpret_cast<uint64_t>(kernel_pdpt));
    m_pml4[511].set_present(true);
    m_pml4[511].set_writable(true);
    m_pml4[511].set_user(false);
    write_debug("VMM: Kernel PDPT created", 32);

    map_page(0xB8000, 0xB8000, true);
    write_debug("VMM: VGA buffer mapped", 33);

    write_debug("VMM: Initialization complete", 34);
}

PageTableEntry* VirtualMemoryManager::create_page_table() {
    auto& pmm = PhysicalMemoryManager::instance();
    auto table = reinterpret_cast<PageTableEntry*>(pmm.allocate_frame());

    for (size_t i = 0; i < ENTRIES_PER_TABLE; i++) {
        table[i].value = 0;
    }

    return table;
}

PageTableEntry* VirtualMemoryManager::get_next_level(PageTableEntry* table, size_t index,
                                                     bool create) {
    if (!table[index].present() && create) {
        auto next_table = create_page_table();
        table[index].set_address(reinterpret_cast<uint64_t>(next_table));
        table[index].set_present(true);
        table[index].set_writable(true);
        table[index].set_user(false);
    }

    if (!table[index].present()) return nullptr;

    return reinterpret_cast<PageTableEntry*>(table[index].address());
}

void VirtualMemoryManager::map_page(uintptr_t virtual_addr, uintptr_t physical_addr,
                                    bool writable) {
    virtual_addr &= PAGE_MASK;
    physical_addr &= PAGE_MASK;

    size_t pml4_index = (virtual_addr >> PML4_SHIFT) & 0x1FF;
    size_t pdpt_index = (virtual_addr >> PDPT_SHIFT) & 0x1FF;
    size_t pd_index = (virtual_addr >> PD_SHIFT) & 0x1FF;
    size_t pt_index = (virtual_addr >> PT_SHIFT) & 0x1FF;

    auto pdpt = get_next_level(m_pml4, pml4_index, true);
    if (!pdpt) return;

    auto pd = get_next_level(pdpt, pdpt_index, true);
    if (!pd) return;

    auto pt = get_next_level(pd, pd_index, true);
    if (!pt) return;

    if (!pt[pt_index].present()) {
        pt[pt_index].set_address(physical_addr);
        pt[pt_index].set_present(true);
        pt[pt_index].set_writable(writable);
        pt[pt_index].set_user(false);
    }
}

void VirtualMemoryManager::unmap_page(uintptr_t virtual_addr) {
    virtual_addr &= PAGE_MASK;

    size_t pml4_index = (virtual_addr >> PML4_SHIFT) & 0x1FF;
    size_t pdpt_index = (virtual_addr >> PDPT_SHIFT) & 0x1FF;
    size_t pd_index = (virtual_addr >> PD_SHIFT) & 0x1FF;
    size_t pt_index = (virtual_addr >> PT_SHIFT) & 0x1FF;

    auto pdpt = get_next_level(m_pml4, pml4_index, false);
    if (!pdpt) return;

    auto pd = get_next_level(pdpt, pdpt_index, false);
    if (!pd) return;

    auto pt = get_next_level(pd, pd_index, false);
    if (!pt) return;

    pt[pt_index].value = 0;
}

uintptr_t VirtualMemoryManager::get_physical_address(uintptr_t virtual_addr) {
    virtual_addr &= PAGE_MASK;

    size_t pml4_index = (virtual_addr >> PML4_SHIFT) & 0x1FF;
    size_t pdpt_index = (virtual_addr >> PDPT_SHIFT) & 0x1FF;
    size_t pd_index = (virtual_addr >> PD_SHIFT) & 0x1FF;
    size_t pt_index = (virtual_addr >> PT_SHIFT) & 0x1FF;

    auto pdpt = get_next_level(m_pml4, pml4_index, false);
    if (!pdpt) return 0;

    auto pd = get_next_level(pdpt, pdpt_index, false);
    if (!pd) return 0;

    auto pt = get_next_level(pd, pd_index, false);
    if (!pt) return 0;

    if (!pt[pt_index].present()) return 0;

    return (pt[pt_index].address() & PAGE_MASK) | (virtual_addr & ~PAGE_MASK);
}

extern "C" {
uint64_t read_cr4();
void write_cr4(uint64_t value);
void write_cr3(uint64_t value);
uint64_t read_cr0();
void write_cr0(uint64_t value);
uint64_t read_efer();
void write_efer(uint64_t value);
void flush_tlb();
}

void VirtualMemoryManager::load_page_directory() {
    volatile uint16_t* vga = reinterpret_cast<uint16_t*>(0xB8000);
    auto write_debug = [&](const char* msg, int line, int offset = 0) {
        for (int i = 0; msg[i] != '\0'; i++) {
            vga[i + offset + (line * 80)] = 0x0400 | msg[i];
        }
    };

    write_debug("VMM: Loading page directory", 23);

    uint64_t cr4 = read_cr4();
    cr4 |= (1ULL << 5);
    cr4 |= (1ULL << 4);
    write_cr4(cr4);
    write_debug("VMM: PAE and PSE enabled in CR4", 24);

    uint64_t cr0 = read_cr0();
    cr0 |= (1ULL << 16);
    write_cr0(cr0);
    write_debug("VMM: Write protection enabled", 25);

    write_debug("VMM: CR3 value: ", 26);

    char hex[19] = "0x0000000000000000";
    uint64_t cr3_val = reinterpret_cast<uint64_t>(m_pml4);
    for (int i = 15; i >= 2; i--) {
        int digit = (cr3_val >> ((15 - i) * 4)) & 0xF;
        hex[i] = digit < 10 ? '0' + digit : 'A' + (digit - 10);
    }
    for (int i = 0; i < 18; i++) {
        vga[i + 16 + (26 * 80)] = 0x0400 | hex[i];
    }

    write_cr3(reinterpret_cast<uint64_t>(m_pml4));
    write_debug("VMM: CR3 updated", 27);

    flush_tlb();
    write_debug("VMM: TLB flushed", 28);
}