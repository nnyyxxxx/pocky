#include "shell.hpp"

#include <cstdio>
#include <cstring>

#include "commands/commands.hpp"
#include "core/ipc.hpp"
#include "core/process.hpp"
#include "core/scheduler.hpp"
#include "core/types.hpp"
#include "drivers/keyboard.hpp"
#include "editor.hpp"
#include "fs/fat32.hpp"
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

[[maybe_unused]] void split_path(const char* input, char* first, char* second) {
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

[[maybe_unused]] void list_callback(const char* name, uint8_t attributes, uint32_t size) {
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return;

    if (attributes & 0x10)
        printf("d ");
    else {
        printf("f ");
        if (size < 1024)
            printf("(%u B) ", size);
        else if (size < 1024 * 1024)
            printf("(%u KB) ", size / 1024);
        else
            printf("(%u MB) ", size / (1024 * 1024));
    }

    printf("%s\n", name);
}

template <typename F>
void handle_wildcard_command(const char* pattern, F cmd_fn) {
    auto& fs = fs::CFat32FileSystem::instance();
    uint8_t buffer[1024];
    fs.readFile(ROOT_CLUSTER, buffer, sizeof(buffer));

    for (size_t i = 0; i < sizeof(buffer); i += 32) {
        uint8_t* entry = &buffer[i];
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        char name[13] = {0};
        memcpy(name, entry, 11);
        name[11] = '\0';

        if (match_wildcard(pattern, name)) cmd_fn(name);
    }
}

}  // namespace

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
    char filename[MAX_PATH] = {0};

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

    auto& fs = fs::CFat32FileSystem::instance();
    const uint8_t* data = reinterpret_cast<const uint8_t*>(text);
    fs.writeFile(ROOT_CLUSTER, data, strlen(text));

    memset(input_buffer, 0, sizeof(input_buffer));
    input_pos = 0;
}

const char* get_dir_name(const char* path) {
    static char buffer[MAX_PATH];

    if (!path || !*path) {
        strcpy(buffer, "/");
        return buffer;
    }

    if (strcmp(path, "/") == 0) {
        strcpy(buffer, "/");
        return buffer;
    }

    strncpy(buffer, path, MAX_PATH - 1);
    buffer[MAX_PATH - 1] = '\0';

    size_t len = strlen(buffer);
    if (len > 1 && buffer[len - 1] == '/') buffer[len - 1] = '\0';

    char* last_slash = strrchr(buffer, '/');
    if (!last_slash) return buffer;

    if (last_slash == buffer) return last_slash + 1;

    return last_slash + 1;
}

