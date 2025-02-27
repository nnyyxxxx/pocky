#include "elf.hpp"
#include "gdt.hpp"
#include "heap.hpp"
#include "idt.hpp"
#include "io.hpp"
#include "keyboard.hpp"
#include "physical_memory.hpp"
#include "pic.hpp"
#include "shell.hpp"
#include "terminal.hpp"
#include "vga.hpp"
#include "virtual_memory.hpp"

extern "C" void kernel_main() {
    volatile uint16_t* vga = reinterpret_cast<uint16_t*>(0xB8000);
    const char* msg = "Kernel Start";
    for (int i = 0; msg[i] != '\0'; i++) {
        vga[i] = 0x0F00 | msg[i];
    }

    vga[80] = 0x0E00 | '>';

    init_gdt();
    vga[81] = 0x0200 | 'G';
    vga[82] = 0x0200 | 'D';
    vga[83] = 0x0200 | 'T';

    init_pic();
    vga[84] = 0x0100 | 'P';

    init_idt();
    vga[85] = 0x0100 | 'I';

    init_keyboard();
    vga[86] = 0x0100 | 'K';

    constexpr size_t MEMORY_START = 0x100000;
    constexpr size_t MEMORY_SIZE = 64 * 1024 * 1024;
    constexpr size_t HEAP_SIZE = 16 * 1024 * 1024;
    constexpr size_t IDENTITY_MAP_SIZE = 32 * 1024 * 1024;

    vga[87] = 0x0500 | '1';

    auto& pmm = PhysicalMemoryManager::instance();
    pmm.initialize(MEMORY_START, MEMORY_SIZE);
    vga[88] = 0x0500 | '2';

    auto& vmm = VirtualMemoryManager::instance();
    vmm.initialize();
    vga[89] = 0x0500 | '3';

    for (uintptr_t addr = 0; addr < IDENTITY_MAP_SIZE; addr += 4096) {
        vmm.map_page(addr, addr, true);
        if ((addr & 0xFFFFF) == 0) vga[90] = static_cast<uint8_t>('0' + ((addr >> 20) & 0xF));
    }
    vga[91] = 0x0500 | '4';

    vmm.map_page(0xB8000, 0xB8000, true);
    vga[92] = 0x0500 | '5';

    uintptr_t heap_start = MEMORY_START + MEMORY_SIZE - HEAP_SIZE;
    for (uintptr_t addr = heap_start; addr < heap_start + HEAP_SIZE; addr += 4096) {
        vmm.map_page(addr, addr, true);
    }
    vga[93] = 0x0500 | '6';

    vmm.load_page_directory();
    vga[94] = 0x0500 | '7';

    auto& heap = HeapAllocator::instance();
    heap.initialize(heap_start, HEAP_SIZE);
    vga[95] = 0x0500 | '8';

    if (kernel::DynamicLinker::initialize())
        vga[96] = 0x0300 | 'D';
    else
        vga[96] = 0x0C00 | 'D';

    terminal_initialize();
    vga[97] = 0x0500 | '9';

    init_shell();
    vga[98] = 0x0500 | 'A';

    terminal_writestring("Welcome to the Kernel!\n");
    terminal_writestring("Type 'help' to see the list of available commands.\n\n");
    terminal_writestring("$ ");

    asm volatile("sti");

    for (;;) {
        asm volatile("hlt");
    }
}