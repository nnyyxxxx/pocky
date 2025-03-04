#include "process.hpp"

#include <cstring>

#include "fs/filesystem.hpp"
#include "memory/heap.hpp"
#include "memory/physical_memory.hpp"
#include "memory/virtual_memory.hpp"

namespace kernel {

namespace {
char* strdup(const char* str) {
    size_t len = strlen(str) + 1;
    char* new_str = new char[len];
    memcpy(new_str, str, len);
    return new_str;
}

void add_memory_region(Process* process, uint64_t start, uint64_t size, bool writable,
                       bool executable) {
    auto* region = new MemoryRegion;
    region->start = start;
    region->size = size;
    region->writable = writable;
    region->executable = executable;
    region->next = process->memory_regions;
    process->memory_regions = region;
}

extern "C" void switch_context(RegisterState* old_state, RegisterState* new_state);
}  // namespace

ProcessManager& ProcessManager::instance() {
    static ProcessManager instance;
    return instance;
}

pid_t ProcessManager::create_process(const char* name, pid_t ppid) {
    auto* process = new Process;
    process->pid = m_next_pid++;
    process->ppid = ppid;
    process->name = strdup(name);
    process->state = ProcessState::Running;
    process->next = m_first_process;

    auto& pmm = PhysicalMemoryManager::instance();
    auto& vmm = VirtualMemoryManager::instance();

    uint64_t kernel_stack_pages = (KERNEL_STACK_SIZE + 4095) / 4096;
    uint64_t kernel_stack_base = 0xFFFFFFFF80000000 + process->pid * KERNEL_STACK_SIZE;

    for (uint64_t i = 0; i < kernel_stack_pages; i++) {
        uint64_t addr = kernel_stack_base + i * 4096;
        uintptr_t phys_page = reinterpret_cast<uintptr_t>(pmm.allocate_frame());
        if (!phys_page) {
            cleanup_process_memory(process);
            delete[] process->name;
            delete process;
            return -1;
        }
        vmm.map_page(addr, phys_page, true);
    }

    process->kernel_stack = kernel_stack_base + KERNEL_STACK_SIZE;

    m_first_process = process;
    if (!m_current_process) m_current_process = process;

    return process->pid;
}

void ProcessManager::terminate_process(pid_t pid) {
    Process* prev = nullptr;
    Process* current = m_first_process;

    while (current) {
        if (current->pid == pid) {
            if (prev)
                prev->next = current->next;
            else
                m_first_process = current->next;

            cleanup_process_memory(current);

            if (current->argv) {
                for (int i = 0; i < current->argc; i++) {
                    delete[] current->argv[i];
                }
                delete[] current->argv;
            }

            if (current->envp) {
                for (int i = 0; current->envp[i]; i++) {
                    delete[] current->envp[i];
                }
                delete[] current->envp;
            }

            delete[] current->name;

            if (m_current_process == current) {
                m_current_process = m_first_process;
                if (m_current_process) {
                    switch_to_process(m_current_process);
                }
            }

            delete current;
            return;
        }
        prev = current;
        current = current->next;
    }
}

Process* ProcessManager::get_process(pid_t pid) {
    Process* current = m_first_process;
    while (current) {
        if (current->pid == pid) return current;
        current = current->next;
    }
    return nullptr;
}

bool ProcessManager::load_program(Process* process, const char* path) {
    auto& fs = fs::FileSystem::instance();
    auto* file = fs.get_file(path);
    if (!file) return false;

    auto* elf_header = reinterpret_cast<const Elf64_Ehdr*>(file->data);
    if (!ElfLoader::validate_elf_header(elf_header)) return false;

    process->entry_point = elf_header->e_entry;

    auto* phdr = reinterpret_cast<const Elf64_Phdr*>(file->data + elf_header->e_phoff);
    auto& vmm = VirtualMemoryManager::instance();
    auto& pmm = PhysicalMemoryManager::instance();

    uint64_t highest_addr = 0;

    for (uint16_t i = 0; i < elf_header->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;

        uint64_t vaddr_start = phdr[i].p_vaddr & ~0xFFF;
        uint64_t vaddr_end = (phdr[i].p_vaddr + phdr[i].p_memsz + 0xFFF) & ~0xFFF;
        uint64_t size = vaddr_end - vaddr_start;

        for (uint64_t addr = vaddr_start; addr < vaddr_end; addr += 4096) {
            uintptr_t phys_page = reinterpret_cast<uintptr_t>(pmm.allocate_frame());
            if (phys_page == 0) {
                cleanup_process_memory(process);
                return false;
            }
            vmm.map_page(addr, phys_page, (phdr[i].p_flags & PF_W) != 0);
        }

        memcpy(reinterpret_cast<void*>(phdr[i].p_vaddr), file->data + phdr[i].p_offset,
               phdr[i].p_filesz);

        if (phdr[i].p_memsz > phdr[i].p_filesz) {
            memset(reinterpret_cast<void*>(phdr[i].p_vaddr + phdr[i].p_filesz), 0,
                   phdr[i].p_memsz - phdr[i].p_filesz);
        }

        add_memory_region(process, vaddr_start, size, (phdr[i].p_flags & PF_W) != 0,
                          (phdr[i].p_flags & PF_X) != 0);

        if (vaddr_end > highest_addr) highest_addr = vaddr_end;
    }

    process->program_break = process->brk = highest_addr;

    return true;
}

bool ProcessManager::setup_process_stack(Process* process, char* const argv[], char* const envp[]) {
    auto& vmm = VirtualMemoryManager::instance();
    auto& pmm = PhysicalMemoryManager::instance();

    uint64_t stack_pages = (USER_STACK_SIZE + 4095) / 4096;
    uint64_t stack_bottom = USER_STACK_TOP - USER_STACK_SIZE;

    for (uint64_t i = 0; i < stack_pages; i++) {
        uint64_t addr = stack_bottom + i * 4096;
        uintptr_t phys_page = reinterpret_cast<uintptr_t>(pmm.allocate_frame());
        if (!phys_page) {
            cleanup_process_memory(process);
            return false;
        }
        vmm.map_page(addr, phys_page, true);
    }

    process->user_stack = USER_STACK_TOP;
    uint64_t* stack_ptr = reinterpret_cast<uint64_t*>(USER_STACK_TOP);

    size_t required_space = sizeof(uint64_t);

    int argc = 0;
    if (argv) {
        while (argv[argc])
            argc++;
    }
    process->argc = argc;
    required_space += (argc + 1) * sizeof(char*);

    int envc = 0;
    if (envp) {
        while (envp[envc])
            envc++;
        required_space += (envc + 1) * sizeof(char*);
    }

    if (required_space > USER_STACK_SIZE) {
        cleanup_process_memory(process);
        return false;
    }

    process->argv = new char*[argc + 1];
    for (int i = 0; i < argc; i++) {
        process->argv[i] = strdup(argv[i]);
    }
    process->argv[argc] = nullptr;

    if (envp) {
        process->envp = new char*[envc + 1];
        for (int i = 0; i < envc; i++) {
            process->envp[i] = strdup(envp[i]);
        }
        process->envp[envc] = nullptr;
    }

    stack_ptr = reinterpret_cast<uint64_t*>(USER_STACK_TOP - required_space);
    uint64_t current_ptr = reinterpret_cast<uint64_t>(stack_ptr);

    *stack_ptr++ = argc;

    char** argv_array = reinterpret_cast<char**>(stack_ptr);
    for (int i = 0; i < argc; i++) {
        argv_array[i] = process->argv[i];
    }
    argv_array[argc] = nullptr;
    stack_ptr += argc + 1;

    if (envp) {
        char** envp_array = reinterpret_cast<char**>(stack_ptr);
        for (int i = 0; i < envc; i++) {
            envp_array[i] = process->envp[i];
        }
        envp_array[envc] = nullptr;
        stack_ptr += envc + 1;
    }

    process->registers.rsp = current_ptr;
    process->registers.rbp = process->registers.rsp;
    process->registers.rip = process->entry_point;

    process->registers.cs = 0x23;
    process->registers.ss = 0x1B;
    process->registers.rflags = 0x202;

    add_memory_region(process, USER_STACK_TOP - USER_STACK_SIZE, USER_STACK_SIZE, true, false);

    return true;
}

void ProcessManager::switch_to_process(Process* process) {
    if (!process || process->state != ProcessState::Running) return;

    if (m_current_process != process) {
        Process* old = m_current_process;
        m_current_process = process;

        if (old)
            switch_context(&old->registers, &process->registers);
        else
            switch_context(nullptr, &process->registers);
    }
}

void ProcessManager::cleanup_process_memory(Process* process) {
    auto& vmm = VirtualMemoryManager::instance();
    auto& pmm = PhysicalMemoryManager::instance();

    MemoryRegion* region = process->memory_regions;
    while (region) {
        for (uint64_t addr = region->start; addr < region->start + region->size; addr += 4096) {
            uintptr_t phys_addr = vmm.get_physical_address(addr);
            if (phys_addr) {
                pmm.free_frame(reinterpret_cast<void*>(phys_addr));
                vmm.unmap_page(addr);
            }
        }

        auto* next = region->next;
        delete region;
        region = next;
    }

    process->memory_regions = nullptr;
    process->brk = 0;
    process->program_break = 0;

    if (process->kernel_stack) {
        uint64_t kernel_stack_pages = (KERNEL_STACK_SIZE + 4095) / 4096;
        uint64_t kernel_stack_base = process->kernel_stack - KERNEL_STACK_SIZE;

        for (uint64_t i = 0; i < kernel_stack_pages; i++) {
            uint64_t addr = kernel_stack_base + i * 4096;
            uintptr_t phys_addr = vmm.get_physical_address(addr);
            if (phys_addr) {
                pmm.free_frame(reinterpret_cast<void*>(phys_addr));
                vmm.unmap_page(addr);
            }
        }
        process->kernel_stack = 0;
    }
}

}  // namespace kernel