#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "fs/fat32.hpp"
#include "printf.hpp"

namespace commands {

void cmd_touch(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("touch", shell_pid);

    if (!path || !*path) {
        printf("touch: missing operand\n");
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::CFat32FileSystem::instance();
    fs.createFile(path, 0x20);

    pm.terminate_process(pid);
}

}  // namespace commands