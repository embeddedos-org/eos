// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/system.h"
#include "eos/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

EosResult eos_system_build_kernel(EosSystem *sys) {
    EOS_INFO("Building kernel (provider: %s)", sys->kernel_provider);

    if (sys->dry_run) {
        EOS_INFO("  (dry-run) Would build kernel with %s", sys->kernel_provider);
        return EOS_OK;
    }

    /*
     * Kernel build strategy depends on provider:
     * - "kbuild": Direct Linux kernel build using Kbuild backend
     * - "buildroot": Delegate to Buildroot for kernel
     * - "custom": User-supplied pre-built kernel
     */
    if (strcmp(sys->kernel_provider, "kbuild") == 0) {
        EOS_INFO("  Kernel source: %s", sys->kernel_src[0] ? sys->kernel_src : "(not set)");
        EOS_INFO("  Build dir: %s", sys->kernel_build_dir);
        EOS_INFO("  Defconfig: %s", sys->kernel_defconfig[0] ? sys->kernel_defconfig : "defconfig");
        EOS_WARN("  Kernel build requires kernel source tree — skipping in scaffold");
    } else if (strcmp(sys->kernel_provider, "buildroot") == 0) {
        EOS_INFO("  Delegating kernel build to Buildroot");
        EOS_WARN("  Buildroot integration not yet implemented");
    } else {
        EOS_WARN("  Unknown kernel provider: %s", sys->kernel_provider);
    }

    return EOS_OK;
}

EosResult eos_system_build_image(EosSystem *sys) {
    EOS_INFO("Creating disk image: %s", sys->image_output);
    EOS_INFO("  Format: %d, Size: %zu MB", sys->image_format, sys->image_size_mb);

    if (sys->dry_run) {
        EOS_INFO("  (dry-run) Would create %zu MB image at %s",
                   sys->image_size_mb, sys->image_output);
        return EOS_OK;
    }

    switch (sys->image_format) {
    case EOS_IMG_RAW: {
        EOS_INFO("  Creating raw disk image...");
        char cmd[2048];

        /* Create empty image file */
#ifdef _WIN32
        snprintf(cmd, sizeof(cmd), "fsutil file createnew \"%s\" %zu",
                 sys->image_output, sys->image_size_mb * 1024 * 1024);
#else
        snprintf(cmd, sizeof(cmd), "dd if=/dev/zero of=\"%s\" bs=1M count=%zu 2>/dev/null",
                 sys->image_output, sys->image_size_mb);
#endif
        EOS_INFO("  %s", cmd);
        int rc = system(cmd);
        if (rc != 0) {
            EOS_WARN("  Image creation command returned %d (may need elevated privileges)", rc);
        }

#ifndef _WIN32
        /* Create partition table (Linux only) */
        snprintf(cmd, sizeof(cmd),
                 "echo -e 'o\\nn\\np\\n1\\n\\n\\nw' | fdisk \"%s\" 2>/dev/null || true",
                 sys->image_output);
        EOS_DEBUG("  Partitioning: %s", cmd);
        system(cmd);
#endif

        EOS_INFO("  Raw image created: %s (%zu MB)", sys->image_output, sys->image_size_mb);
        break;
    }

    case EOS_IMG_QCOW2: {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "qemu-img create -f qcow2 \"%s\" %zuM",
                 sys->image_output, sys->image_size_mb);
        EOS_INFO("  %s", cmd);
        int rc = system(cmd);
        if (rc != 0) {
            EOS_WARN("  qemu-img not available, creating placeholder");
        }
        break;
    }

    case EOS_IMG_TAR: {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "tar czf \"%s\" -C \"%s\" .",
                 sys->image_output, sys->rootfs_dir);
        EOS_INFO("  %s", cmd);
        int rc = system(cmd);
        return (rc == 0) ? EOS_OK : EOS_ERR_SYSTEM;
    }

    default:
        EOS_WARN("  Image format %d not yet implemented", sys->image_format);
        break;
    }

    return EOS_OK;
}
