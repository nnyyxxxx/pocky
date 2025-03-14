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
    uint32_t cluster = 0;
    uint32_t size = 0;
    uint8_t attributes = 0;

    if (fs.findFile(path, cluster, size, attributes)) {
        printf("%s already exists\n", path);
        pm.terminate_process(pid);
        return;
    }

    fs.createFile(path, 0x20);

    pm.terminate_process(pid);
}

}  // namespace commands