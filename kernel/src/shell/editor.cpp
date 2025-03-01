#include "editor.hpp"

#include <cstdio>
#include <cstring>

#include "fs/filesystem.hpp"
#include "io.hpp"
#include "printf.hpp"
#include "shell.hpp"
#include "terminal.hpp"
#include "vga.hpp"

namespace editor {

namespace {
constexpr uint8_t STATUS_COLOR = 0x70;
constexpr uint8_t TEXT_COLOR = 0x07;
constexpr size_t TERMINAL_WIDTH = 80;
constexpr size_t TERMINAL_HEIGHT = 25;
constexpr size_t STATUS_LINE = TERMINAL_HEIGHT - 1;

constexpr uint8_t CURSOR_NORMAL_START = 0;
constexpr uint8_t CURSOR_NORMAL_END = 15;
constexpr uint8_t CURSOR_INSERT_START = 14;
constexpr uint8_t CURSOR_INSERT_END = 15;

inline bool is_ctrl_key(char c, char key) {
    return (c == (key & 0x1f));
}
}  // namespace

TextEditor& TextEditor::instance() {
    static TextEditor instance;
    return instance;
}

bool TextEditor::open(const char* filename) {
    if (!filename || !*filename) return false;

    strncpy(m_filename, filename, MAX_FILENAME_LENGTH - 1);
    m_filename[MAX_FILENAME_LENGTH - 1] = '\0';

    auto& fs = fs::FileSystem::instance();
    auto node = fs.get_file(filename);

    if (!node) {
        fs.create_file(filename, fs::FileType::Regular);
        m_buffer_size = 0;
        m_buffer[0] = '\0';
    } else if (node->type == fs::FileType::Directory)
        return false;
    else if (node->type != fs::FileType::Regular)
        return false;
    else if (node->size >= MAX_BUFFER_SIZE)
        return false;

    m_buffer_size = node->size;
    if (m_buffer_size > 0 &&
        !fs.read_file(filename, reinterpret_cast<uint8_t*>(m_buffer), m_buffer_size))
        return false;
    m_buffer[m_buffer_size] = '\0';

    m_cursor_pos = 0;
    m_cursor_row = 0;
    m_cursor_col = 0;
    m_screen_row = 0;
    m_screen_col = 0;
    m_screen_offset = 0;
    m_active = true;
    m_modified = false;
    m_mode = EditorMode::NORMAL;

    terminal_clear();

    outb(VGA_CTRL_PORT, 0x0A);
    outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xC0) | CURSOR_NORMAL_START);
    outb(VGA_CTRL_PORT, 0x0B);
    outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xE0) | CURSOR_NORMAL_END);

    render();

    return true;
}

bool TextEditor::save() {
    if (!m_active || !m_filename[0]) return false;

    auto& fs = fs::FileSystem::instance();
    bool success =
        fs.write_file(m_filename, reinterpret_cast<const uint8_t*>(m_buffer), m_buffer_size);

    if (success) {
        m_modified = false;
        display_status_line();
    }

    return success;
}

void TextEditor::close() {
    if (!m_active) return;

    m_active = false;

    for (uint16_t y = 0; y < TERMINAL_HEIGHT; y++) {
        for (uint16_t x = 0; x < TERMINAL_WIDTH; x++) {
            terminal_putchar_at(' ', TEXT_COLOR, x, y);
        }
    }

    terminal_row = 0;
    terminal_column = 0;

    cursor_initialize();
    update_cursor();

    m_buffer_size = 0;
    m_cursor_pos = 0;
    m_cursor_row = 0;
    m_cursor_col = 0;
    m_screen_row = 0;
    m_screen_col = 0;
    m_screen_offset = 0;
    m_modified = false;
    m_filename[0] = '\0';

    terminal_writestring("\n");
    print_prompt();
}

void TextEditor::process_keypress(char c) {
    if (!m_active) return;

    if (is_ctrl_key(c, 'q')) {
        if (!m_modified || (m_modified && save())) close();
        return;
    }

    if (is_ctrl_key(c, 's')) {
        save();
        return;
    }

    if (m_mode == EditorMode::NORMAL)
        process_normal_mode(c);
    else
        process_insert_mode(c);

    render();
}

void TextEditor::process_normal_mode(char c) {
    switch (c) {
        case 'h':
            move_cursor_left();
            break;
        case 'j':
            move_cursor_down();
            break;
        case 'k':
            move_cursor_up();
            break;
        case 'l':
            move_cursor_right();
            break;
        case 'i':
            switch_to_insert_mode();
            break;
        case 'a':
            if (m_cursor_pos < m_buffer_size) move_cursor_right();
            switch_to_insert_mode();
            break;
        case 'x':
            delete_char();
            break;
        case 'd':
            size_t line_start = m_cursor_pos;
            while (line_start > 0 && m_buffer[line_start - 1] != '\n') {
                line_start--;
            }

            size_t line_end = m_cursor_pos;
            while (line_end < m_buffer_size && m_buffer[line_end] != '\n') {
                line_end++;
            }
            if (line_end < m_buffer_size) line_end++;

            size_t line_length = line_end - line_start;
            if (line_length > 0) {
                memmove(&m_buffer[line_start], &m_buffer[line_end], m_buffer_size - line_end + 1);
                m_buffer_size -= line_length;
                m_cursor_pos = line_start;
                m_cursor_col = 0;
                m_modified = true;
                update_screen_position();
            }
            break;
    }
}

