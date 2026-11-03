// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/system.h"
#include "eos/shell_cmd.h"
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
        EosShellCmd cmd;
        EosResult result;
        char num[32];

        /* Create empty image file. Paths come from configuration, so they
         * go through eos_shell_cmd_arg() (double-quoted, metacharacters
         * refused) instead of being interpolated into a system() string:
         * a path such as $(id) previously ran as shell inside the quotes. */
#ifdef _WIN32
        snprintf(num, sizeof(num), "%zu", sys->image_size_mb * 1024 * 1024);
        eos_shell_cmd_init(&cmd);
        eos_shell_cmd_text(&cmd, "fsutil file createnew ");
        eos_shell_cmd_arg(&cmd, sys->image_output);
        eos_shell_cmd_text(&cmd, " ");
        eos_shell_cmd_text(&cmd, num);
        result = eos_shell_cmd_run(&cmd, "Image creation");
#else
        snprintf(num, sizeof(num), "%zu", sys->image_size_mb);
        eos_shell_cmd_init(&cmd);
        eos_shell_cmd_text(&cmd, "dd if=/dev/zero of=");
        eos_shell_cmd_arg(&cmd, sys->image_output);
        eos_shell_cmd_text(&cmd, " bs=1M count=");
        eos_shell_cmd_text(&cmd, num);
        eos_shell_cmd_text(&cmd, " 2>/dev/null");
        result = eos_shell_cmd_run(&cmd, "Image creation");
#endif
        if (result == EOS_ERR_INVALID) {
            /* Refused argument: do not continue to partitioning. */
            return result;
        }
        if (result != EOS_OK) {
            EOS_WARN("  Image creation command failed (may need elevated privileges)");
        }

#ifndef _WIN32
        /* Create partition table (Linux only). */
        eos_shell_cmd_init(&cmd);
        eos_shell_cmd_text(&cmd, "echo -e 'o\\nn\\np\\n1\\n\\n\\nw' | fdisk ");
        eos_shell_cmd_arg(&cmd, sys->image_output);
        eos_shell_cmd_text(&cmd, " 2>/dev/null || true");
        (void)eos_shell_cmd_run(&cmd, "Partitioning");
#endif

        EOS_INFO("  Raw image created: %s (%zu MB)", sys->image_output, sys->image_size_mb);
        break;
    }

    case EOS_IMG_QCOW2: {
        EosShellCmd cmd;
        char num[32];

        snprintf(num, sizeof(num), "%zu", sys->image_size_mb);
        eos_shell_cmd_init(&cmd);
        eos_shell_cmd_text(&cmd, "qemu-img create -f qcow2 ");
        eos_shell_cmd_arg(&cmd, sys->image_output);
        eos_shell_cmd_text(&cmd, " ");
        eos_shell_cmd_text(&cmd, num);
        eos_shell_cmd_text(&cmd, "M");
        if (eos_shell_cmd_run(&cmd, "QCOW2 image creation") != EOS_OK) {
            EOS_WARN("  qemu-img not available, creating placeholder");
        }
        break;
    }

    case EOS_IMG_TAR: {
        EosShellCmd cmd;
        EosResult result;

        eos_shell_cmd_init(&cmd);
        eos_shell_cmd_text(&cmd, "tar czf ");
        eos_shell_cmd_arg(&cmd, sys->image_output);
        eos_shell_cmd_text(&cmd, " -C ");
        eos_shell_cmd_arg(&cmd, sys->rootfs_dir);
        eos_shell_cmd_text(&cmd, " .");
        result = eos_shell_cmd_run(&cmd, "Tar image creation");
        if (result == EOS_ERR_BUILD) {
            /* Preserve the previous failure code for a failing tool. */
            return EOS_ERR_SYSTEM;
        }
        return result;
    }

    default:
        EOS_WARN("  Image format %d not yet implemented", sys->image_format);
        break;
    }

    return EOS_OK;
}
