#pragma once

#include <cstddef>
#include <cstdint>

class PhysicalMemoryManager {
public:
    static constexpr size_t PAGE_SIZE = 4096;

    static PhysicalMemoryManager& instance();

    void initialize(uintptr_t memory_start, size_t memory_size);
    void* allocate_frame();
    void free_frame(void* frame);
    size_t get_free_frames() const;
    size_t get_total_frames() const {
        return m_total_frames;
    }

private:
    PhysicalMemoryManager() = default;
    ~PhysicalMemoryManager() = default;

    PhysicalMemoryManager(const PhysicalMemoryManager&) = delete;
    PhysicalMemoryManager& operator=(const PhysicalMemoryManager&) = delete;

    uint32_t* m_bitmap = nullptr;
    size_t m_total_frames = 0;
    size_t m_free_frames = 0;
    uintptr_t m_memory_start = 0;
};