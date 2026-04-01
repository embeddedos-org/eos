// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/system.h"
#include "eos/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#define PATH_SEP "\\"
#else
#include <sys/types.h>
#define MKDIR(d) mkdir(d, 0755)
#define PATH_SEP "/"
#endif

static void mkdirs(const char *base, const char *sub) {
    char path[EOS_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s%s", base, PATH_SEP, sub);
    MKDIR(path);
}

EosResult eos_system_build_rootfs(EosSystem *sys) {
    EOS_INFO("Building rootfs: %s", sys->rootfs_dir);

    MKDIR(sys->rootfs_dir);

    /* Create FHS directory skeleton */
    const char *dirs[] = {
        "bin", "sbin", "usr", "usr/bin", "usr/sbin", "usr/lib",
        "etc", "etc/init.d", "var", "var/log", "var/run",
        "tmp", "home", "root", "dev", "proc", "sys",
        "mnt", "opt", "lib", "boot"
    };

    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        mkdirs(sys->rootfs_dir, dirs[i]);
    }

    EOS_INFO("Created FHS directory skeleton in %s", sys->rootfs_dir);

    /* Create /etc/hostname */
    char path[EOS_MAX_PATH];
    snprintf(path, sizeof(path), "%s%setc%shostname", sys->rootfs_dir, PATH_SEP, PATH_SEP);
    FILE *fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "%s\n", sys->hostname);
        fclose(fp);
    }

    /* Create /etc/fstab */
    snprintf(path, sizeof(path), "%s%setc%sfstab", sys->rootfs_dir, PATH_SEP, PATH_SEP);
    fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "# EoS auto-generated fstab\n");
        fprintf(fp, "proc      /proc  proc   defaults  0  0\n");
        fprintf(fp, "sysfs     /sys   sysfs  defaults  0  0\n");
        fprintf(fp, "devtmpfs  /dev   devtmpfs defaults 0  0\n");
        fprintf(fp, "/dev/sda1 /      ext4   defaults  1  1\n");
        fclose(fp);
    }

    /* Create init script based on init system */
    switch (sys->init_system) {
    case EOS_INIT_BUSYBOX:
        snprintf(path, sizeof(path), "%s%setc%sinittab", sys->rootfs_dir, PATH_SEP, PATH_SEP);
        fp = fopen(path, "w");
        if (fp) {
            fprintf(fp, "::sysinit:/etc/init.d/rcS\n");
            fprintf(fp, "::respawn:-/bin/sh\n");
            fprintf(fp, "::ctrlaltdel:/sbin/reboot\n");
            fprintf(fp, "::shutdown:/bin/umount -a -r\n");
            fclose(fp);
        }
        snprintf(path, sizeof(path), "%s%setc%sinit.d%srcS",
                 sys->rootfs_dir, PATH_SEP, PATH_SEP, PATH_SEP);
        fp = fopen(path, "w");
        if (fp) {
            fprintf(fp, "#!/bin/sh\n");
            fprintf(fp, "mount -t proc proc /proc\n");
            fprintf(fp, "mount -t sysfs sysfs /sys\n");
            fprintf(fp, "mount -t devtmpfs devtmpfs /dev\n");
            fprintf(fp, "hostname %s\n", sys->hostname);
            fprintf(fp, "echo \"EoS booted successfully\"\n");
            fclose(fp);
        }
        break;

    case EOS_INIT_SYSVINIT:
        EOS_INFO("SysVinit: init scripts would be installed from packages");
        break;

    case EOS_INIT_SYSTEMD:
        EOS_INFO("systemd: unit files would be installed from packages");
        break;

    default:
        EOS_WARN("No init system configured");
        break;
    }

    /* Create /etc/passwd and /etc/group */
    snprintf(path, sizeof(path), "%s%setc%spasswd", sys->rootfs_dir, PATH_SEP, PATH_SEP);
    fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "root:x:0:0:root:/root:/bin/sh\n");
        fprintf(fp, "nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin\n");
        fclose(fp);
    }

    snprintf(path, sizeof(path), "%s%setc%sgroup", sys->rootfs_dir, PATH_SEP, PATH_SEP);
    fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "root:x:0:\n");
        fprintf(fp, "nogroup:x:65534:\n");
        fclose(fp);
    }

    EOS_INFO("Rootfs assembly complete");
    return EOS_OK;
}
