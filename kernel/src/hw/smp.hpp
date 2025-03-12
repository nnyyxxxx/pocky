#pragma once

#include <cstdint>

#include "lib/vector.hpp"

namespace kernel {

constexpr uint8_t IPI_VECTOR = 0x40;

struct CPUInfo {
    uint32_t id = 0;
    uint32_t lapic_id = 0;
    bool is_bsp = false;
    void* local_apic_base = nullptr;
    uint64_t kernel_stack = 0;
    uint64_t tss_address = 0;
    bool is_active = false;
};

class SMPManager {
public:
    static SMPManager& instance();

    void initialize();

    void startup_application_processors();

    uint32_t get_cpu_count() const {
        return m_cpu_count;
    }

    uint32_t get_current_cpu_id();

    CPUInfo* get_cpu_info(uint32_t id);

    CPUInfo* get_current_cpu_info();

    void set_cpu_active(uint32_t lapic_id);

    void send_ipi(uint32_t cpu_id, uint8_t vector);

    void send_ipi_all_excluding_self(uint8_t vector);

    bool is_smp_enabled() const {
        return m_smp_enabled;
    }

    void detect_active_cores();

private:
    SMPManager() = default;
    ~SMPManager() = default;

    SMPManager(const SMPManager&) = delete;
    SMPManager& operator=(const SMPManager&) = delete;

    void detect_cpus();
    void init_local_apic();
    void init_io_apic();

    uint32_t m_cpu_count = 1;
    Vector<CPUInfo> m_cpus;
    bool m_smp_enabled = false;
};

}  // namespace kernel