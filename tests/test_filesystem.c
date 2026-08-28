// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eos/filesystem.h"

static void test_fs_init(void) {
    assert(eos_fs_init(NULL) == 0);
    eos_fs_stat_t st;
    (void)st;
    assert(eos_fs_stat(&st) == 0);
    assert(st.free_bytes > 0);
    eos_fs_deinit();
    printf("[PASS] fs init\n");
}

static void test_fs_write_read(void) {
    eos_fs_init(NULL);
    eos_file_t fd = eos_fs_open("/test.txt", EOS_O_CREATE | EOS_O_WRITE);
    assert(fd != EOS_FILE_INVALID);
    const char *data = "Hello EoS!";
    int n = eos_fs_write(fd, data, strlen(data));
    assert(n == (int)strlen(data));
    eos_fs_close(fd);
    fd = eos_fs_open("/test.txt", EOS_O_READ);
    assert(fd != EOS_FILE_INVALID);
    char buf[64] = {0};
    n = eos_fs_read(fd, buf, sizeof(buf));
    assert(n == (int)strlen(data));
    assert(strcmp(buf, data) == 0);
    eos_fs_close(fd);
    eos_fs_deinit();
    printf("[PASS] fs write/read\n");
}

static void test_fs_seek(void) {
    eos_fs_init(NULL);
    eos_file_t fd = eos_fs_open("/s.txt", EOS_O_CREATE | EOS_O_WRITE);
    eos_fs_write(fd, "ABCDEFGH", 8);
    eos_fs_seek(fd, 4, EOS_SEEK_SET);
    uint32_t pos; eos_fs_tell(fd, &pos);
    assert(pos == 4);
    char buf[4]; eos_fs_read(fd, buf, 4);
    assert(memcmp(buf, "EFGH", 4) == 0);
    eos_fs_close(fd);
    eos_fs_deinit();
    printf("[PASS] fs seek\n");
}

static void test_fs_truncate(void) {
    eos_fs_init(NULL);
    eos_file_t fd = eos_fs_open("/t.txt", EOS_O_CREATE | EOS_O_WRITE);
    eos_fs_write(fd, "1234567890", 10);
    eos_fs_truncate(fd, 5);
    uint32_t pos; eos_fs_tell(fd, &pos);
    assert(pos == 5);
    eos_fs_close(fd);
    eos_fs_deinit();
    printf("[PASS] fs truncate\n");
}

static void test_fs_dir(void) {
    eos_fs_init(NULL);
    assert(eos_fs_mkdir("/data") == 0);
    assert(eos_fs_exists("/data"));
    eos_file_t fd = eos_fs_open("/f1.txt", EOS_O_CREATE | EOS_O_WRITE);
    eos_fs_close(fd);
    assert(eos_fs_exists("/f1.txt"));
    eos_dir_t dir = eos_fs_opendir("/");
    assert(dir != EOS_DIR_INVALID);
    eos_dirent_t entry; int count = 0;
    while (eos_fs_readdir(dir, &entry) == 0) count++;
    eos_fs_closedir(dir);
    assert(count >= 2);
    eos_fs_deinit();
    printf("[PASS] fs dir\n");
}

static void test_fs_remove_rename(void) {
    eos_fs_init(NULL);
    eos_file_t fd = eos_fs_open("/rm.txt", EOS_O_CREATE | EOS_O_WRITE);
    eos_fs_write(fd, "x", 1);
    eos_fs_close(fd);
    assert(eos_fs_exists("/rm.txt"));
    assert(eos_fs_rename("/rm.txt", "/renamed.txt") == 0);
    assert(!eos_fs_exists("/rm.txt"));
    assert(eos_fs_exists("/renamed.txt"));
    assert(eos_fs_remove("/renamed.txt") == 0);
    assert(!eos_fs_exists("/renamed.txt"));
    eos_fs_deinit();
    printf("[PASS] fs remove/rename\n");
}

static void test_fs_format(void) {
    eos_fs_init(NULL);
    eos_fs_open("/f.txt", EOS_O_CREATE | EOS_O_WRITE);
    eos_fs_format();
    assert(!eos_fs_exists("/f.txt"));
    eos_fs_deinit();
    printf("[PASS] fs format\n");
}

