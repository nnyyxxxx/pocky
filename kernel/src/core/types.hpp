#pragma once

#include <cstdint>

namespace kernel {

using mode_t = uint32_t;
using off_t = int64_t;
using pid_t = int32_t;
using uid_t = uint32_t;
using gid_t = uint32_t;

struct stat {
    uint64_t st_dev;
    uint64_t st_ino;
    mode_t st_mode;
    uint32_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    uint64_t st_rdev;
    off_t st_size;
    uint32_t st_blksize;
    uint32_t st_blocks;
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
};

constexpr int O_RDONLY = 0;
constexpr int O_WRONLY = 1;
constexpr int O_RDWR = 2;
constexpr int O_CREAT = 0100;
constexpr int O_EXCL = 0200;
constexpr int O_TRUNC = 01000;
constexpr int O_APPEND = 02000;

constexpr int PROT_NONE = 0;
constexpr int PROT_READ = 1;
constexpr int PROT_WRITE = 2;
constexpr int PROT_EXEC = 4;

constexpr int MAP_SHARED = 0x01;
constexpr int MAP_PRIVATE = 0x02;
constexpr int MAP_FIXED = 0x10;
constexpr int MAP_ANONYMOUS = 0x20;

}