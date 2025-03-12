#pragma once
#include <cstddef>
#include <cstdint>

namespace editor {

constexpr size_t MAX_BUFFER_SIZE = 16384;
constexpr size_t MAX_FILENAME_LENGTH = 256;

enum class EditorMode { NORMAL, INSERT };

class TextEditor {
public:
    static TextEditor& instance();

    bool open(const char* filename);
    bool save();
    void close();
    void process_keypress(char c);
    void render();
    bool is_active() const {
        return m_active;
    }
    EditorMode get_mode() const {
        return m_mode;
    }

private:
    TextEditor() = default;
    ~TextEditor() = default;
    TextEditor(const TextEditor&) = delete;
    TextEditor& operator=(const TextEditor&) = delete;

    void move_cursor_left();
    void move_cursor_right();
    void move_cursor_up();
    void move_cursor_down();
    void insert_char(char c);
    void delete_char();
    void backspace();
    void update_screen_position();
    void display_status_line();
    void update_cursor();

    void process_normal_mode(char c);
    void process_insert_mode(char c);
    void switch_to_normal_mode();
    void switch_to_insert_mode();

    char m_buffer[MAX_BUFFER_SIZE] = {0};
    char m_filename[MAX_FILENAME_LENGTH] = {0};
    size_t m_buffer_size = 0;
    size_t m_cursor_pos = 0;
    size_t m_cursor_row = 0;
    size_t m_cursor_col = 0;
    size_t m_screen_row = 0;
    size_t m_screen_col = 0;
    size_t m_screen_offset = 0;
    bool m_active = false;
    bool m_modified = false;
    EditorMode m_mode = EditorMode::NORMAL;
};

void init_editor();
void cmd_edit(const char* filename);

}  // namespace editor