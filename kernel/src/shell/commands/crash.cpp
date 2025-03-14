#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"

namespace commands {

void cmd_crash() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("crash", shell_pid);

    asm volatile("ud2");

    pm.terminate_process(pid);
}

}  // namespace commands