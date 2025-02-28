#pragma once
#include <cstdint>
#include <cstddef>

constexpr uint16_t PIT_CHANNEL0 = 0x40;
constexpr uint16_t PIT_CHANNEL1 = 0x41;
constexpr uint16_t PIT_CHANNEL2 = 0x42;
constexpr uint16_t PIT_COMMAND = 0x43;

constexpr uint32_t PIT_FREQUENCY = 1193182;

void init_timer(uint32_t frequency = 100);

uint64_t get_ticks();

uint64_t get_uptime_seconds();

void format_uptime(char* buffer, size_t size);