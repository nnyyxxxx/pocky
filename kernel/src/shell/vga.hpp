#pragma once
#include <cstddef>
#include <cstdint>

constexpr uint16_t VGA_WIDTH = 80;
constexpr uint16_t VGA_HEIGHT = 25;
constexpr uint8_t VGA_COLOR_BLACK = 0;
constexpr uint8_t VGA_COLOR_LIGHT_GREY = 7;
constexpr uint8_t VGA_COLOR_WHITE = 15;
constexpr uint8_t VGA_COLOR_RED = 4;
constexpr uint8_t VGA_COLOR_GREEN = 2;
constexpr uint8_t VGA_COLOR_BLUE = 1;
constexpr uint8_t VGA_COLOR_CYAN = 3;

constexpr uint16_t VGA_CTRL_PORT = 0x3D4;
constexpr uint16_t VGA_DATA_PORT = 0x3D5;

extern volatile uint16_t* const VGA_MEMORY;

uint8_t vga_color(uint8_t fg, uint8_t bg);
uint16_t vga_entry(char c, uint8_t color);
void update_cursor();
void update_cursor_position(size_t x, size_t y);
void cursor_initialize();