#include "shell.hpp"

#include <cstdio>
#include <cstring>

#include "fs/filesystem.hpp"
#include "io.hpp"
#include "physical_memory.hpp"
#include "printf.hpp"
#include "terminal.hpp"

char input_buffer[256] = {0};
size_t input_pos = 0;
bool handling_exception = false;
bool command_running = false;

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
        terminal_writestring("d");
    else
        terminal_writestring("-");
}

void print_file_size(const fs::FileNode* node) {
    char size_str[32];
    snprintf(size_str, sizeof(size_str), "%8zu", node->size);
    terminal_writestring(size_str);
}

void print_file_name(const fs::FileNode* node) {
    terminal_writestring(" ");
    terminal_writestring(node->name);
    if (node->type == fs::FileType::Directory) terminal_writestring("/");
}

void list_callback(const fs::FileNode* node) {
    print_file_type(node);
    print_file_size(node);
    print_file_name(node);
    terminal_writestring("\n");
}

}  // namespace

void cmd_help() {
    terminal_writestring("\n");
    terminal_writestring("Available commands:\n");
    terminal_writestring("  help     - Show this help message\n");
    terminal_writestring("  echo     - Echo the arguments\n");
    terminal_writestring("  clear    - Clear the screen\n");
    terminal_writestring("  crash    - Trigger a crash (for testing)\n");
    terminal_writestring("  memory   - Show memory usage\n");
    terminal_writestring("  ls       - List directory contents\n");
    terminal_writestring("  mkdir    - Create a new directory\n");
    terminal_writestring("  cd       - Change current directory\n");
    terminal_writestring("  cat      - Display file contents\n");
    terminal_writestring("  cp       - Copy a file\n");
    terminal_writestring("  mv       - Move/rename a file\n");
    terminal_writestring("  rm       - Remove a file or directory\n");
    terminal_writestring("  touch    - Create an empty file\n");
    terminal_writestring("  shutdown - Power off the system\n");
    terminal_writestring("\n");
    command_running = false;
}

void cmd_echo(const char* args) {
    command_running = true;
    terminal_writestring(args);
    terminal_writestring("\n");
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
    terminal_writestring("\nShutting down the system...\n");

    outb(0x604, 0x00);
    outb(0x604, 0x01);

    outb(0xB004, 0x00);
    outb(0x4004, 0x00);

    outb(0x64, 0xFE);

    terminal_writestring("Shutdown failed.\n");

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
        terminal_writestring("\nError: Invalid memory state\n");
        command_running = false;
        return;
    }

    size_t used_frames = total_frames - free_frames;

    constexpr size_t max_size = static_cast<size_t>(-1);
    if (total_frames > max_size / PhysicalMemoryManager::PAGE_SIZE) {
        terminal_writestring("\nError: Memory size too large to represent\n");
        command_running = false;
        return;
    }

    size_t total_bytes = total_frames * PhysicalMemoryManager::PAGE_SIZE;
    size_t used_bytes = used_frames * PhysicalMemoryManager::PAGE_SIZE;

    constexpr size_t bytes_per_mb = 1024 * 1024;
    size_t total_mb = total_bytes / bytes_per_mb;
    size_t used_mb = used_bytes / bytes_per_mb;

    terminal_writestring("\n");
    printf("%u MB used of available %u MB\n\n", used_mb, total_mb);

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
        terminal_writestring("mkdir: missing operand\n");
        command_running = false;
        return;
    }

    auto& fs = fs::FileSystem::instance();
    if (!fs.create_file(path, fs::FileType::Directory)) {
        terminal_writestring("mkdir: cannot create directory '");
        terminal_writestring(path);
        terminal_writestring("'\n");
    }
    command_running = false;
}

void cmd_cd(const char* path) {
    command_running = true;
    if (!path || !*path) {
        terminal_writestring("cd: missing operand\n");
        command_running = false;
        return;
    }

    auto& fs = fs::FileSystem::instance();
    if (!fs.change_directory(path)) {
        terminal_writestring("cd: cannot change directory to '");
        terminal_writestring(path);
        terminal_writestring("'\n");
    }
    command_running = false;
}

