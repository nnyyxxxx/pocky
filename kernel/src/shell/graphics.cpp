#include "graphics.hpp"

#include <cstring>

#include "io.hpp"
#include "keyboard.hpp"
#include "mouse.hpp"
#include "terminal.hpp"
#include "vga.hpp"

bool in_graphics_mode = false;

constexpr uint8_t SCAN_ESC = 0x01;
constexpr uint8_t STATUS_OUTPUT_BUFFER_FULL = 0x01;
constexpr uint8_t STATUS_MOUSE_DATA = 0x20;

constexpr uint16_t VGA_AC_INDEX = 0x3C0;
constexpr uint16_t VGA_AC_WRITE = 0x3C0;
constexpr uint16_t VGA_AC_READ = 0x3C1;
constexpr uint16_t VGA_MISC_WRITE = 0x3C2;
constexpr uint16_t VGA_SEQ_INDEX = 0x3C4;
constexpr uint16_t VGA_SEQ_DATA = 0x3C5;
constexpr uint16_t VGA_DAC_INDEX_READ = 0x3C7;
constexpr uint16_t VGA_DAC_INDEX_WRITE = 0x3C8;
constexpr uint16_t VGA_DAC_DATA = 0x3C9;
constexpr uint16_t VGA_MISC_READ = 0x3CC;
constexpr uint16_t VGA_GC_INDEX = 0x3CE;
constexpr uint16_t VGA_GC_DATA = 0x3CF;
constexpr uint16_t VGA_CRTC_INDEX = 0x3D4;
constexpr uint16_t VGA_CRTC_DATA = 0x3D5;
constexpr uint16_t VGA_INSTAT_READ = 0x3DA;

constexpr uint32_t VGA_FRAMEBUFFER = 0xA0000;
uint8_t* vga_framebuffer = reinterpret_cast<uint8_t*>(VGA_FRAMEBUFFER);

