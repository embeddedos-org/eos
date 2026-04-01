// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <string.h>
#include "eos/config.h"
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

int main(void) {
    eos_log_set_level(EOS_LOG_ERROR);

    printf("=== EoS Config Tests ===\n\n");

    test_config_init();
    test_config_load();
    test_config_missing_file();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
