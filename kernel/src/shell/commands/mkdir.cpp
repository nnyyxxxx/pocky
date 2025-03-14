#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "fs/fat32.hpp"

namespace commands {

void cmd_mkdir(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("mkdir", shell_pid);

    if (!path || !*path) {
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::CFat32FileSystem::instance();
    fs.createDirectory(path);

    pm.terminate_process(pid);
}

}  // namespace commands