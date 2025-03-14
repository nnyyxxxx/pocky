#include "smp.hpp"

#include <cstring>

#include "acpi.hpp"
#include "io.hpp"
#include "memory/heap.hpp"
#include "memory/physical_memory.hpp"
#include "memory/virtual_memory.hpp"
#include "printf.hpp"

namespace kernel {

constexpr uint32_t LAPIC_ID = 0x20;
constexpr uint32_t LAPIC_VERSION = 0x30;
constexpr uint32_t LAPIC_TPR = 0x80;
constexpr uint32_t LAPIC_APR = 0x90;
constexpr uint32_t LAPIC_PPR = 0xA0;
constexpr uint32_t LAPIC_EOI = 0xB0;
constexpr uint32_t LAPIC_RRD = 0xC0;
constexpr uint32_t LAPIC_LDR = 0xD0;
constexpr uint32_t LAPIC_DFR = 0xE0;
constexpr uint32_t LAPIC_SVR = 0xF0;
constexpr uint32_t LAPIC_ISR = 0x100;
constexpr uint32_t LAPIC_TMR = 0x180;
constexpr uint32_t LAPIC_IRR = 0x200;
constexpr uint32_t LAPIC_ESR = 0x280;
constexpr uint32_t LAPIC_ICR_LOW = 0x300;
constexpr uint32_t LAPIC_ICR_HIGH = 0x310;
constexpr uint32_t LAPIC_TIMER = 0x320;
constexpr uint32_t LAPIC_THERMAL = 0x330;
constexpr uint32_t LAPIC_PERF = 0x340;
constexpr uint32_t LAPIC_LINT0 = 0x350;
constexpr uint32_t LAPIC_LINT1 = 0x360;
constexpr uint32_t LAPIC_ERROR = 0x370;
constexpr uint32_t LAPIC_TIMER_INIT = 0x380;
constexpr uint32_t LAPIC_TIMER_CUR = 0x390;
constexpr uint32_t LAPIC_TIMER_DIV = 0x3E0;

constexpr uint64_t CPU_STACK_SIZE = 16 * 1024;
constexpr uint64_t CPU_TRAMPOLINE_ADDR = 0x8000;
constexpr uint32_t AP_STARTUP_VECTOR = 0x08;

constexpr uint32_t CPUID_FEAT_EDX_APIC = 1 << 9;

extern "C" volatile uint32_t g_ap_ready_count;
extern "C" volatile uint32_t g_ap_lock;
extern "C" volatile uint32_t g_ap_target_cpu;
extern "C" void* g_ap_trampoline_start;
extern "C" void* g_ap_trampoline_end;

namespace {

constexpr uint32_t MSR_APIC_BASE = 0x1B;
constexpr uint64_t MSR_APIC_ENABLE = (1 << 11);
constexpr uint64_t MSR_BSP_FLAG = (1 << 8);

constexpr uint64_t DEFAULT_LAPIC_BASE = 0xFEE00000;

bool check_apic_available() {
    uint32_t eax, ebx, ecx, edx;
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    return (edx & CPUID_FEAT_EDX_APIC) != 0;
}

uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

void lapic_write(void* base, uint32_t reg, uint32_t value) {
    *((volatile uint32_t*)((uint8_t*)base + reg)) = value;
}

void delay(uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        asm volatile("pause");
    }
}

extern "C" void ap_trampoline();
extern "C" void ap_start();

extern "C" void ap_main(uint32_t cpu_id) {
    auto& smp = SMPManager::instance();
    auto* cpu_info = smp.get_cpu_info(cpu_id);

    if (!cpu_info) {
        printf("AP CPU ID %u not found in CPU info table\n", cpu_id);
        for (;;)
            asm volatile("hlt");
    }

    cpu_info->is_active = true;
    __atomic_add_fetch(&g_ap_ready_count, 1, __ATOMIC_SEQ_CST);

    printf("AP CPU %u (LAPIC ID: %u) is now active\n", cpu_id, cpu_info->lapic_id);

    asm volatile("sti");

    for (;;) {
        asm volatile("hlt");
    }
}

}  // namespace

SMPManager& SMPManager::instance() {
    static SMPManager instance;
    return instance;
}

void SMPManager::initialize() {
    if (!check_apic_available()) {
        printf("APIC not available, SMP not supported\n");
        return;
    }

    if (!init_acpi()) {
        printf("ACPI initialization failed, SMP not fully supported\n");
        init_local_apic();
        m_smp_enabled = false;
        return;
    }

    detect_cpus();

    init_local_apic();

    init_io_apic();

    m_smp_enabled = true;

    printf("SMP initialized with %u CPUs\n", m_cpu_count);
}

