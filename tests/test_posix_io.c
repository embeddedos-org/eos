// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/*
 * POSIX file layer (services/posix/src/posix_io.c) over the RAM filesystem.
 *
 * posix_io.c defines open/close/read/write/lseek/stat/fstat, which would
 * replace the host C library's own in a test executable. It is #included
 * here with those names renamed, the same way test_ed25519_canonical_s.c
 * reaches ed25519_verify.c, and runs against the real eos_filesystem.
 *
 * eos/filesystem.h is deliberately not included: posix_io.c declares the
 * eos_fs_* calls itself (they are weak, the filesystem being optional), and
 * the enum-typed whence parameter would conflict. Only init/deinit are
 * declared below; everything else is observed through the POSIX API.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <sys/types.h>

/* posix_io.h typedefs ssize_t as int32_t unless it is already a macro; the
 * host's ssize_t is already a typedef here, so keep that one. */
#define ssize_t ssize_t

#define open   eos_test_posix_open
#define close  eos_test_posix_close
#define read   eos_test_posix_read
#define write  eos_test_posix_write
#define lseek  eos_test_posix_lseek
#define stat   eos_test_posix_stat
#define fstat  eos_test_posix_fstat
#include "../services/posix/src/posix_io.c"

int  eos_fs_init(const void *cfg);
void eos_fs_deinit(void);

static void setup(void) {
    assert(eos_fs_init(NULL) == 0);
    eos_posix_io_init();
}

static void teardown(void) {
    for (int fd = 3; fd < EOS_POSIX_FD_MAX; fd++) close(fd);
    eos_fs_deinit();
}

/* EOS_O_CREAT_IO (0x100) was passed straight to eos_fs_open(), which
 * expects EOS_O_CREATE (1 << 2): a missing file was never created. */
static void test_open_creat_creates(void) {
    setup();
    int fd = open("/new.txt", EOS_O_WRONLY_IO | EOS_O_CREAT_IO);
    assert(fd >= 3);
    assert(write(fd, "hello world", 11) == 11);
    assert(close(fd) == 0);

    char buf[32] = {0};
    fd = open("/new.txt", EOS_O_RDONLY_IO);
    assert(fd >= 3);
    assert(read(fd, buf, sizeof(buf)) == 11);
    assert(memcmp(buf, "hello world", 11) == 0);
    close(fd);

    /* Without O_CREAT a missing file is still an error. */
    assert(open("/absent.txt", EOS_O_RDONLY_IO) == -ENOENT);
    teardown();
    printf("[PASS] posix open O_CREAT creates\n");
}

/* EOS_O_RDWR_IO (2) arrived as EOS_O_WRITE alone and EOS_O_TRUNC_IO
 * (0x200) as nothing at all. */
static void test_open_trunc_truncates(void) {
    setup();
    int fd = open("/t.txt", EOS_O_RDWR_IO | EOS_O_CREAT_IO);
    assert(fd >= 3);
    assert(write(fd, "0123456789", 10) == 10);
    close(fd);

    fd = open("/t.txt", EOS_O_RDWR_IO | EOS_O_TRUNC_IO);
    assert(fd >= 3);
    struct eos_stat st;
    assert(stat("/t.txt", &st) == 0);
    assert(st.st_size == 0);
    assert(write(fd, "ab", 2) == 2);
    assert(lseek(fd, 0, EOS_SEEK_SET) == 0);
    char buf[16] = {0};
    assert(read(fd, buf, sizeof(buf)) == 2);
    assert(memcmp(buf, "ab", 2) == 0);
    close(fd);
    teardown();
    printf("[PASS] posix open O_TRUNC truncates\n");
}

/* eos_fs_seek() returns 0 on success; lseek() returned that instead of the
 * resulting offset. */
static void test_lseek_returns_offset(void) {
    setup();
    int fd = open("/s.txt", EOS_O_RDWR_IO | EOS_O_CREAT_IO);
    assert(fd >= 3);
    assert(write(fd, "hello world", 11) == 11);
    assert(lseek(fd, 0, EOS_SEEK_CUR) == 11);
    assert(lseek(fd, 6, EOS_SEEK_SET) == 6);
    assert(lseek(fd, -1, EOS_SEEK_CUR) == 5);
    assert(lseek(fd, 0, EOS_SEEK_END) == 11);
    assert(lseek(fd, 0, 99) == -EINVAL);
    close(fd);
    assert(lseek(fd, 0, EOS_SEEK_SET) == -EBADF);
    teardown();
    printf("[PASS] posix lseek returns the offset\n");
}

/* stat() called the volume-wide eos_fs_stat(eos_fs_stat_t *) with the
 * caller's path as its argument, overwriting the path with byte counts. */
static void test_stat_leaves_path_alone(void) {
    setup();
    int fd = open("/stat.txt", EOS_O_WRONLY_IO | EOS_O_CREAT_IO);
    assert(fd >= 3);
    assert(write(fd, "12345", 5) == 5);
    close(fd);

    char path[32] = "/stat.txt";
    struct eos_stat st;
    memset(&st, 0xA5, sizeof(st));
    assert(stat(path, &st) == 0);
    assert(strcmp(path, "/stat.txt") == 0);
    assert(st.st_size == 5);
    assert(stat("/missing.txt", &st) == -ENOENT);
    teardown();
    printf("[PASS] posix stat reports size and leaves the path alone\n");
}

int main(void) {
    printf("=== EoS POSIX I/O Tests ===\n");
    test_open_creat_creates();
    test_open_trunc_truncates();
    test_lseek_returns_offset();
    test_stat_leaves_path_alone();
    printf("=== ALL POSIX I/O TESTS PASSED (4/4) ===\n");
    return 0;
}
