// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <string.h>
#include "eos/config.h"
#include "eos/lockfile.h"
#include "eos/log.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(expr, msg)                                         \
    do {                                                          \
        tests_run++;                                              \
        if (!(expr)) {                                            \
            printf("  FAIL: %s (line %d)\n", msg, __LINE__);     \
        } else {                                                  \
            tests_passed++;                                       \
            printf("  PASS: %s\n", msg);                         \
        }                                                         \
    } while (0)

static void test_config_init(void) {
    printf("test_config_init:\n");
    static EosConfig cfg;
    eos_config_init(&cfg);

    ASSERT(strcmp(cfg.workspace.backend, "ninja") == 0, "default backend is ninja");
    ASSERT(strcmp(cfg.workspace.build_dir, ".eos/build") == 0, "default build_dir");
    ASSERT(strcmp(cfg.workspace.cache_dir, ".eos/cache") == 0, "default cache_dir");
    ASSERT(cfg.system.image_format == EOS_IMG_RAW, "default image format is raw");
    ASSERT(cfg.system.rootfs.init == EOS_INIT_BUSYBOX, "default init is busybox");
    ASSERT(cfg.package_count == 0, "no packages initially");
    ASSERT(cfg.layer_count == 0, "no layers initially");
}

static void test_config_load(void) {
    printf("test_config_load:\n");
    static EosConfig cfg;

    /* Write a test config file */
    FILE *fp = fopen("test_eos.yaml", "w");
    if (!fp) {
        printf("  SKIP: cannot create test config file\n");
        return;
    }

    fprintf(fp, "project:\n");
    fprintf(fp, "  name: test-project\n");
    fprintf(fp, "  version: 1.2.3\n");
    fprintf(fp, "\n");
    fprintf(fp, "workspace:\n");
    fprintf(fp, "  backend: cmake\n");
    fprintf(fp, "  build_dir: build/output\n");
    fprintf(fp, "\n");
    fprintf(fp, "toolchain:\n");
    fprintf(fp, "  target: aarch64-linux-gnu\n");
    fprintf(fp, "\n");
    fprintf(fp, "layers:\n");
    fprintf(fp, "  - layers/core\n");
    fprintf(fp, "  - layers/bsp/qemu-arm64\n");
    fprintf(fp, "\n");
    fprintf(fp, "packages:\n");
    fprintf(fp, "  - name: zlib\n");
    fprintf(fp, "    version: 1.2.13\n");
    fprintf(fp, "    build:\n");
    fprintf(fp, "      type: cmake\n");
    fprintf(fp, "\n");
    fprintf(fp, "system:\n");
    fprintf(fp, "  kernel:\n");
    fprintf(fp, "    provider: kbuild\n");
    fprintf(fp, "  rootfs:\n");
    fprintf(fp, "    provider: eos\n");
    fclose(fp);

    EosResult res = eos_config_load(&cfg, "test_eos.yaml");

    ASSERT(res == EOS_OK, "config loads successfully");
    ASSERT(strcmp(cfg.project.name, "test-project") == 0, "project name parsed");
    ASSERT(strcmp(cfg.project.version, "1.2.3") == 0, "project version parsed");
    ASSERT(strcmp(cfg.workspace.backend, "cmake") == 0, "workspace backend parsed");
    ASSERT(strcmp(cfg.toolchain.target, "aarch64-linux-gnu") == 0, "toolchain target parsed");
    ASSERT(cfg.layer_count == 2, "two layers parsed");
    ASSERT(strcmp(cfg.layers[0], "layers/core") == 0, "first layer path");
    ASSERT(cfg.package_count == 1, "one package parsed");
    ASSERT(strcmp(cfg.packages[0].name, "zlib") == 0, "package name parsed");
    ASSERT(strcmp(cfg.packages[0].version, "1.2.13") == 0, "package version parsed");
    ASSERT(cfg.packages[0].build_type == EOS_BUILD_CMAKE, "package build type parsed");
    ASSERT(strcmp(cfg.system.kernel.provider, "kbuild") == 0, "kernel provider parsed");
    ASSERT(strcmp(cfg.system.rootfs.provider, "eos") == 0, "rootfs provider parsed");

    remove("test_eos.yaml");
}

static void test_config_missing_file(void) {
    printf("test_config_missing_file:\n");
    static EosConfig cfg;
    EosResult res = eos_config_load(&cfg, "nonexistent.yaml");
    ASSERT(res == EOS_ERR_IO, "returns IO error for missing file");
}

