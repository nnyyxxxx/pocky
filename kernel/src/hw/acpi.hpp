#pragma once

#include <cstdint>

namespace kernel {

struct ACPISDTHeader {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};

struct ACPIRSDP {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
};

struct ACPIRSDT {
    ACPISDTHeader header;
    uint32_t pointers[];
};

struct ACPIXSDT {
    ACPISDTHeader header;
    uint64_t pointers[];
};

struct ACPIMADT {
    ACPISDTHeader header;
    uint32_t local_apic_addr;
    uint32_t flags;
};

struct ACPIMADTEntry {
    uint8_t type;
    uint8_t length;
};

struct ACPIMADTLocalAPIC {
    ACPIMADTEntry entry;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags;
};

struct ACPIMADIOAPIC {
    ACPIMADTEntry entry;
    uint8_t id;
    uint8_t reserved;
    uint32_t address;
    uint32_t global_int_base;
};

struct ACPIMADTIntSrcOverride {
    ACPIMADTEntry entry;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t global_sys_interrupt;
    uint16_t flags;
};

struct ACPIMADTNmiSource {
    ACPIMADTEntry entry;
    uint16_t flags;
    uint32_t global_sys_interrupt;
};

struct ACPIMADTLocalNmi {
    ACPIMADTEntry entry;
    uint8_t acpi_processor_id;
    uint16_t flags;
    uint8_t lint_num;
};

struct ACPIMADTLocalAPICOverride {
    ACPIMADTEntry entry;
    uint16_t reserved;
    uint64_t local_apic_address;
};

struct ACPIMADTIOSAPICEntry {
    ACPIMADTEntry entry;
    uint8_t io_apic_id;
    uint8_t reserved;
    uint32_t global_system_interrupt_base;
    uint64_t io_sapic_address;
};

struct ACPIMADTLocalSAPICEntry {
    ACPIMADTEntry entry;
    uint8_t acpi_processor_id;
    uint8_t local_sapic_id;
    uint8_t local_sapic_eid;
    uint8_t reserved[3];
    uint32_t flags;
    uint32_t acpi_processor_uid;
};

struct ACPIMCFGEntry {
    uint64_t base_address;
    uint16_t segment_group;
    uint8_t start_bus;
    uint8_t end_bus;
    uint32_t reserved;
};

struct ACPIMCFG {
    ACPISDTHeader header;
    uint64_t reserved;
    ACPIMCFGEntry entries[];
};

bool init_acpi();
void* find_acpi_table(const char* signature);
ACPIMADT* get_madt();
ACPIMCFG* get_mcfg();

}  // namespace kernel