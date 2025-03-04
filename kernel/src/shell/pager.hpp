#pragma once
#include <cstddef>
#include "vga.hpp"
#include "shell.hpp"

namespace pager {

constexpr size_t MAX_LINES = 1000;
constexpr size_t MAX_LINE_LENGTH = 256;

void init_pager();
void show_text(const char* text);
void process_keypress(char c);
bool is_active();
void clear();

} // namespace pager