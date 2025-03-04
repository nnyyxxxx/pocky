#include "shell.hpp"

#include <cstdio>
#include <cstring>

#include "editor.hpp"
#include "fs/filesystem.hpp"
#include "graphics.hpp"
#include "io.hpp"
#include "lib/lib.hpp"
#include "physical_memory.hpp"
#include "printf.hpp"
#include "terminal.hpp"
#include "timer.hpp"
#include "vga.hpp"

char input_buffer[256] = {0};
size_t input_pos = 0;
bool handling_exception = false;
bool command_running = false;

char command_history[MAX_HISTORY_SIZE][256] = {0};
size_t history_count = 0;

namespace {

void split_path(const char* input, char* first, char* second) {
    const char* space = strchr(input, ' ');
    if (!space) {
        strcpy(first, input);
        second[0] = '\0';
        return;
    }

    size_t first_len = space - input;
    strncpy(first, input, first_len);
    first[first_len] = '\0';

    while (*space == ' ')
        space++;
    strcpy(second, space);
}

void print_file_type(const fs::FileNode* node) {
    if (node->type == fs::FileType::Directory)
        printf("d");
    else
        printf("f");
}

void print_file_name(const fs::FileNode* node) {
    printf(" ");
    printf(node->name);
    if (node->type == fs::FileType::Directory) printf("/");
}

void list_callback(const fs::FileNode* node) {
    print_file_type(node);
    print_file_name(node);
    printf("\n");
}

template <typename F>
void handle_wildcard_command(const char* pattern, F cmd_fn) {
    auto& fs = fs::FileSystem::instance();
    fs::FileNode* current = fs.get_current_directory();
    fs::FileNode* child = current->first_child;
    bool found = false;

    while (child) {
        if (match_wildcard(pattern, child->name)) {
            cmd_fn(child->name);
            found = true;
        }
        child = child->next_sibling;
    }

    if (!found) {
        printf("No matches found for pattern: ");
        printf(pattern);
        printf("\n");
    }
}

}  // namespace

void cmd_help() {
    printf("Available commands:\n");
    printf("  help     - Show this help message\n");
    printf("  echo     - Echo the arguments\n");
    printf("  clear    - Clear the screen\n");
    printf("  crash    - Trigger a crash (for testing)\n");
    printf("  memory   - Show memory usage\n");
    printf("  ls       - List directory contents\n");
    printf("  mkdir    - Create a new directory\n");
    printf("  cd       - Change current directory\n");
    printf("  cat      - Display file contents\n");
    printf("  cp       - Copy a file\n");
    printf("  mv       - Move/rename a file\n");
    printf("  rm       - Remove a file or directory\n");
    printf("  touch    - Create an empty file\n");
    printf("  edit     - Edit a file\n");
    printf("  history  - Show command history\n");
    printf("  uptime   - Show system uptime\n");
    printf("  shutdown - Power off the system\n");
    printf("  graphics - Enter graphics mode\n");
    printf("  count    - Count from 0 to idk\n");
    printf("  TAB      - Auto-complete a dir,file, this is not a command\n");
    command_running = false;
}

void cmd_echo(const char* args) {
    command_running = true;
    printf(args);
    printf("\n");
    command_running = false;
}

void cmd_crash() {
    command_running = true;
    handling_exception = true;
    volatile int* ptr = nullptr;
    *ptr = 0;
    command_running = false;
}

void cmd_shutdown() {
    command_running = true;
    printf("\nShutting down the system...\n");

    outb(0x604, 0x00);
    outb(0x604, 0x01);

    outb(0xB004, 0x00);
    outb(0x4004, 0x00);

    outb(0x64, 0xFE);

    printf("Shutdown failed.\n");

    asm volatile("cli");
    for (;;) {
        asm volatile("hlt");
    }
    command_running = false;
}

