#pragma once
#include <cstddef>

extern char input_buffer[256];
extern size_t input_pos;

void process_command();
void cmd_help();
void cmd_echo(const char* args);
void cmd_crash();
void init_shell();