static void test_lockfile_freshness(void) {
    printf("test_lockfile_freshness:\n");
    static EosConfig cfg;
    static EosLockfile lock;

    eos_config_init(&cfg);
    snprintf(cfg.project.name, sizeof(cfg.project.name), "%s", "demo");
    snprintf(cfg.project.version, sizeof(cfg.project.version), "%s", "1.0.0");
    cfg.package_count = 2;

    snprintf(cfg.packages[0].name, sizeof(cfg.packages[0].name), "%s", "alpha");
    snprintf(cfg.packages[0].version, sizeof(cfg.packages[0].version), "%s", "1.2.3");
    snprintf(cfg.packages[0].source, sizeof(cfg.packages[0].source), "%s", "https://example.com/alpha.tar.gz");
    snprintf(cfg.packages[0].hash, sizeof(cfg.packages[0].hash), "%s", "explicit-checksum");
    cfg.packages[0].build_type = EOS_BUILD_CMAKE;

    snprintf(cfg.packages[1].name, sizeof(cfg.packages[1].name), "%s", "beta");
    snprintf(cfg.packages[1].version, sizeof(cfg.packages[1].version), "%s", "4.5.6");
    snprintf(cfg.packages[1].source, sizeof(cfg.packages[1].source), "%s", "https://example.com/beta.tar.gz");
    cfg.packages[1].build_type = EOS_BUILD_MAKE;

    ASSERT(eos_lockfile_generate(&lock, &cfg) == EOS_OK, "lockfile generates");
    ASSERT(eos_lockfile_is_current(&lock, &cfg), "generated lockfile is current");

    EosLockEntry tmp = lock.entries[0];
    lock.entries[0] = lock.entries[1];
    lock.entries[1] = tmp;
    ASSERT(eos_lockfile_is_current(&lock, &cfg), "package order does not affect freshness");
    tmp = lock.entries[0];
    lock.entries[0] = lock.entries[1];
    lock.entries[1] = tmp;

    snprintf(cfg.project.version, sizeof(cfg.project.version), "%s", "2.0.0");
    ASSERT(!eos_lockfile_is_current(&lock, &cfg), "project version change is stale");
    snprintf(cfg.project.version, sizeof(cfg.project.version), "%s", "1.0.0");

    snprintf(cfg.packages[0].name, sizeof(cfg.packages[0].name), "%s", "renamed-alpha");
    ASSERT(!eos_lockfile_is_current(&lock, &cfg), "package name change is stale");
    snprintf(cfg.packages[0].name, sizeof(cfg.packages[0].name), "%s", "alpha");

    snprintf(cfg.packages[0].version, sizeof(cfg.packages[0].version), "%s", "1.2.4");
    ASSERT(!eos_lockfile_is_current(&lock, &cfg), "requested version change is stale");
    snprintf(cfg.packages[0].version, sizeof(cfg.packages[0].version), "%s", "1.2.3");

    snprintf(cfg.packages[0].source, sizeof(cfg.packages[0].source), "%s", "https://example.com/alpha-v2.tar.gz");
    ASSERT(!eos_lockfile_is_current(&lock, &cfg), "package source change is stale");
    snprintf(cfg.packages[0].source, sizeof(cfg.packages[0].source), "%s", "https://example.com/alpha.tar.gz");

    snprintf(cfg.packages[0].hash, sizeof(cfg.packages[0].hash), "%s", "new-checksum");
    ASSERT(!eos_lockfile_is_current(&lock, &cfg), "explicit checksum change is stale");
    snprintf(cfg.packages[0].hash, sizeof(cfg.packages[0].hash), "%s", "explicit-checksum");

    cfg.packages[0].build_type = EOS_BUILD_MAKE;
    ASSERT(!eos_lockfile_is_current(&lock, &cfg), "build type change is stale");
    cfg.packages[0].build_type = EOS_BUILD_CMAKE;

    snprintf(lock.entries[0].resolved_version, sizeof(lock.entries[0].resolved_version), "%s", "1.2.4");
    ASSERT(!eos_lockfile_is_current(&lock, &cfg), "resolved version change is stale");
    snprintf(lock.entries[0].resolved_version, sizeof(lock.entries[0].resolved_version), "%s", "1.2.3");

    lock.entries[1].hash[0] ^= 1;
    ASSERT(!eos_lockfile_is_current(&lock, &cfg), "generated checksum change is stale");
}

int main(void) {
    eos_log_set_level(EOS_LOG_ERROR);

    printf("=== EoS Config Tests ===\n\n");

    test_config_init();
    test_config_load();
    test_config_missing_file();
    test_lockfile_freshness();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
