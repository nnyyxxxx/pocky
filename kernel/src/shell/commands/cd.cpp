#include <cstdio>
#include <cstring>

#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "fs/fat32.hpp"

namespace commands {

void cmd_cd(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("cd", shell_pid);

    if (!path || !*path) {
        auto& fs = fs::CFat32FileSystem::instance();
        fs.set_current_path("/");
        fs.set_current_dir_name("/");
        fs.set_current_directory_cluster(ROOT_CLUSTER);
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::CFat32FileSystem::instance();

    if (strcmp(path, "/") == 0) {
        fs.set_current_path("/");
        fs.set_current_dir_name("/");
        fs.set_current_directory_cluster(ROOT_CLUSTER);
        pm.terminate_process(pid);
        return;
    }

    if (strcmp(path, "..") == 0) {
        if (fs.pop_directory()) {
            pm.terminate_process(pid);
            return;
        }

        fs.set_current_path("/");
        fs.set_current_dir_name("/");
        fs.set_current_directory_cluster(ROOT_CLUSTER);
        pm.terminate_process(pid);
        return;
    }

    uint8_t buffer[1024];
    uint32_t current_dir_cluster = fs.get_current_directory_cluster();
    fs.readFile(current_dir_cluster, buffer, sizeof(buffer));

    for (size_t i = 0; i < sizeof(buffer); i += 32) {
        uint8_t* entry = &buffer[i];
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        char name[13] = {0};
        memcpy(name, entry, 11);
        name[11] = '\0';

        for (int j = 10; j >= 0; j--) {
            if (name[j] == ' ')
                name[j] = '\0';
            else
                break;
        }

        if (strcmp(name, path) == 0) {
            if (!(entry[11] & 0x10)) {
                pm.terminate_process(pid);
                return;
            }

            uint32_t dir_cluster = (entry[26] | (entry[27] << 8));

            char new_path[MAX_PATH] = {0};
            if (strcmp(fs.get_current_path(), "/") == 0)
                snprintf(new_path, sizeof(new_path), "/%s", path);
            else
                snprintf(new_path, sizeof(new_path), "%s/%s", fs.get_current_path(), path);

            fs.push_directory(path, new_path, dir_cluster);

            fs.set_current_path(new_path);
            fs.set_current_dir_name(path);
            fs.set_current_directory_cluster(dir_cluster);
            break;
        }
    }

    pm.terminate_process(pid);
}

}  // namespace commands