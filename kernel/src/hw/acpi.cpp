#include "acpi.hpp"

#include <cstring>

#include "memory/physical_memory.hpp"
#include "memory/virtual_memory.hpp"
#include "printf.hpp"

namespace kernel {

namespace {
ACPIRSDP* g_rsdp = nullptr;
ACPIRSDT* g_rsdt = nullptr;
ACPIXSDT* g_xsdt = nullptr;
ACPIMADT* g_madt = nullptr;
ACPIMCFG* g_mcfg = nullptr;

ACPIRSDP* find_rsdp(uintptr_t start, uintptr_t end) {
    const char signature[] = "RSD PTR ";

    for (uintptr_t addr = start; addr < end; addr += 16) {
        if (memcmp((void*)addr, signature, 8) == 0) {
            ACPIRSDP* rsdp = (ACPIRSDP*)addr;

            uint8_t sum = 0;
            for (size_t i = 0; i < 20; i++) {
                sum += ((uint8_t*)rsdp)[i];
            }

            if (sum == 0) {
                if (rsdp->revision >= 2) {
                    sum = 0;
                    for (size_t i = 0; i < sizeof(ACPIRSDP); i++) {
                        sum += ((uint8_t*)rsdp)[i];
                    }

                    if (sum == 0) return rsdp;
                } else
                    return rsdp;
            }
        }
    }

    return nullptr;
}

bool verify_table_checksum(ACPISDTHeader* table) {
    uint8_t sum = 0;
    for (size_t i = 0; i < table->length; i++) {
        sum += ((uint8_t*)table)[i];
    }

    return sum == 0;
}

void* map_and_verify_table(uintptr_t phys_addr) {
    auto& vmm = VirtualMemoryManager::instance();
    uintptr_t virt_addr = phys_addr + 0xFFFF800000000000;

    vmm.map_page(virt_addr, phys_addr, true);

    ACPISDTHeader* header = (ACPISDTHeader*)virt_addr;

    for (uintptr_t offset = 4096; offset < header->length; offset += 4096) {
        vmm.map_page(virt_addr + offset, phys_addr + offset, true);
    }

    if (!verify_table_checksum(header)) {
        printf("ACPI table checksum failed: %.4s\n", header->signature);
        return nullptr;
    }

    return header;
}

}  // namespace

bool init_acpi() {
    uintptr_t ebda_addr = 0;

    constexpr uintptr_t BDA_EBDA_PTR = 0x40E;

    uint16_t ebda_segment = 0;

    if (BDA_EBDA_PTR < 0x1000) {
        asm volatile("movw (%1), %0" : "=r"(ebda_segment) : "r"(BDA_EBDA_PTR));

        if (ebda_segment > 0 && ebda_segment < 0xA000)
            ebda_addr = static_cast<uintptr_t>(ebda_segment) << 4;
    }

    g_rsdp = nullptr;
    if (ebda_addr > 0 && ebda_addr < 0x100000) {
        g_rsdp = find_rsdp(ebda_addr, ebda_addr + 1024);
    }

    if (!g_rsdp) g_rsdp = find_rsdp(0xE0000, 0x100000);

    if (!g_rsdp) {
        printf("Failed to find ACPI RSDP\n");
        return false;
    }

    printf("ACPI RSDP found at 0x%lx, revision %d\n", (uintptr_t)g_rsdp, g_rsdp->revision);

    if (g_rsdp->revision >= 2 && g_rsdp->xsdt_address) {
        g_xsdt = (ACPIXSDT*)map_and_verify_table(g_rsdp->xsdt_address);

        if (!g_xsdt) {
            printf("Failed to map XSDT\n");
            return false;
        }

        int entries = (g_xsdt->header.length - sizeof(ACPISDTHeader)) / sizeof(uint64_t);

        for (int i = 0; i < entries; i++) {
            auto* table = (ACPISDTHeader*)map_and_verify_table(g_xsdt->pointers[i]);

            if (!table) continue;

            if (memcmp(table->signature, "APIC", 4) == 0)
                g_madt = (ACPIMADT*)table;
            else if (memcmp(table->signature, "MCFG", 4) == 0)
                g_mcfg = (ACPIMCFG*)table;
        }
    } else {
        g_rsdt = (ACPIRSDT*)map_and_verify_table(g_rsdp->rsdt_address);

        if (!g_rsdt) {
            printf("Failed to map RSDT\n");
            return false;
        }

        int entries = (g_rsdt->header.length - sizeof(ACPISDTHeader)) / sizeof(uint32_t);

        for (int i = 0; i < entries; i++) {
            auto* table = (ACPISDTHeader*)map_and_verify_table(g_rsdt->pointers[i]);

            if (!table) continue;

            if (memcmp(table->signature, "APIC", 4) == 0)
                g_madt = (ACPIMADT*)table;
            else if (memcmp(table->signature, "MCFG", 4) == 0)
                g_mcfg = (ACPIMCFG*)table;
        }
    }

    if (!g_madt) {
        printf("Failed to find MADT (APIC) table\n");
        return false;
    }

    printf("ACPI MADT (APIC) table found at 0x%lx\n", (uintptr_t)g_madt);

    return true;
}

void* find_acpi_table(const char* signature) {
    if (!g_rsdt && !g_xsdt) return nullptr;

    if (g_xsdt) {
        int entries = (g_xsdt->header.length - sizeof(ACPISDTHeader)) / sizeof(uint64_t);

        for (int i = 0; i < entries; i++) {
            auto* table = (ACPISDTHeader*)map_and_verify_table(g_xsdt->pointers[i]);

            if (!table) continue;

            if (memcmp(table->signature, signature, 4) == 0) return table;
        }
    } else {
        int entries = (g_rsdt->header.length - sizeof(ACPISDTHeader)) / sizeof(uint32_t);

        for (int i = 0; i < entries; i++) {
            auto* table = (ACPISDTHeader*)map_and_verify_table(g_rsdt->pointers[i]);

            if (!table) continue;

            if (memcmp(table->signature, signature, 4) == 0) return table;
        }
    }

    return nullptr;
}

ACPIMADT* get_madt() {
    return g_madt;
}

ACPIMCFG* get_mcfg() {
    return g_mcfg;
}

}  // namespace kernel