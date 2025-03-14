#include "editor.hpp"

#include <cstdio>
#include <cstring>

#include "fs/fat32.hpp"
#include "io.hpp"
#include "printf.hpp"
#include "screen_state.hpp"
#include "shell.hpp"
#include "terminal.hpp"
#include "vga.hpp"

namespace editor {

namespace {
constexpr uint8_t STATUS_COLOR = 0x70;
constexpr uint8_t TEXT_COLOR = 0x07;
constexpr uint8_t LINE_NUMBER_CURRENT_COLOR = 0x0F;
constexpr uint8_t LINE_NUMBER_COLOR = 0x08;
constexpr size_t TERMINAL_WIDTH = 80;
constexpr size_t TERMINAL_HEIGHT = 25;
constexpr size_t STATUS_LINE = TERMINAL_HEIGHT - 1;
constexpr size_t LINE_NUMBER_WIDTH = 3;

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

size_t TextEditor::get_line_length(size_t row) const {
    size_t pos = 0;
    size_t current_row = 0;

    while (pos < m_buffer_size) {
        if (current_row == row) {
            size_t len = 0;
            while (pos + len < m_buffer_size && m_buffer[pos + len] != '\n')
                len++;
            return len;
        }

        if (m_buffer[pos] == '\n') current_row++;
        pos++;
    }

    return static_cast<size_t>(-1);
}

void TextEditor::update_cursor_pos() {
    size_t pos = 0;
    size_t current_row = 0;
    size_t current_col = 0;

    while (pos < m_buffer_size) {
        if (current_row == m_cursor_row && current_col == m_cursor_col) {
            m_cursor_pos = pos;
            return;
        }

        if (m_buffer[pos] == '\n') {
            current_row++;
            current_col = 0;
        } else
            current_col++;

        pos++;
    }

    m_cursor_pos = pos;
}

bool TextEditor::open(const char* filename) {
    if (!filename || !*filename) return false;

    strncpy(m_filename, filename, MAX_FILENAME_LENGTH - 1);
    m_filename[MAX_FILENAME_LENGTH - 1] = '\0';

    auto& fs = fs::CFat32FileSystem::instance();
    uint32_t cluster, size;
    uint8_t attributes;
    bool file_exists = fs.findFile(filename, cluster, size, attributes);

    if (!file_exists) {
        if (!fs.createFile(filename, 0)) return false;
        m_buffer_size = 0;
        m_buffer[0] = '\0';
    } else {
        if (attributes & 0x10) return false;
        if (size >= MAX_BUFFER_SIZE) return false;

        m_buffer_size = size;
        if (m_buffer_size > 0) {
            uint8_t* file_data = new uint8_t[m_buffer_size];
            fs.readFile(cluster, file_data, m_buffer_size);
            memcpy(m_buffer, file_data, m_buffer_size);
            delete[] file_data;
        }
        m_buffer[m_buffer_size] = '\0';
    }

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
    if (!m_active) return false;

    while (m_buffer_size > 0 && m_buffer[m_buffer_size - 1] == '\n') {
        m_buffer_size--;
    }

    auto& fs = fs::CFat32FileSystem::instance();
    uint32_t cluster, size;
    uint8_t attributes;
    if (!fs.findFile(m_filename, cluster, size, attributes))
        if (!fs.createFile(m_filename, 0)) return false;

    if (!fs.writeFile(cluster, reinterpret_cast<uint8_t*>(m_buffer), m_buffer_size)) return false;

    if (!fs.updateFileSize(m_filename, m_buffer_size)) return false;

    m_modified = false;

    if (m_cursor_pos > m_buffer_size) {
        m_cursor_pos = m_buffer_size;
        update_cursor_pos();
    }

    render();
    return true;
}

void TextEditor::close() {
    if (!m_active) return;

    m_active = false;

    outb(VGA_CTRL_PORT, 0x0A);
    outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xC0) | CURSOR_INSERT_START);
    outb(VGA_CTRL_PORT, 0x0B);
    outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xE0) | CURSOR_INSERT_END);

    screen_state::restore();

    m_buffer_size = 0;
    m_cursor_pos = 0;
    m_cursor_row = 0;
    m_cursor_col = 0;
    m_screen_row = 0;
    m_screen_col = 0;
    m_screen_offset = 0;
    m_mode = EditorMode::NORMAL;
    m_filename[0] = '\0';
    m_modified = false;

    print_prompt();
}

