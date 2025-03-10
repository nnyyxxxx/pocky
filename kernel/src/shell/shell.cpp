#include "shell.hpp"

#include <cstdio>
#include <cstring>

#include "core/ipc.hpp"
#include "core/process.hpp"
#include "core/scheduler.hpp"
#include "core/types.hpp"
#include "drivers/keyboard.hpp"
#include "editor.hpp"
#include "fs/filesystem.hpp"
#include "fs/users.hpp"
#include "graphics.hpp"
#include "io.hpp"
#include "lib/lib.hpp"
#include "lib/string.hpp"
#include "lib/vector.hpp"
#include "memory/heap.hpp"
#include "pager.hpp"
#include "physical_memory.hpp"
#include "printf.hpp"
#include "rtc.hpp"
#include "screen_state.hpp"
#include "terminal.hpp"
#include "timer.hpp"
#include "vga.hpp"

uint8_t saved_terminal_color = 0;

void set_red() {
    saved_terminal_color = terminal_color;
    terminal_color = vga_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
}

void set_green() {
    saved_terminal_color = terminal_color;
    terminal_color = vga_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
}

void set_blue() {
    saved_terminal_color = terminal_color;
    terminal_color = vga_color(VGA_COLOR_BLUE, VGA_COLOR_BLACK);
}

void set_gray() {
    saved_terminal_color = terminal_color;
    terminal_color = vga_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void set_white() {
    saved_terminal_color = terminal_color;
    terminal_color = vga_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

void reset_color() {
    terminal_color = saved_terminal_color;
}

char input_buffer[256] = {0};
size_t input_pos = 0;
pid_t shell_pid = 0;

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
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("help", shell_pid);
    const char* help_text = "Available commands:\n\n"
                            "  help     - Display this help message\n"
                            "  echo     - Echo arguments\n"
                            "  clear    - Clear the screen\n"
                            "  crash    - Trigger a kernel panic (for testing)\n"
                            "  shutdown - Power off the system\n"
                            "  memory   - Display memory usage information\n"
                            "  ls       - List directory contents\n"
                            "  mkdir    - Create a new directory\n"
                            "  cd       - Change current directory\n"
                            "  cat      - Display file contents\n"
                            "  cp       - Copy a file\n"
                            "  mv       - Move or rename a file\n"
                            "  rm       - Remove a file or directory\n"
                            "  touch    - Create an empty file\n"
                            "  edit     - Edit a file\n"
                            "  history  - Display command history\n"
                            "  uptime   - Display system uptime\n"
                            "  time     - Display system time\n"
                            "  graphics - Enter graphics mode\n"
                            "  ps       - List running processes\n"
                            "  pkill    - Kill a process\n"
                            "  ipctest  - Run IPC test\n"
                            "  useradd  - Add a new user\n"
                            "  userremove - Remove a user\n"
                            "  su       - Switch to a different user\n"
                            "  whoami   - Show current user\n";

    pager::show_text(help_text);

    pm.terminate_process(pid);
}

void cmd_echo(const char* args) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("echo", shell_pid);

    printf(args);
    printf("\n");

    pm.terminate_process(pid);
}

void cmd_crash() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("crash", shell_pid);

    asm volatile("ud2");

    pm.terminate_process(pid);
}

void cmd_shutdown() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("shutdown", shell_pid);

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

    pm.terminate_process(pid);
}

void cmd_memory() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("memory", shell_pid);

    auto& pmm = PhysicalMemoryManager::instance();

    size_t free_frames = pmm.get_free_frames();
    size_t total_frames = pmm.get_total_frames();

    if (total_frames == 0 || free_frames > total_frames) {
        set_red();
        printf("Error: Invalid memory state\n");
        reset_color();
        pm.terminate_process(pid);
        return;
    }

    size_t used_frames = total_frames - free_frames;

    constexpr size_t max_size = static_cast<size_t>(-1);
    if (total_frames > max_size / PhysicalMemoryManager::PAGE_SIZE) {
        set_red();
        printf("Error: Memory size too large to represent\n");
        reset_color();
        pm.terminate_process(pid);
        return;
    }

    size_t total_bytes = total_frames * PhysicalMemoryManager::PAGE_SIZE;
    size_t used_bytes = used_frames * PhysicalMemoryManager::PAGE_SIZE;

    constexpr size_t bytes_per_mb = 1024 * 1024;
    double total_mb = static_cast<double>(total_bytes) / bytes_per_mb;
    double used_mb = static_cast<double>(used_bytes) / bytes_per_mb;

    printf("%.2f MB used of available %.2f MB\n", used_mb, total_mb);

    pm.terminate_process(pid);
}