void cmd_memory() {
    command_running = true;
    auto& pmm = PhysicalMemoryManager::instance();

    size_t free_frames = pmm.get_free_frames();
    size_t total_frames = pmm.get_total_frames();

    if (total_frames == 0 || free_frames > total_frames) {
        printf("Error: Invalid memory state\n");
        command_running = false;
        return;
    }

    size_t used_frames = total_frames - free_frames;

    constexpr size_t max_size = static_cast<size_t>(-1);
    if (total_frames > max_size / PhysicalMemoryManager::PAGE_SIZE) {
        printf("Error: Memory size too large to represent\n");
        command_running = false;
        return;
    }

    size_t total_bytes = total_frames * PhysicalMemoryManager::PAGE_SIZE;
    size_t used_bytes = used_frames * PhysicalMemoryManager::PAGE_SIZE;

    constexpr size_t bytes_per_mb = 1024 * 1024;
    double total_mb = static_cast<double>(total_bytes) / bytes_per_mb;
    double used_mb = static_cast<double>(used_bytes) / bytes_per_mb;

    printf("%.2f MB used of available %.2f MB\n", used_mb, total_mb);

    command_running = false;
}

void cmd_ls(const char* path) {
    command_running = true;
    auto& fs = fs::FileSystem::instance();
    fs.list_directory(path, list_callback);
    command_running = false;
}

void cmd_mkdir(const char* path) {
    command_running = true;
    if (!path || !*path) {
        printf("mkdir: missing operand\n");
        command_running = false;
        return;
    }

    auto& fs = fs::FileSystem::instance();
    if (!fs.create_file(path, fs::FileType::Directory)) {
        printf("mkdir: cannot create directory '");
        printf(path);
        printf("'\n");
    }
    command_running = false;
}

void cmd_cd(const char* path) {
    command_running = true;
    auto& fs = fs::FileSystem::instance();

    if (!path || !*path) {
        fs.change_directory("/");
        command_running = false;
        return;
    }

    if (!fs.change_directory(path)) {
        printf("cd: cannot change directory to '");
        printf(path);
        printf("'\n");
    }
    command_running = false;
}

void cmd_cat(const char* path) {
    command_running = true;
    if (!path || !*path) {
        printf("cat: missing operand\n");
        command_running = false;
        return;
    }

    if (strchr(path, '*')) {
        handle_wildcard_command(path, [](const char* matched_path) {
            auto& fs = fs::FileSystem::instance();
            auto node = fs.get_file(matched_path);
            if (!node || node->type != fs::FileType::Regular) return;

            printf("==> ");
            printf(matched_path);
            printf(" <==\n");

            uint8_t buffer[1024];
            size_t remaining = node->size;
            size_t offset = 0;

            while (remaining > 0) {
                size_t to_read = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
                if (!fs.read_file(matched_path, buffer + offset, to_read)) break;

                for (size_t i = 0; i < to_read; i++) {
                    terminal_putchar(buffer[i]);
                }

                remaining -= to_read;
                offset += to_read;
            }
            printf("\n\n");
        });
        command_running = false;
        return;
    }

    auto& fs = fs::FileSystem::instance();
    auto node = fs.get_file(path);
    if (!node || node->type != fs::FileType::Regular) {
        printf("cat: cannot read '");
        printf(path);
        printf("'\n");
        command_running = false;
        return;
    }

    uint8_t buffer[1024];
    size_t remaining = node->size;
    size_t offset = 0;

    while (remaining > 0) {
        size_t to_read = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        if (!fs.read_file(path, buffer + offset, to_read)) break;

        for (size_t i = 0; i < to_read; i++) {
            terminal_putchar(buffer[i]);
        }

        remaining -= to_read;
        offset += to_read;
    }
    printf("\n");
    command_running = false;
}

