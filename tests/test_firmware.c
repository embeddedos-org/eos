// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eos/types.h"
#include "eos/config.h"
#include "eos/system.h"
#include "eos/graph.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  TEST: %s ... ", #name); \
        name(); \
        tests_passed++; \
        printf("PASS\n"); \
    } while (0)

static void test_system_kind_strings(void) {
    assert(strcmp(eos_system_kind_str(EOS_SYSTEM_LINUX), "linux") == 0);
    assert(strcmp(eos_system_kind_str(EOS_SYSTEM_RTOS), "rtos") == 0);
    assert(strcmp(eos_system_kind_str(EOS_SYSTEM_HYBRID), "hybrid") == 0);

    assert(eos_system_kind_from_str("linux") == EOS_SYSTEM_LINUX);
    assert(eos_system_kind_from_str("rtos") == EOS_SYSTEM_RTOS);
    assert(eos_system_kind_from_str("hybrid") == EOS_SYSTEM_HYBRID);
    assert(eos_system_kind_from_str(NULL) == EOS_SYSTEM_LINUX);
}

static void test_rtos_provider_strings(void) {
    assert(strcmp(eos_rtos_provider_str(EOS_RTOS_FREERTOS), "freertos") == 0);
    assert(strcmp(eos_rtos_provider_str(EOS_RTOS_ZEPHYR), "zephyr") == 0);
    assert(strcmp(eos_rtos_provider_str(EOS_RTOS_NUTTX), "nuttx") == 0);
    assert(strcmp(eos_rtos_provider_str(EOS_RTOS_THREADX), "threadx") == 0);

    assert(eos_rtos_provider_from_str("freertos") == EOS_RTOS_FREERTOS);
    assert(eos_rtos_provider_from_str("zephyr") == EOS_RTOS_ZEPHYR);
    assert(eos_rtos_provider_from_str("nuttx") == EOS_RTOS_NUTTX);
    assert(eos_rtos_provider_from_str("threadx") == EOS_RTOS_THREADX);
    assert(eos_rtos_provider_from_str(NULL) == EOS_RTOS_FREERTOS);
}

static void test_firmware_format_strings(void) {
    assert(strcmp(eos_firmware_format_str(EOS_FW_BIN), "bin") == 0);
    assert(strcmp(eos_firmware_format_str(EOS_FW_ELF), "elf") == 0);
    assert(strcmp(eos_firmware_format_str(EOS_FW_HEX), "hex") == 0);
    assert(strcmp(eos_firmware_format_str(EOS_FW_UF2), "uf2") == 0);

    assert(eos_firmware_format_from_str("bin") == EOS_FW_BIN);
    assert(eos_firmware_format_from_str("elf") == EOS_FW_ELF);
    assert(eos_firmware_format_from_str("hex") == EOS_FW_HEX);
    assert(eos_firmware_format_from_str("uf2") == EOS_FW_UF2);
    assert(eos_firmware_format_from_str(NULL) == EOS_FW_BIN);
}

static void test_rtos_build_types(void) {
    assert(strcmp(eos_build_type_str(EOS_BUILD_ZEPHYR), "zephyr") == 0);
    assert(strcmp(eos_build_type_str(EOS_BUILD_FREERTOS), "freertos") == 0);
    assert(strcmp(eos_build_type_str(EOS_BUILD_NUTTX), "nuttx") == 0);

    assert(eos_build_type_from_str("zephyr") == EOS_BUILD_ZEPHYR);
    assert(eos_build_type_from_str("freertos") == EOS_BUILD_FREERTOS);
    assert(eos_build_type_from_str("nuttx") == EOS_BUILD_NUTTX);
}

static void test_arm_cortex_arch(void) {
    assert(strcmp(eos_arch_str(EOS_ARCH_ARM_CORTEX_M), "arm-cortex-m") == 0);
    assert(strcmp(eos_arch_str(EOS_ARCH_ARM_CORTEX_R), "arm-cortex-r") == 0);
}

static void test_firmware_init(void) {
    EosConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.workspace.build_dir, ".eos/build", EOS_MAX_PATH - 1);
    strncpy(cfg.toolchain.rtos_target, "arm-none-eabi", EOS_MAX_NAME - 1);

    EosRtosConfig rtos_cfg;
    memset(&rtos_cfg, 0, sizeof(rtos_cfg));
    rtos_cfg.provider = EOS_RTOS_FREERTOS;
    strncpy(rtos_cfg.board, "stm32h743", EOS_MAX_NAME - 1);
    strncpy(rtos_cfg.entry, "apps/motor-control", EOS_MAX_PATH - 1);
    strncpy(rtos_cfg.output, "firmware.bin", EOS_MAX_PATH - 1);
    rtos_cfg.format = EOS_FW_BIN;

    EosFirmware fw;
    eos_firmware_init(&fw, &rtos_cfg, &cfg);

    assert(fw.provider == EOS_RTOS_FREERTOS);
    assert(strcmp(fw.board, "stm32h743") == 0);
    assert(strcmp(fw.entry, "apps/motor-control") == 0);
    assert(strcmp(fw.output, "firmware.bin") == 0);
    assert(fw.format == EOS_FW_BIN);
    assert(strcmp(fw.toolchain_target, "arm-none-eabi") == 0);
}

