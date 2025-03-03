#include "graphics.hpp"

#include <cstring>

#include "io.hpp"
#include "keyboard.hpp"
#include "terminal.hpp"
#include "vga.hpp"

bool in_graphics_mode = false;

constexpr uint8_t SCAN_ESC = 0x01;
constexpr uint16_t CURSOR_X = 40;
constexpr uint16_t CURSOR_Y = 12;

void graphics_initialize() {
    // no-op
}

void render_cursor() {
    const uint16_t index = CURSOR_Y * VGA_WIDTH + CURSOR_X;
    VGA_MEMORY[index] = vga_entry(30, vga_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

void enter_graphics_mode() {
    in_graphics_mode = true;

    for (uint16_t y = 0; y < VGA_HEIGHT; y++) {
        for (uint16_t x = 0; x < VGA_WIDTH; x++) {
            const uint16_t index = y * VGA_WIDTH + x;
            VGA_MEMORY[index] = vga_entry(' ', vga_color(VGA_COLOR_BLACK, VGA_COLOR_BLACK));
        }
    }

    outb(VGA_CTRL_PORT, 0x0A);
    outb(VGA_DATA_PORT, 0x20);

    render_cursor();

    while (in_graphics_mode) {
        if (inb(KEYBOARD_STATUS_PORT) & 1) {
            uint8_t scancode = inb(KEYBOARD_DATA_PORT);

            if (scancode == SCAN_ESC) {
                exit_graphics_mode();
                break;
            }
        }

        for (volatile int i = 0; i < 10000; i++)
            ;
    }
}

void exit_graphics_mode() {
    in_graphics_mode = false;

    cursor_initialize();

    terminal_initialize();
}