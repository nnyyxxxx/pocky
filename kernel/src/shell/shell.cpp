#include "shell.hpp"

#include <cstring>

#include "io.hpp"
#include "terminal.hpp"

char input_buffer[256] = {0};
size_t input_pos = 0;
bool handling_exception = false;
bool command_running = false;
bool crash_loop_enabled = false;

void cmd_help() {
    command_running = true;
    terminal_writestring("\n");
    terminal_writestring("Available commands:\n");
    terminal_writestring("  help       - Display this help message\n");
    terminal_writestring("  clear      - Clear the screen\n");
    terminal_writestring("  echo       - Display the text that follows\n");
    terminal_writestring(
        "  crash      - Crash the kernel - Gets caught by the exception handler\n");
    terminal_writestring("  crash_loop - Repeatedly crash the kernel\n");
    terminal_writestring("  shutdown   - Power off the system\n");
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

void cmd_crash_loop() {
    command_running = true;
    terminal_writestring(
        "Starting crash loop...\n");
    crash_loop_enabled = true;

    volatile int* ptr = nullptr;
    *ptr = 0;

    crash_loop_enabled = false;
    command_running = false;
}

void interrupt_command() {
    if (command_running || crash_loop_enabled) {
        terminal_writestring("\nCommand interrupted\n");
        command_running = false;
        crash_loop_enabled = false;
    }
}

void process_command() {
    if (handling_exception) {
        handling_exception = false;

        if (crash_loop_enabled) {
            terminal_writestring("Restarting crash loop...\n");
            for (volatile int i = 0; i < 5000000; i++)
                ;
            volatile int* ptr = nullptr;
            *ptr = 0;
        }

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
    else if (strcmp(input_buffer, "crash_loop") == 0 || strcmp(input_buffer, "crashloop") == 0)
        cmd_crash_loop();
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
    command_running = false;
    crash_loop_enabled = false;
}