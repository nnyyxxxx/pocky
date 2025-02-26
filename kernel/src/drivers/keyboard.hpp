#pragma once
#include <cstdint>

constexpr uint16_t KEYBOARD_DATA_PORT = 0x60;
constexpr uint16_t KEYBOARD_STATUS_PORT = 0x64;

char keyboard_read();
void init_keyboard();
void process_keypress(char c);