const uint8_t cursor_bitmap[16][16] = {{1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                       {1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                       {1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                       {1, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                       {1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                       {1, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                       {1, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0},
                                       {1, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0},
                                       {1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0},
                                       {1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
                                       {1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                       {1, 2, 2, 1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                       {1, 2, 1, 0, 1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0},
                                       {1, 1, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0},
                                       {0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0, 0, 0},
                                       {0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0}};

uint8_t cursor_backup[16][16];
int32_t cursor_x = -1;
int32_t cursor_y = -1;

void write_registers(uint8_t* registers) {
    outb(VGA_MISC_WRITE, *registers++);

    for (uint8_t i = 0; i < 5; i++) {
        outb(VGA_SEQ_INDEX, i);
        outb(VGA_SEQ_DATA, *registers++);
    }

    outb(VGA_CRTC_INDEX, 0x03);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) | 0x80);
    outb(VGA_CRTC_INDEX, 0x11);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) & ~0x80);

    registers[0x03] = registers[0x03] | 0x80;
    registers[0x11] = registers[0x11] & ~0x80;

    for (uint8_t i = 0; i < 25; i++) {
        outb(VGA_CRTC_INDEX, i);
        outb(VGA_CRTC_DATA, *registers++);
    }

    for (uint8_t i = 0; i < 9; i++) {
        outb(VGA_GC_INDEX, i);
        outb(VGA_GC_DATA, *registers++);
    }

    for (uint8_t i = 0; i < 21; i++) {
        inb(VGA_INSTAT_READ);
        outb(VGA_AC_INDEX, i);
        outb(VGA_AC_WRITE, *registers++);
    }

    inb(VGA_INSTAT_READ);
    outb(VGA_AC_INDEX, 0x20);
}

uint8_t g_320x200x256[] = {
    0x63,
    0x03, 0x01, 0x0F, 0x00, 0x0E,
    0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x41, 0x00, 0x0F, 0x00, 0x00};

void set_mode13h() {
    write_registers(g_320x200x256);

    outb(VGA_DAC_INDEX_WRITE, 1);
    outb(VGA_DAC_DATA, 0);
    outb(VGA_DAC_DATA, 0);
    outb(VGA_DAC_DATA, 0);

    outb(VGA_DAC_INDEX_WRITE, 2);
    outb(VGA_DAC_DATA, 63);
    outb(VGA_DAC_DATA, 63);
    outb(VGA_DAC_DATA, 63);
}

void restore_text_mode() {
    outb(0x3C0, 0x30);
    outb(0x3C0, 0x00);

    outb(0x3C2, 0x67);

    outb(0x3C4, 0x00);
    outb(0x3C5, 0x03);

    outb(0x3C4, 0x01);
    outb(0x3C5, 0x00);

    outb(0x3C4, 0x02);
    outb(0x3C5, 0x03);

    outb(0x3C4, 0x03);
    outb(0x3C5, 0x00);

    outb(0x3C4, 0x04);
    outb(0x3C5, 0x02);

    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x06);

    outb(0x3D4, 0x0B);
    outb(0x3D5, 0x07);

    outb(0x3D4, 0x0C);
    outb(0x3D5, 0x00);

    outb(0x3D4, 0x0D);
    outb(0x3D5, 0x00);

    outb(0x3D4, 0x0E);
    outb(0x3D5, 0x00);

    outb(0x3D4, 0x0F);
    outb(0x3D5, 0x00);

    outb(0x3C0, 0x20);
}

void graphics_initialize() {
    // no-op
}

void set_pixel(uint16_t x, uint16_t y, uint8_t color) {
    if (x < GRAPHICS_WIDTH && y < GRAPHICS_HEIGHT) vga_framebuffer[y * GRAPHICS_WIDTH + x] = color;
}

void clear_screen(uint8_t color) {
    memset(vga_framebuffer, color, GRAPHICS_WIDTH * GRAPHICS_HEIGHT);
}

void save_cursor_background(int32_t x, int32_t y) {
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            if (x + j < GRAPHICS_WIDTH && y + i < GRAPHICS_HEIGHT && x + j >= 0 && y + i >= 0)
                cursor_backup[i][j] = vga_framebuffer[(y + i) * GRAPHICS_WIDTH + (x + j)];
        }
    }
}

void restore_cursor_background(int32_t x, int32_t y) {
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            if (x + j < GRAPHICS_WIDTH && y + i < GRAPHICS_HEIGHT && x + j >= 0 && y + i >= 0)
                vga_framebuffer[(y + i) * GRAPHICS_WIDTH + (x + j)] = cursor_backup[i][j];
        }
    }
}

void render_mouse_cursor(int32_t x, int32_t y) {
    if (cursor_x != -1 && cursor_y != -1) restore_cursor_background(cursor_x, cursor_y);

    save_cursor_background(x, y);

    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            if (cursor_bitmap[i][j] != 0 && x + j < GRAPHICS_WIDTH && y + i < GRAPHICS_HEIGHT &&
                x + j >= 0 && y + i >= 0)
                vga_framebuffer[(y + i) * GRAPHICS_WIDTH + (x + j)] = cursor_bitmap[i][j];
        }
    }

    cursor_x = x;
    cursor_y = y;
}

void enter_graphics_mode() {
    in_graphics_mode = true;
    set_mode13h();
    clear_screen(0);

    MouseState mouse = get_mouse_state();
    cursor_x = -1;
    cursor_y = -1;

    while (in_graphics_mode) {
        if ((inb(KEYBOARD_STATUS_PORT) & STATUS_OUTPUT_BUFFER_FULL) != 0) {
            if ((inb(KEYBOARD_STATUS_PORT) & STATUS_MOUSE_DATA) == 0) {
                uint8_t scancode = inb(KEYBOARD_DATA_PORT);
                if (scancode == SCAN_ESC) {
                    exit_graphics_mode();
                    break;
                }
            } else
                inb(KEYBOARD_DATA_PORT);
        }

        MouseState current = get_mouse_state();

        if (current.x != cursor_x || current.y != cursor_y) {
            render_mouse_cursor(current.x, current.y);
        }

        for (volatile int i = 0; i < 500; i++)
            ;
    }
}

void exit_graphics_mode() {
    in_graphics_mode = false;
    restore_text_mode();
    cursor_initialize();
    terminal_initialize();
}