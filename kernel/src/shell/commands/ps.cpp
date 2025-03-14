#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "printf.hpp"

namespace commands {

void cmd_ps() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("ps", shell_pid);

    printf("  PID  PPID  STATE    NAME\n");

    kernel::Process* current = pm.get_first_process();
    while (current) {
        printf("%5d %5d  ", current->pid, current->ppid);

        switch (current->state) {
            case kernel::ProcessState::Running:
                printf("Running  ");
                break;
            case kernel::ProcessState::Stopped:
                printf("Stopped  ");
                break;
            case kernel::ProcessState::Zombie:
                printf("Zombie   ");
                break;
            case kernel::ProcessState::Ready:
                printf("Ready    ");
                break;
            case kernel::ProcessState::Waiting:
                printf("Waiting  ");
                break;
        }

        printf(current->name);
        printf("\n");

        current = current->next;
    }

    pm.terminate_process(pid);
}

}  // namespace commands