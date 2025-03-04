#include "keyboard.hpp"

#include "io.hpp"
#include "pic.hpp"
#include "printf.hpp"

namespace {
KeyboardHandler keyboard_callback = nullptr;

const char scancode_to_ascii[] = {0,   '\x1B', '1',  '2',  '3',  '4', '5', '6',  '7', '8', '9', '0',
                                  '-', '=',    '\b', '\t', 'q',  'w', 'e', 'r',  't', 'y', 'u', 'i',
                                  'o', 'p',    '[',  ']',  '\n', 0,   'a', 's',  'd', 'f', 'g', 'h',
                                  'j', 'k',    'l',  ';',  '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v',
                                  'b', 'n',    'm',  ',',  '.',  '/', 0,   '*',  0,   ' '};

const char scancode_to_ascii_shifted[] = {
    0,    '\x1B', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',  '\b',
    '\t', 'Q',    'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A',  'S',    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|',  'Z',
    'X',  'C',    'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' '};

bool shift_pressed = false;
bool ctrl_pressed = false;

}  // namespace

void init_keyboard() {
    keyboard_callback = nullptr;
}

char keyboard_read() {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    if (scancode >= sizeof(scancode_to_ascii)) return 0;
    return scancode_to_ascii[scancode];
}

void register_keyboard_handler(KeyboardHandler handler) {
    keyboard_callback = handler;
}

extern "C" void keyboard_handler() {
    uint8_t scancode = inb(0x60);

    if (scancode == 0x2A) {
        shift_pressed = true;
        return;
    }
    if (scancode == 0xAA) {
        shift_pressed = false;
        return;
    }

    if (scancode == 0x36) {
        shift_pressed = true;
        return;
    }
    if (scancode == 0xB6) {
        shift_pressed = false;
        return;
    }

    if (scancode == 0x1D) {
        ctrl_pressed = true;
        return;
    }
    if (scancode == 0x9D) {
        ctrl_pressed = false;
        return;
    }

    if (scancode < 60) {
        const char* current_map = shift_pressed ? scancode_to_ascii_shifted : scancode_to_ascii;
        char c = current_map[scancode];

        if (c != 0 && keyboard_callback) {
            if (ctrl_pressed) {
                if (c >= 'a' && c <= 'z')
                    c = c - 'a' + 1;
                else if (c >= 'A' && c <= 'Z')
                    c = c - 'A' + 1;
            }
            keyboard_callback(c);
        }
    }

    outb(PIC1_COMMAND, 0x20);
}