#include "editor.hpp"
#include "elf.hpp"
#include "fs/filesystem.hpp"
#include "gdt.hpp"
#include "graphics.hpp"
#include "heap.hpp"
#include "idt.hpp"
#include "io.hpp"
#include "keyboard.hpp"
#include "mouse.hpp"
#include "multiboot2.hpp"
#include "physical_memory.hpp"
#include "pic.hpp"
#include "printf.hpp"
#include "shell.hpp"
#include "terminal.hpp"
#include "timer.hpp"
#include "vga.hpp"
#include "virtual_memory.hpp"

extern "C" void kernel_main() {
    volatile uint16_t* vga = reinterpret_cast<uint16_t*>(0xB8000);

    const char* msg1 = "[1] Kernel Start";
    for (int i = 0; msg1[i] != '\0'; i++) {
        vga[i] = 0x0F00 | msg1[i];
    }

    init_gdt();

    const char* msg2 = "[2] GDT Init Done";
    for (int i = 0; msg2[i] != '\0'; i++) {
        vga[i + 80] = 0x0F00 | msg2[i];
    }

    init_pic();

    const char* msg3 = "[3] PIC Init Done";
    for (int i = 0; msg3[i] != '\0'; i++) {
        vga[i + 160] = 0x0F00 | msg3[i];
    }

    init_idt();

    const char* msg4 = "[4] IDT Init Done";
    for (int i = 0; msg4[i] != '\0'; i++) {
        vga[i + 240] = 0x0F00 | msg4[i];
    }

    constexpr size_t MEMORY_START = 0x100000;
    constexpr size_t MEMORY_SIZE = 64 * 1024 * 1024;
    constexpr size_t HEAP_SIZE = 16 * 1024 * 1024;

    auto& pmm = PhysicalMemoryManager::instance();

    const auto* mb_memory_map = kernel::get_memory_map();
    if (mb_memory_map)
        pmm.initialize(MEMORY_START, MEMORY_SIZE);
    else
        pmm.initialize(MEMORY_START, MEMORY_SIZE);

    const char* msg5 = "[5] PMM Init Done";
    for (int i = 0; msg5[i] != '\0'; i++) {
        vga[i + 320] = 0x0F00 | msg5[i];
    }

    auto& vmm = VirtualMemoryManager::instance();
    vmm.initialize();

    const char* msg6 = "[6] VMM Init Done";
    for (int i = 0; msg6[i] != '\0'; i++) {
        vga[i + 400] = 0x0F00 | msg6[i];
    }

    uintptr_t heap_start = MEMORY_START + MEMORY_SIZE - HEAP_SIZE;
    for (uintptr_t addr = heap_start; addr < heap_start + HEAP_SIZE; addr += 4096) {
        vmm.map_page(addr, addr, true);
    }

    const char* msg7 = "[7] Heap Mapped";
    for (int i = 0; msg7[i] != '\0'; i++) {
        vga[i + 480] = 0x0F00 | msg7[i];
    }

    vmm.load_page_directory();

    const char* msg8 = "[8] Page Tables Loaded";
    for (int i = 0; msg8[i] != '\0'; i++) {
        vga[i + 560] = 0x0F00 | msg8[i];
    }

    init_keyboard();

    const char* msg9 = "[9] Keyboard Init Done";
    for (int i = 0; msg9[i] != '\0'; i++) {
        vga[i + 640] = 0x0F00 | msg9[i];
    }

    register_keyboard_handler(process_keypress);

    init_mouse();

    const char* msg10 = "[10] Mouse Init Done";
    for (int i = 0; msg10[i] != '\0'; i++) {
        vga[i + 720] = 0x0F00 | msg10[i];
    }

    init_timer(100);

    const char* msg11 = "[11] Timer Init Done";
    for (int i = 0; msg11[i] != '\0'; i++) {
        vga[i + 800] = 0x0F00 | msg11[i];
    }

    auto& heap = HeapAllocator::instance();
    heap.initialize(heap_start, HEAP_SIZE);

    auto& fs = fs::FileSystem::instance();
    fs.initialize();

    const char* msg12 = "[12] Filesystem Init Done";
    for (int i = 0; msg12[i] != '\0'; i++) {
        vga[i + 880] = 0x0F00 | msg12[i];
    }

    kernel::DynamicLinker::initialize();

    terminal_initialize();
    printf("Welcome to the Kernel!\n");
    printf("Type 'help' to see the list of available commands.\n");

    graphics_initialize();

    init_shell();

    asm volatile("sti");
    for (;;) {
        asm volatile("hlt");
    }
}