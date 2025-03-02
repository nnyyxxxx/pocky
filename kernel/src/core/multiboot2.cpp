#include "multiboot2.hpp"

#include "terminal.hpp"

static uintptr_t multiboot_info_addr = 0;
static const kernel::MultibootMemoryMapTag* memory_map = nullptr;

static void early_print(const char* msg, uint64_t value = 0) {
    volatile uint16_t* vga = reinterpret_cast<uint16_t*>(0xB8000);
    static int y = 0;

    for (int i = 0; msg[i] != '\0' && i < 80; i++) {
        vga[y * 80 + i] = 0x0F00 | msg[i];
    }

    if (value != 0) {
        const char* hex_chars = "0123456789ABCDEF";
        for (int i = 0; i < 16; i++) {
            int pos = 70 - i;
            if (pos >= 0) vga[y * 80 + pos] = 0x0F00 | hex_chars[(value >> (i * 4)) & 0xF];
        }
    }

    y++;
    if (y >= 25) y = 0;
}

static bool validate_multiboot_info(uintptr_t mb_info_addr) {
    if (mb_info_addr == 0) {
        early_print("FATAL: Multiboot info address is zero");
        return false;
    }

    early_print("Multiboot info addr:", mb_info_addr);

    if (mb_info_addr > 0xFFFFFFFF - 8) {
        early_print("FATAL: Multiboot info address out of range");
        return false;
    }

    uint32_t total_size = *reinterpret_cast<uint32_t*>(mb_info_addr);
    if (total_size < 8) {
        early_print("FATAL: Invalid multiboot info size:", total_size);
        return false;
    }

    early_print("Multiboot info size:", total_size);

    if (mb_info_addr & 0x7) {
        early_print("FATAL: Multiboot info misaligned:", mb_info_addr);
        return false;
    }

    if (total_size > 0x10000) early_print("WARNING: Unusually large multiboot size:", total_size);

    bool found_valid_tag = false;
    for (uintptr_t tag_addr = mb_info_addr + 8; tag_addr < mb_info_addr + total_size;) {
        if (tag_addr > 0xFFFFFFFF - 8) {
            early_print("FATAL: Tag address out of range:", tag_addr);
            return false;
        }

        auto tag = reinterpret_cast<kernel::MultibootInfoTag*>(tag_addr);
        early_print("Tag type:", tag->type);

        if (tag->size < 8) {
            early_print("ERROR: Invalid tag size, stopping", tag->size);
            return false;
        }

        early_print("Tag size:", tag->size);

        if (tag->type == 0 && tag->size == 8) {
            early_print("Found end tag - valid structure");
            found_valid_tag = true;
            break;
        }

        if (tag_addr > 0xFFFFFFFF - tag->size - 7) {
            early_print("FATAL: Tag size overflow");
            return false;
        }

        tag_addr = (tag_addr + tag->size + 7) & ~7;
        found_valid_tag = true;
    }

    if (!found_valid_tag) {
        early_print("FATAL: No valid tags found");
        return false;
    }

    early_print("Multiboot validation complete");
    return true;
}

extern "C" void init_multiboot(uintptr_t mb_info_addr) {
    early_print("Initializing multiboot");
    early_print("Info address:", mb_info_addr);

    multiboot_info_addr = mb_info_addr;

    if (multiboot_info_addr == 0) {
        early_print("FATAL: Null multiboot info address");
        return;
    }

    if (!validate_multiboot_info(multiboot_info_addr)) {
        early_print("FATAL: Multiboot validation failed");
        return;
    }

    uint32_t total_size = *reinterpret_cast<uint32_t*>(multiboot_info_addr);

    for (uintptr_t tag_addr = multiboot_info_addr + 8;
         tag_addr < multiboot_info_addr + total_size;) {
        auto tag = reinterpret_cast<kernel::MultibootInfoTag*>(tag_addr);

        if (tag->type == 0 && tag->size == 8) break;

        if (tag->type == static_cast<uint32_t>(kernel::MultibootInfoType::MemoryMap)) {
            memory_map = reinterpret_cast<const kernel::MultibootMemoryMapTag*>(tag);
            early_print("Found memory map tag");

            if (memory_map->entry_size > 0) {
                early_print("Memory map entry size:", memory_map->entry_size);
                early_print("Memory map entry count:",
                            (memory_map->size - sizeof(kernel::MultibootMemoryMapTag)) /
                                memory_map->entry_size);
            }
        }

        tag_addr = (tag_addr + tag->size + 7) & ~7;
    }

    early_print("Multiboot init complete");
}

namespace kernel {

const MultibootMemoryMapTag* get_memory_map() {
    return memory_map;
}

}  // namespace kernel