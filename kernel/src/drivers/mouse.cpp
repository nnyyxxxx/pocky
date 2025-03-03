#include "mouse.hpp"

#include "graphics.hpp"
#include "idt.hpp"
#include "io.hpp"
#include "pic.hpp"
#include "terminal.hpp"

namespace {
bool mouse_initialized = false;
uint8_t mouse_cycle = 0;
uint8_t mouse_packet[3];

MouseState current_mouse_state;

void mouse_wait_output() {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (inb(MOUSE_COMMAND_PORT) & 0x01) return;
    }
}

void mouse_wait_input() {
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(MOUSE_COMMAND_PORT) & 0x02) == 0) return;
    }
}

uint8_t mouse_read() {
    mouse_wait_output();
    return inb(MOUSE_DATA_PORT);
}

void mouse_write(uint8_t cmd) {
    mouse_wait_input();
    outb(MOUSE_COMMAND_PORT, 0xD4);

    mouse_wait_input();
    outb(MOUSE_DATA_PORT, cmd);

    uint8_t ack = mouse_read();
    if (ack != 0xFA) terminal_writestring("Mouse command failed: ");
}

}  // namespace

void process_mouse_packet(uint8_t packet[3]) {
    if (!mouse_initialized) return;

    if (!(packet[0] & 0x08)) return;

    bool left = packet[0] & MOUSE_LEFT_BUTTON;
    bool right = packet[0] & MOUSE_RIGHT_BUTTON;
    bool middle = packet[0] & MOUSE_MIDDLE_BUTTON;

    int8_t x_mov = (int8_t)packet[1];
    int8_t y_mov = (int8_t)packet[2];

    y_mov = -y_mov;

    const int SENSITIVITY = 4;
    current_mouse_state.x += x_mov * SENSITIVITY;
    current_mouse_state.y += y_mov * SENSITIVITY;

    if (current_mouse_state.x < 0) current_mouse_state.x = 0;
    if (current_mouse_state.y < 0) current_mouse_state.y = 0;
    if (current_mouse_state.x >= GRAPHICS_WIDTH) current_mouse_state.x = GRAPHICS_WIDTH - 1;
    if (current_mouse_state.y >= GRAPHICS_HEIGHT) current_mouse_state.y = GRAPHICS_HEIGHT - 1;

    current_mouse_state.left_button = left;
    current_mouse_state.right_button = right;
    current_mouse_state.middle_button = middle;
}

extern "C" void mouse_callback() {
    uint8_t status = inb(MOUSE_COMMAND_PORT);

    if (!(status & 0x20) || !(status & 0x01)) {
        outb(PIC2_COMMAND, 0x20);
        outb(PIC1_COMMAND, 0x20);
        return;
    }

    uint8_t data = inb(MOUSE_DATA_PORT);

    switch (mouse_cycle) {
        case 0:
            if (data & 0x08) {
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

    outb(PIC2_COMMAND, 0x20);
    outb(PIC1_COMMAND, 0x20);
}

void init_mouse() {
    mouse_wait_input();
    outb(MOUSE_COMMAND_PORT, 0xA8);

    mouse_wait_input();
    outb(MOUSE_COMMAND_PORT, 0x20);
    mouse_wait_output();
    uint8_t status = inb(MOUSE_DATA_PORT);
    status |= 0x02;
    mouse_wait_input();
    outb(MOUSE_COMMAND_PORT, 0x60);
    mouse_wait_input();
    outb(MOUSE_DATA_PORT, status);

    mouse_write(MOUSE_CMD_RESET);
    mouse_read();

    mouse_write(MOUSE_CMD_SET_DEFAULTS);

    mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);
    mouse_write(200);

    mouse_write(MOUSE_CMD_SET_RESOLUTION);
    mouse_write(3);

    mouse_write(MOUSE_CMD_ENABLE_STREAMING);

    current_mouse_state.x = GRAPHICS_WIDTH / 2;
    current_mouse_state.y = GRAPHICS_HEIGHT / 2;
    current_mouse_state.left_button = false;
    current_mouse_state.right_button = false;
    current_mouse_state.middle_button = false;

    mouse_cycle = 0;

    set_interrupt_handler(32 + MOUSE_IRQ, mouse_handler,
                          IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT_GATE);

    outb(PIC1_DATA, inb(PIC1_DATA) & ~(1 << 2));
    outb(PIC2_DATA, inb(PIC2_DATA) & ~(1 << 4));

    mouse_initialized = true;
}

MouseState get_mouse_state() {
    return current_mouse_state;
}