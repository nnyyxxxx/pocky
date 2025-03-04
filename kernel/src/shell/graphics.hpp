#pragma once
#include <cstdint>
#include "mouse.hpp"
#include "../hw/rtc.hpp"

constexpr uint16_t GRAPHICS_WIDTH = 320;
constexpr uint16_t GRAPHICS_HEIGHT = 200;

constexpr uint16_t STATUS_BAR_HEIGHT = 12;
constexpr uint8_t STATUS_BAR_COLOR = 8;
constexpr uint8_t STATUS_BAR_TEXT_COLOR = 15;

void graphics_initialize();
void enter_graphics_mode();
void set_pixel(uint16_t x, uint16_t y, uint8_t color);
void clear_screen(uint8_t color);
void render_mouse_cursor(int32_t x, int32_t y);

void draw_char(uint16_t x, uint16_t y, char c, uint8_t color);
void draw_string(uint16_t x, uint16_t y, const char* str, uint8_t color);
void draw_status_bar();
void update_status_bar_time(const RTCTime& time);

extern bool in_graphics_mode;