#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "printf.hpp"

namespace commands {

void cmd_pkill(const char* args) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("pkill", shell_pid);

    if (!args) {
        printf("pkill: missing process id\n");
        pm.terminate_process(pid);
        return;
    }

    pid_t target_pid = 0;
    while (*args) {
        if (*args < '0' || *args > '9') {
            printf("pkill: invalid process id\n");
            pm.terminate_process(pid);
            return;
        }
        target_pid = target_pid * 10 + (*args - '0');
        args++;
    }

    if (target_pid == shell_pid) {
        printf("pkill: cannot kill shell process\n");
        pm.terminate_process(pid);
        return;
    }

    kernel::Process* target = pm.get_process(target_pid);
    if (!target) {
        printf("pkill: no such process\n");
        pm.terminate_process(pid);
        return;
    }

    pm.terminate_process(target_pid);
    pm.terminate_process(pid);
}

}  // namespace commands