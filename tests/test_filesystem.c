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

int main(void) {
    printf("=== EoS Filesystem Tests ===\n");
    test_fs_init();
    test_fs_write_read();
    test_fs_seek();
    test_fs_truncate();
    test_fs_dir();
    test_fs_remove_rename();
    test_fs_format();
    printf("=== ALL FS TESTS PASSED (7/7) ===\n");
    return 0;
}