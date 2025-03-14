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

constexpr size_t MAX_PATH = 256;
constexpr uint32_t ROOT_CLUSTER = 2;

void process_keypress(char c);
void process_command();
void print_prompt();
void init_shell();
void interrupt_command();
void handle_tab_completion();
void initialize_filesystem();

void set_red();
void set_green();
void set_blue();
void set_gray();
void set_white();
void reset_color();