#pragma once
#include <cstddef>
#include <cstdint>

#include "vga.hpp"

namespace screen_state {

constexpr size_t SCREEN_SIZE = VGA_WIDTH * VGA_HEIGHT;

void save();

void restore();

void init();

}  // namespace screen_state