#include "shell.hpp"

#include <cstdio>
#include <cstring>

#include "core/ipc.hpp"
#include "core/process.hpp"
#include "core/scheduler.hpp"
#include "core/types.hpp"
#include "drivers/keyboard.hpp"
#include "editor.hpp"
#include "fs/fat32.hpp"
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

constexpr size_t MAX_PATH = 256;
constexpr uint32_t ROOT_CLUSTER = 2;

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

void print_file_type(uint8_t attributes) {
    if (attributes & 0x10)
        printf("d");
    else if (attributes & 0x08)
        printf("v");
    else if (attributes & 0x01)
        printf("r");
    else
        printf("-");

    printf("%c", (attributes & 0x02) ? 'h' : '-');
    printf("%c", (attributes & 0x04) ? 's' : '-');
    printf("%c", (attributes & 0x20) ? 'a' : '-');
}

void print_file_size(uint32_t size) {
    if (size < 1024)
        printf("%5u ", size);
    else if (size < 1024 * 1024)
        printf("%4uK ", size / 1024);
    else
        printf("%4uM ", size / (1024 * 1024));
}

void list_callback(const char* name, uint8_t attributes, uint32_t size) {
    print_file_type(attributes);
    printf(" ");
    print_file_size(size);
    printf("%s", name);
    if (attributes & 0x10) printf("/");
    printf("\n");
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
                            "  userrm   - Remove a user\n"
                            "  su       - Switch to a different user\n"
                            "  whoami   - Show current user\n"
                            "  userlist - List user accounts\n"
                            "  cores    - List CPU cores\n";

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

void cmd_ls([[maybe_unused]] const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("ls", shell_pid);

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

        uint8_t attributes = entry[11];
        uint32_t size = (entry[28] | (entry[29] << 8) | (entry[30] << 16) | (entry[31] << 24));

        list_callback(name, attributes, size);
    }

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

    auto& fs = fs::CFat32FileSystem::instance();
    if (!fs.createDirectory(path)) {
        printf("mkdir: failed to create directory '%s'\n", path);
        pm.terminate_process(pid);
        return;
    }

    pm.terminate_process(pid);
}

