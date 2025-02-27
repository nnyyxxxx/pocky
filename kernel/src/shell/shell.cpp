#include "shell.hpp"

#include <cstring>

#include "io.hpp"
#include "terminal.hpp"

char input_buffer[256] = {0};
size_t input_pos = 0;
bool handling_exception = false;

void cmd_help() {
    terminal_writestring("\n");
    terminal_writestring("Available commands:\n");
    terminal_writestring("  help     - Display this help message\n");
    terminal_writestring("  clear    - Clear the screen\n");
    terminal_writestring("  echo     - Display the text that follows\n");
    terminal_writestring("  crash    - Crash the kernel - Gets caught by the exception handler\n");
    terminal_writestring("  shutdown - Power off the system\n");
    terminal_writestring("\n");
}

void cmd_echo(const char* args) {
    terminal_writestring(args);
    terminal_writestring("\n");
}

void cmd_crash() {
    volatile int* ptr = nullptr;
    *ptr = 0;
}

void cmd_shutdown() {
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
}

void process_command() {
    if (handling_exception) {
        handling_exception = false;
        terminal_writestring("");
        return;
    }

    if (strncmp(input_buffer, "echo ", 5) == 0)
        cmd_echo(input_buffer + 5);
    else if (strcmp(input_buffer, "echo") == 0)
        terminal_writestring("\n");
    else if (strcmp(input_buffer, "help") == 0)
        cmd_help();
    else if (strcmp(input_buffer, "clear") == 0)
        terminal_clear();
    else if (strcmp(input_buffer, "crash") == 0)
        cmd_crash();
    else if (strcmp(input_buffer, "shutdown") == 0 || strcmp(input_buffer, "poweroff") == 0)
        cmd_shutdown();
    else if (input_buffer[0] != '\0') {
        terminal_writestring("Unknown command: ");
        terminal_writestring(input_buffer);
        terminal_writestring("\n");
    }

    memset(input_buffer, 0, sizeof(input_buffer));
    input_pos = 0;
    terminal_writestring("$ ");
}

void init_shell() {
    memset(input_buffer, 0, sizeof(input_buffer));
    input_pos = 0;
    handling_exception = false;
}