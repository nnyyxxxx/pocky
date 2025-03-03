#pragma once
#include <cstdint>

constexpr uint16_t MOUSE_DATA_PORT = 0x60;
constexpr uint16_t MOUSE_COMMAND_PORT = 0x64;
constexpr uint8_t MOUSE_IRQ = 12;

constexpr uint8_t MOUSE_CMD_RESET = 0xFF;
constexpr uint8_t MOUSE_CMD_RESEND = 0xFE;
constexpr uint8_t MOUSE_CMD_SET_DEFAULTS = 0xF6;
constexpr uint8_t MOUSE_CMD_DISABLE_STREAMING = 0xF5;
constexpr uint8_t MOUSE_CMD_ENABLE_STREAMING = 0xF4;
constexpr uint8_t MOUSE_CMD_SET_SAMPLE_RATE = 0xF3;
constexpr uint8_t MOUSE_CMD_GET_DEVICE_ID = 0xF2;
constexpr uint8_t MOUSE_CMD_SET_RESOLUTION = 0xE8;

constexpr uint8_t MOUSE_LEFT_BUTTON = 0x01;
constexpr uint8_t MOUSE_RIGHT_BUTTON = 0x02;
constexpr uint8_t MOUSE_MIDDLE_BUTTON = 0x04;
constexpr uint8_t MOUSE_X_SIGN = 0x10;
constexpr uint8_t MOUSE_Y_SIGN = 0x20;
constexpr uint8_t MOUSE_X_OVERFLOW = 0x40;
constexpr uint8_t MOUSE_Y_OVERFLOW = 0x80;

struct MouseState {
    int32_t x = 0;
    int32_t y = 0;
    bool left_button = false;
    bool right_button = false;
    bool middle_button = false;
};

void init_mouse();
MouseState get_mouse_state();
extern "C" void mouse_handler();
void process_mouse_packet(uint8_t packet[3]);