void TextEditor::process_keypress(char c) {
    if (!m_active) return;

    if (c == 0x1B) {
        m_mode = EditorMode::NORMAL;
        outb(VGA_CTRL_PORT, 0x0A);
        outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xC0) | CURSOR_NORMAL_START);
        outb(VGA_CTRL_PORT, 0x0B);
        outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xE0) | CURSOR_NORMAL_END);
        render();
        return;
    }

    if (m_mode == EditorMode::NORMAL) {
        if (c == 'i') {
            m_mode = EditorMode::INSERT;
            outb(VGA_CTRL_PORT, 0x0A);
            outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xC0) | CURSOR_INSERT_START);
            outb(VGA_CTRL_PORT, 0x0B);
            outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xE0) | CURSOR_INSERT_END);
            render();
            return;
        }

        if (c == 'a') {
            m_mode = EditorMode::INSERT;
            outb(VGA_CTRL_PORT, 0x0A);
            outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xC0) | CURSOR_INSERT_START);
            outb(VGA_CTRL_PORT, 0x0B);
            outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xE0) | CURSOR_INSERT_END);
            render();
            return;
        }

        if (is_ctrl_key(c, 'S')) {
            if (save())
                display_status_line();
            else {
                char status[TERMINAL_WIDTH + 1] = "ERROR: Failed to save file";
                for (size_t i = 0; i < TERMINAL_WIDTH; i++) {
                    terminal_putchar_at(i < strlen(status) ? status[i] : ' ', 0x4F, i, STATUS_LINE);
                }
                update_cursor();
            }
            return;
        }

        if (is_ctrl_key(c, 'Q')) {
            close();
            return;
        }

        if (c == 'h' && m_cursor_col > 0) {
            m_cursor_col--;
            m_cursor_pos--;
            render();
            return;
        }

        if (c == 'l' && m_cursor_col < get_line_length(m_cursor_row)) {
            m_cursor_col++;
            m_cursor_pos++;
            render();
            return;
        }

        if (c == 'k' && m_cursor_row > 0) {
            size_t prev_line_length = get_line_length(m_cursor_row - 1);
            if (prev_line_length != static_cast<size_t>(-1)) {
                m_cursor_row--;
                m_cursor_col = m_cursor_col > prev_line_length ? prev_line_length : m_cursor_col;
                update_cursor_pos();

                if (m_cursor_row < m_screen_row) m_screen_row = m_cursor_row;

                render();
            }
            return;
        }

        if (c == 'j') {
            size_t next_line_length = get_line_length(m_cursor_row + 1);
            if (next_line_length != static_cast<size_t>(-1)) {
                m_cursor_row++;
                m_cursor_col = m_cursor_col > next_line_length ? next_line_length : m_cursor_col;
                update_cursor_pos();

                if (m_cursor_row >= m_screen_row + TERMINAL_HEIGHT - 1) {
                    m_screen_row = m_cursor_row - TERMINAL_HEIGHT + 2;
                }

                render();
            }
            return;
        }

        return;
    }

    if (m_mode == EditorMode::INSERT) {
        if (is_ctrl_key(c, 'S')) {
            if (save())
                display_status_line();
            else {
                char status[TERMINAL_WIDTH + 1] = "ERROR: Failed to save file";
                for (size_t i = 0; i < TERMINAL_WIDTH; i++) {
                    terminal_putchar_at(i < strlen(status) ? status[i] : ' ', 0x4F, i, STATUS_LINE);
                }
                update_cursor();
            }
            return;
        }

        if (is_ctrl_key(c, 'Q')) {
            close();
            return;
        }

        if (c == '\b' && m_cursor_pos > 0) {
            bool deleting_newline = (m_cursor_pos > 0 && m_buffer[m_cursor_pos - 1] == '\n');

            size_t prev_line_length = 0;
            if (deleting_newline) {
                size_t prev_line_start = m_cursor_pos - 2;
                while (prev_line_start > 0 && m_buffer[prev_line_start] != '\n') {
                    prev_line_start--;
                }
                if (m_buffer[prev_line_start] == '\n') prev_line_start++;
                prev_line_length = (m_cursor_pos - 1) - prev_line_start;
            }

            memmove(&m_buffer[m_cursor_pos - 1], &m_buffer[m_cursor_pos],
                    m_buffer_size - m_cursor_pos);
            m_buffer_size--;
            m_cursor_pos--;

            if (deleting_newline) {
                m_cursor_row--;
                m_cursor_col = prev_line_length;
            } else if (m_cursor_col > 0)
                m_cursor_col--;

            m_modified = true;
            render();
            return;
        }

        if (c >= 32 && c < 127 && m_buffer_size < MAX_BUFFER_SIZE - 1) {
            memmove(&m_buffer[m_cursor_pos + 1], &m_buffer[m_cursor_pos],
                    m_buffer_size - m_cursor_pos);
            m_buffer[m_cursor_pos] = c;
            m_buffer_size++;
            m_cursor_pos++;
            m_cursor_col++;
            m_modified = true;
            render();
            return;
        }

        if ((c == '\r' || c == '\n') && m_buffer_size < MAX_BUFFER_SIZE - 1) {
            memmove(&m_buffer[m_cursor_pos + 1], &m_buffer[m_cursor_pos],
                    m_buffer_size - m_cursor_pos);
            m_buffer[m_cursor_pos] = '\n';
            m_buffer_size++;
            m_cursor_pos++;
            m_cursor_row++;
            m_cursor_col = 0;
            m_modified = true;
            if (m_cursor_row >= m_screen_row + TERMINAL_HEIGHT - 1)
                m_screen_row = m_cursor_row - TERMINAL_HEIGHT + 2;
            render();
            return;
        }
    }
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
    size_t col = LINE_NUMBER_WIDTH + 1;
    size_t abs_row = m_screen_row;

    for (size_t y = 0; y < visible_rows; y++) {
        bool has_content = false;
        size_t check_pos = line_start;
        size_t check_row = 0;

        while (check_pos < m_buffer_size && check_row < y) {
            if (m_buffer[check_pos] == '\n') check_row++;
            check_pos++;
            if (check_pos >= m_buffer_size) break;
        }

        has_content = (check_pos < m_buffer_size && check_row == y) || (abs_row == m_cursor_row);

        if (has_content) {
            uint8_t color =
                (abs_row == m_cursor_row) ? LINE_NUMBER_CURRENT_COLOR : LINE_NUMBER_COLOR;

            if (abs_row < 999) {
                size_t line_num = abs_row + 1;

                size_t digits = 0;
                size_t temp = line_num;
                do {
                    temp /= 10;
                    digits++;
                } while (temp > 0);

                for (size_t i = 0; i < LINE_NUMBER_WIDTH - digits; i++) {
                    terminal_putchar_at(' ', color, i, y);
                }

                for (size_t i = 0; i < digits; i++) {
                    char digit = '0' + (line_num % 10);
                    line_num /= 10;
                    terminal_putchar_at(digit, color, LINE_NUMBER_WIDTH - i - 1, y);
                }
            }
        } else {
            for (size_t i = 0; i < LINE_NUMBER_WIDTH; i++) {
                terminal_putchar_at(' ', TEXT_COLOR, i, y);
            }
        }

        terminal_putchar_at(' ', TEXT_COLOR, LINE_NUMBER_WIDTH, y);

        abs_row++;
    }

    current_row = 0;
    col = LINE_NUMBER_WIDTH + 1;

    for (size_t i = line_start; i < m_buffer_size && current_row < visible_rows; i++) {
        if (m_buffer[i] == '\n') {
            while (col < TERMINAL_WIDTH) {
                terminal_putchar_at(' ', TEXT_COLOR, col, current_row);
                col++;
            }
            current_row++;
            col = LINE_NUMBER_WIDTH + 1;
        } else {
            terminal_putchar_at(m_buffer[i], TEXT_COLOR, col, current_row);
            col++;
            if (col >= TERMINAL_WIDTH) {
                col = LINE_NUMBER_WIDTH + 1;
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
        col = LINE_NUMBER_WIDTH + 1;
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
        } else
            m_cursor_col--;

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
    size_t screen_x = m_cursor_col + LINE_NUMBER_WIDTH + 1;

    if (screen_y >= TERMINAL_HEIGHT - 1) screen_y = TERMINAL_HEIGHT - 2;

    if (screen_x >= TERMINAL_WIDTH) screen_x = TERMINAL_WIDTH - 1;

    update_cursor_position(screen_x, screen_y);
}

void init_editor() {
    // no-op
}

void cmd_edit(const char* filename) {
    if (!filename || !*filename) {
        printf("Usage: edit <filename>\n");
        print_prompt();
        return;
    }

    screen_state::save();

    terminal_row = 0;
    terminal_column = 0;
    update_cursor();

    auto& editor = TextEditor::instance();
    if (!editor.open(filename)) {
        screen_state::restore();
        printf("Failed to open file: ");
        printf(filename);
        printf("\n");
        print_prompt();
    }
}

}  // namespace editor