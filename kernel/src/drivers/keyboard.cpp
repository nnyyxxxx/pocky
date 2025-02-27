#include "keyboard.hpp"

#include <cstring>

#include "io.hpp"
#include "shell.hpp"
#include "terminal.hpp"
#include "vga.hpp"

static bool ctrl_pressed = false;
static bool shift_pressed = false;

constexpr uint8_t SCAN_CTRL = 0x1D;
constexpr uint8_t SCAN_CTRL_RELEASE = 0x9D;
constexpr uint8_t SCAN_C = 0x2E;
constexpr uint8_t SCAN_LEFT_SHIFT = 0x2A;
constexpr uint8_t SCAN_RIGHT_SHIFT = 0x36;
constexpr uint8_t SCAN_LEFT_SHIFT_RELEASE = 0xAA;
constexpr uint8_t SCAN_RIGHT_SHIFT_RELEASE = 0xB6;

static const char keyboard_map[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7',  '8', '9', '0',  '-',  '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o',  'p', '[', ']',  '\n', 0,   'a',  's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,   '\\', 'z',  'x', 'c',  'v',
    'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,    ' ', 0,   0,    0,    0,   0,    0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,
};

static const char keyboard_shift_map[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_',  '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,   'A',  'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|', 'Z',  'X', 'C',  'V',
    'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,    0,   0,    0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,    0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,    0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,
};

char keyboard_read() {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    if (scancode == SCAN_CTRL) {
        ctrl_pressed = true;
        return 0;
    } else if (scancode == SCAN_CTRL_RELEASE) {
        ctrl_pressed = false;
        return 0;
    } else if (scancode == SCAN_LEFT_SHIFT || scancode == SCAN_RIGHT_SHIFT) {
        shift_pressed = true;
        return 0;
    } else if (scancode == SCAN_LEFT_SHIFT_RELEASE || scancode == SCAN_RIGHT_SHIFT_RELEASE) {
        shift_pressed = false;
        return 0;
    }

    if (ctrl_pressed && scancode == SCAN_C) return 3;

    if (scancode < 128) {
        if (shift_pressed)
            return keyboard_shift_map[scancode];
        else
            return keyboard_map[scancode];
    }

    return 0;
}

void init_keyboard() {
    while (inb(KEYBOARD_STATUS_PORT) & 1)
        inb(KEYBOARD_DATA_PORT);

    ctrl_pressed = false;
    shift_pressed = false;

    outb(KEYBOARD_DATA_PORT, 0xF4);
}

void process_keypress(char c) {
    if (c == 3) {
        terminal_writestring("^C\n");
        interrupt_command();
        memset(input_buffer, 0, sizeof(input_buffer));
        input_pos = 0;
        terminal_writestring("$ ");
    } else if (c == '\n') {
        terminal_putchar('\n');
        process_command();
    } else if (c == '\b' && input_pos > 0) {
        input_pos--;
        input_buffer[input_pos] = '\0';

        if (terminal_column > 0)
            terminal_column--;
        else if (terminal_row > 0) {
            terminal_row--;
            terminal_column = VGA_WIDTH - 1;
        }

        terminal_putchar_at(' ', terminal_color, terminal_column, terminal_row);
        update_cursor();
    } else if (c >= ' ' && c <= '~' && input_pos < sizeof(input_buffer) - 1) {
        input_buffer[input_pos++] = c;
        terminal_putchar(c);
    }
}