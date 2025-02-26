#pragma once
#include <cstddef>

extern char input_buffer[256];
extern size_t input_pos;
extern bool handling_exception;

void process_command();
void cmd_help();
void cmd_echo(const char* args);
void cmd_crash();
void init_shell();