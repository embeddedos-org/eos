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

/* These prototypes must match eos/filesystem.h. That header is not included
 * because the filesystem is optional here, so a mismatch is not diagnosed:
 * eos_fs_stat() once appeared below as (path, buf) while the filesystem's is
 * the volume-wide eos_fs_stat(eos_fs_stat_t *), so stat() wrote volume
 * totals over the caller's path string. There is no per-path stat in the
 * filesystem API; stat() is built from open/seek/tell instead. */
WEAK_ALIAS int eos_fs_open(const char *path, uint32_t flags);
WEAK_ALIAS int eos_fs_close(int fh);
WEAK_ALIAS int eos_fs_read(int fh, void *buf, size_t len);
WEAK_ALIAS int eos_fs_write(int fh, const void *buf, size_t len);
WEAK_ALIAS int eos_fs_seek(int fh, int32_t offset, int whence);
WEAK_ALIAS int eos_fs_tell(int fh, uint32_t *pos);
WEAK_ALIAS int eos_fs_fstat(int fh, void *stat_buf);

WEAK_ALIAS int eos_console_write(const void *buf, size_t len);
WEAK_ALIAS int eos_console_read(void *buf, size_t len);

/* eos_open_flags_t from eos/filesystem.h, mirrored for the same reason as
 * the prototypes above. The POSIX EOS_O_*_IO values are a different
 * encoding (an access mode in the low two bits, then 0x100/0x200/0x400) and
 * must be translated, not passed through. */
#define EOS_FS_O_READ    (1u << 0)
#define EOS_FS_O_WRITE   (1u << 1)
#define EOS_FS_O_CREATE  (1u << 2)
#define EOS_FS_O_APPEND  (1u << 3)
#define EOS_FS_O_TRUNC   (1u << 4)

static uint32_t posix_to_fs_flags(int flags) {
    uint32_t fs;
    switch (flags & 0x3) {
    case EOS_O_WRONLY_IO: fs = EOS_FS_O_WRITE; break;
    case EOS_O_RDWR_IO:   fs = EOS_FS_O_READ | EOS_FS_O_WRITE; break;
    default:              fs = EOS_FS_O_READ; break;
    }
    if (flags & EOS_O_CREAT_IO)  fs |= EOS_FS_O_CREATE;
    if (flags & EOS_O_TRUNC_IO)  fs |= EOS_FS_O_TRUNC;
    if (flags & EOS_O_APPEND_IO) fs |= EOS_FS_O_APPEND;
    return fs;
}

/* ============================================================
 * open / close
 * ============================================================ */

int open(const char *path, int flags) {
    ensure_fd_init();
    if (!path) return -EINVAL;

    if (!eos_fs_open) return -ENOSYS;

    int fh = eos_fs_open(path, posix_to_fs_flags(flags));
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

    if (!eos_fs_seek || !eos_fs_tell) return -ENOSYS;
    /* eos_fs_seek() returns 0 on success, not the offset lseek() reports. */
    if (eos_fs_seek(e->handle, offset, whence) != 0) return -EINVAL;
    uint32_t pos;
    if (eos_fs_tell(e->handle, &pos) != 0) return -EBADF;
    return (int32_t)pos;
}

/* ============================================================
 * stat / fstat
 * ============================================================ */

int stat(const char *path, struct eos_stat *buf) {
    if (!path || !buf) return -EINVAL;
    if (!eos_fs_open || !eos_fs_close || !eos_fs_seek || !eos_fs_tell)
        return -ENOSYS;

    int fh = eos_fs_open(path, EOS_FS_O_READ);
    if (fh < 0) return -ENOENT;
    uint32_t size = 0;
    int rc = (eos_fs_seek(fh, 0, EOS_SEEK_END) == 0 &&
              eos_fs_tell(fh, &size) == 0) ? 0 : -EINVAL;
    eos_fs_close(fh);
    if (rc != 0) return rc;

    memset(buf, 0, sizeof(*buf));
    buf->st_size = size;
    return 0;
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
