#include "io.hpp"
#include "vga.hpp"
#include "terminal.hpp"
#include "keyboard.hpp"
#include "pic.hpp"
#include "shell.hpp"

extern "C" void kernel_main() {
    *((volatile uint32_t*)0xB8000) = 0x4F724F65;
    *((volatile uint32_t*)0xB8004) = 0x4F6E4F6C;
    *((volatile uint32_t*)0xB8008) = 0x4F214F21;

    terminal_initialize();

    init_pic();

    init_keyboard();

    terminal_writestring("$ ");

    for (;;) {
        char c = keyboard_read();
        if (c != 0)
            process_keypress(c);
    }
}