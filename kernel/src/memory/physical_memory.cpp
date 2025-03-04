#include "physical_memory.hpp"

#include <cstring>

#include "printf.hpp"

static uint32_t initial_bitmap[16384];

PhysicalMemoryManager& PhysicalMemoryManager::instance() {
    static PhysicalMemoryManager instance;
    return instance;
}

void PhysicalMemoryManager::initialize(uintptr_t memory_start, size_t memory_size) {
    m_memory_start = memory_start;
    m_total_frames = memory_size / PAGE_SIZE;
    m_free_frames = m_total_frames;

    m_bitmap = initial_bitmap;
    size_t bitmap_size = (m_total_frames + 31) / 32 * sizeof(uint32_t);

    memset(m_bitmap, 0, bitmap_size);

    size_t first_mb_frames = 0x100000 / PAGE_SIZE;
    for (size_t i = 0; i < first_mb_frames; i++) {
        size_t idx = i / 32;
        size_t bit = i % 32;
        m_bitmap[idx] |= (1u << bit);
        m_free_frames--;
    }

    size_t heap_start_frame = (memory_size - (16 * 1024 * 1024)) / PAGE_SIZE;
    size_t heap_frames = (16 * 1024 * 1024) / PAGE_SIZE;
    for (size_t i = heap_start_frame; i < heap_start_frame + heap_frames; i++) {
        size_t idx = i / 32;
        size_t bit = i % 32;
        m_bitmap[idx] |= (1u << bit);
        m_free_frames--;
    }

    size_t page_table_frames = 5;
    for (size_t i = first_mb_frames; i < first_mb_frames + page_table_frames; i++) {
        size_t idx = i / 32;
        size_t bit = i % 32;
        m_bitmap[idx] |= (1u << bit);
        m_free_frames--;
    }

    volatile uint16_t* vga = reinterpret_cast<uint16_t*>(0xB8000);
    const char* msg = "[5] PMM Init Done";
    for (int i = 0; msg[i] != '\0'; i++) {
        vga[i + 320] = 0x0F00 | msg[i];
    }
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

    volatile uint16_t* vga = reinterpret_cast<uint16_t*>(0xB8000);
    const char* msg = "ERROR: No free frames available";
    for (int i = 0; msg[i] != '\0'; i++) {
        vga[i + (27 * 80)] = 0x0C00 | msg[i];
    }
    return nullptr;
}

void PhysicalMemoryManager::free_frame(void* frame) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(frame);
    if (addr < m_memory_start) return;

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