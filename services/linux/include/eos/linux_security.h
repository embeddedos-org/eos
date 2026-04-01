// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file linux_security.h
 * @brief EoS Linux Security Services
 *
 * Linux-specific kernel security features: SELinux policy management,
 * IMA (Integrity Measurement Architecture), dm-verity for block-level
 * integrity, Linux kernel audit subsystem, and BusyBox integration.
 */

#ifndef EOS_LINUX_SECURITY_H
#define EOS_LINUX_SECURITY_H

#include <stdint.h>
#include <stddef.h>

/* ---- SELinux ---- */

typedef enum {
    EOS_SELINUX_DISABLED,
    EOS_SELINUX_PERMISSIVE,
    EOS_SELINUX_ENFORCING
} EosSelinuxMode;

typedef struct {
    EosSelinuxMode mode;
    char policy_dir[512];
    char policy_name[128];
    char contexts_file[512];
    char file_contexts[512];
    int policy_loaded;
} EosSelinux;

void eos_selinux_init(EosSelinux *se, EosSelinuxMode mode);
int  eos_selinux_set_policy(EosSelinux *se, const char *policy_dir,
                            const char *policy_name);
int  eos_selinux_install_to_rootfs(const EosSelinux *se, const char *rootfs_dir);
int  eos_selinux_label_rootfs(const EosSelinux *se, const char *rootfs_dir);
void eos_selinux_dump(const EosSelinux *se);

/* ---- IMA (Integrity Measurement Architecture) ---- */

typedef enum {
    EOS_IMA_OFF,
    EOS_IMA_MEASURE,
    EOS_IMA_APPRAISE,
    EOS_IMA_ENFORCE
} EosImaMode;

typedef struct {
    EosImaMode mode;
    char policy_file[512];
    char key_file[512];
    char algo[32];
    int sign_executables;
    int measure_files;
    int appraise_modules;
} EosIma;

void eos_ima_init(EosIma *ima, EosImaMode mode);
int  eos_ima_set_policy(EosIma *ima, const char *policy_file);
int  eos_ima_set_key(EosIma *ima, const char *key_file);
int  eos_ima_install_to_rootfs(const EosIma *ima, const char *rootfs_dir);
int  eos_ima_sign_file(const EosIma *ima, const char *file_path);
int  eos_ima_generate_kernel_params(const EosIma *ima, char *params, size_t params_sz);
void eos_ima_dump(const EosIma *ima);

/* ---- dm-verity ---- */

typedef struct {
    char data_device[256];
    char hash_device[256];
    char root_hash[129];
    char hash_algo[32];
    uint32_t data_block_size;
    uint32_t hash_block_size;
    uint64_t data_blocks;
    char salt[129];
    int verified;
} EosDmVerity;

void eos_dmverity_init(EosDmVerity *dv);
int  eos_dmverity_create(EosDmVerity *dv, const char *image_path,
                         const char *hash_output);
int  eos_dmverity_generate_table(const EosDmVerity *dv, char *table, size_t table_sz);
int  eos_dmverity_generate_kernel_params(const EosDmVerity *dv,
                                         char *params, size_t params_sz);
int  eos_dmverity_verify(EosDmVerity *dv, const char *image_path);
void eos_dmverity_dump(const EosDmVerity *dv);

/* ---- Linux Kernel Audit ---- */

typedef enum {
    EOS_KAUDIT_DISABLED,
    EOS_KAUDIT_ENABLED,
    EOS_KAUDIT_IMMUTABLE
} EosKauditState;

#define EOS_KAUDIT_MAX_RULES 64

typedef struct {
    char rule[256];
    char key[64];
    int enabled;
} EosKauditRule;

typedef struct {
    EosKauditState state;
    EosKauditRule rules[EOS_KAUDIT_MAX_RULES];
    int rule_count;
    char rules_file[512];
    int log_exec;
    int log_net;
    int log_file_access;
    int log_mount;
} EosKaudit;

void eos_kaudit_init(EosKaudit *ka, EosKauditState state);
int  eos_kaudit_add_rule(EosKaudit *ka, const char *rule, const char *key);
int  eos_kaudit_add_watch(EosKaudit *ka, const char *path,
                          const char *permissions, const char *key);
int  eos_kaudit_add_syscall(EosKaudit *ka, const char *syscall,
                            const char *filter, const char *key);
int  eos_kaudit_install_to_rootfs(const EosKaudit *ka, const char *rootfs_dir);
int  eos_kaudit_generate_rules_file(const EosKaudit *ka, const char *output_path);
void eos_kaudit_dump(const EosKaudit *ka);

/* ---- BusyBox Integration ---- */

#define EOS_BUSYBOX_MAX_APPLETS 128

typedef struct {
    char version[64];
    char config_path[512];
    char source_dir[512];
    char install_dir[512];
    char applets[EOS_BUSYBOX_MAX_APPLETS][64];
    int applet_count;
    int use_static;
    int install_symlinks;
    char cross_compile[128];
    char defconfig[128];
} EosBusybox;

void eos_busybox_init(EosBusybox *bb);
int  eos_busybox_set_version(EosBusybox *bb, const char *version);
int  eos_busybox_add_applet(EosBusybox *bb, const char *applet);
int  eos_busybox_add_minimal_set(EosBusybox *bb);
int  eos_busybox_add_network_set(EosBusybox *bb);
int  eos_busybox_configure(EosBusybox *bb);
int  eos_busybox_build(EosBusybox *bb);
int  eos_busybox_install_to_rootfs(const EosBusybox *bb, const char *rootfs_dir);
void eos_busybox_dump(const EosBusybox *bb);

#endif /* EOS_LINUX_SECURITY_H */