void cmd_ls(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("ls", shell_pid);

    auto& fs = fs::FileSystem::instance();
    fs.list_directory(path, list_callback);

    pm.terminate_process(pid);
}

void cmd_mkdir(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("mkdir", shell_pid);

    if (!path || !*path) {
        printf("mkdir: missing operand\n");
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::FileSystem::instance();
    if (!fs.create_file(path, fs::FileType::Directory)) {
        printf("mkdir: cannot create directory '");
        printf(path);
        printf("'\n");
    }

    pm.terminate_process(pid);
}

void cmd_cd(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("cd", shell_pid);

    auto& fs = fs::FileSystem::instance();

    if (!path || !*path) {
        fs.change_directory("/");
        pm.terminate_process(pid);
        return;
    }

    if (!fs.change_directory(path)) {
        printf("cd: cannot change directory to '");
        printf(path);
        printf("'\n");
    }

    pm.terminate_process(pid);
}

void cmd_cat(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("cat", shell_pid);

    if (!path || !*path) {
        printf("cat: missing operand\n");
        pm.terminate_process(pid);
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
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::FileSystem::instance();
    auto node = fs.get_file(path);
    if (!node || node->type != fs::FileType::Regular) {
        printf("cat: cannot read '");
        printf(path);
        printf("'\n");
        pm.terminate_process(pid);
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

    pm.terminate_process(pid);
}

void cmd_cp(const char* args) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("cp", shell_pid);

    char src[fs::MAX_PATH];
    char dst[fs::MAX_PATH];
    split_path(args, src, dst);

    if (!*src || !*dst) {
        set_red();
        printf("cp: missing operand\n");
        reset_color();
        pm.terminate_process(pid);
        return;
    }

    if (strchr(src, '*')) {
        handle_wildcard_command(src, [dst](const char* matched_path) {
            auto& fs = fs::FileSystem::instance();
            auto src_node = fs.get_file(matched_path);
            if (!src_node || src_node->type != fs::FileType::Regular) return;

            char full_dst[fs::MAX_PATH];
            strcpy(full_dst, dst);
            if (dst[strlen(dst) - 1] == '/') strcat(full_dst, matched_path);

            auto dst_node = fs.create_file(full_dst, fs::FileType::Regular);
            if (!dst_node) {
                set_red();
                printf("cp: cannot create '%s'\n", full_dst);
                reset_color();
                return;
            }

            if (!fs.write_file(full_dst, src_node->data, src_node->size)) {
                set_red();
                printf("cp: write error\n");
                reset_color();
                fs.delete_file(full_dst);
            }
        });
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::FileSystem::instance();
    auto src_node = fs.get_file(src);
    if (!src_node || src_node->type != fs::FileType::Regular) {
        set_red();
        printf("cp: cannot read '%s'\n", src);
        reset_color();
        pm.terminate_process(pid);
        return;
    }

    auto dst_node = fs.create_file(dst, fs::FileType::Regular);
    if (!dst_node) {
        set_red();
        printf("cp: cannot create '%s'\n", dst);
        reset_color();
        pm.terminate_process(pid);
        return;
    }

    if (!fs.write_file(dst, src_node->data, src_node->size)) {
        set_red();
        printf("cp: write error\n");
        reset_color();
        fs.delete_file(dst);
    }

    pm.terminate_process(pid);
}

void cmd_mv(const char* args) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("mv", shell_pid);

    char src[fs::MAX_PATH];
    char dst[fs::MAX_PATH];
    split_path(args, src, dst);

    if (!*src || !*dst) {
        printf("mv: missing operand\n");
        pm.terminate_process(pid);
        return;
    }

    if (strchr(src, '*')) {
        handle_wildcard_command(src, [dst](const char* matched_path) {
            auto& fs = fs::FileSystem::instance();
            auto src_node = fs.get_file(matched_path);
            if (!src_node) return;

            char full_dst[fs::MAX_PATH];
            strcpy(full_dst, dst);
            if (dst[strlen(dst) - 1] == '/') strcat(full_dst, matched_path);

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
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::FileSystem::instance();
    auto src_node = fs.get_file(src);
    if (!src_node) {
        printf("mv: cannot stat '");
        printf(src);
        printf("'\n");
        pm.terminate_process(pid);
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

    pm.terminate_process(pid);
}

void cmd_rm(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("rm", shell_pid);

    if (!path || !*path) {
        printf("rm: missing operand\n");
        pm.terminate_process(pid);
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
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::FileSystem::instance();
    if (!fs.delete_file(path)) {
        printf("rm: cannot remove '");
        printf(path);
        printf("'\n");
    }

    pm.terminate_process(pid);
}

void cmd_touch(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("touch", shell_pid);

    if (!path) {
        printf("Usage: touch <path>\n");
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::FileSystem::instance();
    auto* file = fs.create_file(path, fs::FileType::Regular);

    if (!file) {
        printf("Failed to create file: ");
        printf(path);
        printf("\n");
    }

    pm.terminate_process(pid);
}

void cmd_edit(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("edit", shell_pid);

    editor::cmd_edit(path);

    pm.terminate_process(pid);
}

void cmd_history() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("history", shell_pid);

    for (size_t i = 0; i < history_count; i++) {
        print_number(i + 1, 10, 4, false);
        printf("  ");
        printf(command_history[i]);
        printf("\n");
    }

    pm.terminate_process(pid);
}

void cmd_uptime() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("uptime", shell_pid);

    char uptime_str[100] = {0};
    format_uptime(uptime_str, sizeof(uptime_str));

    printf("System uptime: ");
    printf(uptime_str);
    printf("\n");

    pm.terminate_process(pid);
}

void cmd_time() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("time", shell_pid);

    RTCTime time = get_rtc_time();
    printf("Current UTC time: %02u:%02u:%02u\n", time.hours, time.minutes, time.seconds);

    pm.terminate_process(pid);
}

void cmd_graphics() {
    for (volatile int i = 0; i < 1000000; i++)
        ;

    enter_graphics_mode();

    // never reached
    in_graphics_mode = false;
}

void cmd_ps() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("ps", shell_pid);

    printf("  PID  PPID  STATE    NAME\n");

    kernel::Process* current = pm.get_first_process();
    while (current) {
        printf("%5d %5d  ", current->pid, current->ppid);

        switch (current->state) {
            case kernel::ProcessState::Running:
                printf("Running  ");
                break;
            case kernel::ProcessState::Stopped:
                printf("Stopped  ");
                break;
            case kernel::ProcessState::Zombie:
                printf("Zombie   ");
                break;
            case kernel::ProcessState::Ready:
                printf("Ready    ");
                break;
            case kernel::ProcessState::Waiting:
                printf("Waiting  ");
                break;
        }

        printf(current->name);
        printf("\n");

        current = current->next;
    }

    pm.terminate_process(pid);
}

void cmd_pkill(const char* args) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("pkill", shell_pid);

    if (!args) {
        printf("pkill: missing process id\n");
        pm.terminate_process(pid);
        return;
    }

    pid_t target_pid = 0;
    while (*args) {
        if (*args < '0' || *args > '9') {
            printf("pkill: invalid process id\n");
            pm.terminate_process(pid);
            return;
        }
        target_pid = target_pid * 10 + (*args - '0');
        args++;
    }

    if (target_pid == shell_pid) {
        printf("pkill: cannot kill shell process\n");
        pm.terminate_process(pid);
        return;
    }

    kernel::Process* target = pm.get_process(target_pid);
    if (!target) {
        printf("pkill: no such process\n");
        pm.terminate_process(pid);
        return;
    }

    pm.terminate_process(target_pid);
    pm.terminate_process(pid);
}

void cmd_less(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("less", shell_pid);

    if (!path) {
        printf("Usage: less <path>\n");
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::FileSystem::instance();
    auto* file = fs.get_file(path);
    if (!file || file->type != fs::FileType::Regular) {
        printf("less: cannot read '");
        printf(path);
        printf("'\n");
        pm.terminate_process(pid);
        return;
    }

    char* text = new char[file->size + 1];
    if (!text) {
        printf("less: memory allocation failed\n");
        pm.terminate_process(pid);
        return;
    }

    fs.read_file(path, reinterpret_cast<uint8_t*>(text), file->size);
    text[file->size] = '\0';

    pager::show_text(text);

    delete[] text;
    pm.terminate_process(pid);
}

void interrupt_command() {
    auto& pm = kernel::ProcessManager::instance();
    kernel::Process* current = pm.get_first_process();

    while (current) {
        if (current->state == kernel::ProcessState::Running && current->pid != shell_pid) {
            printf("\nCommand interrupted\n");
            pm.terminate_process(current->pid);
            return;
        }
        current = current->next;
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
    const char* current_path = fs.get_current_path();
    char dir_name[256];

    if (strcmp(current_path, "/") == 0)
        strcpy(dir_name, "/");
    else {
        const char* last_slash = strrchr(current_path, '/');
        if (last_slash)
            strcpy(dir_name, last_slash + 1);
        else
            strcpy(dir_name, current_path);
    }

    auto& user_manager = fs::CUserManager::instance();
    const char* username = user_manager.get_current_username();

    set_white();
    printf("[");
    set_green();
    printf("%s", username);
    set_white();
    printf("@");
    set_blue();
    printf("kernel");
    printf(" ");
    set_gray();
    printf("%s", dir_name);
    reset_color();
    set_white();
    printf("]");
    set_green();
    printf("$");
    reset_color();
    printf(" ");
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

    if (pager::is_active()) {
        pager::process_keypress(c);
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
    while (input_pos > 0 &&
           (input_buffer[input_pos - 1] == ' ' || input_buffer[input_pos - 1] == '\t')) {
        input_buffer[--input_pos] = '\0';
    }

    if (input_pos == 0) {
        print_prompt();
        return;
    }

    if (history_count < MAX_HISTORY_SIZE)
        memcpy(command_history[history_count++], input_buffer, input_pos + 1);
    else {
        memmove(command_history[0], command_history[1],
                sizeof(command_history[0]) * (MAX_HISTORY_SIZE - 1));
        memcpy(command_history[MAX_HISTORY_SIZE - 1], input_buffer, input_pos + 1);
    }

    char* cmd = input_buffer;
    char* args = nullptr;
    for (size_t i = 0; i < input_pos; ++i) {
        if (input_buffer[i] == ' ') {
            input_buffer[i] = '\0';
            args = &input_buffer[i + 1];
            break;
        }
    }

    if (strcmp(cmd, "help") == 0)
        cmd_help();
    else if (strcmp(cmd, "echo") == 0)
        cmd_echo(args);
    else if (strcmp(cmd, "clear") == 0)
        terminal_clear();
    else if (strcmp(cmd, "crash") == 0)
        cmd_crash();
    else if (strcmp(cmd, "shutdown") == 0 || strcmp(cmd, "poweroff") == 0)
        cmd_shutdown();
    else if (strcmp(cmd, "memory") == 0)
        cmd_memory();
    else if (strcmp(cmd, "ls") == 0)
        cmd_ls(args);
    else if (strcmp(cmd, "mkdir") == 0)
        cmd_mkdir(args);
    else if (strcmp(cmd, "cd") == 0)
        cmd_cd(args);
    else if (strcmp(cmd, "cat") == 0)
        cmd_cat(args);
    else if (strcmp(cmd, "cp") == 0)
        cmd_cp(args);
    else if (strcmp(cmd, "mv") == 0)
        cmd_mv(args);
    else if (strcmp(cmd, "rm") == 0)
        cmd_rm(args);
    else if (strcmp(cmd, "touch") == 0)
        cmd_touch(args);
    else if (strcmp(cmd, "edit") == 0) {
        char filename[256];
        if (args)
            strcpy(filename, args);
        else
            filename[0] = '\0';

        memset(input_buffer, 0, sizeof(input_buffer));
        input_pos = 0;

        cmd_edit(filename);
        return;
    } else if (strcmp(cmd, "history") == 0)
        cmd_history();
    else if (strcmp(cmd, "uptime") == 0)
        cmd_uptime();
    else if (strcmp(cmd, "shutdown") == 0 || strcmp(cmd, "poweroff") == 0)
        cmd_shutdown();
    else if (strcmp(cmd, "graphics") == 0)
        cmd_graphics();
    else if (strcmp(cmd, "ps") == 0)
        cmd_ps();
    else if (strcmp(cmd, "pkill") == 0)
        cmd_pkill(args);
    else if (strcmp(cmd, "time") == 0)
        cmd_time();
    else if (strcmp(cmd, "less") == 0)
        cmd_less(args);
    else if (strcmp(cmd, "ipctest") == 0)
        cmd_ipc_test();
    else if (strcmp(cmd, "useradd") == 0)
        cmd_useradd(args);
    else if (strcmp(cmd, "userremove") == 0)
        cmd_userremove(args);
    else if (strcmp(cmd, "su") == 0)
        cmd_su(args);
    else if (strcmp(cmd, "whoami") == 0)
        cmd_whoami();
    else {
        set_red();
        printf("Unknown command: %s\n", cmd);
        reset_color();
    }

    memset(input_buffer, 0, sizeof(input_buffer));
    input_pos = 0;

    if (!pager::is_active()) print_prompt();
}

void init_shell() {
    memset(input_buffer, 0, sizeof(input_buffer));
    memset(command_history, 0, sizeof(command_history));
    input_pos = 0;
    history_count = 0;

    auto& pm = kernel::ProcessManager::instance();
    shell_pid = pm.create_process("shell", 0);

    pager::init_pager();

    screen_state::init();

    printf("\n");
    print_prompt();
    editor::init_editor();
}

void cmd_ipc_test() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t test_pid = pm.create_process("ipc_test", shell_pid);
    auto& ipc = kernel::IPCManager::instance();

    char queue_name[64];
    snprintf(queue_name, sizeof(queue_name), "test_queue_%d_%lu", test_pid, get_ticks());

    int32_t queue_id = ipc.create_message_queue(test_pid, queue_name);
    if (queue_id < 0) {
        printf("Failed to create message queue\n");
        pm.terminate_process(test_pid);
        return;
    }

    printf("Created message queue with ID: %d\n", queue_id);

    const char* message = "Hello from IPC test!";
    if (!ipc.send_message(queue_id, test_pid, message, strlen(message) + 1)) {
        printf("Failed to send message\n");
        ipc.destroy_message_queue(queue_id);
        pm.terminate_process(test_pid);
        return;
    }

    printf("Sent message: \"%s\"\n", message);

    struct {
        pid_t sender;
        uint64_t timestamp;
        size_t size;
        uint8_t data[1024];
    } received_msg;

    if (!ipc.receive_message(queue_id, *reinterpret_cast<kernel::IPCMessage*>(&received_msg),
                             false)) {
        printf("Failed to receive message\n");
        ipc.destroy_message_queue(queue_id);
        pm.terminate_process(test_pid);
        return;
    }

    printf("Received message from PID %d: \"%s\"\n", received_msg.sender, received_msg.data);

    int32_t shm_id = ipc.create_shared_memory(test_pid, 4096);
    if (shm_id < 0) {
        printf("Failed to create shared memory\n");
        ipc.destroy_message_queue(queue_id);
        pm.terminate_process(test_pid);
        return;
    }

    printf("Created shared memory with ID: %d\n", shm_id);

    void* shm_addr = ipc.attach_shared_memory(shm_id, test_pid);
    if (!shm_addr) {
        printf("Failed to attach shared memory\n");
        ipc.destroy_shared_memory(shm_id);
        ipc.destroy_message_queue(queue_id);
        pm.terminate_process(test_pid);
        return;
    }

    printf("Attached shared memory at address: 0x%llx\n", reinterpret_cast<uint64_t>(shm_addr));

    const char* shm_message = "Hello from shared memory!";
    memcpy(shm_addr, shm_message, strlen(shm_message) + 1);

    printf("Wrote to shared memory: \"%s\"\n", shm_message);
    printf("Read from shared memory: \"%s\"\n", static_cast<char*>(shm_addr));

    if (!ipc.detach_shared_memory(shm_id, test_pid))
        printf("Failed to detach shared memory\n");
    else
        printf("Detached shared memory\n");

    if (!ipc.destroy_shared_memory(shm_id))
        printf("Failed to destroy shared memory\n");
    else
        printf("Destroyed shared memory\n");

    if (!ipc.destroy_message_queue(queue_id))
        printf("Failed to destroy message queue\n");
    else
        printf("Destroyed message queue\n");

    printf("IPC test completed successfully!\n");
    pm.terminate_process(test_pid);
}

void cmd_useradd(const char* args) {
    if (!args || args[0] == '\0') {
        printf("useradd: missing arguments\n");
        printf("Usage: useradd <username> <password>\n");
        return;
    }

    char username[fs::MAX_USERNAME] = {0};
    char password[fs::MAX_PASSWORD] = {0};

    size_t arg_len = strlen(args);
    size_t i = 0;

    size_t username_len = 0;
    while (i < arg_len && args[i] != ' ' && username_len < fs::MAX_USERNAME - 1) {
        username[username_len++] = args[i++];
    }
    username[username_len] = '\0';

    while (i < arg_len && args[i] == ' ')
        i++;

    size_t password_len = 0;
    while (i < arg_len && password_len < fs::MAX_PASSWORD - 1) {
        password[password_len++] = args[i++];
    }
    password[password_len] = '\0';

    if (username_len == 0) {
        printf("useradd: missing username\n");
        return;
    }

    if (password_len == 0) {
        printf("useradd: missing password\n");
        return;
    }

    auto& user_manager = fs::CUserManager::instance();
    if (user_manager.add_user(username, password))
        printf("User '%s' created successfully\n", username);
    else
        printf("Failed to create user '%s'\n", username);
}

void cmd_userremove(const char* args) {
    if (!args || args[0] == '\0') {
        printf("userremove: missing username\n");
        printf("Usage: userremove <username>\n");
        return;
    }

    auto& user_manager = fs::CUserManager::instance();
    if (user_manager.remove_user(args))
        printf("User '%s' removed successfully\n", args);
    else
        printf("Failed to remove user '%s'\n", args);
}

void cmd_su(const char* args) {
    if (!args || args[0] == '\0') {
        printf("su: missing username\n");
        printf("Usage: su <username> <password>\n");
        return;
    }

    char username[fs::MAX_USERNAME] = {0};
    char password[fs::MAX_PASSWORD] = {0};

    size_t arg_len = strlen(args);
    size_t i = 0;

    size_t username_len = 0;
    while (i < arg_len && args[i] != ' ' && username_len < fs::MAX_USERNAME - 1) {
        username[username_len++] = args[i++];
    }
    username[username_len] = '\0';

    while (i < arg_len && args[i] == ' ')
        i++;

    size_t password_len = 0;
    while (i < arg_len && password_len < fs::MAX_PASSWORD - 1) {
        password[password_len++] = args[i++];
    }
    password[password_len] = '\0';

    if (username_len == 0) {
        printf("su: missing username\n");
        return;
    }

    if (password_len == 0) {
        printf("su: missing password\n");
        return;
    }

    auto& user_manager = fs::CUserManager::instance();
    if (user_manager.switch_user(username, password))
        printf("Switched to user '%s'\n", username);
    else
        printf("Authentication failed for user '%s'\n", username);
}

void cmd_whoami() {
    auto& user_manager = fs::CUserManager::instance();
    printf("%s\n", user_manager.get_current_username());
}