void TextEditor::process_insert_mode(char c) {
    switch (c) {
        case 27:
            switch_to_normal_mode();
            break;
        case '\n':
            insert_char('\n');
            break;
        case '\b':
            backspace();
            break;
        case 127:
            delete_char();
            break;
        default:
            if (c >= 32 && c < 127) insert_char(c);
            break;
    }
}

void TextEditor::switch_to_normal_mode() {
    m_mode = EditorMode::NORMAL;

    outb(VGA_CTRL_PORT, 0x0A);
    outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xC0) | CURSOR_NORMAL_START);
    outb(VGA_CTRL_PORT, 0x0B);
    outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xE0) | CURSOR_NORMAL_END);

    display_status_line();
}

void TextEditor::switch_to_insert_mode() {
    m_mode = EditorMode::INSERT;

    outb(VGA_CTRL_PORT, 0x0A);
    outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xC0) | CURSOR_INSERT_START);
    outb(VGA_CTRL_PORT, 0x0B);
    outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xE0) | CURSOR_INSERT_END);

    display_status_line();
}

void TextEditor::render() {
    if (!m_active) return;

    for (uint16_t y = 0; y < TERMINAL_HEIGHT; y++) {
        for (uint16_t x = 0; x < TERMINAL_WIDTH; x++) {
            terminal_putchar_at(' ', TEXT_COLOR, x, y);
        }
    }

    terminal_row = 0;
    terminal_column = 0;

    size_t line_start = 0;
    size_t current_row = 0;
    size_t visible_rows = TERMINAL_HEIGHT - 1;

    for (size_t i = 0; i < m_buffer_size && current_row < m_screen_row; i++) {
        if (m_buffer[i] == '\n') {
            current_row++;
            line_start = i + 1;
        }
    }

    current_row = 0;
    size_t col = 0;

    for (size_t i = line_start; i < m_buffer_size && current_row < visible_rows; i++) {
        if (m_buffer[i] == '\n') {
            while (col < TERMINAL_WIDTH) {
                terminal_putchar_at(' ', TEXT_COLOR, col, current_row);
                col++;
            }
            current_row++;
            col = 0;
        } else {
            terminal_putchar_at(m_buffer[i], TEXT_COLOR, col, current_row);
            col++;
            if (col >= TERMINAL_WIDTH) {
                col = 0;
                current_row++;
            }
        }

        if (current_row >= visible_rows) break;
    }

    while (current_row < visible_rows) {
        while (col < TERMINAL_WIDTH) {
            terminal_putchar_at(' ', TEXT_COLOR, col, current_row);
            col++;
        }
        col = 0;
        current_row++;
    }

    display_status_line();

    update_cursor();
}

void TextEditor::move_cursor_left() {
    if (m_cursor_pos > 0) {
        m_cursor_pos--;
        if (m_buffer[m_cursor_pos] == '\n') {
            m_cursor_row--;
            m_cursor_col = 0;

            size_t line_start = m_cursor_pos;
            while (line_start > 0 && m_buffer[line_start - 1] != '\n') {
                line_start--;
            }

            m_cursor_col = m_cursor_pos - line_start;
        } else {
            m_cursor_col--;
        }

        update_screen_position();
    }
}

void TextEditor::move_cursor_right() {
    if (m_cursor_pos < m_buffer_size) {
        if (m_buffer[m_cursor_pos] == '\n') {
            m_cursor_row++;
            m_cursor_col = 0;
        } else
            m_cursor_col++;

        m_cursor_pos++;
        update_screen_position();
    }
}

void TextEditor::move_cursor_up() {
    size_t line_start = m_cursor_pos;
    while (line_start > 0 && m_buffer[line_start - 1] != '\n') {
        line_start--;
    }

    if (line_start == 0) return;

    size_t prev_line_start = line_start - 1;
    while (prev_line_start > 0 && m_buffer[prev_line_start - 1] != '\n') {
        prev_line_start--;
    }

    size_t target_col = m_cursor_col;
    size_t prev_line_length = line_start - prev_line_start - 1;

    if (target_col > prev_line_length) target_col = prev_line_length;

    m_cursor_pos = prev_line_start + target_col;
    m_cursor_row--;
    m_cursor_col = target_col;

    update_screen_position();
}

void TextEditor::move_cursor_down() {
    size_t line_end = m_cursor_pos;
    while (line_end < m_buffer_size && m_buffer[line_end] != '\n') {
        line_end++;
    }

    if (line_end >= m_buffer_size) return;

    size_t next_line_start = line_end + 1;

    size_t next_line_end = next_line_start;
    while (next_line_end < m_buffer_size && m_buffer[next_line_end] != '\n') {
        next_line_end++;
    }

    size_t target_col = m_cursor_col;
    size_t next_line_length = next_line_end - next_line_start;

    if (target_col > next_line_length) target_col = next_line_length;

    m_cursor_pos = next_line_start + target_col;
    m_cursor_row++;
    m_cursor_col = target_col;

    update_screen_position();
}

