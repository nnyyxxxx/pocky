#include "scheduler.hpp"

#include "hw/timer.hpp"
#include "lib/vector.hpp"
#include "process.hpp"

namespace kernel {

namespace {
extern "C" void switch_context(RegisterState* old_state, RegisterState* new_state);
}

Scheduler& Scheduler::instance() {
    static Scheduler instance;
    return instance;
}

void Scheduler::initialize(SchedulerPolicy policy) {
    m_policy = policy;
    m_current_index = 0;
    m_current_time_slice = DEFAULT_TIME_SLICE;
}

void Scheduler::add_process(Process* process) {
    for (size_t i = 0; i < m_process_queue.size(); i++) {
        if (m_process_queue[i] == process) return;
    }

    process->state = ProcessState::Ready;

    m_process_queue.push_back(process);
}

void Scheduler::remove_process(Process* process) {
    for (size_t i = 0; i < m_process_queue.size(); i++) {
        if (m_process_queue[i] == process) {
            if (m_current_index >= i) {
                if (m_current_index > 0) m_current_index--;
            }

            for (size_t j = i; j < m_process_queue.size() - 1; j++) {
                m_process_queue[j] = m_process_queue[j + 1];
            }

            m_process_queue.pop_back();
            return;
        }
    }
}

void Scheduler::schedule() {
    Process* next_process = select_next_process();
    if (!next_process) return;

    auto& pm = ProcessManager::instance();
    Process* current = pm.get_current_process();

    if (current == next_process) return;

    m_current_time_slice = DEFAULT_TIME_SLICE;

    next_process->last_run = get_ticks();

    if (current) current->state = ProcessState::Ready;

    next_process->state = ProcessState::Running;

    pm.switch_to_process(next_process);
}

void Scheduler::tick() {
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0" : "=r"(flags));
    if (!(flags & (1 << 9))) return;

    auto& pm = ProcessManager::instance();
    Process* current = pm.get_current_process();

    if (!current) return;

    current->total_runtime++;

    if (current->priority >= 9) return;

    if (m_current_time_slice > 0) m_current_time_slice--;

    if (m_current_time_slice == 0) schedule();
}

Process* Scheduler::select_next_process() {
    if (m_process_queue.size() == 0) return nullptr;

    Process* selected = nullptr;

    switch (m_policy) {
        case SchedulerPolicy::RoundRobin: {
            size_t start_index = m_current_index;

            do {
                m_current_index = (m_current_index + 1) % m_process_queue.size();

                if (m_process_queue[m_current_index]->state == ProcessState::Ready) {
                    selected = m_process_queue[m_current_index];
                    break;
                }

            } while (m_current_index != start_index && !selected);

            break;
        }

        case SchedulerPolicy::Priority: {
            uint8_t highest_priority = 0;
            size_t index = 0;

            for (size_t i = 0; i < m_process_queue.size(); i++) {
                auto* process = m_process_queue[i];

                if (process->state == ProcessState::Ready && process->priority > highest_priority) {
                    highest_priority = process->priority;
                    selected = process;
                    index = i;
                }
            }

            if (selected) m_current_index = index;

            break;
        }
    }

    return selected;
}

void Scheduler::set_process_priority(pid_t pid, uint8_t priority) {
    auto& pm = ProcessManager::instance();
    Process* process = pm.get_process(pid);

    if (process) {
        if (priority > 10) priority = 10;

        process->priority = priority;
    }
}

}  // namespace kernel