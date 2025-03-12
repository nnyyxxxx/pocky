#include "scheduler.hpp"

#include "hw/smp.hpp"
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
    m_load_balance_counter = 0;

    auto& smp = SMPManager::instance();
    uint32_t cpu_count = smp.get_cpu_count();

    m_runqueues.clear();

    for (uint32_t i = 0; i < cpu_count; i++) {
        CPURunQueue runqueue;
        runqueue.current_index = 0;
        runqueue.current_time_slice = DEFAULT_TIME_SLICE;
        runqueue.current = nullptr;
        runqueue.cpu_id = i;
        runqueue.needs_resched = false;

        m_runqueues.push_back(runqueue);
    }
}

void Scheduler::add_process(Process* process) {
    if (!process) return;

    uint32_t target_cpu = 0;
    size_t min_queue_size = SIZE_MAX;

    for (size_t i = 0; i < m_runqueues.size(); i++) {
        if (m_runqueues[i].process_queue.size() < min_queue_size) {
            min_queue_size = m_runqueues[i].process_queue.size();
            target_cpu = m_runqueues[i].cpu_id;
        }
    }

    CPURunQueue* runqueue = get_runqueue(target_cpu);
    if (!runqueue) return;

    for (size_t i = 0; i < runqueue->process_queue.size(); i++) {
        if (runqueue->process_queue[i] == process) return;
    }

    process->state = ProcessState::Ready;
    runqueue->process_queue.push_back(process);
}

void Scheduler::remove_process(Process* process) {
    if (!process) return;

    for (size_t i = 0; i < m_runqueues.size(); i++) {
        auto& runqueue = m_runqueues[i];

        for (size_t j = 0; j < runqueue.process_queue.size(); j++) {
            if (runqueue.process_queue[j] == process) {
                if (runqueue.current_index >= j)
                    if (runqueue.current_index > 0) runqueue.current_index--;

                for (size_t k = j; k < runqueue.process_queue.size() - 1; k++) {
                    runqueue.process_queue[k] = runqueue.process_queue[k + 1];
                }

                runqueue.process_queue.pop_back();

                if (runqueue.current == process) {
                    runqueue.current = nullptr;
                    runqueue.needs_resched = true;
                }

                return;
            }
        }
    }
}

void Scheduler::schedule() {
    auto& smp = SMPManager::instance();
    uint32_t current_cpu = smp.get_current_cpu_id();

    schedule_on_cpu(current_cpu);
}

void Scheduler::schedule_on_cpu(uint32_t cpu_id) {
    CPURunQueue* runqueue = get_runqueue(cpu_id);
    if (!runqueue) return;

    Process* next_process = select_next_process(runqueue);
    if (!next_process) return;

    auto& pm = ProcessManager::instance();
    Process* current = runqueue->current;

    if (current == next_process) return;

    runqueue->current_time_slice = DEFAULT_TIME_SLICE;
    runqueue->current = next_process;

    next_process->last_run = get_ticks();

    if (current) current->state = ProcessState::Ready;

    next_process->state = ProcessState::Running;

    pm.switch_to_process(next_process);
}

void Scheduler::tick() {
    auto& smp = SMPManager::instance();
    uint32_t current_cpu = smp.get_current_cpu_id();

    tick_on_cpu(current_cpu);

    m_load_balance_counter++;
    if (m_load_balance_counter >= LOAD_BALANCE_PERIOD) {
        m_load_balance_counter = 0;

        if (current_cpu == 0) load_balance();
    }
}

void Scheduler::tick_on_cpu(uint32_t cpu_id) {
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0" : "=r"(flags));
    if (!(flags & (1 << 9))) return;

    CPURunQueue* runqueue = get_runqueue(cpu_id);
    if (!runqueue) return;

    Process* current = runqueue->current;

    if (!current) return;

    current->total_runtime++;

    if (current->priority >= 9) return;

    if (runqueue->current_time_slice > 0) runqueue->current_time_slice--;

    if (runqueue->current_time_slice == 0) {
        runqueue->needs_resched = true;

        auto& smp = SMPManager::instance();
        if (cpu_id == smp.get_current_cpu_id())
            schedule_on_cpu(cpu_id);
        else
            smp.send_ipi(cpu_id, IPI_VECTOR);
    }
}

