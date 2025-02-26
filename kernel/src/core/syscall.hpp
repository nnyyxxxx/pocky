#pragma once

#include <cstdint>
#include <memory>
#include "types.hpp"

namespace kernel {

enum class SyscallNumber : uint64_t {
    Read = 0,
    Write = 1,
    Open = 2,
    Close = 3,
    Stat = 4,
    Fstat = 5,
    Lstat = 6,
    Poll = 7,
    Lseek = 8,
    Mmap = 9,
    Mprotect = 10,
    Munmap = 11,
    Brk = 12,
    Exit = 60,
    Fork = 57,
    Execve = 59,
};

struct SyscallContext {
    uint64_t rax;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t r10;
    uint64_t r8;
    uint64_t r9;
};

class SyscallHandler {
public:
    static int64_t handle(SyscallContext& context);

private:
    static int64_t sys_read(int fd, void* buf, size_t count);
    static int64_t sys_write(int fd, const void* buf, size_t count);
    static int64_t sys_open(const char* pathname, int flags, mode_t mode);
    static int64_t sys_close(int fd);

    static int64_t sys_execve(const char* filename, char* const argv[], char* const envp[]);
    static void sys_exit(int status);
    static pid_t sys_fork();

    static void* sys_mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
    static int64_t sys_munmap(void* addr, size_t length);
    static int64_t sys_brk(void* addr);

    static off_t sys_lseek(int fd, off_t offset, int whence);
    static int64_t sys_stat(const char* path, struct stat* statbuf);
    static int64_t sys_fstat(int fd, struct stat* statbuf);
    static int64_t sys_lstat(const char* path, struct stat* statbuf);
};

}