#include "graphics.hpp"

#include <cstring>

#include "io.hpp"
#include "keyboard.hpp"
#include "mouse.hpp"
#include "terminal.hpp"
#include "vga.hpp"

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

constexpr uint8_t cursor_bitmap[16][16] = {{1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
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

uint8_t g_320x200x256[] = {
    0x63, 0x03, 0x01, 0x0F, 0x00, 0x0E, 0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F, 0x00, 0x41,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3, 0xFF, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00, 0x0F, 0x00, 0x00};

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

void set_mode13h() {
    __asm__ volatile("cli");

    outb(VGA_SEQ_INDEX, 0x01);
    outb(VGA_SEQ_DATA, inb(VGA_SEQ_DATA) | 0x20);

    write_registers(g_320x200x256);

    outb(VGA_DAC_INDEX_WRITE, 0);
    outb(VGA_DAC_DATA, 0);
    outb(VGA_DAC_DATA, 0);
    outb(VGA_DAC_DATA, 0);

    outb(VGA_DAC_INDEX_WRITE, 1);
    outb(VGA_DAC_DATA, 0);
    outb(VGA_DAC_DATA, 0);
    outb(VGA_DAC_DATA, 0);

    outb(VGA_DAC_INDEX_WRITE, 2);
    outb(VGA_DAC_DATA, 63);
    outb(VGA_DAC_DATA, 63);
    outb(VGA_DAC_DATA, 63);

    for (int i = 3; i < 16; i++) {
        outb(VGA_DAC_INDEX_WRITE, i);

        switch (i) {
            case 4:
                outb(VGA_DAC_DATA, 63);
                outb(VGA_DAC_DATA, 0);
                outb(VGA_DAC_DATA, 0);
                break;
            case 2:
                outb(VGA_DAC_DATA, 0);
                outb(VGA_DAC_DATA, 63);
                outb(VGA_DAC_DATA, 0);
                break;
            case 9:
                outb(VGA_DAC_DATA, 0);
                outb(VGA_DAC_DATA, 0);
                outb(VGA_DAC_DATA, 63);
                break;
            case 15:
                outb(VGA_DAC_DATA, 63);
                outb(VGA_DAC_DATA, 63);
                outb(VGA_DAC_DATA, 63);
                break;
            default:
                outb(VGA_DAC_DATA, 32);
                outb(VGA_DAC_DATA, 32);
                outb(VGA_DAC_DATA, 32);
                break;
        }
    }

    outb(VGA_SEQ_INDEX, 0x01);
    outb(VGA_SEQ_DATA, inb(VGA_SEQ_DATA) & ~0x20);

    __asm__ volatile("sti");
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
            if (x + j >= 0 && y + i >= 0 && x + j < GRAPHICS_WIDTH && y + i < GRAPHICS_HEIGHT)
                cursor_backup[i][j] = vga_framebuffer[(y + i) * GRAPHICS_WIDTH + (x + j)];
        }
    }
}

void restore_cursor_background(int32_t x, int32_t y) {
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            if (x + j >= 0 && y + i >= 0 && x + j < GRAPHICS_WIDTH && y + i < GRAPHICS_HEIGHT)
                vga_framebuffer[(y + i) * GRAPHICS_WIDTH + (x + j)] = cursor_backup[i][j];
        }
    }
}

void render_mouse_cursor(int32_t x, int32_t y) {
    if (cursor_x != -1 && cursor_y != -1) restore_cursor_background(cursor_x, cursor_y);

    save_cursor_background(x, y);

    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            if (cursor_bitmap[i][j] != 0 && x + j >= 0 && y + i >= 0 && x + j < GRAPHICS_WIDTH &&
                y + i < GRAPHICS_HEIGHT)
                vga_framebuffer[(y + i) * GRAPHICS_WIDTH + (x + j)] = cursor_bitmap[i][j];
        }
    }

    cursor_x = x;
    cursor_y = y;
}

void enter_graphics_mode() {
    set_mode13h();

    clear_screen(0);

    cursor_x = -1;
    cursor_y = -1;

    MouseState mouse = get_mouse_state();
    render_mouse_cursor(mouse.x, mouse.y);

    MouseState lastMouseState = mouse;

    while (true) {
        if ((inb(KEYBOARD_STATUS_PORT) & STATUS_OUTPUT_BUFFER_FULL) != 0) {
            if ((inb(KEYBOARD_STATUS_PORT) & STATUS_MOUSE_DATA) != 0) inb(KEYBOARD_DATA_PORT);
        }

        MouseState mouse = get_mouse_state();
        if (mouse.x != lastMouseState.x || mouse.y != lastMouseState.y ||
            mouse.left_button != lastMouseState.left_button ||
            mouse.right_button != lastMouseState.right_button ||
            mouse.middle_button != lastMouseState.middle_button) {
            render_mouse_cursor(mouse.x, mouse.y);

            lastMouseState = mouse;
        }

        for (volatile int i = 0; i < 1000; i++)
            ;
    }
}