#include "syscall.hpp"
#include "process.hpp"
#include "printf.hpp"
#include "ipc.hpp"
#include "scheduler.hpp"
#include <cstring>

namespace kernel {

int64_t SyscallHandler::handle(SyscallContext& ctx) {
    auto syscall = static_cast<SyscallNumber>(ctx.rax);

    switch (syscall) {
        case SyscallNumber::Read:
            return sys_read(ctx.rdi, reinterpret_cast<void*>(ctx.rsi), ctx.rdx);

        case SyscallNumber::Write:
            return sys_write(ctx.rdi, reinterpret_cast<const void*>(ctx.rsi), ctx.rdx);

        case SyscallNumber::Open:
            return sys_open(reinterpret_cast<const char*>(ctx.rdi), ctx.rsi, ctx.rdx);

        case SyscallNumber::Close:
            return sys_close(ctx.rdi);

        case SyscallNumber::Mmap:
            return reinterpret_cast<int64_t>(sys_mmap(
                reinterpret_cast<void*>(ctx.rdi),
                ctx.rsi,
                ctx.rdx,
                ctx.r10,
                ctx.r8,
                ctx.r9
            ));

        case SyscallNumber::Munmap:
            return sys_munmap(reinterpret_cast<void*>(ctx.rdi), ctx.rsi);

        case SyscallNumber::Brk:
            return sys_brk(reinterpret_cast<void*>(ctx.rdi));

        case SyscallNumber::Exit:
            sys_exit(ctx.rdi);
            return 0;

        case SyscallNumber::Fork:
            return sys_fork();

        case SyscallNumber::Execve:
            return sys_execve(
                reinterpret_cast<const char*>(ctx.rdi),
                reinterpret_cast<char* const*>(ctx.rsi),
                reinterpret_cast<char* const*>(ctx.rdx)
            );

        case SyscallNumber::MsgCreate:
            return sys_msg_create(reinterpret_cast<const char*>(ctx.rdi));

        case SyscallNumber::MsgDestroy:
            return sys_msg_destroy(ctx.rdi);

        case SyscallNumber::MsgOpen:
            return sys_msg_open(reinterpret_cast<const char*>(ctx.rdi));

        case SyscallNumber::MsgSend:
            return sys_msg_send(ctx.rdi, reinterpret_cast<const void*>(ctx.rsi), ctx.rdx);

        case SyscallNumber::MsgReceive:
            return sys_msg_receive(ctx.rdi, reinterpret_cast<void*>(ctx.rsi), ctx.rdx, ctx.r10);

        case SyscallNumber::ShmCreate:
            return sys_shm_create(ctx.rdi);

        case SyscallNumber::ShmDestroy:
            return sys_shm_destroy(ctx.rdi);

        case SyscallNumber::ShmAttach:
            return reinterpret_cast<int64_t>(sys_shm_attach(ctx.rdi));

        case SyscallNumber::ShmDetach:
            return sys_shm_detach(ctx.rdi);

        case SyscallNumber::SchedYield:
            return sys_sched_yield();

        case SyscallNumber::SchedSetPriority:
            return sys_sched_set_priority(ctx.rdi, ctx.rsi);

        case SyscallNumber::SchedGetPriority:
            return sys_sched_get_priority(ctx.rdi);

        default:
            return -1;
    }
}

int64_t SyscallHandler::sys_read(int fd, void* buf, size_t count) {
    return -1;
}

int64_t SyscallHandler::sys_write(int fd, const void* buf, size_t count) {
    if (fd == 1 || fd == 2) {
        const char* cbuf = reinterpret_cast<const char*>(buf);
        for (size_t i = 0; i < count; i++) {
            printf("%c", cbuf[i]);
        }
        return count;
    }
    return -1;
}

int64_t SyscallHandler::sys_open(const char* pathname, int flags, mode_t mode) {
    return -1;
}

int64_t SyscallHandler::sys_close(int fd) {
    return -1;
}

void* SyscallHandler::sys_mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return nullptr;
}

int64_t SyscallHandler::sys_munmap(void* addr, size_t length) {
    return -1;
}

int64_t SyscallHandler::sys_brk(void* addr) {
    auto& pm = ProcessManager::instance();
    auto* process = pm.get_current_process();
    if (!process) return -1;

    if (addr == nullptr) return process->brk;

    uint64_t new_brk = reinterpret_cast<uint64_t>(addr);
    if (new_brk < process->program_break) return -1;

    process->brk = new_brk;
    return new_brk;
}

void SyscallHandler::sys_exit(int status) {
    auto& pm = ProcessManager::instance();
    auto* process = pm.get_current_process();
    if (process) {
        process->state = ProcessState::Zombie;
        pm.terminate_process(process->pid);
    }
}

