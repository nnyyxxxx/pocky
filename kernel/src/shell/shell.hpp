#pragma once
#include <cstddef>

extern char input_buffer[256];
extern size_t input_pos;
extern bool handling_exception;
extern bool command_running;

void process_keypress(char c);
void process_command();
void cmd_help();
void cmd_echo(const char* args);
void cmd_crash();
void cmd_shutdown();
void cmd_memory();
void cmd_ls(const char* path);
void cmd_mkdir(const char* path);
void cmd_cd(const char* path);
void cmd_cat(const char* path);
void cmd_cp(const char* src, const char* dst);
void cmd_mv(const char* src, const char* dst);
void cmd_rm(const char* path);
void cmd_touch(const char* path);
void init_shell();
void interrupt_command();