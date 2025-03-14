#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "io.hpp"
#include "printf.hpp"

namespace commands {

void cmd_shutdown() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("shutdown", shell_pid);

    printf("\nShutting down the system...\n");

    outb(0x604, 0x00);
    outb(0x604, 0x01);

    outb(0xB004, 0x00);
    outb(0x4004, 0x00);

    outb(0x64, 0xFE);

    printf("Shutdown failed.\n");

    asm volatile("cli");
    for (;;) {
        asm volatile("hlt");
    }

    pm.terminate_process(pid);
}

}  // namespace commands