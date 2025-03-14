#pragma once

namespace commands {

void init_commands();

void cmd_cd(const char* path);
void cmd_ls(const char* path);
void cmd_mkdir(const char* path);
void cmd_cat(const char* path);
void cmd_touch(const char* path);
void cmd_rm(const char* path);
void cmd_mv(const char* args);
void cmd_edit(const char* path);
void cmd_echo(const char* args);
void cmd_pkill(const char* args);
void cmd_less(const char* path);
void cmd_help();
void cmd_crash();
void cmd_shutdown();
void cmd_memory();
void cmd_history();
void cmd_uptime();
void cmd_time();
void cmd_ps();
void cmd_ipc_test();
void cmd_cores();

}  // namespace commands