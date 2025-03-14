#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "printf.hpp"
#include "rtc.hpp"

namespace commands {

void cmd_time() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t pid = pm.create_process("time", shell_pid);

    RTCTime time = get_rtc_time();
    printf("Current UTC time: %02u:%02u:%02u\n", time.hours, time.minutes, time.seconds);

    pm.terminate_process(pid);
}

}  // namespace commands