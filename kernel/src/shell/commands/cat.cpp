#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "fs/fat32.hpp"
#include "memory/heap.hpp"
#include "printf.hpp"
#include "terminal.hpp"

namespace commands {

void cmd_cat(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("cat", shell_pid);

    if (!path || !*path) {
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::CFat32FileSystem::instance();
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
            if (entry[11] & 0x10) {
                pm.terminate_process(pid);
                return;
            }

            uint32_t cluster = (entry[26] | (entry[27] << 8));
            uint32_t size = (entry[28] | (entry[29] << 8) | (entry[30] << 16) | (entry[31] << 24));

            if (size == 0) {
                pm.terminate_process(pid);
                return;
            }

            uint8_t* content = new uint8_t[size];
            fs.readFile(cluster, content, size);

            for (size_t j = 0; j < size; j++) {
                if (content[j] == '\n')
                    terminal_putchar('\n');
                else if (content[j] >= 32 && content[j] <= 126)
                    terminal_putchar(content[j]);
                else
                    terminal_putchar('.');
            }

            delete[] content;
            break;
        }
    }

    printf("\n");
    pm.terminate_process(pid);
}

}  // namespace commands