#include "vga.hpp"
#include "io.hpp"

extern volatile uint16_t* const VGA_MEMORY = reinterpret_cast<uint16_t*>(0xB8000);

uint16_t vga_entry(char c, uint8_t color) {
    return static_cast<uint16_t>(c) | (static_cast<uint16_t>(color) << 8);
}

uint8_t vga_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}

extern uint16_t terminal_row;
extern uint16_t terminal_column;

void update_cursor() {
    uint16_t pos = terminal_row * VGA_WIDTH + terminal_column;

    outb(VGA_CTRL_PORT, 14);
    outb(VGA_DATA_PORT, static_cast<uint8_t>(pos >> 8));
    outb(VGA_CTRL_PORT, 15);
    outb(VGA_DATA_PORT, static_cast<uint8_t>(pos & 0xFF));
}

void cursor_initialize() {
    outb(VGA_CTRL_PORT, 0x0A);
    outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xC0) | 0);

    outb(VGA_CTRL_PORT, 0x0B);
    outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xE0) | 15);
}