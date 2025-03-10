#include "pager.hpp"

#include <cstring>

#include "printf.hpp"
#include "screen_state.hpp"
#include "shell.hpp"
#include "terminal.hpp"
#include "vga.hpp"

namespace pager {

namespace {

char lines[MAX_LINES][MAX_LINE_LENGTH];
size_t line_count = 0;
size_t current_line = 0;
bool pager_active = false;

void display_current_page() {
    terminal_clear();

    size_t rows = VGA_HEIGHT - 1;
    for (size_t i = 0; i < rows && (current_line + i) < line_count; i++) {
        printf("%s\n", lines[current_line + i]);
    }

    uint8_t old_color = terminal_color;

    terminal_color = vga_color(VGA_COLOR_BLACK, VGA_COLOR_WHITE);

    terminal_row = VGA_HEIGHT - 1;
    terminal_column = 0;

    if (line_count <= rows)
        printf("(END) q:quit");
    else {
        size_t percent = (current_line * 100) / (line_count - rows + 1);
        if (current_line + rows >= line_count)
            printf("(END %lu%%) q:quit", percent);
        else
            printf("(%lu%%) j/%c:down k/%c:up q:quit", percent, 25, 24);
    }

    terminal_color = old_color;
}

void split_text(const char* text) {
    line_count = 0;
    size_t pos = 0;
    size_t line_pos = 0;

    while (text[pos] && line_count < MAX_LINES) {
        if (text[pos] == '\n' || line_pos == MAX_LINE_LENGTH - 1) {
            lines[line_count][line_pos] = '\0';
            line_count++;
            line_pos = 0;
            if (text[pos] == '\n') pos++;
            continue;
        }
        lines[line_count][line_pos++] = text[pos++];
    }

    if (line_pos > 0 && line_count < MAX_LINES) {
        lines[line_count][line_pos] = '\0';
        line_count++;
    }
}

}  // namespace

void init_pager() {
    memset(lines, 0, sizeof(lines));
    line_count = 0;
    current_line = 0;
    pager_active = false;
}

void show_text(const char* text) {
    screen_state::save();

    split_text(text);
    current_line = 0;
    pager_active = true;
    display_current_page();
}

void process_keypress(char c) {
    if (!pager_active) return;

    size_t rows = VGA_HEIGHT - 1;
    bool should_update = false;

    switch (c) {
        case 'q':
            clear();
            break;

        case 'j':
        case '\x1B':
            if (current_line + rows < line_count) {
                current_line++;
                should_update = true;
            }
            break;

        case 'k':
            if (current_line > 0) {
                current_line--;
                should_update = true;
            }
            break;

        case ' ':
        case '\x04':
            if (current_line + rows < line_count) {
                current_line += rows;
                if (current_line + rows > line_count) current_line = line_count - rows;
                should_update = true;
            }
            break;

        case '\x15':
            if (current_line > 0) {
                if (current_line >= rows)
                    current_line -= rows;
                else
                    current_line = 0;
                should_update = true;
            }
            break;

        case 'g':
            if (current_line > 0) {
                current_line = 0;
                should_update = true;
            }
            break;

        case 'G':
            if (current_line + rows < line_count) {
                current_line = line_count - rows;
                should_update = true;
            }
            break;
    }

    if (should_update) display_current_page();
}

bool is_active() {
    return pager_active;
}

void clear() {
    pager_active = false;
    screen_state::restore();
    print_prompt();
}

}  // namespace pager