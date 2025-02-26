#include "virtual_memory.hpp"

#include "physical_memory.hpp"

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
    vga[160] = 0x0F00 | 'M';

    auto& pmm = PhysicalMemoryManager::instance();
    m_pml4 = reinterpret_cast<PageTableEntry*>(pmm.allocate_frame());
    vga[161] = 0x0F00 | '1';

    for (size_t i = 0; i < ENTRIES_PER_TABLE; i++) {
        m_pml4[i].value = 0;
    }
    vga[162] = 0x0F00 | '2';

    auto pdpt = create_page_table();
    m_pml4[0].set_address(reinterpret_cast<uint64_t>(pdpt));
    m_pml4[0].set_present(true);
    m_pml4[0].set_writable(true);
    vga[163] = 0x0F00 | '3';

    auto pd = create_page_table();
    pdpt[0].set_address(reinterpret_cast<uint64_t>(pd));
    pdpt[0].set_present(true);
    pdpt[0].set_writable(true);
    vga[164] = 0x0F00 | '4';

    auto pt = create_page_table();
    pd[0].set_address(reinterpret_cast<uint64_t>(pt));
    pd[0].set_present(true);
    pd[0].set_writable(true);
    vga[165] = 0x0F00 | '5';

    for (uintptr_t addr = 0; addr < 0x400000; addr += PAGE_SIZE) {
        map_page(addr, addr, true);
        if ((addr & 0xFFFFF) == 0) vga[180] = 0x0F00 | ('0' + (addr >> 20));
    }
    vga[166] = 0x0F00 | '6';
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

void VirtualMemoryManager::load_page_directory() {
    volatile uint16_t* vga = reinterpret_cast<uint16_t*>(0xB8000);
    vga[167] = 0x0F00 | 'P';

    uint64_t efer;
    asm volatile("rdmsr" : "=a"(efer) : "c"(0xC0000080));
    if (efer & (1 << 8)) {
        vga[190] = 0x0400 | 'L';
        return;
    }

    uint64_t cr4;
    asm volatile("movq %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 5);
    cr4 |= (1 << 7);
    cr4 |= (1 << 9);
    cr4 |= (1 << 10);
    asm volatile("movq %0, %%cr4" : : "r"(cr4) : "memory");
    vga[168] = 0x0F00 | '1';

    asm volatile("movq %0, %%cr3" : : "r"(m_pml4) : "memory");
    vga[169] = 0x0F00 | '2';

    asm volatile("rdmsr" : "=a"(efer) : "c"(0xC0000080));
    efer |= (1 << 8);
    efer |= (1 << 11);
    asm volatile("wrmsr" : : "a"(efer), "d"(0), "c"(0xC0000080));
    vga[170] = 0x0F00 | '3';

    uint64_t cr0;
    asm volatile("movq %%cr0, %0" : "=r"(cr0));
    cr0 |= (1 << 16);
    cr0 |= (1 << 31);
    asm volatile("movq %0, %%cr0" : : "r"(cr0) : "memory");

    asm volatile("movq %%cr3, %%rax\n"
                 "movq %%rax, %%cr3\n"
                 "jmp 1f\n"
                 "1:\n"
                 "nop"
                 :
                 :
                 : "memory", "rax");
    vga[171] = 0x0F00 | '4';
}