/*
 * Regression: eos_fs_remove() released the inode outright, so the next
 * eos_fs_open(..., EOS_O_CREATE) was handed the same slot while a descriptor
 * was still open on it. The stale descriptor then read and wrote the new,
 * unrelated file.
 */
static void test_fs_remove_with_open_fd_does_not_alias(void) {
    eos_fs_init(NULL);
    eos_file_t a = eos_fs_open("/a.txt", EOS_O_CREATE | EOS_O_WRITE | EOS_O_READ);
    assert(a != EOS_FILE_INVALID);
    assert(eos_fs_write(a, "AAAAAAAA", 8) == 8);

    assert(eos_fs_remove("/a.txt") == 0);
    assert(!eos_fs_exists("/a.txt"));       /* the name is gone at once */

    eos_file_t b = eos_fs_open("/b.txt", EOS_O_CREATE | EOS_O_WRITE | EOS_O_READ);
    assert(b != EOS_FILE_INVALID);
    assert(eos_fs_write(b, "BBBBBBBB", 8) == 8);

    /* The unlinked file stays readable through its own descriptor... */
    char buf[16] = {0};
    assert(eos_fs_seek(a, 0, EOS_SEEK_SET) == 0);
    assert(eos_fs_read(a, buf, 8) == 8);
    assert(memcmp(buf, "AAAAAAAA", 8) == 0);

    /* ...and writing through it must not reach /b.txt. */
    assert(eos_fs_seek(a, 0, EOS_SEEK_SET) == 0);
    assert(eos_fs_write(a, "XXXXXXXX", 8) == 8);
    memset(buf, 0, sizeof(buf));
    assert(eos_fs_seek(b, 0, EOS_SEEK_SET) == 0);
    assert(eos_fs_read(b, buf, 8) == 8);
    assert(memcmp(buf, "BBBBBBBB", 8) == 0);

    assert(eos_fs_close(a) == 0);
    assert(eos_fs_close(b) == 0);
    eos_fs_deinit();
    printf("[PASS] fs remove with open fd does not alias\n");
}

/* The inode an unlinked-but-open file holds is released on the last close. */
static void test_fs_unlinked_inode_is_reclaimed_on_close(void) {
    eos_fs_init(NULL);
    eos_file_t fd = eos_fs_open("/u.txt", EOS_O_CREATE | EOS_O_WRITE);
    assert(eos_fs_write(fd, "0123456789", 10) == 10);

    eos_fs_stat_t st;
    assert(eos_fs_stat(&st) == 0);
    assert(st.used_bytes == 10);

    assert(eos_fs_remove("/u.txt") == 0);
    assert(eos_fs_stat(&st) == 0);
    assert(st.used_bytes == 10);            /* still occupying space */

    assert(eos_fs_close(fd) == 0);
    assert(eos_fs_stat(&st) == 0);
    assert(st.used_bytes == 0);             /* reclaimed */
    eos_fs_deinit();
    printf("[PASS] fs unlinked inode reclaimed on close\n");
}

/*
 * Regression: used_bytes was a running total that eos_fs_remove() and
 * eos_fs_truncate() could each subtract for the same bytes, wrapping the
 * unsigned counter so free_bytes came out larger than total_bytes.
 */
static void test_fs_used_bytes_never_underflows(void) {
    eos_fs_init(NULL);
    eos_file_t fd = eos_fs_open("/c.txt", EOS_O_CREATE | EOS_O_WRITE);
    assert(eos_fs_write(fd, "0123456789", 10) == 10);
    assert(eos_fs_remove("/c.txt") == 0);
    assert(eos_fs_truncate(fd, 0) == 0);

    eos_fs_stat_t st;
    assert(eos_fs_stat(&st) == 0);
    assert(st.used_bytes == 0);
    assert(st.free_bytes <= st.total_bytes);
    assert(st.used_bytes + st.free_bytes == st.total_bytes);

    assert(eos_fs_close(fd) == 0);
    eos_fs_deinit();
    printf("[PASS] fs used_bytes never underflows\n");
}

/*
 * Regression: eos_fs_rename() overwrote the source inode's name without
 * looking at the destination, leaving two in_use inodes carrying the same
 * path. readdir() listed both and removing the name once left one behind.
 */
