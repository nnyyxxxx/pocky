#include "physical_memory.hpp"

#include <cstring>

PhysicalMemoryManager& PhysicalMemoryManager::instance() {
    static PhysicalMemoryManager instance;
    return instance;
}

void PhysicalMemoryManager::initialize(uintptr_t memory_start, size_t memory_size) {
    m_memory_start = memory_start;
    m_total_frames = memory_size / PAGE_SIZE;
    m_free_frames = m_total_frames;

    size_t bitmap_size = (m_total_frames + 31) / 32;
    m_bitmap = reinterpret_cast<uint32_t*>(memory_start);
    memset(m_bitmap, 0, bitmap_size * sizeof(uint32_t));
}

void* PhysicalMemoryManager::allocate_frame() {
    if (m_free_frames == 0) return nullptr;

    for (size_t i = 0; i < m_total_frames; i++) {
        size_t idx = i / 32;
        size_t bit = i % 32;

        if ((m_bitmap[idx] & (1u << bit)) == 0) {
            m_bitmap[idx] |= (1u << bit);
            m_free_frames--;
            return reinterpret_cast<void*>(m_memory_start + i * PAGE_SIZE);
        }
    }

    return nullptr;
}

void PhysicalMemoryManager::free_frame(void* frame) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(frame);
    size_t frame_index = (addr - m_memory_start) / PAGE_SIZE;

    if (frame_index >= m_total_frames) return;

    size_t idx = frame_index / 32;
    size_t bit = frame_index % 32;

    if (m_bitmap[idx] & (1u << bit)) {
        m_bitmap[idx] &= ~(1u << bit);
        m_free_frames++;
    }
}

size_t PhysicalMemoryManager::get_free_frames() const {
    return m_free_frames;
}