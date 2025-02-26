#include "syscall.hpp"
#include <memory>

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

        default:
            return -1;
    }
}

int64_t SyscallHandler::sys_read(int fd, void* buf, size_t count) {
    return -1;
}

int64_t SyscallHandler::sys_write(int fd, const void* buf, size_t count) {
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
    return -1;
}

void SyscallHandler::sys_exit(int status) {
    // TODO: implement process termination
}

pid_t SyscallHandler::sys_fork() {
    return -1;
}

int64_t SyscallHandler::sys_execve(const char* filename, char* const argv[], char* const envp[]) {
    return -1;
}

}