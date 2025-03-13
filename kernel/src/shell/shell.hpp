#pragma once
#include <cstddef>

#include "core/types.hpp"
#include "pager.hpp"
#include "rtc.hpp"

using kernel::pid_t;

extern char input_buffer[256];
extern size_t input_pos;
extern pid_t shell_pid;

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
void cmd_mv(const char* args);
void cmd_rm(const char* path);
void cmd_touch(const char* path);
void cmd_history();
void cmd_uptime();
void cmd_edit(const char* path);
void cmd_graphics();
void cmd_ps();
void cmd_pkill(const char* args);
void cmd_time();
void cmd_less(const char* path);
void cmd_sched_policy(const char* args);
void cmd_priority(const char* args);
void cmd_ipc_test();
void init_shell();
void interrupt_command();
void handle_tab_completion();
void cmd_cores();
void set_red();
void set_green();
void set_blue();
void set_gray();
void set_white();
void reset_color();