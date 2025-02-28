#include "shell.hpp"

#include <cstdio>
#include <cstring>

#include "io.hpp"
#include "physical_memory.hpp"
#include "printf.hpp"
#include "terminal.hpp"

char input_buffer[256] = {0};
size_t input_pos = 0;
bool handling_exception = false;
bool command_running = false;

void cmd_help() {
    command_running = true;
    terminal_writestring("\n");
    terminal_writestring("Available commands:\n");
    terminal_writestring("  help       - Display this help message\n");
    terminal_writestring("  clear      - Clear the screen\n");
    terminal_writestring("  echo       - Display the text that follows\n");
    terminal_writestring(
        "  crash      - Crash the kernel - Gets caught by the exception handler\n");
    terminal_writestring("  memory     - Display memory usage information\n");
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

void interrupt_command() {
    if (command_running) {
        terminal_writestring("\nCommand interrupted\n");
        command_running = false;
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
    else if (strcmp(input_buffer, "memory") == 0)
        cmd_memory();
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
}