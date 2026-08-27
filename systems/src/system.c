// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/system.h"
#include "eos/log.h"
#include <string.h>
#include <stdio.h>

/* Compose a path and report truncation rather than letting it pass.
 * EOS_MAX_PATH is 512 and workspace.build_dir may itself be that long, so
 * appending "/kernel" or "/staging" can overflow. A truncated directory is not
 * a cosmetic problem in a build system: two configurations can collapse onto
 * the same path and overwrite each other's output. */
static EosResult join_path(char *dst, const char *base, const char *suffix,
                           const char *what) {
    int n = snprintf(dst, EOS_MAX_PATH, "%s%s", base, suffix);
    if (n < 0 || n >= EOS_MAX_PATH) {
        EOS_ERROR("%s path exceeds %d bytes (base '%s')", what, EOS_MAX_PATH, base);
        return EOS_ERR_OVERFLOW;
    }
    return EOS_OK;
}

EosResult eos_system_init(EosSystem *sys, const EosConfig *cfg) {
    memset(sys, 0, sizeof(*sys));

    strncpy(sys->kernel_provider, cfg->system.kernel.provider, EOS_MAX_NAME - 1);
    strncpy(sys->kernel_defconfig, cfg->system.kernel.defconfig, EOS_MAX_PATH - 1);
    strncpy(sys->rootfs_provider, cfg->system.rootfs.provider, EOS_MAX_NAME - 1);
    strncpy(sys->hostname, cfg->system.rootfs.hostname, EOS_MAX_NAME - 1);
    sys->init_system = cfg->system.rootfs.init;
    sys->image_format = cfg->system.image_format;

    const char *bd = cfg->workspace.build_dir;
    EosResult rc;
    if ((rc = join_path(sys->kernel_build_dir, bd, "/kernel",  "Kernel build")) != EOS_OK) return rc;
    if ((rc = join_path(sys->rootfs_dir,       bd, "/rootfs",  "Rootfs"))       != EOS_OK) return rc;
    if ((rc = join_path(sys->staging_dir,      bd, "/staging", "Staging"))      != EOS_OK) return rc;
    if ((rc = join_path(sys->install_dir,      bd, "/install", "Install"))      != EOS_OK) return rc;

    if (cfg->system.output[0]) {
        strncpy(sys->image_output, cfg->system.output, EOS_MAX_PATH - 1);
    } else {
        /* project.name is up to EOS_MAX_NAME, so this can overflow too. */
        int n = snprintf(sys->image_output, EOS_MAX_PATH, "%s/%s.img",
                         bd, cfg->project.name);
        if (n < 0 || n >= EOS_MAX_PATH) {
            EOS_ERROR("Image output path exceeds %d bytes (project '%s')",
                      EOS_MAX_PATH, cfg->project.name);
            return EOS_ERR_OVERFLOW;
        }
    }

    sys->image_size_mb = 256;
    return EOS_OK;
}

EosResult eos_system_build(EosSystem *sys) {
    EOS_INFO("=== EoS System Build ===");
    EOS_INFO("  Kernel: %s (defconfig: %s)", sys->kernel_provider,
               sys->kernel_defconfig[0] ? sys->kernel_defconfig : "default");
    EOS_INFO("  Rootfs: %s (init: %d)", sys->rootfs_provider, sys->init_system);
    EOS_INFO("  Output: %s", sys->image_output);

    if (sys->dry_run) {
        EOS_INFO("--- DRY RUN: System Build Pipeline ---");
        EOS_INFO("Step 1: Build all packages (cross-compiled)");
        EOS_INFO("Step 2: Install packages to staging directory");
        EOS_INFO("Step 3: Build kernel (%s)", sys->kernel_provider);
        EOS_INFO("Step 4: Assemble rootfs (%s)", sys->rootfs_provider);
        EOS_INFO("  - Create directory skeleton");
        EOS_INFO("  - Install packages from staging");
        EOS_INFO("  - Configure init system");
        EOS_INFO("  - Set hostname: %s", sys->hostname);
        EOS_INFO("Step 5: Create disk image");
        EOS_INFO("  - Format: %d", sys->image_format);
        EOS_INFO("  - Size: %zu MB", sys->image_size_mb);
        EOS_INFO("  - Create partition table");
        EOS_INFO("  - Format filesystems");
        EOS_INFO("  - Install kernel + rootfs");
        EOS_INFO("  - Install bootloader");
        EOS_INFO("Step 6: Output: %s", sys->image_output);
        EOS_INFO("--- End Pipeline ---");
        return EOS_OK;
    }

    EOS_CHECK(eos_system_build_kernel(sys));
    EOS_CHECK(eos_system_build_rootfs(sys));
    EOS_CHECK(eos_system_build_image(sys));

    EOS_INFO("System build complete: %s", sys->image_output);
    return EOS_OK;
}

void eos_system_dump(const EosSystem *sys) {
    printf("System Configuration:\n");
    printf("  Kernel:\n");
    printf("    provider:  %s\n", sys->kernel_provider);
    printf("    defconfig: %s\n", sys->kernel_defconfig[0] ? sys->kernel_defconfig : "(default)");
    printf("    build_dir: %s\n", sys->kernel_build_dir);
    printf("  Rootfs:\n");
    printf("    provider:  %s\n", sys->rootfs_provider);
    printf("    init:      %d\n", sys->init_system);
    printf("    hostname:  %s\n", sys->hostname);
    printf("    rootfs_dir:%s\n", sys->rootfs_dir);
    printf("  Image:\n");
    printf("    format:    %d\n", sys->image_format);
    printf("    size:      %zu MB\n", sys->image_size_mb);
    printf("    output:    %s\n", sys->image_output);
}