void cmd_cat(const char* path) {
    command_running = true;
    if (!path || !*path) {
        terminal_writestring("cat: missing operand\n");
        command_running = false;
        return;
    }

    auto& fs = fs::FileSystem::instance();
    auto node = fs.get_file(path);
    if (!node || node->type != fs::FileType::Regular) {
        terminal_writestring("cat: cannot read '");
        terminal_writestring(path);
        terminal_writestring("'\n");
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
    terminal_writestring("\n");
    command_running = false;
}

void cmd_cp(const char* args) {
    command_running = true;
    char src[fs::MAX_PATH];
    char dst[fs::MAX_PATH];
    split_path(args, src, dst);

    if (!*src || !*dst) {
        terminal_writestring("cp: missing operand\n");
        command_running = false;
        return;
    }

    auto& fs = fs::FileSystem::instance();
    auto src_node = fs.get_file(src);
    if (!src_node || src_node->type != fs::FileType::Regular) {
        terminal_writestring("cp: cannot read '");
        terminal_writestring(src);
        terminal_writestring("'\n");
        command_running = false;
        return;
    }

    auto dst_node = fs.create_file(dst, fs::FileType::Regular);
    if (!dst_node) {
        terminal_writestring("cp: cannot create '");
        terminal_writestring(dst);
        terminal_writestring("'\n");
        command_running = false;
        return;
    }

    if (!fs.write_file(dst, src_node->data, src_node->size)) {
        terminal_writestring("cp: write error\n");
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
        terminal_writestring("mv: missing operand\n");
        command_running = false;
        return;
    }

    auto& fs = fs::FileSystem::instance();
    auto src_node = fs.get_file(src);
    if (!src_node) {
        terminal_writestring("mv: cannot stat '");
        terminal_writestring(src);
        terminal_writestring("'\n");
        command_running = false;
        return;
    }

    if (src_node->type == fs::FileType::Regular) {
        cmd_cp(args);
        if (!fs.delete_file(src)) {
            terminal_writestring("mv: cannot remove '");
            terminal_writestring(src);
            terminal_writestring("'\n");
        }
    } else
        terminal_writestring("mv: directory move not supported\n");
    command_running = false;
}

void cmd_rm(const char* path) {
    command_running = true;
    if (!path || !*path) {
        terminal_writestring("rm: missing operand\n");
        command_running = false;
        return;
    }

    auto& fs = fs::FileSystem::instance();
    if (!fs.delete_file(path)) {
        terminal_writestring("rm: cannot remove '");
        terminal_writestring(path);
        terminal_writestring("'\n");
    }
    command_running = false;
}

void cmd_touch(const char* path) {
    if (!path) {
        terminal_writestring("Usage: touch <path>\n");
        return;
    }

    auto& fs = fs::FileSystem::instance();
    auto* file = fs.create_file(path, fs::FileType::Regular);

    if (!file) {
        terminal_writestring("Failed to create file: ");
        terminal_writestring(path);
        terminal_writestring("\n");
    }
}

void interrupt_command() {
    if (command_running) {
        terminal_writestring("\nCommand interrupted\n");
        command_running = false;
    }
}

void process_command() {
    if (handling_exception) {
        terminal_writestring("\n$ ");
        handling_exception = false;
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
    else if (strcmp(input_buffer, "shutdown") == 0 || strcmp(input_buffer, "poweroff") == 0)
        cmd_shutdown();
    else if (input_buffer[0] != '\0') {
        terminal_writestring("Unknown command: ");
        terminal_writestring(input_buffer);
        terminal_writestring("\n");
    }

    if (!handling_exception) {
        terminal_writestring("$ ");
        memset(input_buffer, 0, sizeof(input_buffer));
        input_pos = 0;
    }
}

void init_shell() {
    memset(input_buffer, 0, sizeof(input_buffer));
    input_pos = 0;
    handling_exception = false;
    command_running = false;
}