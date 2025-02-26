#include "heap.hpp"

#include <cstring>

HeapAllocator& HeapAllocator::instance() {
    static HeapAllocator instance;
    return instance;
}

void HeapAllocator::initialize(uintptr_t heap_start, size_t heap_size) {
    m_first_block = reinterpret_cast<HeapBlock*>(heap_start);
    m_first_block->size = heap_size - sizeof(HeapBlock);
    m_first_block->free = true;
    m_first_block->next = nullptr;

    m_total_size = heap_size;
    m_used_memory = sizeof(HeapBlock);
}

HeapBlock* HeapAllocator::find_free_block(size_t size) {
    HeapBlock* current = m_first_block;
    while (current) {
        if (current->free && current->size >= size) return current;
        current = current->next;
    }
    return nullptr;
}

void HeapAllocator::split_block(HeapBlock* block, size_t size) {
    if (block->size >= size + sizeof(HeapBlock) + 32) {
        HeapBlock* new_block = reinterpret_cast<HeapBlock*>(reinterpret_cast<uint8_t*>(block) +
                                                            sizeof(HeapBlock) + size);

        new_block->size = block->size - size - sizeof(HeapBlock);
        new_block->free = true;
        new_block->next = block->next;

        block->size = size;
        block->next = new_block;

        m_used_memory += sizeof(HeapBlock);
    }
}

void* HeapAllocator::allocate(size_t size) {
    if (size == 0) return nullptr;

    size = (size + 15) & ~15;
    HeapBlock* block = find_free_block(size);

    if (!block) return nullptr;

    split_block(block, size);
    block->free = false;
    m_used_memory += size;

    return reinterpret_cast<uint8_t*>(block) + sizeof(HeapBlock);
}

void HeapAllocator::merge_free_blocks() {
    HeapBlock* current = m_first_block;

    while (current && current->next) {
        if (current->free && current->next->free) {
            current->size += sizeof(HeapBlock) + current->next->size;
            current->next = current->next->next;
            m_used_memory -= sizeof(HeapBlock);
        } else {
            current = current->next;
        }
    }
}

void HeapAllocator::free(void* ptr) {
    if (!ptr) return;

    HeapBlock* block =
        reinterpret_cast<HeapBlock*>(reinterpret_cast<uint8_t*>(ptr) - sizeof(HeapBlock));

    block->free = true;
    m_used_memory -= block->size;

    merge_free_blocks();
}

size_t HeapAllocator::get_free_memory() const {
    return m_total_size - m_used_memory;
}

size_t HeapAllocator::get_used_memory() const {
    return m_used_memory;
}