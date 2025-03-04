#pragma once

#include <cstdint>
#include <cstddef>
#include "types.hpp"

namespace kernel {

enum class ProcessState {
    Running,
    Stopped,
    Zombie
};

struct Process {
    pid_t pid = 0;
    pid_t ppid = 0;
    const char* name = nullptr;
    ProcessState state = ProcessState::Stopped;
    Process* next = nullptr;
};

class ProcessManager {
public:
    static ProcessManager& instance();

    pid_t create_process(const char* name, pid_t ppid = 0);
    void terminate_process(pid_t pid);
    Process* get_process(pid_t pid);
    Process* get_first_process() { return m_first_process; }
    pid_t get_next_pid() { return m_next_pid; }

private:
    ProcessManager() = default;
    ~ProcessManager() = default;
    ProcessManager(const ProcessManager&) = delete;
    ProcessManager& operator=(const ProcessManager&) = delete;

    Process* m_first_process = nullptr;
    pid_t m_next_pid = 1;
};

}