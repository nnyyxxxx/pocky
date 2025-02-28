#pragma once
#include <cstddef>

extern char input_buffer[256];
extern size_t input_pos;
extern bool handling_exception;
extern bool command_running;

void process_command();
void cmd_help();
void cmd_echo(const char* args);
void cmd_crash();
void cmd_shutdown();
void cmd_memory();
void init_shell();
void interrupt_command();