static void test_firmware_dry_run(void) {
    EosConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.workspace.build_dir, ".eos/build", EOS_MAX_PATH - 1);

    EosRtosConfig rtos_cfg;
    memset(&rtos_cfg, 0, sizeof(rtos_cfg));
    rtos_cfg.provider = EOS_RTOS_ZEPHYR;
    strncpy(rtos_cfg.board, "nrf52840dk", EOS_MAX_NAME - 1);
    strncpy(rtos_cfg.entry, "apps/ble-sensor", EOS_MAX_PATH - 1);

    EosFirmware fw;
    eos_firmware_init(&fw, &rtos_cfg, &cfg);
    fw.dry_run = 1;

    EosResult res = eos_firmware_build(&fw);
    assert(res == EOS_OK);
}

static void test_hybrid_init(void) {
    EosConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.workspace.build_dir, ".eos/build", EOS_MAX_PATH - 1);
    cfg.system.kind = EOS_SYSTEM_HYBRID;
    strncpy(cfg.system.kernel.provider, "buildroot", EOS_MAX_NAME - 1);
    strncpy(cfg.system.rootfs.provider, "buildroot", EOS_MAX_NAME - 1);

    cfg.system.rtos[0].provider = EOS_RTOS_FREERTOS;
    strncpy(cfg.system.rtos[0].board, "stm32mp1-m4", EOS_MAX_NAME - 1);
    strncpy(cfg.system.rtos[0].core, "m4", EOS_MAX_NAME - 1);
    cfg.system.rtos_count = 1;

    EosHybridSystem hybrid;
    eos_hybrid_init(&hybrid, &cfg);

    assert(hybrid.kind == EOS_SYSTEM_HYBRID);
    assert(hybrid.firmware_count == 1);
    assert(hybrid.firmware[0].provider == EOS_RTOS_FREERTOS);
    assert(strcmp(hybrid.firmware[0].core, "m4") == 0);
}

static void test_firmware_graph_node(void) {
    EosGraph graph;
    eos_graph_init(&graph);

    int fw_id = -1;
    EosResult res = eos_graph_add_node(&graph, "firmware-m4",
                                       EOS_NODE_FIRMWARE, EOS_BUILD_FREERTOS, &fw_id);
    assert(res == EOS_OK);
    assert(fw_id >= 0);
    assert(graph.nodes[fw_id].type == EOS_NODE_FIRMWARE);
    assert(graph.nodes[fw_id].build_type == EOS_BUILD_FREERTOS);
}

static void test_config_load_rtos(void) {
    EosConfig cfg;
    EosResult res = eos_config_load(&cfg, "examples/motor-controller/eos.yaml");
    if (res != EOS_OK) {
        printf("(skip - example file not found) ");
        return;
    }

    assert(cfg.system.kind == EOS_SYSTEM_RTOS);
    assert(cfg.system.rtos_count == 1);
    assert(cfg.system.rtos[0].provider == EOS_RTOS_FREERTOS);
    assert(strcmp(cfg.system.rtos[0].board, "stm32h743") == 0);
}

static void test_config_load_hybrid(void) {
    EosConfig cfg;
    EosResult res = eos_config_load(&cfg, "examples/industrial-gateway/eos.yaml");
    if (res != EOS_OK) {
        printf("(skip - example file not found) ");
        return;
    }

    assert(cfg.system.kind == EOS_SYSTEM_HYBRID);
    assert(cfg.system.rtos_count >= 1);
    assert(cfg.system.rtos[0].provider == EOS_RTOS_FREERTOS);
    assert(strcmp(cfg.system.rtos[0].core, "m4") == 0);
    assert(strcmp(cfg.toolchain.rtos_target, "arm-none-eabi") == 0);
}

int main(void) {
    printf("\n=== EoS Firmware Tests ===\n\n");

    TEST(test_system_kind_strings);
    TEST(test_rtos_provider_strings);
    TEST(test_firmware_format_strings);
    TEST(test_rtos_build_types);
    TEST(test_arm_cortex_arch);
    TEST(test_firmware_init);
    TEST(test_firmware_dry_run);
    TEST(test_hybrid_init);
    TEST(test_firmware_graph_node);
    TEST(test_config_load_rtos);
    TEST(test_config_load_hybrid);

    printf("\n=== Results: %d/%d passed ===\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
