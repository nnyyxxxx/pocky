#pragma once

#include <cstdint>

struct GDTDescriptor {
    uint16_t size;
    uint64_t offset;
} __attribute__((packed));

struct GDTEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t limit_high_flags;
    uint8_t base_high;
} __attribute__((packed));

struct GDT {
    GDTEntry null;
    GDTEntry kernel_code;
    GDTEntry kernel_data;
    GDTEntry user_code;
    GDTEntry user_data;
} __attribute__((packed));

void init_gdt();