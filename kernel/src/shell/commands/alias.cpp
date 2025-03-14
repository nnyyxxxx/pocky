#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "fs/fat32.hpp"
#include "printf.hpp"

namespace commands {

struct SAlias {
    char name[64];
    char value[256];
};

static SAlias aliases[100];
static size_t alias_count = 0;
static uint32_t last_file_size = 0;

void load_aliases() {
    auto& fs = fs::CFat32FileSystem::instance();
    uint32_t shell_cluster, file_cluster, size;
    uint8_t attributes;

    if (!fs.findFile("/shell", shell_cluster, size, attributes)) fs.createDirectory("shell");

    if (!fs.findFile("/shell/config", file_cluster, size, attributes)) {
        const char* default_aliases = "c=clear\n"
                                      "..=cd ..\n";
        fs.createFileWithContent("/shell/config", reinterpret_cast<const uint8_t*>(default_aliases),
                                 strlen(default_aliases), 0);
    }

    uint8_t buffer[4096] = {0};
    if (fs.readFileByPath("/shell/config", buffer, sizeof(buffer) - 1)) {
        char* content = reinterpret_cast<char*>(buffer);
        char* line_start = content;
        char* line_end;
        alias_count = 0;

        while (*line_start && alias_count < 100) {
            line_end = strchr(line_start, '\n');
            if (line_end) {
                *line_end = '\0';
                if (*line_start) {
                    char* equals = strchr(line_start, '=');
                    if (equals) {
                        *equals = '\0';
                        strncpy(aliases[alias_count].name, line_start,
                                sizeof(aliases[alias_count].name) - 1);
                        strncpy(aliases[alias_count].value, equals + 1,
                                sizeof(aliases[alias_count].value) - 1);
                        alias_count++;
                    }
                }
                line_start = line_end + 1;
            } else {
                if (*line_start) {
                    char* equals = strchr(line_start, '=');
                    if (equals) {
                        *equals = '\0';
                        strncpy(aliases[alias_count].name, line_start,
                                sizeof(aliases[alias_count].name) - 1);
                        strncpy(aliases[alias_count].value, equals + 1,
                                sizeof(aliases[alias_count].value) - 1);
                        alias_count++;
                    }
                }
                break;
            }
        }
    }
}

const char* get_alias(const char* name) {
    auto& fs = fs::CFat32FileSystem::instance();
    uint32_t cluster, size;
    uint8_t attributes;

    if (fs.findFile("/shell/config", cluster, size, attributes)) {
        if (size != last_file_size) {
            load_aliases();
            last_file_size = size;
        }
    }

    for (size_t i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) return aliases[i].value;
    }
    return nullptr;
}

void cmd_alias() {
    for (size_t i = 0; i < alias_count; i++) {
        printf("%s='%s'\n", aliases[i].name, aliases[i].value);
    }
}

}  // namespace commands