void cmd_cd(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("cd", shell_pid);

    if (!path || !*path) {
        printf("cd: missing operand\n");
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::CFat32FileSystem::instance();

    if (strcmp(path, "/") == 0) {
        fs.set_current_path("/");
        pm.terminate_process(pid);
        return;
    }

    uint8_t buffer[1024];
    fs.readFile(ROOT_CLUSTER, buffer, sizeof(buffer));

    bool found = false;
    for (size_t i = 0; i < sizeof(buffer); i += 32) {
        uint8_t* entry = &buffer[i];
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        char name[13] = {0};
        memcpy(name, entry, 11);
        name[11] = '\0';

        if (strcmp(name, path) == 0) {
            if (!(entry[11] & 0x10)) {
                printf("cd: not a directory: %s\n", path);
                pm.terminate_process(pid);
                return;
            }
            found = true;

            char new_path[MAX_PATH];
            if (strcmp(fs.get_current_path(), "/") == 0)
                snprintf(new_path, sizeof(new_path), "/%s", path);
            else
                snprintf(new_path, sizeof(new_path), "%s/%s", fs.get_current_path(), path);

            fs.set_current_path(new_path);
            break;
        }
    }

    if (!found) printf("cd: no such directory: %s\n", path);

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

    auto& fs = fs::CFat32FileSystem::instance();
    uint8_t buffer[1024];
    fs.readFile(ROOT_CLUSTER, buffer, sizeof(buffer));

    bool found = false;
    for (size_t i = 0; i < sizeof(buffer); i += 32) {
        uint8_t* entry = &buffer[i];
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        char name[13] = {0};
        memcpy(name, entry, 11);
        name[11] = '\0';

        if (strcmp(name, path) == 0) {
            found = true;
            if (entry[11] & 0x10) {
                printf("cat: %s: Is a directory\n", path);
                pm.terminate_process(pid);
                return;
            }

            uint32_t cluster = (entry[26] | (entry[27] << 8));
            uint32_t size = (entry[28] | (entry[29] << 8) | (entry[30] << 16) | (entry[31] << 24));

            if (size == 0) {
                pm.terminate_process(pid);
                return;
            }

            uint8_t* content = new uint8_t[size];
            fs.readFile(cluster, content, size);

            for (size_t j = 0; j < size; j++) {
                if (content[j] == '\n')
                    terminal_putchar('\n');
                else if (content[j] >= 32 && content[j] <= 126)
                    terminal_putchar(content[j]);
                else
                    terminal_putchar('.');
            }

            delete[] content;
            break;
        }
    }

    if (!found) printf("cat: %s: No such file or directory\n", path);

    printf("\n");
    pm.terminate_process(pid);
}

void cmd_cp(const char* args) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("cp", shell_pid);

    char src[MAX_PATH];
    char dst[MAX_PATH];
    split_path(args, src, dst);

    if (!*src || !*dst) {
        printf("cp: missing operand\n");
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::CFat32FileSystem::instance();
    uint8_t buffer[1024];
    fs.readFile(ROOT_CLUSTER, buffer, sizeof(buffer));

    bool found = false;
    uint32_t src_cluster = 0;
    uint32_t src_size = 0;
    uint8_t src_attributes = 0;

    for (size_t i = 0; i < sizeof(buffer); i += 32) {
        uint8_t* entry = &buffer[i];
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        char name[13] = {0};
        memcpy(name, entry, 11);
        name[11] = '\0';

        if (strcmp(name, src) == 0) {
            found = true;
            if (entry[11] & 0x10) {
                printf("cp: %s: Is a directory\n", src);
                pm.terminate_process(pid);
                return;
            }
            src_cluster = (entry[26] | (entry[27] << 8));
            src_size = (entry[28] | (entry[29] << 8) | (entry[30] << 16) | (entry[31] << 24));
            src_attributes = entry[11];
            break;
        }
    }

    if (!found) {
        printf("cp: %s: No such file or directory\n", src);
        pm.terminate_process(pid);
        return;
    }

    size_t free_entry = 0;
    bool found_free = false;
    for (size_t i = 0; i < sizeof(buffer); i += 32) {
        uint8_t* entry = &buffer[i];
        if (entry[0] == 0x00 || entry[0] == 0xE5) {
            free_entry = i;
            found_free = true;
            break;
        }
    }

    if (!found_free) {
        printf("cp: no space left in directory\n");
        pm.terminate_process(pid);
        return;
    }

    uint8_t* content = new uint8_t[src_size];
    fs.readFile(src_cluster, content, src_size);

    uint8_t* dst_entry = &buffer[free_entry];
    memset(dst_entry, 0, 32);
    memset(dst_entry, ' ', 11);
    strncpy(reinterpret_cast<char*>(dst_entry), dst, strlen(dst));

    dst_entry[11] = src_attributes;
    dst_entry[26] = ROOT_CLUSTER & 0xFF;
    dst_entry[27] = (ROOT_CLUSTER >> 8) & 0xFF;
    dst_entry[28] = src_size & 0xFF;
    dst_entry[29] = (src_size >> 8) & 0xFF;
    dst_entry[30] = (src_size >> 16) & 0xFF;
    dst_entry[31] = (src_size >> 24) & 0xFF;

    fs.writeFile(ROOT_CLUSTER, buffer, sizeof(buffer));
    fs.writeFile(ROOT_CLUSTER, content, src_size);

    delete[] content;
    pm.terminate_process(pid);
}

void cmd_mv(const char* args) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("mv", shell_pid);

    char src[MAX_PATH];
    char dst[MAX_PATH];
    split_path(args, src, dst);

    if (!*src || !*dst) {
        printf("mv: missing operand\n");
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::CFat32FileSystem::instance();
    if (!fs.renameFile(src, dst)) {
        printf("mv: failed to rename '%s' to '%s'\n", src, dst);
        pm.terminate_process(pid);
        return;
    }

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

    auto& fs = fs::CFat32FileSystem::instance();
    uint32_t cluster, size;
    uint8_t attributes;
    if (!fs.findFile(path, cluster, size, attributes)) {
        printf("rm: cannot remove '%s': No such file or directory\n", path);
        pm.terminate_process(pid);
        return;
    }

    if (attributes & 0x10) {
        if (!fs.deleteDirectory(path)) {
            printf("rm: cannot remove '%s': Directory not empty\n", path);
            pm.terminate_process(pid);
            return;
        }
    } else {
        if (!fs.deleteFile(path)) {
            printf("rm: failed to remove '%s'\n", path);
            pm.terminate_process(pid);
            return;
        }
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

    auto& fs = fs::CFat32FileSystem::instance();
    uint8_t empty[0];
    fs.writeFile(ROOT_CLUSTER, empty, 0);

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
    for (int i = 0; i < 1000000; i++) {
        asm volatile("nop");
    }

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

    auto& fs = fs::CFat32FileSystem::instance();
    uint8_t buffer[1024];
    fs.readFile(ROOT_CLUSTER, buffer, sizeof(buffer));

    bool found = false;
    for (size_t i = 0; i < sizeof(buffer); i += 32) {
        uint8_t* entry = &buffer[i];
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        char name[13] = {0};
        memcpy(name, entry, 11);
        name[11] = '\0';

        if (strcmp(name, path) == 0) {
            found = true;
            uint32_t cluster = (entry[26] | (entry[27] << 8));
            uint32_t size = (entry[28] | (entry[29] << 8) | (entry[30] << 16) | (entry[31] << 24));

            char* text = new char[size + 1];
            if (!text) {
                printf("less: memory allocation failed\n");
                pm.terminate_process(pid);
                return;
            }

            fs.readFile(cluster, reinterpret_cast<uint8_t*>(text), size);
            text[size] = '\0';
            pager::show_text(text);
            delete[] text;
            break;
        }
    }

    if (!found) printf("less: cannot read '%s': No such file\n", path);

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

void print_prompt() {
    auto& fs = fs::CFat32FileSystem::instance();
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
    set_gray();
    printf("%s", username);
    set_white();
    printf("@");
    set_gray();
    printf("kernel");
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
    uint8_t buffer[1024];
    fs.readFile(ROOT_CLUSTER, buffer, sizeof(buffer));

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
    else if (strcmp(cmd, "userrm") == 0)
        cmd_userrm(args);
    else if (strcmp(cmd, "su") == 0)
        cmd_su(args);
    else if (strcmp(cmd, "whoami") == 0)
        cmd_whoami();
    else if (strcmp(cmd, "userlist") == 0)
        cmd_userlist();
    else if (strcmp(cmd, "cores") == 0)
        cmd_cores();
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

void cmd_userrm(const char* args) {
    if (!args || args[0] == '\0') {
        printf("userrm: missing username\n");
        printf("Usage: userrm <username>\n");
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

void cmd_userlist() {
    auto& user_manager = fs::CUserManager::instance();

    printf("User accounts:\n");
    user_manager.list_users([](const char* username, uint32_t uid, uint32_t gid) {
        printf("  %s (UID: %d, GID: %d)\n", username, uid, gid);
    });
}

void cmd_cores() {
    auto& smp = kernel::SMPManager::instance();

    smp.detect_active_cores();

    uint32_t cpu_count = smp.get_cpu_count();
    printf("ID | LAPIC ID | BSP | Active | Stack Address\n");
    printf("---+---------+-----+--------+-------------\n");

    uint32_t active_count = 0;
    for (uint32_t i = 0; i < cpu_count; i++) {
        auto* cpu_info = smp.get_cpu_info(i);
        if (cpu_info && cpu_info->is_active) active_count++;
    }

    for (uint32_t i = 0; i < cpu_count; i++) {
        auto* cpu_info = smp.get_cpu_info(i);
        if (cpu_info) {
            printf("%2u | %7u | %3s | %6s | 0x%016lx\n", cpu_info->id, cpu_info->lapic_id,
                   cpu_info->is_bsp ? "Yes" : "No", cpu_info->is_active ? "Yes" : "No",
                   cpu_info->kernel_stack);
        }
    }
}