#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "core/scheduler.hpp"
#include "printf.hpp"

namespace commands {

void cmd_cores() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("cores", shell_pid);

    auto& smp = kernel::SMPManager::instance();

    smp.detect_active_cores();

    uint32_t cpu_count = smp.get_cpu_count();
    printf("ID | LAPIC ID | BSP | Active | Stack Address\n");
    printf("---+---------+-----+--------+-------------\n");

    uint32_t active_count = 0;
    for (uint32_t i = 0; i < cpu_count; i++) {
        auto* cpu_info = smp.get_cpu_info(i);
        if (cpu_info && cpu_info->is_active) active_count++;
    }

    for (uint32_t i = 0; i < cpu_count; i++) {
        auto* cpu_info = smp.get_cpu_info(i);
        if (cpu_info) {
            printf("%2u | %7u | %3s | %6s | 0x%016lx\n", cpu_info->id, cpu_info->lapic_id,
                   cpu_info->is_bsp ? "Yes" : "No", cpu_info->is_active ? "Yes" : "No",
                   cpu_info->kernel_stack);
        }
    }

    pm.terminate_process(pid);
}

}  // namespace commands