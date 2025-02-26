#include "gdt.hpp"
#include "idt.hpp"
#include "io.hpp"
#include "keyboard.hpp"
#include "pic.hpp"
#include "shell.hpp"
#include "terminal.hpp"
#include "vga.hpp"

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

    terminal_initialize();
    vga[87] = 0x0400 | 'T';

    init_shell();
    vga[88] = 0x0400 | 'S';

    terminal_writestring("Welcome to the Kernel!\n");
    terminal_writestring("Type 'help' to see the list of available commands.\n\n");
    terminal_writestring("$ ");

    for (;;)
        asm volatile("hlt");
}