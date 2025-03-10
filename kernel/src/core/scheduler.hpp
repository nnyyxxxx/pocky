#pragma once

#include <cstdint>
#include "process.hpp"
#include "lib/vector.hpp"

namespace kernel {

struct Process;

enum class SchedulerPolicy {
    RoundRobin,
    Priority,
};

class Scheduler {
public:
    static Scheduler& instance();

    void initialize(SchedulerPolicy policy = SchedulerPolicy::RoundRobin);

    void add_process(Process* process);

    void remove_process(Process* process);

    void schedule();

    void tick();

    SchedulerPolicy get_policy() const { return m_policy; }

    void set_process_priority(pid_t pid, uint8_t priority);

private:
    Scheduler() = default;
    ~Scheduler() = default;

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    Process* select_next_process();

    Vector<Process*> m_process_queue;

    SchedulerPolicy m_policy = SchedulerPolicy::RoundRobin;

    size_t m_current_index = 0;

    uint64_t m_current_time_slice = 0;

    static constexpr uint64_t DEFAULT_TIME_SLICE = 5;
};

} // namespace kernel