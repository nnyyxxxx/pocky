#include "mouse.hpp"

#include <cstring>

#include "idt.hpp"
#include "io.hpp"
#include "pic.hpp"
#include "terminal.hpp"

namespace {

MouseState current_mouse_state;
uint8_t mouse_cycle = 0;
uint8_t mouse_packet[3] = {0};
bool mouse_initialized = false;

void mouse_wait_for_output() {
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(MOUSE_COMMAND_PORT) & 0x01) == 0x01) return;
    }
}

void mouse_wait_for_input() {
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(MOUSE_COMMAND_PORT) & 0x02) == 0) return;
    }
}

void mouse_write(uint8_t value) {
    mouse_wait_for_input();
    outb(MOUSE_COMMAND_PORT, 0xD4);
    mouse_wait_for_input();
    outb(MOUSE_DATA_PORT, value);
}

uint8_t mouse_read() {
    mouse_wait_for_output();
    return inb(MOUSE_DATA_PORT);
}

}  // namespace

void process_mouse_packet(uint8_t packet[3]) {
    if (!mouse_initialized) return;

    bool x_overflow = packet[0] & MOUSE_X_OVERFLOW;
    bool y_overflow = packet[0] & MOUSE_Y_OVERFLOW;

    if (!x_overflow && !y_overflow) {
        int8_t x_movement = packet[1];
        int8_t y_movement = packet[2];

        if (packet[0] & MOUSE_X_SIGN) x_movement |= 0xFFFFFF00;

        if (packet[0] & MOUSE_Y_SIGN) y_movement |= 0xFFFFFF00;

        y_movement = -y_movement;

        current_mouse_state.x += x_movement;
        current_mouse_state.y += y_movement;

        constexpr int32_t SCREEN_WIDTH = 640;
        constexpr int32_t SCREEN_HEIGHT = 480;

        if (current_mouse_state.x < 0) current_mouse_state.x = 0;
        if (current_mouse_state.y < 0) current_mouse_state.y = 0;
        if (current_mouse_state.x >= SCREEN_WIDTH) current_mouse_state.x = SCREEN_WIDTH - 1;
        if (current_mouse_state.y >= SCREEN_HEIGHT) current_mouse_state.y = SCREEN_HEIGHT - 1;
    }

    current_mouse_state.left_button = (packet[0] & MOUSE_LEFT_BUTTON) != 0;
    current_mouse_state.right_button = (packet[0] & MOUSE_RIGHT_BUTTON) != 0;
    current_mouse_state.middle_button = (packet[0] & MOUSE_MIDDLE_BUTTON) != 0;
}

extern "C" void mouse_callback() {
    uint8_t status = inb(MOUSE_COMMAND_PORT);

    if (!(status & 0x01) || !(status & 0x20)) return;

    uint8_t data = inb(MOUSE_DATA_PORT);

    switch (mouse_cycle) {
        case 0:
            if ((data & 0x08) == 0x08) {
                mouse_packet[0] = data;
                mouse_cycle = 1;
            }
            break;

        case 1:
            mouse_packet[1] = data;
            mouse_cycle = 2;
            break;

        case 2:
            mouse_packet[2] = data;
            process_mouse_packet(mouse_packet);
            mouse_cycle = 0;
            break;
    }
}

void init_mouse() {
    uint8_t status;

    mouse_wait_for_input();
    outb(MOUSE_COMMAND_PORT, 0xA8);

    mouse_wait_for_input();
    outb(MOUSE_COMMAND_PORT, 0x20);
    mouse_wait_for_output();
    status = inb(MOUSE_DATA_PORT) | 2;
    mouse_wait_for_input();
    outb(MOUSE_COMMAND_PORT, 0x60);
    mouse_wait_for_input();
    outb(MOUSE_DATA_PORT, status);

    mouse_write(MOUSE_CMD_SET_DEFAULTS);
    if (mouse_read() != 0xFA) return;

    mouse_write(MOUSE_CMD_ENABLE_STREAMING);
    if (mouse_read() != 0xFA) return;

    set_interrupt_handler(32 + MOUSE_IRQ, mouse_handler,
                          IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT_GATE);

    outb(PIC1_DATA, inb(PIC1_DATA) & ~(1 << 2));
    outb(PIC2_DATA, inb(PIC2_DATA) & ~(1 << 4));

    current_mouse_state.x = 0;
    current_mouse_state.y = 0;
    current_mouse_state.left_button = false;
    current_mouse_state.right_button = false;
    current_mouse_state.middle_button = false;

    mouse_cycle = 0;
    mouse_initialized = true;
}

MouseState get_mouse_state() {
    return current_mouse_state;
}