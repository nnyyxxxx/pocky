#include "gdt.hpp"

extern "C" {
uint32_t read_cr4();
void write_cr4(uint32_t value);
uint32_t read_cr0();
void write_cr0(uint32_t value);
}

namespace {
GDT gdt = {
    {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0},
};

GDTDescriptor gdtr = {sizeof(GDT) - 1, 0};

constexpr uint8_t GDT_PRESENT = 0x80;
constexpr uint8_t GDT_USER = 0x60;
constexpr uint8_t GDT_DESCRIPTOR = 0x10;
constexpr uint8_t GDT_EXECUTABLE = 0x08;
constexpr uint8_t GDT_READWRITE = 0x02;
constexpr uint8_t GDT_ACCESSED = 0x01;

constexpr uint8_t GDT_FLAGS_64BIT = 0x20;
constexpr uint8_t GDT_FLAGS_32BIT = 0x40;
constexpr uint8_t GDT_FLAGS_4K_GRAN = 0x80;

void create_descriptor(GDTEntry& entry, uint32_t base, uint32_t limit, uint8_t access,
                       uint8_t flags) {
    entry.base_low = base & 0xFFFF;
    entry.base_middle = (base >> 16) & 0xFF;
    entry.base_high = (base >> 24) & 0xFF;

    entry.limit_low = limit & 0xFFFF;
    entry.limit_high_flags = ((limit >> 16) & 0x0F) | (flags & 0xF0);

    entry.access = access;
}

extern "C" void load_gdt(GDTDescriptor* gdtr);
}  // namespace

void init_gdt() {
    volatile uint16_t* vga = reinterpret_cast<uint16_t*>(0xB8000);
    vga[40] = 0x0400 | '1';

    create_descriptor(gdt.null, 0, 0, 0, 0);
    vga[41] = 0x0400 | '2';

    create_descriptor(gdt.kernel_code, 0, 0,
                      GDT_PRESENT | GDT_DESCRIPTOR | GDT_EXECUTABLE | GDT_READWRITE,
                      GDT_FLAGS_64BIT | GDT_FLAGS_4K_GRAN);
    vga[42] = 0x0400 | '3';

    create_descriptor(gdt.kernel_data, 0, 0,
                      GDT_PRESENT | GDT_DESCRIPTOR | GDT_READWRITE, GDT_FLAGS_4K_GRAN);
    vga[43] = 0x0400 | '4';

    create_descriptor(gdt.user_code, 0, 0,
                      GDT_PRESENT | GDT_DESCRIPTOR | GDT_USER | GDT_EXECUTABLE | GDT_READWRITE,
                      GDT_FLAGS_64BIT | GDT_FLAGS_4K_GRAN);
    vga[44] = 0x0400 | '5';

    create_descriptor(gdt.user_data, 0, 0,
                      GDT_PRESENT | GDT_DESCRIPTOR | GDT_USER | GDT_READWRITE, GDT_FLAGS_4K_GRAN);
    vga[45] = 0x0400 | '6';

    gdtr.offset = reinterpret_cast<uint64_t>(&gdt);
    vga[46] = 0x0400 | '7';

    vga[47] = 0x0400 | '8';
    load_gdt(&gdtr);
    vga[48] = 0x0400 | '9';

    uint32_t cr0 = read_cr0();
    cr0 &= ~(1 << 2);
    cr0 |= (1 << 1);
    write_cr0(cr0);

    uint32_t cr4 = read_cr4();
    cr4 |= (1 << 9);
    cr4 |= (1 << 10);
    write_cr4(cr4);

    vga[49] = 0x0400 | 'A';
}