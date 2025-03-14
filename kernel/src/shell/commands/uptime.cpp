#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "printf.hpp"
#include "timer.hpp"

namespace commands {

void cmd_uptime() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("uptime", shell_pid);

    char uptime_str[100] = {0};
    format_uptime(uptime_str, sizeof(uptime_str));

    printf("System uptime: ");
    printf(uptime_str);
    printf("\n");

    pm.terminate_process(pid);
}

}  // namespace commands