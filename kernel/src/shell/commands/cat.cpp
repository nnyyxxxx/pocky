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
        printf("cat: missing operand\n");
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::CFat32FileSystem::instance();
    uint32_t cluster = 0;
    uint32_t size = 0;
    uint8_t attributes = 0;

    if (!fs.findFile(path, cluster, size, attributes)) {
        printf("File not found: %s\n", path);
        pm.terminate_process(pid);
        return;
    }

    if (attributes & 0x10) {
        printf("%s is a directory\n", path);
        pm.terminate_process(pid);
        return;
    }

    if (size == 0) {
        pm.terminate_process(pid);
        return;
    }

    constexpr size_t CHUNK_SIZE = 4096;
    uint8_t* buffer = new uint8_t[CHUNK_SIZE];
    size_t remaining = size;
    size_t offset = 0;

    while (remaining > 0) {
        size_t chunk = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;
        fs.readFile(cluster, buffer, chunk);

        for (size_t i = 0; i < chunk; i++) {
            if (buffer[i] == '\n')
                terminal_putchar('\n');
            else if (buffer[i] >= 32 && buffer[i] <= 126)
                terminal_putchar(buffer[i]);
            else
                terminal_putchar('.');
        }

        remaining -= chunk;
        offset += chunk;
    }

    delete[] buffer;
    printf("\n");
    pm.terminate_process(pid);
}

}  // namespace commands