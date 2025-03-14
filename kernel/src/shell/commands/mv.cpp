#include <cstring>

#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "fs/fat32.hpp"

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
        pm.terminate_process(pid);
        return;
    }

    auto& fs = fs::CFat32FileSystem::instance();
    fs.renameFile(src, dst);

    pm.terminate_process(pid);
}

}  // namespace commands