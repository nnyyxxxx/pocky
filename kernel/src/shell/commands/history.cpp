#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "fs/fat32.hpp"
#include "printf.hpp"

namespace commands {

void append_to_history_file(const char* command) {
    auto& fs = fs::CFat32FileSystem::instance();

    uint32_t shell_cluster, file_cluster, size;
    uint8_t attributes;

    if (!fs.findFile("/shell", shell_cluster, size, attributes)) fs.createDirectory("shell");

    if (!fs.findFile("/shell/history", file_cluster, size, attributes))
        fs.createFileWithContent("/shell/history", nullptr, 0, 0);

    uint8_t buffer[4096] = {0};
    fs.readFileByPath("/shell/history", buffer, sizeof(buffer) - 1);

    size_t current_length = strlen(reinterpret_cast<const char*>(buffer));
    size_t command_length = strlen(command);

    if (current_length + command_length + 2 < sizeof(buffer)) {
        memcpy(buffer + current_length, command, command_length);
        buffer[current_length + command_length] = '\n';
        buffer[current_length + command_length + 1] = '\0';

        fs.writeFileByPath("/shell/history", buffer, strlen(reinterpret_cast<const char*>(buffer)));
    }
}

void cmd_history() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("history", shell_pid);

    auto& fs = fs::CFat32FileSystem::instance();

    uint32_t cluster, size;
    uint8_t attributes;

    if (fs.findFile("/shell/history", cluster, size, attributes)) {
        uint8_t file_buffer[4096] = {0};
        if (fs.readFileByPath("/shell/history", file_buffer, sizeof(file_buffer) - 1)) {
            char* content = reinterpret_cast<char*>(file_buffer);
            char* line_start = content;
            char* line_end;
            size_t history_num = 1;

            while (*line_start) {
                line_end = strchr(line_start, '\n');
                if (line_end) {
                    *line_end = '\0';
                    if (*line_start) {
                        print_number(history_num++, 10, 4, false);
                        printf("  %s\n", line_start);
                    }
                    line_start = line_end + 1;
                } else {
                    if (*line_start) {
                        print_number(history_num++, 10, 4, false);
                        printf("  %s\n", line_start);
                    }
                    break;
                }
            }
        } else
            printf("Failed to read history file\n");
    } else
        printf("No history file found\n");

    pm.terminate_process(pid);
}

}  // namespace commands