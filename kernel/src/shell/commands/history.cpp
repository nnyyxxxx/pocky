#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "printf.hpp"

namespace commands {

void cmd_history() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("history", shell_pid);

    for (size_t i = 0; i < history_count; i++) {
        print_number(i + 1, 10, 4, false);
        printf("  ");
        printf(command_history[i]);
        printf("\n");
    }

    pm.terminate_process(pid);
}

}  // namespace commands