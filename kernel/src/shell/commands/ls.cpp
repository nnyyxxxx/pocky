#include <cstring>

#include "../shell.hpp"
#include "core/process.hpp"
#include "fs/fat32.hpp"
#include "printf.hpp"

namespace commands {

namespace {
void list_callback(const char* name, uint8_t attributes, uint32_t size) {
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return;

    if (attributes & 0x10)
        printf("d ");
    else {
        printf("f ");
        if (size < 1024)
            printf("(%u B) ", size);
        else if (size < 1024 * 1024)
            printf("(%u KB) ", size / 1024);
        else
            printf("(%u MB) ", size / (1024 * 1024));
    }

    printf("%s\n", name);
}
}  // namespace

void cmd_ls(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("ls", shell_pid);

    auto& fs = fs::CFat32FileSystem::instance();
    uint8_t buffer[1024];

    uint32_t cluster = 0;
    uint32_t size = 0;
    uint8_t attributes = 0;

    if (path && *path) {
        if (!fs.findFile(path, cluster, size, attributes)) {
            printf("Directory not found: %s\n", path);
            pm.terminate_process(pid);
            return;
        }

        if (!(attributes & 0x10)) {
            printf("Not a directory: %s\n", path);
            pm.terminate_process(pid);
            return;
        }
    } else
        cluster = fs.get_current_directory_cluster();

    fs.readFile(cluster, buffer, sizeof(buffer));

    for (size_t i = 0; i < sizeof(buffer); i += 32) {
        uint8_t* entry = &buffer[i];
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        char name[13] = {0};
        memcpy(name, entry, 11);

        for (int j = 10; j >= 0; j--) {
            if (name[j] == ' ')
                name[j] = '\0';
            else
                break;
        }

        if (name[0] == '\0') continue;

        uint8_t attributes = entry[11];
        uint32_t size = (entry[28] | (entry[29] << 8) | (entry[30] << 16) | (entry[31] << 24));

        list_callback(name, attributes, size);
    }

    pm.terminate_process(pid);
}

}  // namespace commands