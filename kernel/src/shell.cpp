#include "shell.hpp"
#include "terminal.hpp"
#include <cstring>

char input_buffer[256] = {0};
size_t input_pos = 0;

void cmd_help() {
    terminal_writestring("Available commands:\n");
    terminal_writestring("  help  - Display this help message\n");
    terminal_writestring("  clear - Clear the screen\n");
}

void process_command() {
    if (strcmp(input_buffer, "help") == 0)
        cmd_help();
    else if (strcmp(input_buffer, "clear") == 0)
        terminal_clear();
    else if (input_buffer[0] != '\0') {
        terminal_writestring("Unknown command: ");
        terminal_writestring(input_buffer);
        terminal_writestring("\n");
    }

    memset(input_buffer, 0, sizeof(input_buffer));
    input_pos = 0;
}