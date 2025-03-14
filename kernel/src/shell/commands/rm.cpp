#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "fs/fat32.hpp"
#include "lib/lib.hpp"
#include "lib/string.hpp"
#include "printf.hpp"

namespace commands {

namespace {
template <typename F>
void handle_wildcard_command(const char* pattern, F cmd_fn) {
    auto& fs = fs::CFat32FileSystem::instance();
    uint8_t buffer[1024];
    fs.readFile(ROOT_CLUSTER, buffer, sizeof(buffer));

    for (size_t i = 0; i < sizeof(buffer); i += 32) {
        uint8_t* entry = &buffer[i];
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        char name[13] = {0};
        memcpy(name, entry, 11);
        name[11] = '\0';

        if (match_wildcard(pattern, name)) cmd_fn(name);
    }
}
}  // namespace

void cmd_rm(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("rm", shell_pid);

    if (!path || !*path) {
        printf("rm: missing operand\n");
        pm.terminate_process(pid);
        return;
    }

    if (strchr(path, '*')) {
        handle_wildcard_command(path, [](const char* name) {
            auto& fs = fs::CFat32FileSystem::instance();
            uint32_t cluster, size;
            uint8_t attributes;
            if (fs.findFile(name, cluster, size, attributes)) {
                if (attributes & 0x10)
                    fs.deleteDirectoryRecursive(name);
                else
                    fs.deleteFile(name);
            }
        });
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::CFat32FileSystem::instance();
    uint32_t cluster, size;
    uint8_t attributes;
    if (!fs.findFile(path, cluster, size, attributes)) {
        printf("%s: no such file or directory\n", path);
        pm.terminate_process(pid);
        return;
    }

    if (attributes & 0x10)
        fs.deleteDirectoryRecursive(path);
    else
        fs.deleteFile(path);

    pm.terminate_process(pid);
}

}  // namespace commands