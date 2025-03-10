#include "screen_state.hpp"

#include <cstring>

#include "terminal.hpp"
#include "vga.hpp"

namespace screen_state {

namespace {
uint16_t saved_screen[SCREEN_SIZE];
uint16_t saved_terminal_row = 0;
uint16_t saved_terminal_column = 0;
uint8_t saved_terminal_color = 0;
}  // namespace

void init() {
    memset(saved_screen, 0, sizeof(saved_screen));
    saved_terminal_row = 0;
    saved_terminal_column = 0;
    saved_terminal_color = 0;
}

void save() {
    memcpy(saved_screen, const_cast<uint16_t*>(VGA_MEMORY), sizeof(saved_screen));

    saved_terminal_row = terminal_row;
    saved_terminal_column = terminal_column;
    saved_terminal_color = terminal_color;
}

void restore() {
    memcpy(const_cast<uint16_t*>(VGA_MEMORY), saved_screen, sizeof(saved_screen));

    terminal_row = saved_terminal_row;
    terminal_column = saved_terminal_column;
    terminal_color = saved_terminal_color;

    update_cursor();
}

}  // namespace screen_state