void SMPManager::detect_cpus() {
    auto* madt = get_madt();
    if (!madt) {
        printf("Could not find MADT, assuming single CPU\n");

        CPUInfo bsp;
        bsp.id = 0;
        bsp.lapic_id = 0;
        bsp.is_bsp = true;
        bsp.is_active = true;
        bsp.local_apic_base = (void*)(uintptr_t)DEFAULT_LAPIC_BASE;

        m_cpus.push_back(bsp);
        m_cpu_count = 1;
        return;
    }

    uint32_t cpu_count = 0;
    auto* entries = (uint8_t*)(madt + 1);
    auto* end = (uint8_t*)madt + madt->header.length;

    while (entries < end) {
        auto* entry = (ACPIMADTEntry*)entries;

        if (entry->type == 0) {
            auto* proc_lapic = (ACPIMADTLocalAPIC*)entry;

            if (proc_lapic->flags & 1) {
                CPUInfo cpu;
                cpu.id = cpu_count++;
                cpu.lapic_id = proc_lapic->apic_id;
                cpu.is_bsp = (read_msr(MSR_APIC_BASE) & MSR_BSP_FLAG) != 0 &&
                             cpu.lapic_id == (read_msr(MSR_APIC_BASE) >> 24);
                cpu.is_active = cpu.is_bsp;
                cpu.local_apic_base = (void*)(uintptr_t)madt->local_apic_addr;

                auto& pmm = PhysicalMemoryManager::instance();
                auto& vmm = VirtualMemoryManager::instance();

                uint64_t stack_start_addr = 0;
                for (uint64_t i = 0; i < CPU_STACK_SIZE; i += 4096) {
                    void* frame_ptr = pmm.allocate_frame();
                    uint64_t frame = frame_ptr ? reinterpret_cast<uintptr_t>(frame_ptr) : 0;

                    if (i == 0) stack_start_addr = frame;
                }

                if (stack_start_addr) {
                    uint64_t stack_virt = stack_start_addr + 0xFFFF800000000000;
                    for (uint64_t i = 0; i < CPU_STACK_SIZE; i += 4096) {
                        vmm.map_page(stack_virt + i, stack_start_addr + i, true);
                    }
                    cpu.kernel_stack = stack_virt + CPU_STACK_SIZE;
                }

                m_cpus.push_back(cpu);
            }
        }

        entries += entry->length;
    }

    m_cpu_count = cpu_count ? cpu_count : 1;

    if (m_cpus.size() == 0) {
        CPUInfo bsp;
        bsp.id = 0;
        bsp.lapic_id = 0;
        bsp.is_bsp = true;
        bsp.is_active = true;
        bsp.local_apic_base = (void*)(uintptr_t)DEFAULT_LAPIC_BASE;

        m_cpus.push_back(bsp);
        m_cpu_count = 1;
    }

    printf("Detected %u CPUs\n", m_cpu_count);
}

void SMPManager::init_local_apic() {
    uint64_t apic_base = read_msr(MSR_APIC_BASE);

    write_msr(MSR_APIC_BASE, apic_base | MSR_APIC_ENABLE);

    auto& vmm = VirtualMemoryManager::instance();
    uint64_t apic_phys = apic_base & 0xFFFFF000;
    uint64_t apic_virt = apic_phys + 0xFFFF800000000000;
    vmm.map_page(apic_virt, apic_phys, true);

    CPUInfo* cpu_info = get_current_cpu_info();
    if (cpu_info) cpu_info->local_apic_base = (void*)apic_virt;

    void* lapic_base = (void*)apic_virt;

    lapic_write(lapic_base, LAPIC_SVR, 0x1FF);

    lapic_write(lapic_base, LAPIC_DFR, 0xFFFFFFFF);
    lapic_write(lapic_base, LAPIC_LDR, 0x01000000);

    lapic_write(lapic_base, LAPIC_TPR, 0);

    lapic_write(lapic_base, LAPIC_LINT0, 0x700);

    lapic_write(lapic_base, LAPIC_LINT1, 0x400);

    lapic_write(lapic_base, LAPIC_TIMER_DIV, 0x3);
    lapic_write(lapic_base, LAPIC_TIMER, 0x10000);

    printf("Local APIC initialized for CPU %u\n", get_current_cpu_id());
}

void SMPManager::init_io_apic() {
    auto* madt = get_madt();
    if (!madt) {
        printf("Could not find MADT, I/O APIC not initialized\n");
        return;
    }

    auto* entries = (uint8_t*)(madt + 1);
    auto* end = (uint8_t*)madt + madt->header.length;

    while (entries < end) {
        auto* entry = (ACPIMADTEntry*)entries;

        if (entry->type == 1) {
            auto* io_apic = (ACPIMADIOAPIC*)entry;

            auto& vmm = VirtualMemoryManager::instance();
            uint64_t ioapic_phys = io_apic->address;
            uint64_t ioapic_virt = ioapic_phys + 0xFFFF800000000000;
            vmm.map_page(ioapic_virt, ioapic_phys, true);

            printf("I/O APIC found at address 0x%x, ID %u, GSI base %u\n", io_apic->address,
                   io_apic->id, io_apic->global_int_base);
        }

        entries += entry->length;
    }
}

