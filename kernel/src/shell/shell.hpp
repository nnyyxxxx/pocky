#pragma once
#include <cstddef>

extern char input_buffer[256];
extern size_t input_pos;
extern bool handling_exception;
extern bool command_running;

constexpr size_t MAX_HISTORY_SIZE = 1000;
extern char command_history[MAX_HISTORY_SIZE][256];
extern size_t history_count;

void process_keypress(char c);
void process_command();
void print_prompt();
void cmd_help();
void cmd_echo(const char* args);
void cmd_crash();
void cmd_shutdown();
void cmd_memory();
void cmd_ls(const char* path);
void cmd_mkdir(const char* path);
void cmd_cd(const char* path);
void cmd_cat(const char* path);
void cmd_cp(const char* args);
void cmd_mv(const char* args);
void cmd_rm(const char* path);
void cmd_touch(const char* path);
void cmd_history();
void cmd_uptime();
void cmd_edit(const char* path);
void init_shell();
void interrupt_command();
void handle_tab_completion();