Process* Scheduler::select_next_process(CPURunQueue* runqueue) {
    if (!runqueue || runqueue->process_queue.size() == 0) return nullptr;

    Process* selected = nullptr;

    switch (m_policy) {
        case SchedulerPolicy::RoundRobin: {
            size_t start_index = runqueue->current_index;

            do {
                runqueue->current_index =
                    (runqueue->current_index + 1) % runqueue->process_queue.size();

                if (runqueue->process_queue[runqueue->current_index]->state ==
                    ProcessState::Ready) {
                    selected = runqueue->process_queue[runqueue->current_index];
                    break;
                }

            } while (runqueue->current_index != start_index && !selected);

            break;
        }

        case SchedulerPolicy::Priority: {
            uint8_t highest_priority = 0;
            size_t index = 0;

            for (size_t i = 0; i < runqueue->process_queue.size(); i++) {
                auto* process = runqueue->process_queue[i];

                if (process->state == ProcessState::Ready && process->priority > highest_priority) {
                    highest_priority = process->priority;
                    selected = process;
                    index = i;
                }
            }

            if (selected) runqueue->current_index = index;

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

CPURunQueue* Scheduler::get_current_runqueue() {
    auto& smp = SMPManager::instance();
    uint32_t current_cpu = smp.get_current_cpu_id();

    return get_runqueue(current_cpu);
}

CPURunQueue* Scheduler::get_runqueue(uint32_t cpu_id) {
    for (size_t i = 0; i < m_runqueues.size(); i++) {
        if (m_runqueues[i].cpu_id == cpu_id) return &m_runqueues[i];
    }

    return nullptr;
}

bool Scheduler::can_migrate_process(Process* process, uint32_t from_cpu, uint32_t to_cpu) {
    (void)from_cpu;

    if (!process) return false;

    if (process->state == ProcessState::Running) return false;

    if (process->priority >= 8) return false;

    CPURunQueue* to_runqueue = get_runqueue(to_cpu);
    if (!to_runqueue) return false;

    return true;
}

void Scheduler::migrate_process(Process* process, uint32_t from_cpu, uint32_t to_cpu) {
    if (!process || from_cpu == to_cpu) return;

    CPURunQueue* from_runqueue = get_runqueue(from_cpu);
    CPURunQueue* to_runqueue = get_runqueue(to_cpu);

    if (!from_runqueue || !to_runqueue) return;

    for (size_t i = 0; i < from_runqueue->process_queue.size(); i++) {
        if (from_runqueue->process_queue[i] == process) {
            if (from_runqueue->current_index >= i)
                if (from_runqueue->current_index > 0) from_runqueue->current_index--;

            for (size_t j = i; j < from_runqueue->process_queue.size() - 1; j++) {
                from_runqueue->process_queue[j] = from_runqueue->process_queue[j + 1];
            }

            from_runqueue->process_queue.pop_back();
            break;
        }
    }

    to_runqueue->process_queue.push_back(process);
}

void Scheduler::load_balance() {
    size_t max_processes = 0;
    size_t min_processes = SIZE_MAX;
    uint32_t max_cpu = 0;
    uint32_t min_cpu = 0;

    for (size_t i = 0; i < m_runqueues.size(); i++) {
        size_t queue_size = m_runqueues[i].process_queue.size();

        if (queue_size > max_processes) {
            max_processes = queue_size;
            max_cpu = m_runqueues[i].cpu_id;
        }

        if (queue_size < min_processes) {
            min_processes = queue_size;
            min_cpu = m_runqueues[i].cpu_id;
        }
    }

    if (max_processes - min_processes <= 1) return;

    size_t to_move = (max_processes - min_processes) / 2;

    CPURunQueue* max_runqueue = get_runqueue(max_cpu);
    CPURunQueue* min_runqueue = get_runqueue(min_cpu);

    if (!max_runqueue || !min_runqueue) return;

    size_t moved = 0;

    for (size_t i = 0; i < max_runqueue->process_queue.size() && moved < to_move; i++) {
        Process* process = max_runqueue->process_queue[i];

        if (can_migrate_process(process, max_cpu, min_cpu)) {
            migrate_process(process, max_cpu, min_cpu);
            moved++;

            i--;
        }
    }
}

}  // namespace kernel