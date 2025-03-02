#pragma once

#include <cstdint>

extern "C" void init_multiboot(uintptr_t mb_info_addr);

namespace kernel {

constexpr uint32_t MULTIBOOT2_MAGIC = 0x36d76289;
constexpr uint32_t MULTIBOOT2_HEADER_MAGIC = 0xE85250D6;
constexpr uint32_t MULTIBOOT2_ARCHITECTURE = 0;

enum class MultibootTagType : uint16_t {
    End = 0,
    Information = 1,
    AddressInfo = 2,
    EntryPointAddress = 3,
    ConsoleFlags = 4,
    Framebuffer = 5,
    ModuleAlignment = 6,
    EFI_BS = 7,
    EntryPointAddressEFI32 = 8,
    EntryPointAddressEFI64 = 9,
    Relocatable = 10,
};

enum class MultibootInfoType : uint32_t {
    End = 0,
    CmdLine = 1,
    BootLoaderName = 2,
    Module = 3,
    BasicMemoryInfo = 4,
    BIOSBootDevice = 5,
    MemoryMap = 6,
    VBEInfo = 7,
    FramebufferInfo = 8,
    ELFSymbols = 9,
    APMTable = 10,
    EFI32SystemTablePtr = 11,
    EFI64SystemTablePtr = 12,
    SMBIOSTables = 13,
    ACPIRSDPv1 = 14,
    ACPIRSDPv2 = 15,
    NetworkInfo = 16,
    EFIMemoryMap = 17,
    EFIBootServicesNotTerminated = 18,
    EFI32ImageHandle = 19,
    EFI64ImageHandle = 20,
    ImageLoadBase = 21,
};

struct alignas(8) MultibootInfoTag {
    uint32_t type;
    uint32_t size;
};

struct MultibootMemoryMapEntry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t reserved;
};

constexpr uint32_t MULTIBOOT_MEMORY_AVAILABLE = 1;
constexpr uint32_t MULTIBOOT_MEMORY_RESERVED = 2;
constexpr uint32_t MULTIBOOT_MEMORY_ACPI_RECLAIMABLE = 3;
constexpr uint32_t MULTIBOOT_MEMORY_NVS = 4;
constexpr uint32_t MULTIBOOT_MEMORY_BADRAM = 5;

struct MultibootMemoryMapTag {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
};

const MultibootMemoryMapTag* get_memory_map();

} // namespace kernel