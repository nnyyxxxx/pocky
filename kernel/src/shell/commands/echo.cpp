#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "printf.hpp"

namespace commands {

void cmd_echo(const char* args) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("echo", shell_pid);

    printf(args);
    printf("\n");

    pm.terminate_process(pid);
}

}  // namespace commands