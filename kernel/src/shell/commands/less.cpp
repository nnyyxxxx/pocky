#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "fs/fat32.hpp"
#include "memory/heap.hpp"
#include "pager.hpp"
#include "printf.hpp"

namespace commands {

void cmd_less(const char* path) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("less", shell_pid);

    if (!path) {
        printf("Usage: less <path>\n");
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::CFat32FileSystem::instance();
    uint8_t buffer[1024];
    fs.readFile(ROOT_CLUSTER, buffer, sizeof(buffer));

    bool found = false;
    for (size_t i = 0; i < sizeof(buffer); i += 32) {
        uint8_t* entry = &buffer[i];
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        char name[13] = {0};
        memcpy(name, entry, 11);
        name[11] = '\0';

        if (strcmp(name, path) == 0) {
            found = true;
            uint32_t cluster = (entry[26] | (entry[27] << 8));
            uint32_t size = (entry[28] | (entry[29] << 8) | (entry[30] << 16) | (entry[31] << 24));

            char* text = new char[size + 1];
            if (!text) {
                printf("less: memory allocation failed\n");
                pm.terminate_process(pid);
                return;
            }

            fs.readFile(cluster, reinterpret_cast<uint8_t*>(text), size);
            text[size] = '\0';
            pager::show_text(text);
            delete[] text;
            break;
        }
    }

    if (!found) printf("less: cannot read '%s': No such file\n", path);

    pm.terminate_process(pid);
}

}  // namespace commands