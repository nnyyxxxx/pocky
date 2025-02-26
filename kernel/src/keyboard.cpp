#include "keyboard.hpp"
#include "io.hpp"
#include "terminal.hpp"
#include "vga.hpp"
#include "shell.hpp"
#include <cstring>

static const char keyboard_map[128] = {
    0,   27,  '1', '2', '3',  '4', '5', '6',  '7', '8', '9',  '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't',  'y', 'u', 'i',  'o', 'p', '[',  ']', '\n', 0,   'a',  's',
    'd', 'f', 'g', 'h', 'j',  'k', 'l', ';',  '\'', '`', 0,   '\\', 'z', 'x', 'c',  'v',
    'b', 'n', 'm', ',', '.',  '/', 0,   '*',  0,   ' ', 0,    0,   0,   0,    0,    0,
    0,   0,   0,   0,   0,    0,   0,   0,    0,   0,   0,    0,   0,   0,    0,    0,
    0,   0,   0,   0,   0,    0,   0,   0,    0,   0,   0,    0,   0,   0,    0,    0,
    0,   0,   0,   0,   0,    0,   0,   0,    0,   0,   0,    0,   0,   0,
};

char keyboard_read() {
    if (inb(KEYBOARD_STATUS_PORT) & 1) {
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);

        if (scancode < 128)
            return keyboard_map[scancode];
    }

    return 0;
}

void init_keyboard() {
    while (inb(KEYBOARD_STATUS_PORT) & 1)
        inb(KEYBOARD_DATA_PORT);

    outb(KEYBOARD_DATA_PORT, 0xF4);
}

void process_keypress(char c) {
    if (c == '\n') {
        terminal_putchar('\n');
        process_command();
        terminal_writestring("$ ");
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