void cmd_cp(const char* args) {
    command_running = true;
    char src[fs::MAX_PATH];
    char dst[fs::MAX_PATH];
    split_path(args, src, dst);

    if (!*src || !*dst) {
        printf("cp: missing operand\n");
        command_running = false;
        return;
    }

    if (strchr(src, '*')) {
        handle_wildcard_command(src, [dst](const char* matched_path) {
            auto& fs = fs::FileSystem::instance();
            auto src_node = fs.get_file(matched_path);
            if (!src_node || src_node->type != fs::FileType::Regular) return;

            char full_dst[fs::MAX_PATH];
            strcpy(full_dst, dst);
            if (dst[strlen(dst) - 1] == '/') {
                strcat(full_dst, matched_path);
            }

            auto dst_node = fs.create_file(full_dst, fs::FileType::Regular);
            if (!dst_node) {
                printf("cp: cannot create '");
                printf(full_dst);
                printf("'\n");
                return;
            }

            if (!fs.write_file(full_dst, src_node->data, src_node->size)) {
                printf("cp: write error\n");
                fs.delete_file(full_dst);
            }
        });
        command_running = false;
        return;
    }

    auto& fs = fs::FileSystem::instance();
    auto src_node = fs.get_file(src);
    if (!src_node || src_node->type != fs::FileType::Regular) {
        printf("cp: cannot read '");
        printf(src);
        printf("'\n");
        command_running = false;
        return;
    }

    auto dst_node = fs.create_file(dst, fs::FileType::Regular);
    if (!dst_node) {
        printf("cp: cannot create '");
        printf(dst);
        printf("'\n");
        command_running = false;
        return;
    }

    if (!fs.write_file(dst, src_node->data, src_node->size)) {
        printf("cp: write error\n");
        fs.delete_file(dst);
    }
    command_running = false;
}

void cmd_mv(const char* args) {
    command_running = true;
    char src[fs::MAX_PATH];
    char dst[fs::MAX_PATH];
    split_path(args, src, dst);

    if (!*src || !*dst) {
        printf("mv: missing operand\n");
        command_running = false;
        return;
    }

    if (strchr(src, '*')) {
        handle_wildcard_command(src, [dst](const char* matched_path) {
            auto& fs = fs::FileSystem::instance();
            auto src_node = fs.get_file(matched_path);
            if (!src_node) return;

            char full_dst[fs::MAX_PATH];
            strcpy(full_dst, dst);
            if (dst[strlen(dst) - 1] == '/') {
                strcat(full_dst, matched_path);
            }

            if (src_node->type == fs::FileType::Regular) {
                auto dst_node = fs.create_file(full_dst, fs::FileType::Regular);
                if (!dst_node) {
                    printf("mv: cannot create '");
                    printf(full_dst);
                    printf("'\n");
                    return;
                }

                if (!fs.write_file(full_dst, src_node->data, src_node->size)) {
                    printf("mv: write error\n");
                    fs.delete_file(full_dst);
                    return;
                }

                fs.delete_file(matched_path);
            }
        });
        command_running = false;
        return;
    }

    auto& fs = fs::FileSystem::instance();
    auto src_node = fs.get_file(src);
    if (!src_node) {
        printf("mv: cannot stat '");
        printf(src);
        printf("'\n");
        command_running = false;
        return;
    }

    if (src_node->type == fs::FileType::Regular) {
        cmd_cp(args);
        if (!fs.delete_file(src)) {
            printf("mv: cannot remove '");
            printf(src);
            printf("'\n");
        }
    } else
        printf("mv: directory move not supported\n");
    command_running = false;
}

void cmd_rm(const char* path) {
    command_running = true;
    if (!path || !*path) {
        printf("rm: missing operand\n");
        command_running = false;
        return;
    }

    if (strchr(path, '*')) {
        handle_wildcard_command(path, [](const char* matched_path) {
            auto& fs = fs::FileSystem::instance();
            if (!fs.delete_file(matched_path)) {
                printf("rm: cannot remove '");
                printf(matched_path);
                printf("'\n");
            }
        });
        command_running = false;
        return;
    }

    auto& fs = fs::FileSystem::instance();
    if (!fs.delete_file(path)) {
        printf("rm: cannot remove '");
        printf(path);
        printf("'\n");
    }
    command_running = false;
}

