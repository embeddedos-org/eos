// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file posix_io.c
 * @brief POSIX file I/O implementation for EoS
 *
 * Maps open/close/read/write/lseek/stat/fstat to eos_fs_* calls.
 * fds 0/1/2 are reserved for console I/O (stdin/stdout/stderr).
 */

#include <eos/posix_io.h>
#include <eos/kernel.h>
#include <eos/hal.h>
#include <string.h>

/* ============================================================
 * File Descriptor Table
 * ============================================================ */

typedef struct {
    uint8_t  type;      /* EOS_FD_TYPE_* */
    int      handle;    /* eos_fs file handle or -1 */
    int      flags;
} posix_fd_entry_t;

static posix_fd_entry_t posix_fd_table[EOS_POSIX_FD_MAX];
static bool fd_table_initialized = false;

void eos_posix_io_init(void) {
    memset(posix_fd_table, 0, sizeof(posix_fd_table));

    /* Reserve stdin / stdout / stderr */
    posix_fd_table[0].type   = EOS_FD_TYPE_CONSOLE;
    posix_fd_table[0].handle = 0;
    posix_fd_table[1].type   = EOS_FD_TYPE_CONSOLE;
    posix_fd_table[1].handle = 1;
    posix_fd_table[2].type   = EOS_FD_TYPE_CONSOLE;
    posix_fd_table[2].handle = 2;

    fd_table_initialized = true;
}

static void ensure_fd_init(void) {
    if (!fd_table_initialized) eos_posix_io_init();
}

static int alloc_fd(void) {
    for (int i = 3; i < EOS_POSIX_FD_MAX; i++) {
        if (posix_fd_table[i].type == EOS_FD_TYPE_FREE) return i;
    }
    return -1;
}

/* ============================================================
 * Weak filesystem and console symbols.
 * If the EoS filesystem or console module is linked, these
 * resolve to real implementations; otherwise they stay NULL.
 * ============================================================ */

#ifdef __GNUC__
#define WEAK_ALIAS __attribute__((weak))
#else
#define WEAK_ALIAS
#endif

WEAK_ALIAS int eos_fs_open(const char *path, int flags);
WEAK_ALIAS int eos_fs_close(int fh);
WEAK_ALIAS int eos_fs_read(int fh, void *buf, size_t len);
WEAK_ALIAS int eos_fs_write(int fh, const void *buf, size_t len);
WEAK_ALIAS int eos_fs_seek(int fh, int32_t offset, int whence);
WEAK_ALIAS int eos_fs_stat(const char *path, void *stat_buf);
WEAK_ALIAS int eos_fs_fstat(int fh, void *stat_buf);

WEAK_ALIAS int eos_console_write(const void *buf, size_t len);
WEAK_ALIAS int eos_console_read(void *buf, size_t len);

/* ============================================================
 * open / close
 * ============================================================ */

int open(const char *path, int flags) {
    ensure_fd_init();
    if (!path) return -EINVAL;

    if (!eos_fs_open) return -ENOSYS;

    int fh = eos_fs_open(path, flags);
    if (fh < 0) return -ENOENT;

    int fd = alloc_fd();
    if (fd < 0) return -EMFILE;

    posix_fd_table[fd].type   = EOS_FD_TYPE_FILE;
    posix_fd_table[fd].handle = fh;
    posix_fd_table[fd].flags  = flags;
    return fd;
}

int close(int fd) {
    ensure_fd_init();
    if (fd < 0 || fd >= EOS_POSIX_FD_MAX) return -EBADF;
    if (posix_fd_table[fd].type == EOS_FD_TYPE_FREE) return -EBADF;

    /* Don't actually close console descriptors */
    if (posix_fd_table[fd].type == EOS_FD_TYPE_CONSOLE) return 0;

    if (eos_fs_close) {
        eos_fs_close(posix_fd_table[fd].handle);
    }

    posix_fd_table[fd].type   = EOS_FD_TYPE_FREE;
    posix_fd_table[fd].handle = -1;
    return 0;
}

/* ============================================================
 * read / write
 * ============================================================ */

ssize_t read(int fd, void *buf, size_t count) {
    ensure_fd_init();
    if (fd < 0 || fd >= EOS_POSIX_FD_MAX) return -EBADF;
    if (!buf) return -EINVAL;

    posix_fd_entry_t *e = &posix_fd_table[fd];
    if (e->type == EOS_FD_TYPE_FREE) return -EBADF;

    if (e->type == EOS_FD_TYPE_CONSOLE) {
        if (eos_console_read) return eos_console_read(buf, count);
        return -ENOSYS;
    }

    if (!eos_fs_read) return -ENOSYS;
    return eos_fs_read(e->handle, buf, count);
}

ssize_t write(int fd, const void *buf, size_t count) {
    ensure_fd_init();
    if (fd < 0 || fd >= EOS_POSIX_FD_MAX) return -EBADF;
    if (!buf) return -EINVAL;

    posix_fd_entry_t *e = &posix_fd_table[fd];
    if (e->type == EOS_FD_TYPE_FREE) return -EBADF;

    if (e->type == EOS_FD_TYPE_CONSOLE) {
        if (eos_console_write) return eos_console_write(buf, count);
        return -ENOSYS;
    }

    if (!eos_fs_write) return -ENOSYS;
    return eos_fs_write(e->handle, buf, count);
}

/* ============================================================
 * lseek
 * ============================================================ */

int32_t lseek(int fd, int32_t offset, int whence) {
    ensure_fd_init();
    if (fd < 0 || fd >= EOS_POSIX_FD_MAX) return -EBADF;

    posix_fd_entry_t *e = &posix_fd_table[fd];
    if (e->type != EOS_FD_TYPE_FILE) return -EBADF;

    if (!eos_fs_seek) return -ENOSYS;
    return eos_fs_seek(e->handle, offset, whence);
}

/* ============================================================
 * stat / fstat
 * ============================================================ */

int stat(const char *path, struct eos_stat *buf) {
    if (!path || !buf) return -EINVAL;
    if (!eos_fs_stat) return -ENOSYS;
    return eos_fs_stat(path, buf);
}

int fstat(int fd, struct eos_stat *buf) {
    ensure_fd_init();
    if (fd < 0 || fd >= EOS_POSIX_FD_MAX) return -EBADF;
    if (!buf) return -EINVAL;

    posix_fd_entry_t *e = &posix_fd_table[fd];
    if (e->type != EOS_FD_TYPE_FILE) return -EBADF;

    if (!eos_fs_fstat) return -ENOSYS;
    return eos_fs_fstat(e->handle, buf);
}
