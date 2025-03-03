#pragma once
#include <cstdint>
#include "mouse.hpp"

constexpr uint16_t GRAPHICS_WIDTH = 320;
constexpr uint16_t GRAPHICS_HEIGHT = 200;

void graphics_initialize();
void enter_graphics_mode();
void set_pixel(uint16_t x, uint16_t y, uint8_t color);
void clear_screen(uint8_t color);
void render_mouse_cursor(int32_t x, int32_t y);

extern bool in_graphics_mode;