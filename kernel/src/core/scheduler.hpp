#pragma once

#include <cstdint>

#include "hw/smp.hpp"
#include "lib/vector.hpp"
#include "process.hpp"

namespace kernel {

struct Process;

enum class SchedulerPolicy {
    RoundRobin,
    Priority,
};

struct CPURunQueue {
    Vector<Process*> process_queue;
    size_t current_index = 0;
    uint64_t current_time_slice = 0;
    Process* current = nullptr;
    uint32_t cpu_id = 0;
    bool needs_resched = false;
};

class Scheduler {
public:
    static Scheduler& instance();

    void initialize(SchedulerPolicy policy = SchedulerPolicy::RoundRobin);

    void add_process(Process* process);

    void remove_process(Process* process);

    void schedule();

    void schedule_on_cpu(uint32_t cpu_id);

    void tick();

    void tick_on_cpu(uint32_t cpu_id);

    SchedulerPolicy get_policy() const {
        return m_policy;
    }

    void set_process_priority(pid_t pid, uint8_t priority);

    CPURunQueue* get_current_runqueue();

    CPURunQueue* get_runqueue(uint32_t cpu_id);

    void load_balance();

private:
    Scheduler() = default;
    ~Scheduler() = default;

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    Process* select_next_process(CPURunQueue* runqueue);

    bool can_migrate_process(Process* process, uint32_t from_cpu, uint32_t to_cpu);

    void migrate_process(Process* process, uint32_t from_cpu, uint32_t to_cpu);

    Vector<CPURunQueue> m_runqueues;

    SchedulerPolicy m_policy = SchedulerPolicy::RoundRobin;

    static constexpr uint64_t DEFAULT_TIME_SLICE = 5;
    static constexpr uint64_t LOAD_BALANCE_PERIOD = 100;

    uint64_t m_load_balance_counter = 0;
};

}  // namespace kernel