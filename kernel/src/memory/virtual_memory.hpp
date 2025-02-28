#pragma once

#include <cstddef>
#include <cstdint>

struct PageTableEntry {
    uint64_t value = 0;

    bool present() const { return value & 1ULL; }
    bool writable() const { return value & (1ULL << 1); }
    bool user() const { return value & (1ULL << 2); }
    bool pwt() const { return value & (1ULL << 3); }
    bool pcd() const { return value & (1ULL << 4); }
    bool accessed() const { return value & (1ULL << 5); }
    bool dirty() const { return value & (1ULL << 6); }
    bool huge_page() const { return value & (1ULL << 7); }
    bool global() const { return value & (1ULL << 8); }
    uint64_t address() const {
        if (huge_page())
            return value & 0xFFFFFFFE00000ULL;
        return value & 0xFFFFFFFFF000ULL;
    }

    void set_present(bool x) { value = (value & ~1ULL) | (x ? 1ULL : 0); }
    void set_writable(bool x) { value = (value & ~(1ULL << 1)) | (x ? (1ULL << 1) : 0); }
    void set_user(bool x) { value = (value & ~(1ULL << 2)) | (x ? (1ULL << 2) : 0); }
    void set_pwt(bool x) { value = (value & ~(1ULL << 3)) | (x ? (1ULL << 3) : 0); }
    void set_pcd(bool x) { value = (value & ~(1ULL << 4)) | (x ? (1ULL << 4) : 0); }
    void set_accessed(bool x) { value = (value & ~(1ULL << 5)) | (x ? (1ULL << 5) : 0); }
    void set_dirty(bool x) { value = (value & ~(1ULL << 6)) | (x ? (1ULL << 6) : 0); }
    void set_huge_page(bool x) { value = (value & ~(1ULL << 7)) | (x ? (1ULL << 7) : 0); }
    void set_global(bool x) { value = (value & ~(1ULL << 8)) | (x ? (1ULL << 8) : 0); }
    void set_address(uint64_t addr) {
        if (huge_page()) {
            uint64_t flags = value & 0x1FFFFF;
            value = flags | (addr & ~0x1FFFFF);
        } else {
            uint64_t flags = value & 0xFFF;
            value = flags | (addr & ~0xFFF);
        }
    }
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