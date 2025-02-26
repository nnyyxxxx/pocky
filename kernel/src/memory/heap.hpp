#pragma once

#include <cstddef>
#include <cstdint>

struct HeapBlock {
    size_t size = 0;
    bool free = true;
    HeapBlock* next = nullptr;
};

class HeapAllocator {
public:
    static HeapAllocator& instance();

    void initialize(uintptr_t heap_start, size_t heap_size);
    void* allocate(size_t size);
    void free(void* ptr);

    size_t get_free_memory() const;
    size_t get_used_memory() const;

private:
    HeapAllocator() = default;
    ~HeapAllocator() = default;

    HeapAllocator(const HeapAllocator&) = delete;
    HeapAllocator& operator=(const HeapAllocator&) = delete;

    HeapBlock* find_free_block(size_t size);
    void split_block(HeapBlock* block, size_t size);
    void merge_free_blocks();

    HeapBlock* m_first_block = nullptr;
    size_t m_total_size = 0;
    size_t m_used_memory = 0;
};