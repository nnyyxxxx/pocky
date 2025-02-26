#pragma once

#include <cstddef>
#include <cstdint>

struct PageTableEntry {
    uint64_t value = 0;

    bool present() const { return value & 1; }
    void set_present(bool p) { value = p ? value | 1 : value & ~1; }

    uint64_t address() const { return value & ~0xFFFull; }
    void set_address(uint64_t addr) { value = (value & 0xFFF) | (addr & ~0xFFFull); }

    bool writable() const { return value & (1 << 1); }
    void set_writable(bool w) { value = w ? value | (1 << 1) : value & ~(1 << 1); }

    bool user() const { return value & (1 << 2); }
    void set_user(bool u) { value = u ? value | (1 << 2) : value & ~(1 << 2); }
};

class VirtualMemoryManager {
public:
    static VirtualMemoryManager& instance();

    void initialize();
    void map_page(uintptr_t virtual_addr, uintptr_t physical_addr, bool writable = true);
    void unmap_page(uintptr_t virtual_addr);
    uintptr_t get_physical_address(uintptr_t virtual_addr);

    void load_page_directory();

private:
    VirtualMemoryManager() = default;
    ~VirtualMemoryManager() = default;

    VirtualMemoryManager(const VirtualMemoryManager&) = delete;
    VirtualMemoryManager& operator=(const VirtualMemoryManager&) = delete;

    PageTableEntry* create_page_table();
    PageTableEntry* get_next_level(PageTableEntry* table, size_t index, bool create);

    PageTableEntry* m_pml4 = nullptr;
};