void print_prompt() {
    auto& fs = fs::CFat32FileSystem::instance();
    const char* dir_name = fs.get_current_dir_name();

    if (!dir_name || !*dir_name) {
        fs.set_current_dir_name("/");
        dir_name = "/";
    }

    set_white();
    printf("[");
    set_gray();
    printf("kernel");
    set_white();
    printf(" ");
    set_gray();
    printf("%s", dir_name);
    set_white();
    printf("]");
    set_gray();
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

    auto& fs = fs::CFat32FileSystem::instance();
    uint32_t current_cluster;
    uint32_t size;
    uint8_t attributes;
    if (!fs.findFile(".", current_cluster, size, attributes)) current_cluster = ROOT_CLUSTER;

    uint8_t buffer[1024];
    fs.readFile(current_cluster, buffer, sizeof(buffer));

    const char* match = nullptr;
    size_t matches = 0;

    for (size_t i = 0; i < sizeof(buffer); i += 32) {
        uint8_t* entry = &buffer[i];
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        char name[13] = {0};
        memcpy(name, entry, 11);
        name[11] = '\0';

        if (strncmp(name, last_word, word_len) == 0) {
            bool isDirectory = (entry[11] & 0x10) != 0;
            if (strcmp(command, "cd") == 0 && !isDirectory) continue;
            match = name;
            matches++;
        }
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
           (input_buffer[input_pos - 1] == ' ' || input_buffer[input_pos - 1] == '\t'))
        input_buffer[--input_pos] = '\0';

    if (input_pos == 0) {
        print_prompt();
        return;
    }

    commands::append_to_history_file(input_buffer);

    char* current_cmd = input_buffer;
    while (current_cmd && *current_cmd) {
        char* next_cmd = strchr(current_cmd, ';');
        if (next_cmd) *next_cmd++ = '\0';

        while (*current_cmd == ' ' || *current_cmd == '\t')
            current_cmd++;

        if (!*current_cmd) {
            if (!next_cmd) break;
            current_cmd = next_cmd;
            continue;
        }

        char* cmd = current_cmd;
        char* args = nullptr;
        for (size_t i = 0; cmd[i]; ++i) {
            if (cmd[i] == ' ') {
                cmd[i] = '\0';
                args = &cmd[i + 1];
                break;
            }
        }

        if (strcmp(cmd, "help") == 0)
            commands::cmd_help();
        else if (strcmp(cmd, "echo") == 0)
            commands::cmd_echo(args);
        else if (strcmp(cmd, "clear") == 0)
            terminal_clear();
        else if (strcmp(cmd, "crash") == 0)
            commands::cmd_crash();
        else if (strcmp(cmd, "shutdown") == 0 || strcmp(cmd, "poweroff") == 0)
            commands::cmd_shutdown();
        else if (strcmp(cmd, "memory") == 0)
            commands::cmd_memory();
        else if (strcmp(cmd, "ls") == 0)
            commands::cmd_ls(args);
        else if (strcmp(cmd, "mkdir") == 0)
            commands::cmd_mkdir(args);
        else if (strcmp(cmd, "cd") == 0)
            commands::cmd_cd(args);
        else if (strcmp(cmd, "cat") == 0)
            commands::cmd_cat(args);
        else if (strcmp(cmd, "mv") == 0)
            commands::cmd_mv(args);
        else if (strcmp(cmd, "rm") == 0)
            commands::cmd_rm(args);
        else if (strcmp(cmd, "touch") == 0)
            commands::cmd_touch(args);
        else if (strcmp(cmd, "edit") == 0) {
            char filename[256];
            if (args)
                strcpy(filename, args);
            else
                filename[0] = '\0';

            memset(input_buffer, 0, sizeof(input_buffer));
            input_pos = 0;

            commands::cmd_edit(filename);
            return;
        } else if (strcmp(cmd, "history") == 0)
            commands::cmd_history();
        else if (strcmp(cmd, "uptime") == 0)
            commands::cmd_uptime();
        else if (strcmp(cmd, "ps") == 0)
            commands::cmd_ps();
        else if (strcmp(cmd, "pkill") == 0)
            commands::cmd_pkill(args);
        else if (strcmp(cmd, "time") == 0)
            commands::cmd_time();
        else if (strcmp(cmd, "less") == 0)
            commands::cmd_less(args);
        else if (strcmp(cmd, "ipctest") == 0)
            commands::cmd_ipc_test();
        else if (strcmp(cmd, "cores") == 0)
            commands::cmd_cores();
        else {
            set_red();
            printf("Unknown command: %s\n", cmd);
            reset_color();
        }

        if (!next_cmd) break;
        current_cmd = next_cmd;
    }

    memset(input_buffer, 0, sizeof(input_buffer));
    input_pos = 0;

    if (!pager::is_active()) print_prompt();
}

void init_shell() {
    memset(input_buffer, 0, sizeof(input_buffer));
    input_pos = 0;

    auto& pm = kernel::ProcessManager::instance();
    shell_pid = pm.create_process("shell", 0);

    pager::init_pager();
    screen_state::init();

    auto& fs = fs::CFat32FileSystem::instance();
    fs.set_current_path("/");
    fs.set_current_dir_name("/");
    fs.set_current_directory_cluster(ROOT_CLUSTER);

    uint32_t cluster, size;
    uint8_t attributes;

    if (!fs.findFile("tmp", cluster, size, attributes)) fs.createDirectory("tmp");

    const char* welcome_msg = "Welcome to the kernel!\n";
    fs.createFileWithContent("tmp/welcome", reinterpret_cast<const uint8_t*>(welcome_msg),
                             strlen(welcome_msg), 0);

    printf("\n");
    print_prompt();
}
