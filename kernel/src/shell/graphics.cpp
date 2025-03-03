#include "graphics.hpp"

#include <cstring>

#include "io.hpp"
#include "keyboard.hpp"
#include "mouse.hpp"
#include "terminal.hpp"
#include "vga.hpp"

bool in_graphics_mode = false;

constexpr uint8_t SCAN_ESC = 0x01;
constexpr uint8_t MOUSE_CHAR = 219;

constexpr uint8_t STATUS_OUTPUT_BUFFER_FULL = 0x01;
constexpr uint8_t STATUS_INPUT_BUFFER_FULL = 0x02;
constexpr uint8_t STATUS_MOUSE_DATA = 0x20;

void graphics_initialize() {
    // no-op
}

void render_mouse_cursor(int32_t x, int32_t y) {
    uint16_t vga_x = static_cast<uint16_t>(x / 8);
    uint16_t vga_y = static_cast<uint16_t>(y / 16);

    if (vga_x >= VGA_WIDTH) vga_x = VGA_WIDTH - 1;
    if (vga_y >= VGA_HEIGHT) vga_y = VGA_HEIGHT - 1;

    const uint16_t index = vga_y * VGA_WIDTH + vga_x;
    VGA_MEMORY[index] = vga_entry(MOUSE_CHAR, vga_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
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

    int32_t last_x = -1;
    int32_t last_y = -1;
    uint32_t counter = 0;

    MouseState mouse;
    mouse.x = VGA_WIDTH * 4;
    mouse.y = VGA_HEIGHT * 8;

    while (in_graphics_mode) {
        counter++;

        if ((inb(KEYBOARD_STATUS_PORT) & STATUS_OUTPUT_BUFFER_FULL) != 0) {
            if ((inb(KEYBOARD_STATUS_PORT) & STATUS_MOUSE_DATA) == 0) {
                uint8_t scancode = inb(KEYBOARD_DATA_PORT);
                if (scancode == SCAN_ESC) {
                    exit_graphics_mode();
                    break;
                }
            } else
                inb(KEYBOARD_DATA_PORT);
        }

        if (counter % 10 == 0) {
            MouseState current = get_mouse_state();

            if (current.x != last_x || current.y != last_y) {
                if (last_x >= 0 && last_y >= 0) {
                    uint16_t last_vga_x = static_cast<uint16_t>(last_x / 8);
                    uint16_t last_vga_y = static_cast<uint16_t>(last_y / 16);

                    if (last_vga_x < VGA_WIDTH && last_vga_y < VGA_HEIGHT) {
                        const uint16_t index = last_vga_y * VGA_WIDTH + last_vga_x;
                        VGA_MEMORY[index] =
                            vga_entry(' ', vga_color(VGA_COLOR_BLACK, VGA_COLOR_BLACK));
                    }
                }

                render_mouse_cursor(current.x, current.y);
                last_x = current.x;
                last_y = current.y;
            }
        }

        for (volatile int i = 0; i < 1000; i++)
            ;
    }
}

void exit_graphics_mode() {
    in_graphics_mode = false;

    cursor_initialize();
    terminal_initialize();
}