void cmd_touch(const char* path) {
    if (!path) {
        printf("Usage: touch <path>\n");
        return;
    }

    auto& fs = fs::FileSystem::instance();
    auto* file = fs.create_file(path, fs::FileType::Regular);

    if (!file) {
        printf("Failed to create file: ");
        printf(path);
        printf("\n");
    }
}

void cmd_edit(const char* path) {
    command_running = true;
    editor::cmd_edit(path);
    command_running = false;
}

void cmd_history() {
    command_running = true;

    for (size_t i = 0; i < history_count; i++) {
        print_number(i + 1, 10, 4, false);
        printf("  ");
        printf(command_history[i]);
        printf("\n");
    }

    command_running = false;
}

void cmd_uptime() {
    command_running = true;

    char uptime_str[100] = {0};
    format_uptime(uptime_str, sizeof(uptime_str));

    printf("System uptime: ");
    printf(uptime_str);
    printf("\n");

    command_running = false;
}

void cmd_graphics() {
    command_running = true;
    for (volatile int i = 0; i < 1000000; i++)
        ;

    enter_graphics_mode();
    command_running = false;
}

void cmd_count() {
    command_running = true;
    size_t count = 0;

    while (command_running) {
        printf("Count: %zu\n", count++);
        for (volatile size_t i = 0; i < 100000000; i++)
            ;
    }
}

void interrupt_command() {
    if (command_running) {
        printf("\nCommand interrupted\n");
        command_running = false;
    }
}

void handle_redirection(const char* command) {
    const char* redirect = strchr(command, '>');
    if (!redirect) return;

    char text[256] = {0};
    char filename[fs::MAX_PATH] = {0};

    size_t text_len = redirect - command;
    strncpy(text, command, text_len);

    const char* file_start = redirect + 1;
    while (*file_start == ' ')
        file_start++;

    strcpy(filename, file_start);
    char* end = filename + strlen(filename) - 1;
    while (end > filename && *end == ' ')
        *end-- = '\0';

    if (!*filename) {
        printf("No output file specified\n");
        return;
    }

    auto& fs = fs::FileSystem::instance();
    auto* file = fs.create_file(filename, fs::FileType::Regular);
    if (!file) {
        printf("Failed to create file: ");
        printf(filename);
        printf("\n");
        return;
    }

    char* text_end = text + strlen(text) - 1;
    while (text_end > text && *text_end == ' ')
        *text_end-- = '\0';

    const uint8_t* data = reinterpret_cast<const uint8_t*>(text);
    fs.write_file(filename, data, strlen(text));

    memset(input_buffer, 0, sizeof(input_buffer));
    input_pos = 0;
}

void print_prompt() {
    auto& fs = fs::FileSystem::instance();
    printf(fs.get_current_path());
    printf(" $ ");
}

void handle_tab_completion() {
    char* last_word = input_buffer;
    char* space = strrchr(input_buffer, ' ');
    if (space) last_word = space + 1;

    size_t word_len = strlen(last_word);
    if (word_len == 0) return;

    char command[256] = {0};
    if (space) {
        size_t cmd_len = space - input_buffer;
        strncpy(command, input_buffer, cmd_len);
    } else
        strcpy(command, input_buffer);

    auto& fs = fs::FileSystem::instance();
    fs::FileNode* current = fs.get_current_directory();
    fs::FileNode* child = current->first_child;

    const char* match = nullptr;
    size_t matches = 0;

    while (child) {
        if (strncmp(child->name, last_word, word_len) == 0) {
            if (strcmp(command, "cd") == 0 && child->type != fs::FileType::Directory) {
                child = child->next_sibling;
                continue;
            }
            match = child->name;
            matches++;
        }
        child = child->next_sibling;
    }

    if (matches == 1) {
        size_t pos = last_word - input_buffer;
        const char* completion = match + word_len;
        strcpy(input_buffer + pos + word_len, completion);
        input_pos = strlen(input_buffer);
        printf(completion);
    }
}

