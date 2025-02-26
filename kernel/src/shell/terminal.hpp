#pragma once
#include <cstddef>
#include <cstdint>

extern uint16_t terminal_row;
extern uint16_t terminal_column;
extern uint8_t terminal_color;

void terminal_initialize();
void terminal_putchar_at(char c, uint8_t color, uint16_t x, uint16_t y);
void terminal_putchar(char c);
void terminal_writestring(const char* str);
void terminal_write(const char* data, size_t size);
void terminal_clear();