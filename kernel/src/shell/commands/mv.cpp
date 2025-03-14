#include <cstring>

#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "fs/fat32.hpp"
#include "printf.hpp"

namespace commands {

namespace {
void split_path(const char* input, char* first, char* second) {
    const char* space = strchr(input, ' ');
    if (!space) {
        strcpy(first, input);
        second[0] = '\0';
        return;
    }

    size_t first_len = space - input;
    strncpy(first, input, first_len);
    first[first_len] = '\0';

    while (*space == ' ')
        space++;
    strcpy(second, space);
}
}  // namespace

void cmd_mv(const char* args) {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("mv", shell_pid);

    char src[MAX_PATH];
    char dst[MAX_PATH];
    split_path(args, src, dst);

    if (!*src || !*dst) {
        printf("mv: missing source or destination operand\n");
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::CFat32FileSystem::instance();
    uint32_t src_cluster = 0;
    uint32_t src_size = 0;
    uint8_t src_attributes = 0;

    if (!fs.findFile(src, src_cluster, src_size, src_attributes)) {
        printf("mv: cannot stat '%s': No such file or directory\n", src);
        pm.terminate_process(pid);
        return;
    }

    fs.renameFile(src, dst);

    pm.terminate_process(pid);
}

}  // namespace commands