void SMPManager::startup_application_processors() {
    if (!m_smp_enabled || m_cpu_count <= 1) {
        printf("SMP not enabled or only one CPU detected, skipping AP startup\n");
        return;
    }

    g_ap_ready_count = 0;
    g_ap_lock = 0;

    auto& vmm = VirtualMemoryManager::instance();

    vmm.map_page(CPU_TRAMPOLINE_ADDR, CPU_TRAMPOLINE_ADDR, true);

    size_t trampoline_size = (size_t)&g_ap_trampoline_end - (size_t)&g_ap_trampoline_start;
    memcpy((void*)CPU_TRAMPOLINE_ADDR, &g_ap_trampoline_start, trampoline_size);

    CPUInfo* bsp_info = nullptr;
    for (size_t i = 0; i < m_cpus.size(); i++) {
        if (m_cpus[i].is_bsp) {
            bsp_info = &m_cpus[i];
            break;
        }
    }

    if (!bsp_info) {
        printf("Could not find BSP info, aborting AP startup\n");
        return;
    }

    for (size_t i = 0; i < m_cpus.size(); i++) {
        auto& cpu = m_cpus[i];

        if (cpu.is_bsp) continue;

        printf("Starting AP CPU %u (LAPIC ID: %u)...\n", cpu.id, cpu.lapic_id);

        g_ap_target_cpu = cpu.id;

        uint32_t icr_high = cpu.lapic_id << 24;
        uint32_t icr_low = 0x4500;

        lapic_write(bsp_info->local_apic_base, LAPIC_ICR_HIGH, icr_high);
        lapic_write(bsp_info->local_apic_base, LAPIC_ICR_LOW, icr_low);

        delay(10000);

        icr_low = 0x4600 | AP_STARTUP_VECTOR;

        for (int j = 0; j < 2; j++) {
            lapic_write(bsp_info->local_apic_base, LAPIC_ICR_HIGH, icr_high);
            lapic_write(bsp_info->local_apic_base, LAPIC_ICR_LOW, icr_low);
            delay(1000);
        }

        for (int j = 0; j < 1000 && !cpu.is_active; j++) {
            delay(1000);
        }

        if (!cpu.is_active) printf("Failed to start AP CPU %u\n", cpu.id);
    }

    detect_active_cores();

    printf("SMP initialization complete: %u/%u CPUs active\n", g_ap_ready_count + 1, m_cpu_count);
}

uint32_t SMPManager::get_current_cpu_id() {
    uint64_t apic_base = read_msr(MSR_APIC_BASE);
    uint32_t current_lapic_id = (apic_base >> 24) & 0xFF;

    for (size_t i = 0; i < m_cpus.size(); i++) {
        if (m_cpus[i].lapic_id == current_lapic_id) return m_cpus[i].id;
    }

    for (size_t i = 0; i < m_cpus.size(); i++) {
        if (m_cpus[i].is_bsp) return m_cpus[i].id;
    }

    return 0;
}

CPUInfo* SMPManager::get_cpu_info(uint32_t id) {
    for (size_t i = 0; i < m_cpus.size(); i++) {
        if (m_cpus[i].id == id) return &m_cpus[i];
    }

    return nullptr;
}

CPUInfo* SMPManager::get_current_cpu_info() {
    return get_cpu_info(get_current_cpu_id());
}

void SMPManager::set_cpu_active(uint32_t lapic_id) {
    for (size_t i = 0; i < m_cpus.size(); i++) {
        if (m_cpus[i].lapic_id == lapic_id) {
            m_cpus[i].is_active = true;
            return;
        }
    }
}

void SMPManager::detect_active_cores() {
    if (!m_smp_enabled || m_cpu_count <= 1) return;

    CPUInfo* current_cpu = get_current_cpu_info();
    if (!current_cpu || !current_cpu->local_apic_base) return;

    for (size_t i = 0; i < m_cpus.size(); i++) {
        auto& cpu = m_cpus[i];

        if (cpu.is_bsp) {
            cpu.is_active = true;
            continue;
        }

        cpu.is_active = (cpu.kernel_stack != 0);
    }

    uint32_t active_count = 0;
    for (size_t i = 0; i < m_cpus.size(); i++) {
        if (m_cpus[i].is_active) active_count++;
    }
}

void SMPManager::send_ipi(uint32_t cpu_id, uint8_t vector) {
    CPUInfo* target_cpu = get_cpu_info(cpu_id);
    if (!target_cpu) return;

    CPUInfo* current_cpu = get_current_cpu_info();
    if (!current_cpu) return;

    uint32_t icr_high = target_cpu->lapic_id << 24;
    uint32_t icr_low = 0x4000 | vector;

    lapic_write(current_cpu->local_apic_base, LAPIC_ICR_HIGH, icr_high);
    lapic_write(current_cpu->local_apic_base, LAPIC_ICR_LOW, icr_low);
}

void SMPManager::send_ipi_all_excluding_self(uint8_t vector) {
    CPUInfo* current_cpu = get_current_cpu_info();
    if (!current_cpu) return;

    uint32_t icr_low = 0xC000 | vector;

    lapic_write(current_cpu->local_apic_base, LAPIC_ICR_HIGH, 0);
    lapic_write(current_cpu->local_apic_base, LAPIC_ICR_LOW, icr_low);
}

}  // namespace kernel