pid_t SyscallHandler::sys_fork() {
    auto& pm = ProcessManager::instance();
    auto* parent = pm.get_current_process();
    if (!parent) return -1;

    pid_t child_pid = pm.create_process(parent->name, parent->pid);
    auto* child = pm.get_process(child_pid);
    if (!child) return -1;

    auto& vmm = VirtualMemoryManager::instance();
    auto& pmm = PhysicalMemoryManager::instance();

    MemoryRegion* region = parent->memory_regions;
    while (region) {
        for (uint64_t addr = region->start; addr < region->start + region->size; addr += 4096) {
            uintptr_t new_phys = reinterpret_cast<uintptr_t>(pmm.allocate_frame());
            if (!new_phys) {
                pm.terminate_process(child_pid);
                return -1;
            }

            vmm.map_page(addr, new_phys, region->writable);

            uintptr_t parent_phys = vmm.get_physical_address(addr);
            memcpy(reinterpret_cast<void*>(new_phys),
                   reinterpret_cast<void*>(parent_phys),
                   4096);
        }

        add_memory_region(child, region->start, region->size, region->writable, region->executable);
        region = region->next;
    }

    child->brk = parent->brk;
    child->program_break = parent->program_break;
    child->entry_point = parent->entry_point;
    child->registers = parent->registers;

    child->registers.rax = 0;
    parent->registers.rax = child_pid;

    if (parent->argv) {
        child->argc = parent->argc;
        child->argv = new char*[parent->argc + 1];
        for (int i = 0; i < parent->argc; i++) {
            child->argv[i] = strdup(parent->argv[i]);
        }
        child->argv[parent->argc] = nullptr;
    }

    if (parent->envp) {
        int envc = 0;
        while (parent->envp[envc]) envc++;
        child->envp = new char*[envc + 1];
        for (int i = 0; i < envc; i++) {
            child->envp[i] = strdup(parent->envp[i]);
        }
        child->envp[envc] = nullptr;
    }

    return child_pid;
}

int64_t SyscallHandler::sys_execve(const char* filename, char* const argv[], char* const envp[]) {
    auto& pm = ProcessManager::instance();
    auto* process = pm.get_current_process();
    if (!process) return -1;

    auto old_regions = process->memory_regions;
    process->memory_regions = nullptr;

    if (!pm.load_program(process, filename)) {
        process->memory_regions = old_regions;
        return -1;
    }

    while (old_regions) {
        auto* next = old_regions->next;
        delete old_regions;
        old_regions = next;
    }

    if (!pm.setup_process_stack(process, argv, envp)) {
        pm.terminate_process(process->pid);
        return -1;
    }

    pm.switch_to_process(process);

    return 0;
}

int32_t SyscallHandler::sys_msg_create(const char* name) {
    auto& pm = ProcessManager::instance();
    auto* process = pm.get_current_process();
    if (!process) return -1;

    auto& ipc = IPCManager::instance();
    return ipc.create_message_queue(process->pid, name);
}

int64_t SyscallHandler::sys_msg_destroy(int32_t id) {
    auto& pm = ProcessManager::instance();
    auto* process = pm.get_current_process();
    if (!process) return -1;

    auto& ipc = IPCManager::instance();
    return ipc.destroy_message_queue(id) ? 0 : -1;
}

int32_t SyscallHandler::sys_msg_open(const char* name) {
    auto& pm = ProcessManager::instance();
    auto* process = pm.get_current_process();
    if (!process) return -1;

    auto& ipc = IPCManager::instance();
    return ipc.open_message_queue(name);
}

int64_t SyscallHandler::sys_msg_send(int32_t queue_id, const void* data, size_t size) {
    auto& pm = ProcessManager::instance();
    auto* process = pm.get_current_process();
    if (!process) return -1;

    auto& ipc = IPCManager::instance();
    return ipc.send_message(queue_id, process->pid, data, size) ? 0 : -1;
}

int64_t SyscallHandler::sys_msg_receive(int32_t queue_id, void* data, size_t max_size, bool wait) {
    auto& pm = ProcessManager::instance();
    auto* process = pm.get_current_process();
    if (!process) return -1;

    auto& ipc = IPCManager::instance();
    IPCMessage message;
    if (!ipc.receive_message(queue_id, message, wait)) {
        return -1;
    }

    size_t copy_size = (message.size < max_size) ? message.size : max_size;
    memcpy(data, message.data, copy_size);

    return (static_cast<int64_t>(message.sender) << 32) | copy_size;
}

int32_t SyscallHandler::sys_shm_create(size_t size) {
    auto& pm = ProcessManager::instance();
    auto* process = pm.get_current_process();
    if (!process) return -1;

    auto& ipc = IPCManager::instance();
    return ipc.create_shared_memory(process->pid, size);
}

int64_t SyscallHandler::sys_shm_destroy(int32_t id) {
    auto& pm = ProcessManager::instance();
    auto* process = pm.get_current_process();
    if (!process) return -1;

    auto& ipc = IPCManager::instance();
    return ipc.destroy_shared_memory(id) ? 0 : -1;
}

void* SyscallHandler::sys_shm_attach(int32_t id) {
    auto& pm = ProcessManager::instance();
    auto* process = pm.get_current_process();
    if (!process) return nullptr;

    auto& ipc = IPCManager::instance();
    return ipc.attach_shared_memory(id, process->pid);
}

int64_t SyscallHandler::sys_shm_detach(int32_t id) {
    auto& pm = ProcessManager::instance();
    auto* process = pm.get_current_process();
    if (!process) return -1;

    auto& ipc = IPCManager::instance();
    return ipc.detach_shared_memory(id, process->pid) ? 0 : -1;
}

int64_t SyscallHandler::sys_sched_yield() {
    Scheduler::instance().schedule();
    return 0;
}

int64_t SyscallHandler::sys_sched_set_priority(pid_t pid, uint8_t priority) {
    auto& pm = ProcessManager::instance();
    auto* process = pm.get_current_process();

    if (!process || (pid != process->pid && process->pid != 1))
        return -1;

    Scheduler::instance().set_process_priority(pid, priority);
    return 0;
}

int64_t SyscallHandler::sys_sched_get_priority(pid_t pid) {
    auto& pm = ProcessManager::instance();
    auto* process = pm.get_process(pid);

    if (!process)
        return -1;

    return process->priority;
}

}  // namespace kernel