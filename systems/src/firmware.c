// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/system.h"
#include "eos/backend.h"
#include "eos/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#else
#include <sys/types.h>
#define MKDIR(d) mkdir(d, 0755)
#endif

static EosBuildType provider_to_build_type(EosRtosProvider p) {
    switch (p) {
        case EOS_RTOS_ZEPHYR:   return EOS_BUILD_ZEPHYR;
        case EOS_RTOS_FREERTOS: return EOS_BUILD_FREERTOS;
        case EOS_RTOS_NUTTX:    return EOS_BUILD_NUTTX;
        default:                  return EOS_BUILD_CMAKE;
    }
}

void eos_firmware_init(EosFirmware *fw, const EosRtosConfig *rtos_cfg,
                       const EosConfig *cfg) {
    memset(fw, 0, sizeof(*fw));

    fw->provider = rtos_cfg->provider;
    fw->format = rtos_cfg->format;

    strncpy(fw->board, rtos_cfg->board, EOS_MAX_NAME - 1);
    strncpy(fw->core, rtos_cfg->core, EOS_MAX_NAME - 1);
    strncpy(fw->entry, rtos_cfg->entry, EOS_MAX_PATH - 1);

    if (rtos_cfg->output[0]) {
        strncpy(fw->output, rtos_cfg->output, EOS_MAX_PATH - 1);
    } else {
        snprintf(fw->output, EOS_MAX_PATH, "%s/firmware.%s",
                 cfg->workspace.build_dir,
                 eos_firmware_format_str(fw->format));
    }

    /* Set toolchain target for RTOS */
    if (cfg->toolchain.rtos_target[0]) {
        strncpy(fw->toolchain_target, cfg->toolchain.rtos_target, EOS_MAX_NAME - 1);
    } else if (cfg->toolchain.target[0]) {
        strncpy(fw->toolchain_target, cfg->toolchain.target, EOS_MAX_NAME - 1);
    }

    snprintf(fw->build_dir, EOS_MAX_PATH, "%s/firmware/%s",
             cfg->workspace.build_dir,
             fw->core[0] ? fw->core : eos_rtos_provider_str(fw->provider));
    snprintf(fw->src_dir, EOS_MAX_PATH, "%s", fw->entry);
    snprintf(fw->install_dir, EOS_MAX_PATH, "%s/firmware/output", cfg->workspace.build_dir);
}

EosResult eos_firmware_build(EosFirmware *fw) {
    EOS_INFO("=== EoS Firmware Build ===");
    EOS_INFO("  Provider: %s", eos_rtos_provider_str(fw->provider));
    EOS_INFO("  Board:    %s", fw->board[0] ? fw->board : "(not set)");
    EOS_INFO("  Core:     %s", fw->core[0] ? fw->core : "(default)");
    EOS_INFO("  Entry:    %s", fw->entry[0] ? fw->entry : "(not set)");
    EOS_INFO("  Output:   %s", fw->output);
    EOS_INFO("  Format:   %s", eos_firmware_format_str(fw->format));

    if (fw->dry_run) {
        EOS_INFO("--- DRY RUN: Firmware Build Pipeline ---");
        EOS_INFO("Step 1: Resolve RTOS SDK / source tree");
        EOS_INFO("  Provider: %s", eos_rtos_provider_str(fw->provider));
        if (fw->board[0])
            EOS_INFO("  Board: %s", fw->board);

        EOS_INFO("Step 2: Configure firmware build");
        switch (fw->provider) {
        case EOS_RTOS_ZEPHYR:
            EOS_INFO("  west build -b %s -d \"%s\" \"%s\"",
                     fw->board, fw->build_dir, fw->src_dir);
            break;
        case EOS_RTOS_FREERTOS:
            EOS_INFO("  cmake -S \"%s\" -B \"%s\" -G Ninja -DBOARD=%s",
                     fw->src_dir, fw->build_dir, fw->board);
            break;
        case EOS_RTOS_NUTTX:
            EOS_INFO("  ./tools/configure.sh %s", fw->board);
            break;
        default:
            EOS_INFO("  (custom provider configure)");
            break;
        }

        if (fw->toolchain_target[0])
            EOS_INFO("  Cross-compile: %s", fw->toolchain_target);

        EOS_INFO("Step 3: Build firmware");
        EOS_INFO("  Build dir: %s", fw->build_dir);

        EOS_INFO("Step 4: Generate firmware output");
        EOS_INFO("  Output: %s (format: %s)",
                 fw->output, eos_firmware_format_str(fw->format));

        EOS_INFO("Step 5: Post-build (size report, checksums)");
        EOS_INFO("--- End Pipeline ---");
        return EOS_OK;
    }

    /* Create build directories */
    MKDIR(fw->build_dir);
    MKDIR(fw->install_dir);

    /* Look up the RTOS backend */
    EosBuildType bt = provider_to_build_type(fw->provider);
    EosBackend *be = eos_backend_find_by_type(bt);

    if (!be) {
        EOS_WARN("No backend for %s, trying by name",
                 eos_rtos_provider_str(fw->provider));
        be = eos_backend_find(eos_rtos_provider_str(fw->provider));
    }

    if (be) {
        /* Build board option for configure */
        EosKeyValue opts[4];
        int opt_count = 0;
        if (fw->board[0]) {
            strncpy(opts[opt_count].key, "board", EOS_MAX_NAME - 1);
            strncpy(opts[opt_count].value, fw->board, EOS_MAX_PATH - 1);
            opt_count++;
        }

        /* Configure */
        EOS_INFO("Configure: %s -> %s (backend=%s)",
                 fw->src_dir, fw->build_dir, be->name);
        EosResult res = be->configure(be, fw->src_dir, fw->build_dir,
                                      fw->toolchain_target, opts, opt_count);
        if (res != EOS_OK) {
            EOS_ERROR("Firmware configure failed: %s", eos_error_str(res));
            return res;
        }

        /* Build */
        EOS_INFO("Build: %s", fw->build_dir);
        res = be->build(be, fw->build_dir, 4);
        if (res != EOS_OK) {
            EOS_ERROR("Firmware build failed: %s", eos_error_str(res));
            return res;
        }

        /* Install / copy output */
        EOS_INFO("Install: %s -> %s", fw->build_dir, fw->install_dir);
        res = be->install(be, fw->build_dir, fw->install_dir);
        if (res != EOS_OK) {
            EOS_WARN("Install returned non-zero (non-fatal)");
        }
    } else {
        EOS_WARN("No backend found for provider %s — generating placeholder",
                 eos_rtos_provider_str(fw->provider));

        FILE *fp = fopen(fw->output, "wb");
        if (fp) {
            fprintf(fp, "EOS_FW\nProvider: %s\nBoard: %s\nFormat: %s\n",
                    eos_rtos_provider_str(fw->provider),
                    fw->board, eos_firmware_format_str(fw->format));
            fclose(fp);
        }
    }

    EOS_INFO("Firmware build complete: %s", fw->output);
    return EOS_OK;
}