static void test_fs_rename_replaces_existing_target(void) {
    eos_fs_init(NULL);
    eos_file_t x = eos_fs_open("/x.txt", EOS_O_CREATE | EOS_O_WRITE);
    assert(eos_fs_write(x, "XX", 2) == 2);
    assert(eos_fs_close(x) == 0);
    eos_file_t y = eos_fs_open("/y.txt", EOS_O_CREATE | EOS_O_WRITE);
    assert(eos_fs_write(y, "YYYY", 4) == 4);
    assert(eos_fs_close(y) == 0);

    assert(eos_fs_rename("/x.txt", "/y.txt") == 0);
    assert(!eos_fs_exists("/x.txt"));
    assert(eos_fs_exists("/y.txt"));

    /* Exactly one directory entry, and it is the source file's contents. */
    eos_dir_t dir = eos_fs_opendir("/");
    assert(dir != EOS_DIR_INVALID);
    eos_dirent_t entry;
    int seen = 0;
    while (eos_fs_readdir(dir, &entry) == 0)
        if (strcmp(entry.name, "/y.txt") == 0) seen++;
    eos_fs_closedir(dir);
    assert(seen == 1);

    eos_file_t r = eos_fs_open("/y.txt", EOS_O_READ);
    char buf[8] = {0};
    assert(eos_fs_read(r, buf, sizeof(buf)) == 2);
    assert(memcmp(buf, "XX", 2) == 0);
    assert(eos_fs_close(r) == 0);

    /* One remove is enough to make the name go away. */
    assert(eos_fs_remove("/y.txt") == 0);
    assert(!eos_fs_exists("/y.txt"));
    eos_fs_deinit();
    printf("[PASS] fs rename replaces existing target\n");
}

/* Renaming a file onto its own name is a no-op, not a self-destruct. */
static void test_fs_rename_onto_itself_is_a_noop(void) {
    eos_fs_init(NULL);
    eos_file_t fd = eos_fs_open("/same.txt", EOS_O_CREATE | EOS_O_WRITE);
    assert(eos_fs_write(fd, "keep", 4) == 4);
    assert(eos_fs_close(fd) == 0);

    assert(eos_fs_rename("/same.txt", "/same.txt") == 0);
    assert(eos_fs_exists("/same.txt"));

    fd = eos_fs_open("/same.txt", EOS_O_READ);
    char buf[8] = {0};
    assert(eos_fs_read(fd, buf, sizeof(buf)) == 4);
    assert(memcmp(buf, "keep", 4) == 0);
    assert(eos_fs_close(fd) == 0);
    eos_fs_deinit();
    printf("[PASS] fs rename onto itself is a no-op\n");
}

/* Running out of descriptors must not strand the inode open() just created. */
static void test_fs_open_does_not_leak_inode_when_fds_exhausted(void) {
    eos_fs_init(NULL);
    char path[16];
    for (int i = 0; i < EOS_FILE_MAX; i++) {
        snprintf(path, sizeof(path), "/fd%d", i);
        assert(eos_fs_open(path, EOS_O_CREATE | EOS_O_WRITE) != EOS_FILE_INVALID);
    }
    assert(eos_fs_open("/overflow", EOS_O_CREATE | EOS_O_WRITE) == EOS_FILE_INVALID);
    /* The failed open must not have left a nameless inode behind. */
    assert(!eos_fs_exists("/overflow"));

    eos_dir_t dir = eos_fs_opendir("/");
    eos_dirent_t entry;
    int count = 0;
    while (eos_fs_readdir(dir, &entry) == 0) count++;
    eos_fs_closedir(dir);
    assert(count == EOS_FILE_MAX + 1);   /* the files plus the root directory */
    eos_fs_deinit();
    printf("[PASS] fs open does not leak inode when fds exhausted\n");
}

int main(void) {
    printf("=== EoS Filesystem Tests ===\n");
    test_fs_init();
    test_fs_write_read();
    test_fs_seek();
    test_fs_truncate();
    test_fs_dir();
    test_fs_remove_rename();
    test_fs_format();
    test_fs_remove_with_open_fd_does_not_alias();
    test_fs_unlinked_inode_is_reclaimed_on_close();
    test_fs_used_bytes_never_underflows();
    test_fs_rename_replaces_existing_target();
    test_fs_rename_onto_itself_is_a_noop();
    test_fs_open_does_not_leak_inode_when_fds_exhausted();
    printf("=== ALL FS TESTS PASSED (13/13) ===\n");
    return 0;
}