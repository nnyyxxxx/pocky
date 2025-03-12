#pragma once

#include <cstdint>

#include "elf.hpp"

namespace kernel {

using pid_t = int32_t;

enum class ProcessState {
    Running,
    Stopped,
    Zombie,
    Ready,
    Waiting,
};

struct RegisterState {
    uint64_t rax = 0;
    uint64_t rbx = 0;
    uint64_t rcx = 0;
    uint64_t rdx = 0;
    uint64_t rsi = 0;
    uint64_t rdi = 0;
    uint64_t rbp = 0;
    uint64_t rsp = 0;
    uint64_t r8 = 0;
    uint64_t r9 = 0;
    uint64_t r10 = 0;
    uint64_t r11 = 0;
    uint64_t r12 = 0;
    uint64_t r13 = 0;
    uint64_t r14 = 0;
    uint64_t r15 = 0;
    uint64_t rip = 0;
    uint64_t rflags = 0;
    uint64_t cs = 0;
    uint64_t ss = 0;
};

struct MemoryRegion {
    uint64_t start = 0;
    uint64_t size = 0;
    bool writable = false;
    bool executable = false;
    MemoryRegion* next = nullptr;
};

struct Process {
    pid_t pid = 0;
    pid_t ppid = 0;
    char* name = nullptr;
    ProcessState state = ProcessState::Stopped;
    Process* next = nullptr;

    uint64_t entry_point = 0;
    MemoryRegion* memory_regions = nullptr;
    uint64_t brk = 0;
    uint64_t program_break = 0;

    RegisterState registers;
    uint64_t kernel_stack = 0;
    uint64_t user_stack = 0;

    char** argv = nullptr;
    char** envp = nullptr;
    int argc = 0;

    uint8_t priority = 5;
    uint64_t total_runtime = 0;
    uint64_t last_run = 0;
    void* waiting_on = nullptr;
};

class ProcessManager {
public:
    static ProcessManager& instance();

    pid_t create_process(const char* name, pid_t ppid);
    void terminate_process(pid_t pid);
    Process* get_process(pid_t pid);
    Process* get_first_process() {
        return m_first_process;
    }
    Process* get_current_process() {
        return m_current_process;
    }
    void set_current_process(Process* process) {
        m_current_process = process;
    }

    bool load_program(Process* process, const char* path);
    void cleanup_process_memory(Process* process);
    bool setup_process_stack(Process* process, char* const argv[], char* const envp[]);
    void switch_to_process(Process* process);

private:
    ProcessManager() = default;
    ~ProcessManager() = default;

    Process* m_first_process = nullptr;
    Process* m_current_process = nullptr;
    pid_t m_next_pid = 1;

    static constexpr uint64_t USER_STACK_SIZE = 8 * 1024 * 1024;
    static constexpr uint64_t KERNEL_STACK_SIZE = 16 * 1024;
    static constexpr uint64_t USER_STACK_TOP = 0x7FFFFFFFFFFF;
};

}  // namespace kernel