void process_keypress(char c) {
    auto& editor = editor::TextEditor::instance();

    if (editor.is_active()) {
        editor.process_keypress(c);
        return;
    }

    if (c == '\t') {
        handle_tab_completion();
        return;
    }

    if (c == '\n') {
        terminal_putchar('\n');
        process_command();
        return;
    }

    if (c == '\b') {
        if (input_pos > 0) {
            input_pos--;
            input_buffer[input_pos] = '\0';
            terminal_column--;
            terminal_putchar_at(' ', terminal_color, terminal_column, terminal_row);
            update_cursor();
        }
        return;
    }

    if (input_pos < sizeof(input_buffer) - 1) {
        input_buffer[input_pos++] = c;
        input_buffer[input_pos] = '\0';
        terminal_putchar(c);
    }
}

void process_command() {
    if (handling_exception) {
        printf("\n");
        print_prompt();
        handling_exception = false;
        return;
    }

    if (input_buffer[0] != '\0') {
        if (history_count < MAX_HISTORY_SIZE) {
            strcpy(command_history[history_count], input_buffer);
            history_count++;
        } else {
            memmove(command_history[0], command_history[1], (MAX_HISTORY_SIZE - 1) * 256);
            strcpy(command_history[MAX_HISTORY_SIZE - 1], input_buffer);
        }
    }

    if (strchr(input_buffer, '>')) {
        handle_redirection(input_buffer);
        print_prompt();
        return;
    }

    char* args = strchr(input_buffer, ' ');
    if (args) {
        *args = '\0';
        args++;
        while (*args == ' ')
            args++;
    }

    if (strcmp(input_buffer, "help") == 0)
        cmd_help();
    else if (strcmp(input_buffer, "echo") == 0)
        cmd_echo(args);
    else if (strcmp(input_buffer, "clear") == 0)
        terminal_clear();
    else if (strcmp(input_buffer, "crash") == 0)
        cmd_crash();
    else if (strcmp(input_buffer, "memory") == 0)
        cmd_memory();
    else if (strcmp(input_buffer, "ls") == 0)
        cmd_ls(args);
    else if (strcmp(input_buffer, "mkdir") == 0)
        cmd_mkdir(args);
    else if (strcmp(input_buffer, "cd") == 0)
        cmd_cd(args);
    else if (strcmp(input_buffer, "cat") == 0)
        cmd_cat(args);
    else if (strcmp(input_buffer, "cp") == 0)
        cmd_cp(args);
    else if (strcmp(input_buffer, "mv") == 0)
        cmd_mv(args);
    else if (strcmp(input_buffer, "rm") == 0)
        cmd_rm(args);
    else if (strcmp(input_buffer, "touch") == 0)
        cmd_touch(args);
    else if (strcmp(input_buffer, "edit") == 0) {
        char filename[256];
        if (args)
            strcpy(filename, args);
        else
            filename[0] = '\0';

        memset(input_buffer, 0, sizeof(input_buffer));
        input_pos = 0;

        cmd_edit(filename);

        return;
    } else if (strcmp(input_buffer, "history") == 0)
        cmd_history();
    else if (strcmp(input_buffer, "uptime") == 0)
        cmd_uptime();
    else if (strcmp(input_buffer, "shutdown") == 0 || strcmp(input_buffer, "poweroff") == 0)
        cmd_shutdown();
    else if (strcmp(input_buffer, "graphics") == 0)
        cmd_graphics();
    else if (strcmp(input_buffer, "count") == 0)
        cmd_count();
    else if (input_buffer[0] != '\0') {
        printf("Unknown command: ");
        printf(input_buffer);
        printf("\n");
    }

    if (!handling_exception) {
        print_prompt();
        memset(input_buffer, 0, sizeof(input_buffer));
        input_pos = 0;
    }
}

void init_shell() {
    memset(input_buffer, 0, sizeof(input_buffer));
    memset(command_history, 0, sizeof(command_history));
    input_pos = 0;
    history_count = 0;
    handling_exception = false;
    command_running = false;
    printf("\n");
    print_prompt();
    editor::init_editor();
}