void TextEditor::insert_char(char c) {
    if (m_buffer_size >= MAX_BUFFER_SIZE - 1) return;

    memmove(&m_buffer[m_cursor_pos + 1], &m_buffer[m_cursor_pos], m_buffer_size - m_cursor_pos + 1);

    m_buffer[m_cursor_pos] = c;
    m_buffer_size++;

    if (c == '\n') {
        m_cursor_row++;
        m_cursor_col = 0;
    } else
        m_cursor_col++;

    m_cursor_pos++;
    m_modified = true;

    update_screen_position();
}

void TextEditor::delete_char() {
    if (m_cursor_pos >= m_buffer_size) return;

    memmove(&m_buffer[m_cursor_pos], &m_buffer[m_cursor_pos + 1], m_buffer_size - m_cursor_pos);
    m_buffer_size--;
    m_modified = true;
}

void TextEditor::backspace() {
    if (m_cursor_pos <= 0) return;

    move_cursor_left();
    delete_char();
}

void TextEditor::update_screen_position() {
    if (m_cursor_row < m_screen_row)
        m_screen_row = m_cursor_row;
    else if (m_cursor_row >= m_screen_row + TERMINAL_HEIGHT - 1)
        m_screen_row = m_cursor_row - TERMINAL_HEIGHT + 2;

    m_screen_col = m_cursor_col;

    if (m_screen_col >= TERMINAL_WIDTH) m_screen_col = TERMINAL_WIDTH - 1;

    update_cursor();
}

void TextEditor::display_status_line() {
    char status[TERMINAL_WIDTH + 1];

    char modified_indicator[12] = {0};
    if (m_modified) strcpy(modified_indicator, "[modified]");

    char mode_indicator[12] = {0};
    if (m_mode == EditorMode::NORMAL)
        strcpy(mode_indicator, "[NORMAL]");
    else
        strcpy(mode_indicator, "[INSERT]");

    strcpy(status, " ");
    strcat(status, m_filename);
    strcat(status, " ");
    strcat(status, modified_indicator);
    strcat(status, " ");
    strcat(status, mode_indicator);
    strcat(status, " | lines: ");

    char row_str[16] = {0};
    size_t row_val = m_cursor_row + 1;
    size_t i = 0;
    do {
        row_str[i++] = '0' + (row_val % 10);
        row_val /= 10;
    } while (row_val > 0);

    for (size_t j = 0; j < i / 2; j++) {
        char temp = row_str[j];
        row_str[j] = row_str[i - j - 1];
        row_str[i - j - 1] = temp;
    }

    strcat(status, row_str);
    strcat(status, " | Ctrl-S: Save | Ctrl-Q: Quit");

    for (size_t i = 0; i < TERMINAL_WIDTH; i++) {
        terminal_putchar_at(i < strlen(status) ? status[i] : ' ', STATUS_COLOR, i, STATUS_LINE);
    }

    update_cursor();
}

void TextEditor::update_cursor() {
    size_t screen_y = m_cursor_row - m_screen_row;
    size_t screen_x = m_cursor_col;

    if (screen_y >= TERMINAL_HEIGHT - 1) screen_y = TERMINAL_HEIGHT - 2;

    if (screen_x >= TERMINAL_WIDTH) screen_x = TERMINAL_WIDTH - 1;

    update_cursor_position(screen_x, screen_y);
}

void init_editor() {
    // no-op
}

void cmd_edit(const char* filename) {
    if (!filename || !*filename) {
        terminal_writestring("Usage: edit <filename>\n");
        return;
    }

    auto& fs = fs::FileSystem::instance();
    auto node = fs.get_file(filename);
    if (node && node->type == fs::FileType::Directory) {
        terminal_writestring("Cannot edit directory: ");
        terminal_writestring(filename);
        terminal_writestring("\n");
        print_prompt();
        return;
    }

    for (uint16_t y = 0; y < TERMINAL_HEIGHT; y++) {
        for (uint16_t x = 0; x < TERMINAL_WIDTH; x++) {
            terminal_putchar_at(' ', TEXT_COLOR, x, y);
        }
    }

    terminal_row = 0;
    terminal_column = 0;
    update_cursor();

    auto& editor = TextEditor::instance();
    if (!editor.open(filename)) {
        for (uint16_t y = 0; y < TERMINAL_HEIGHT; y++) {
            for (uint16_t x = 0; x < TERMINAL_WIDTH; x++) {
                terminal_putchar_at(' ', TEXT_COLOR, x, y);
            }
        }
        terminal_row = 0;
        terminal_column = 0;
        update_cursor();

        terminal_writestring("Failed to open file: ");
        terminal_writestring(filename);
        terminal_writestring("\n");

        print_prompt();
        return;
    }
}

}  // namespace editor