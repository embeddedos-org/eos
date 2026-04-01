// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file posix_io.h
 * @brief POSIX file I/O compatibility layer for EoS
 *
 * Provides open/close/read/write/lseek/stat/fstat mapped to
 * eos_fs_* from the EoS filesystem layer. fds 0/1/2 are reserved
 * for stdin/stdout/stderr.
 */

#ifndef EOS_POSIX_IO_H
#define EOS_POSIX_IO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ssize_t
typedef int32_t ssize_t;
#endif

#define EOS_POSIX_FD_MAX      16
#define EOS_POSIX_PATH_MAX   128

/* File descriptor types */
#define EOS_FD_TYPE_FREE      0
#define EOS_FD_TYPE_CONSOLE   1
#define EOS_FD_TYPE_FILE      2

/* Open flags */
#define EOS_O_RDONLY_IO  0x0000
#define EOS_O_WRONLY_IO  0x0001
#define EOS_O_RDWR_IO    0x0002
#define EOS_O_CREAT_IO   0x0100
#define EOS_O_TRUNC_IO   0x0200
#define EOS_O_APPEND_IO  0x0400

/* Seek whence */
#define EOS_SEEK_SET  0
#define EOS_SEEK_CUR  1
#define EOS_SEEK_END  2

/* errno values for I/O */
#define ENOSYS   38
#define EBADF     9
#define EINVAL   22
#define ENOMEM   12
#define ENOENT    2
#define EMFILE   24

/* stat structure (minimal) */
struct eos_stat {
    uint32_t st_size;
    uint32_t st_mode;
    uint32_t st_mtime;
};

/**
 * Initialize the POSIX I/O subsystem.
 * Assigns fds 0/1/2 as console descriptors.
 */
void eos_posix_io_init(void);

int     open(const char *path, int flags);
int     close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int32_t lseek(int fd, int32_t offset, int whence);
int     stat(const char *path, struct eos_stat *buf);
int     fstat(int fd, struct eos_stat *buf);

#ifdef __cplusplus
}
#endif

#endif /* EOS_POSIX_IO_H */