EosResult eos_firmware_flash(const EosFirmware *fw) {
    EOS_INFO("Flashing firmware: %s", fw->output);
    EOS_INFO("  Provider: %s, Board: %s",
             eos_rtos_provider_str(fw->provider), fw->board);

    switch (fw->provider) {
    case EOS_RTOS_ZEPHYR:
        EOS_INFO("  Flash command: west flash -d \"%s\"", fw->build_dir);
        break;
    case EOS_RTOS_FREERTOS:
        EOS_INFO("  Flash command: openocd / pyocd / vendor tool");
        break;
    case EOS_RTOS_NUTTX:
        EOS_INFO("  Flash command: nuttx flash / openocd");
        break;
    default:
        EOS_INFO("  Flash command: (provider-specific)");
        break;
    }

    EOS_WARN("Flash not yet wired to actual tool — scaffold only");
    return EOS_OK;
}

void eos_firmware_dump(const EosFirmware *fw) {
    printf("Firmware Configuration:\n");
    printf("  Provider:  %s\n", eos_rtos_provider_str(fw->provider));
    printf("  Board:     %s\n", fw->board[0] ? fw->board : "(not set)");
    printf("  Core:      %s\n", fw->core[0] ? fw->core : "(default)");
    printf("  Entry:     %s\n", fw->entry[0] ? fw->entry : "(not set)");
    printf("  Output:    %s\n", fw->output);
    printf("  Format:    %s\n", eos_firmware_format_str(fw->format));
    printf("  Build dir: %s\n", fw->build_dir);
    if (fw->toolchain_target[0])
        printf("  Toolchain: %s\n", fw->toolchain_target);
}

void eos_hybrid_init(EosHybridSystem *hybrid, const EosConfig *cfg) {
    memset(hybrid, 0, sizeof(*hybrid));
    hybrid->kind = cfg->system.kind;

    /* Initialize Linux side */
    eos_system_init(&hybrid->linux_sys, cfg);

    /* Initialize all RTOS firmware targets */
    hybrid->firmware_count = cfg->system.rtos_count;
    for (int i = 0; i < cfg->system.rtos_count && i < EOS_MAX_RTOS; i++) {
        eos_firmware_init(&hybrid->firmware[i], &cfg->system.rtos[i], cfg);
    }
}

EosResult eos_hybrid_build(EosHybridSystem *hybrid) {
    EOS_INFO("=== EoS Hybrid Build ===");
    EOS_INFO("  Linux + %d RTOS target(s)", hybrid->firmware_count);

    /* Build Linux system first */
    EOS_INFO("--- Phase 1: Linux System ---");
    hybrid->linux_sys.dry_run = hybrid->dry_run;
    hybrid->linux_sys.verbose = hybrid->verbose;
    EOS_CHECK(eos_system_build(&hybrid->linux_sys));

    /* Build each RTOS firmware target */
    for (int i = 0; i < hybrid->firmware_count; i++) {
        EOS_INFO("--- Phase 2.%d: RTOS Firmware [%s / %s] ---",
                 i + 1,
                 eos_rtos_provider_str(hybrid->firmware[i].provider),
                 hybrid->firmware[i].core[0] ? hybrid->firmware[i].core : "default");
        hybrid->firmware[i].dry_run = hybrid->dry_run;
        hybrid->firmware[i].verbose = hybrid->verbose;
        EOS_CHECK(eos_firmware_build(&hybrid->firmware[i]));
    }

    EOS_INFO("=== Hybrid Build Complete ===");
    return EOS_OK;
}

void eos_hybrid_dump(const EosHybridSystem *hybrid) {
    printf("Hybrid System Configuration:\n");
    printf("  Kind: %s\n", eos_system_kind_str(hybrid->kind));
    printf("\n  --- Linux ---\n");
    eos_system_dump(&hybrid->linux_sys);
    printf("\n  --- Firmware Targets (%d) ---\n", hybrid->firmware_count);
    for (int i = 0; i < hybrid->firmware_count; i++) {
        printf("  [%d]:\n", i);
        eos_firmware_dump(&hybrid->firmware[i]);
    }
}
