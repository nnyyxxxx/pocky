#include "shell.hpp"
#include "terminal.hpp"
#include <cstring>

char input_buffer[256] = {0};
size_t input_pos = 0;

void cmd_help() {
    terminal_writestring("Available commands:\n");
    terminal_writestring("  help  - Display this help message\n");
    terminal_writestring("  clear - Clear the screen\n");
    terminal_writestring("  echo  - Display the text that follows\n");
}

void cmd_echo(const char* args) {
    terminal_writestring(args);
    terminal_writestring("\n");
}

void process_command() {
    if (strncmp(input_buffer, "echo ", 5) == 0)
        cmd_echo(input_buffer + 5);
    else if (strcmp(input_buffer, "echo") == 0)
        terminal_writestring("\n");
    else if (strcmp(input_buffer, "help") == 0)
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