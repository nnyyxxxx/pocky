#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "pager.hpp"

namespace commands {

void cmd_help() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("help", shell_pid);
    const char* help_text = "Available commands:\n\n"
                            "  help     - Display this help message\n"
                            "  echo     - Echo arguments\n"
                            "  clear    - Clear the screen\n"
                            "  crash    - Trigger a kernel panic (for testing)\n"
                            "  shutdown - Power off the system\n"
                            "  memory   - Display memory usage information\n"
                            "  ls       - List directory contents\n"
                            "  mkdir    - Create a new directory\n"
                            "  cd       - Change current directory\n"
                            "  cat      - Display file contents\n"
                            "  mv       - Move or rename a file\n"
                            "  rm       - Remove a file or directory\n"
                            "  touch    - Create an empty file\n"
                            "  edit     - Edit a file\n"
                            "  history  - Display command history\n"
                            "  uptime   - Display system uptime\n"
                            "  time     - Display system time\n"
                            "  ps       - List running processes\n"
                            "  pkill    - Kill a process\n"
                            "  ipctest  - Run IPC test\n"
                            "  cores    - List CPU cores\n";

    pager::show_text(help_text);

    pm.terminate_process(pid);
}

}  // namespace commands