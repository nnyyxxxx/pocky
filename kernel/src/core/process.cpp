#include "process.hpp"

#include <cstring>

#include "memory/heap.hpp"

namespace kernel {

namespace {
char* strdup(const char* str) {
    size_t len = strlen(str) + 1;
    char* new_str = new char[len];
    memcpy(new_str, str, len);
    return new_str;
}
}  // namespace

ProcessManager& ProcessManager::instance() {
    static ProcessManager instance;
    return instance;
}

pid_t ProcessManager::create_process(const char* name, pid_t ppid) {
    auto* process = new Process;
    process->pid = m_next_pid++;
    process->ppid = ppid;
    process->name = strdup(name);
    process->state = ProcessState::Running;
    process->next = m_first_process;
    m_first_process = process;
    return process->pid;
}

void ProcessManager::terminate_process(pid_t pid) {
    Process* prev = nullptr;
    Process* current = m_first_process;

    while (current) {
        if (current->pid == pid) {
            if (prev)
                prev->next = current->next;
            else
                m_first_process = current->next;

            delete[] current->name;
            delete current;
            return;
        }
        prev = current;
        current = current->next;
    }
}

Process* ProcessManager::get_process(pid_t pid) {
    Process* current = m_first_process;
    while (current) {
        if (current->pid == pid) return current;
        current = current->next;
    }
    return nullptr;
}

}  // namespace kernel