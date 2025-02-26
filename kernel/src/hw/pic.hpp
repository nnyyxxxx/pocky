#pragma once
#include <cstdint>

constexpr uint16_t PIC1_COMMAND = 0x20;
constexpr uint16_t PIC1_DATA = 0x21;
constexpr uint16_t PIC2_COMMAND = 0xA0;
constexpr uint16_t PIC2_DATA = 0xA1;

void init_pic();