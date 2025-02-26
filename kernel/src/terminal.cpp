#include "terminal.hpp"
#include "vga.hpp"
#include <cstring>

uint16_t terminal_row = 0;
uint16_t terminal_column = 0;
uint8_t terminal_color = 0;

void terminal_initialize() {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    for (uint16_t y = 0; y < VGA_HEIGHT; y++) {
        for (uint16_t x = 0; x < VGA_WIDTH; x++) {
            const uint16_t index = y * VGA_WIDTH + x;
            VGA_MEMORY[index] = vga_entry(' ', terminal_color);
        }
    }

    cursor_initialize();
    update_cursor();
}

void terminal_putchar_at(char c, uint8_t color, uint16_t x, uint16_t y) {
    const uint16_t index = y * VGA_WIDTH + x;
    VGA_MEMORY[index] = vga_entry(c, color);
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
        if (terminal_row == VGA_HEIGHT) {
            for (uint16_t y = 0; y < VGA_HEIGHT - 1; y++) {
                for (uint16_t x = 0; x < VGA_WIDTH; x++) {
                    const uint16_t to_index = y * VGA_WIDTH + x;
                    const uint16_t from_index = (y + 1) * VGA_WIDTH + x;
                    VGA_MEMORY[to_index] = VGA_MEMORY[from_index];
                }
            }

            for (uint16_t x = 0; x < VGA_WIDTH; x++) {
                const uint16_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
                VGA_MEMORY[index] = vga_entry(' ', terminal_color);
            }

            terminal_row = VGA_HEIGHT - 1;
        }
        update_cursor();
        return;
    }

    terminal_putchar_at(c, terminal_color, terminal_column, terminal_row);
    terminal_column++;
    if (terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        terminal_row++;
        if (terminal_row == VGA_HEIGHT) {
            for (uint16_t y = 0; y < VGA_HEIGHT - 1; y++) {
                for (uint16_t x = 0; x < VGA_WIDTH; x++) {
                    const uint16_t to_index = y * VGA_WIDTH + x;
                    const uint16_t from_index = (y + 1) * VGA_WIDTH + x;
                    VGA_MEMORY[to_index] = VGA_MEMORY[from_index];
                }
            }

            for (uint16_t x = 0; x < VGA_WIDTH; x++) {
                const uint16_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
                VGA_MEMORY[index] = vga_entry(' ', terminal_color);
            }

            terminal_row = VGA_HEIGHT - 1;
        }
    }
    update_cursor();
}

void terminal_writestring(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        terminal_putchar(str[i]);
    }
}

void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        terminal_putchar(data[i]);
    }
}

void terminal_clear() {
    terminal_initialize();
}