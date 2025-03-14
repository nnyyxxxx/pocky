#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "physical_memory.hpp"
#include "printf.hpp"

namespace commands {

void cmd_memory() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("memory", shell_pid);

    auto& pmm = PhysicalMemoryManager::instance();

    size_t free_frames = pmm.get_free_frames();
    size_t total_frames = pmm.get_total_frames();

    if (total_frames == 0 || free_frames > total_frames) {
        set_red();
        printf("Error: Invalid memory state\n");
        reset_color();
        pm.terminate_process(pid);
        return;
    }

    size_t used_frames = total_frames - free_frames;

    constexpr size_t max_size = static_cast<size_t>(-1);
    if (total_frames > max_size / PhysicalMemoryManager::PAGE_SIZE) {
        set_red();
        printf("Error: Memory size too large to represent\n");
        reset_color();
        pm.terminate_process(pid);
        return;
    }

    size_t total_bytes = total_frames * PhysicalMemoryManager::PAGE_SIZE;
    size_t used_bytes = used_frames * PhysicalMemoryManager::PAGE_SIZE;

    constexpr size_t bytes_per_mb = 1024 * 1024;
    double total_mb = static_cast<double>(total_bytes) / bytes_per_mb;
    double used_mb = static_cast<double>(used_bytes) / bytes_per_mb;

    printf("%.2f MB used of available %.2f MB\n", used_mb, total_mb);

    pm.terminate_process(pid);
}

}  // namespace commands