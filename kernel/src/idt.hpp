#pragma once

#include <cstdint>

struct IDTEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct IDTDescriptor {
    uint16_t size;
    uint64_t offset;
} __attribute__((packed));

constexpr uint8_t IDT_PRESENT = 0x80;
constexpr uint8_t IDT_DPL0 = 0x00;
constexpr uint8_t IDT_DPL3 = 0x60;
constexpr uint8_t IDT_INTERRUPT_GATE = 0x0E;
constexpr uint8_t IDT_TRAP_GATE = 0x0F;

void init_idt();
void set_interrupt_handler(uint8_t vector, void (*handler)(), uint8_t flags);