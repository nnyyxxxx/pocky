#pragma once
#include <cstdint>

using KeyboardHandler = void (*)(char);

constexpr uint16_t KEYBOARD_DATA_PORT = 0x60;
constexpr uint16_t KEYBOARD_STATUS_PORT = 0x64;

char keyboard_read();
void init_keyboard();
void process_keypress(char c);
void register_keyboard_handler(KeyboardHandler handler);
extern "C" void keyboard_handler();