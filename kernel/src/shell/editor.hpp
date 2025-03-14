#pragma once
#include <cstddef>
#include <cstdint>

namespace editor {

constexpr size_t MAX_BUFFER_SIZE = 32768;
constexpr size_t MAX_FILENAME_LENGTH = 256;

enum class EditorMode { NORMAL, INSERT, VISUAL };

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
    size_t get_line_length(size_t row) const;
    void update_cursor_pos();

    void process_normal_mode(char c);
    void process_insert_mode(char c);
    void process_visual_mode(char c);
    void switch_to_normal_mode();
    void switch_to_insert_mode();
    void switch_to_visual_mode();

    bool is_line_selected(size_t row) const;

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

    bool m_has_selection = false;
    size_t m_selection_start_row = 0;
    size_t m_selection_end_row = 0;
};

void cmd_edit(const char* filename);

}  // namespace editor