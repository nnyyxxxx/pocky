#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "editor.hpp"

namespace commands {

void cmd_edit(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("edit", shell_pid);

    editor::cmd_edit(path);

    pm.terminate_process(pid);
}

}  // namespace commands