// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/linux_security.h"
#include "eos/log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ctype.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#define PATH_SEP "\\"
#else
#include <sys/types.h>
#define MKDIR(d) mkdir(d, 0755)
#define PATH_SEP "/"
#endif

/* ==== SELinux ==== */

void eos_selinux_init(EosSelinux *se, EosSelinuxMode mode) {
    memset(se, 0, sizeof(*se));
    se->mode = mode;
}

int eos_selinux_set_policy(EosSelinux *se, const char *policy_dir,
                           const char *policy_name) {
    strncpy(se->policy_dir, policy_dir, sizeof(se->policy_dir) - 1);
    strncpy(se->policy_name, policy_name, sizeof(se->policy_name) - 1);

    char policy_path[1024];
    snprintf(policy_path, sizeof(policy_path), "%s/%s", policy_dir, policy_name);
    FILE *fp = fopen(policy_path, "r");
    if (fp) { fclose(fp); se->policy_loaded = 1; return 0; }

    snprintf(policy_path, sizeof(policy_path), "%s/policy.%s", policy_dir, policy_name);
    fp = fopen(policy_path, "r");
    if (fp) { fclose(fp); se->policy_loaded = 1; return 0; }

    se->policy_loaded = 0;
    return -1;
}

/* Reject anything a shell reads as syntax rather than as text.
 *
 * The previous list -- ;|&><$()\"' -- missed two that matter. A backtick is
 * command substitution in every POSIX shell, so /tmp/`id` passed as safe and
 * ran id. A newline ends one command and starts another, so a path could
 * append a whole second command. Both were reported SAFE. Control characters
 * and backslash are rejected for the same reason.
 *
 * A NULL path returned 1 -- "safe" -- which is the wrong default for a
 * predicate guarding command construction. It returns 0 now; every caller
 * here treats 0 as refuse. */
static int is_path_safe(const char *path) {
    const unsigned char *p;
    if (!path) return 0;
    for (p = (const unsigned char *)path; *p; p++) {
        if (*p < 0x20 || *p == 0x7f) return 0;
        if (strchr(";|&><$()\"'`\\", (char)*p)) return 0;
    }
    return 1;
}

/* Two busybox call sites interpolate without surrounding quotes
 * (`make -C "%s" %s` and ` CROSS_COMPILE=%s`), where a single space is
 * already enough to turn one argument into two. Those use this instead.
 *
 * This was a second denylist -- space, tab, *, ?, ~ -- and it had the same
 * shape of hole as the first one: it missed [ and ] (a glob character class)
 * and { } (brace expansion, which /bin/sh performs when it is bash), all of
 * which reach make unquoted. Answering an incomplete denylist with another
 * denylist just moves the next gap further out.
 *
 * "One shell word" has a small positive definition, so it is written as one.
 * isalnum() plus ._/+=:- covers every defconfig target, cross-compile prefix,
 * hash algorithm name and hex root hash this tree uses, and an allowlist
 * cannot be incomplete.
 *
 * A leading '-' is refused separately: it is built from allowed characters
 * but turns `make -C dir <defconfig>` into an option rather than a target.
 *
 * The empty string is a word. An unset cross_compile is the ordinary case,
 * and every caller checks the field for content before interpolating it. */
static int is_word_safe(const char *word) {
    const unsigned char *p;
    if (!word) return 0;
    if (word[0] == '-') return 0;
    for (p = (const unsigned char *)word; *p; p++) {
        if (isalnum(*p)) continue;
        if (strchr("._/+=:-", (char)*p)) continue;
        return 0;
    }
    return 1;
}

int eos_selinux_install_to_rootfs(const EosSelinux *se, const char *rootfs_dir) {
    /* rootfs_dir reaches the cp below through `path`; it was unchecked. */
    if (!is_path_safe(se->policy_dir) || !is_path_safe(se->policy_name) ||
        !is_path_safe(rootfs_dir)) {
        return -1;
    }
    char path[1024];

    /* Create /etc/selinux directory */
    snprintf(path, sizeof(path), "%s%setc%sselinux", rootfs_dir, PATH_SEP, PATH_SEP);
    MKDIR(path);

    /* Write /etc/selinux/config */
    snprintf(path, sizeof(path), "%s%setc%sselinux%sconfig",
             rootfs_dir, PATH_SEP, PATH_SEP, PATH_SEP);
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    const char *mode_str[] = {"disabled", "permissive", "enforcing"};
    fprintf(fp, "SELINUX=%s\n", mode_str[se->mode]);
    fprintf(fp, "SELINUXTYPE=%s\n", se->policy_name[0] ? se->policy_name : "targeted");
    fclose(fp);

    /* Copy policy files if available */
    if (se->policy_loaded && se->policy_dir[0]) {
        snprintf(path, sizeof(path), "%s%setc%sselinux%s%s",
                 rootfs_dir, PATH_SEP, PATH_SEP, PATH_SEP,
                 se->policy_name[0] ? se->policy_name : "targeted");
        MKDIR(path);
#ifndef _WIN32
        char cmd[2048];
        /* The `|| true` hid the copy's exit status and system()'s result was
         * discarded on top of it, so a failed copy left /etc/selinux/config
         * naming a policy the rootfs does not contain -- and reported that
         * with a 0. .ai/security.md, Never: "Report a check as passing
         * without running it." */
        snprintf(cmd, sizeof(cmd), "cp -r \"%s/\"* \"%s/\" 2>/dev/null",
                 se->policy_dir, path);
        if (system(cmd) != 0) {
            EOS_ERROR("SELinux policy copy failed (%s -> %s); the rootfs "
                      "config names a policy that is NOT installed.",
                      se->policy_dir, path);
            return -1;
        }
#else
        /* No cp here, so the policy is not installed. Returning 0 would be
         * the same untrue answer the `|| true` gave. */
        EOS_ERROR("SELinux policy files cannot be installed on this platform; "
                  "the rootfs config names a policy that is NOT installed.");
        return -1;
#endif
    }
    return 0;
}

int eos_selinux_label_rootfs(const EosSelinux *se, const char *rootfs_dir) {
    if (se->mode == EOS_SELINUX_DISABLED) return 0;
    if (!is_path_safe(rootfs_dir) || !is_path_safe(se->file_contexts)) {
        return -1;
    }
    /* Labeling needs file_contexts. Without it this used to echo a line
     * about skipping and return 0 -- an unlabeled rootfs reported as a
     * labeled one. SELinux enforcing over unlabeled files is exactly the
     * state a caller has to be told about. */
    if (!se->file_contexts[0]) {
        EOS_ERROR("SELinux is enabled but no file_contexts is set; the rootfs "
                  "at %s is NOT labeled.", rootfs_dir);
        return -1;
    }
#ifndef _WIN32
    char cmd[2048];
    /* The `|| echo` tail masked setfiles' exit status and system()'s result
     * was discarded on top of it, so on any host without setfiles this
     * labeled nothing and returned 0. Absent tool and failed tool are both
     * "not labeled", and neither is a 0. */
    snprintf(cmd, sizeof(cmd),
             "setfiles -r \"%s\" \"%s\" \"%s\" 2>/dev/null",
             rootfs_dir, se->file_contexts, rootfs_dir);
    if (system(cmd) != 0) {
        EOS_ERROR("setfiles failed or is not installed; the rootfs at %s is "
                  "NOT labeled.", rootfs_dir);
        return -1;
    }
    return 0;
#else
    EOS_ERROR("SELinux labeling cannot run on this platform; the rootfs at %s "
              "is NOT labeled.", rootfs_dir);
    return -1;
#endif
}

void eos_selinux_dump(const EosSelinux *se) {
    const char *mode_str[] = {"disabled", "permissive", "enforcing"};
    printf("SELinux:\n");
    printf("  Mode:       %s\n", mode_str[se->mode]);
    printf("  Policy:     %s\n", se->policy_name[0] ? se->policy_name : "(default)");
    printf("  Policy dir: %s\n", se->policy_dir[0] ? se->policy_dir : "(none)");
    printf("  Loaded:     %s\n", se->policy_loaded ? "yes" : "no");
}

/* ==== IMA ==== */

void eos_ima_init(EosIma *ima, EosImaMode mode) {
    memset(ima, 0, sizeof(*ima));
    ima->mode = mode;
    strncpy(ima->algo, "sha256", sizeof(ima->algo) - 1);
    ima->measure_files = 1;
}

int eos_ima_set_policy(EosIma *ima, const char *policy_file) {
    strncpy(ima->policy_file, policy_file, sizeof(ima->policy_file) - 1);
    return 0;
}

int eos_ima_set_key(EosIma *ima, const char *key_file) {
    strncpy(ima->key_file, key_file, sizeof(ima->key_file) - 1);
    return 0;
}

int eos_ima_install_to_rootfs(const EosIma *ima, const char *rootfs_dir) {
    if (!is_path_safe(rootfs_dir) || !is_path_safe(ima->key_file)) return -1;
    if (ima->mode == EOS_IMA_OFF) return 0;

    char path[1024];
    snprintf(path, sizeof(path), "%s%setc%sima", rootfs_dir, PATH_SEP, PATH_SEP);
    MKDIR(path);

    /* Write IMA policy */
    snprintf(path, sizeof(path), "%s%setc%sima%spolicy",
             rootfs_dir, PATH_SEP, PATH_SEP, PATH_SEP);
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    if (ima->policy_file[0]) {
        /* A named policy file that cannot be opened used to leave an empty
         * /etc/ima/policy behind and return 0 -- IMA configured to measure
         * nothing, reported as installed. */
        FILE *src = fopen(ima->policy_file, "r");
        if (!src) {
            fclose(fp);
            EOS_ERROR("IMA policy file %s cannot be read; /etc/ima/policy "
                      "would be empty.", ima->policy_file);
            return -1;
        }
        char buf[1024];
        while (fgets(buf, sizeof(buf), src)) fputs(buf, fp);
        fclose(src);
    } else {
        if (ima->measure_files)
            fprintf(fp, "measure func=FILE_CHECK mask=MAY_EXEC\n");
        if (ima->appraise_modules)
            fprintf(fp, "appraise func=MODULE_CHECK\n");
        if (ima->sign_executables)
            fprintf(fp, "appraise func=FILE_CHECK mask=MAY_EXEC\n");
    }
    fclose(fp);

    /* Install key if provided */
    if (ima->key_file[0]) {
#ifndef _WIN32
        char cmd[2048];
        snprintf(path, sizeof(path), "%s%setc%skeys", rootfs_dir, PATH_SEP, PATH_SEP);
        MKDIR(path);
        /* Same `|| true` + discarded result as the SELinux copy above. An
         * appraise policy with no key installed is a rootfs that will refuse
         * to run its own executables, and it returned 0. */
        snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s/ima-key.pub\" 2>/dev/null",
                 ima->key_file, path);
        if (system(cmd) != 0) {
            EOS_ERROR("IMA key copy failed (%s -> %s/ima-key.pub); the policy "
                      "is installed but the key is NOT.", ima->key_file, path);
            return -1;
        }
#else
        EOS_ERROR("The IMA key cannot be installed on this platform; the "
                  "policy is installed but the key is NOT.");
        return -1;
#endif
    }
    return 0;
}

int eos_ima_sign_file(const EosIma *ima, const char *file_path) {
    if (!ima->key_file[0]) return -1;
    if (!is_path_safe(file_path) || !is_path_safe(ima->key_file) ||
        !is_word_safe(ima->algo)) return -1;
#ifndef _WIN32
    char cmd[2048];
    /* The `|| echo 'evmctl not available'` tail masked the shell's exit
     * status, and the result of system() was discarded on top of that -- so
     * on any host without evmctl this function signed nothing and told its
     * caller it had signed. .ai/security.md: a verification step that cannot
     * run must fail, not pass. Follows eos_dmverity_verify(), which already
     * gets this right. */
    snprintf(cmd, sizeof(cmd),
             "evmctl sign --key \"%s\" --hashalgo %s \"%s\" 2>/dev/null",
             ima->key_file, ima->algo, file_path);
    int rc = system(cmd);
    if (rc != 0) {
        EOS_ERROR("IMA signing failed for %s (evmctl missing or refused); "
                  "the file is NOT signed.", file_path);
        return -1;
    }
    return 0;
#else
    (void)file_path;
    return -1;
#endif
}

int eos_ima_generate_kernel_params(const EosIma *ima, char *params, size_t params_sz) {
    if (ima->mode == EOS_IMA_OFF) {
        params[0] = '\0';
        return 0;
    }
    const char *mode_str[] = {"off", "fix", "log", "enforce"};
    snprintf(params, params_sz, "ima_policy=tcb ima_appraise=%s ima_hash=%s",
             mode_str[ima->mode], ima->algo);
    return 0;
}

void eos_ima_dump(const EosIma *ima) {
    const char *mode_str[] = {"off", "measure", "appraise", "enforce"};
    printf("IMA:\n");
    printf("  Mode:       %s\n", mode_str[ima->mode]);
    printf("  Algorithm:  %s\n", ima->algo);
    printf("  Policy:     %s\n", ima->policy_file[0] ? ima->policy_file : "(default)");
    printf("  Key:        %s\n", ima->key_file[0] ? ima->key_file : "(none)");
}

/* ==== dm-verity ==== */

void eos_dmverity_init(EosDmVerity *dv) {
    memset(dv, 0, sizeof(*dv));
    strncpy(dv->hash_algo, "sha256", sizeof(dv->hash_algo) - 1);
    dv->data_block_size = 4096;
    dv->hash_block_size = 4096;
}

int eos_dmverity_create(EosDmVerity *dv, const char *image_path,
                        const char *hash_output) {
    if (!is_path_safe(image_path) || !is_path_safe(hash_output) ||
        !is_word_safe(dv->hash_algo)) return -1;
    strncpy(dv->data_device, image_path, sizeof(dv->data_device) - 1);
    strncpy(dv->hash_device, hash_output, sizeof(dv->hash_device) - 1);

#ifndef _WIN32
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "veritysetup format \"%s\" \"%s\" --hash %s "
             "--data-block-size %u --hash-block-size %u 2>/dev/null | "
             "grep 'Root hash' | awk '{print $NF}'",
             image_path, hash_output, dv->hash_algo,
             dv->data_block_size, dv->hash_block_size);

    FILE *proc = popen(cmd, "r");
    if (proc) {
        if (fgets(dv->root_hash, sizeof(dv->root_hash), proc)) {
            char *nl = strchr(dv->root_hash, '\n');
            if (nl) *nl = '\0';
        }
        pclose(proc);
    }

    /* No sha256sum fallback.
     *
     * This used to run `sha256sum <image>` when veritysetup was unavailable or
     * failed, and store the result in dv->root_hash. A dm-verity root hash is
     * the root of a Merkle tree over the image's blocks -- it is what lets the
     * kernel check each block as it is read. A flat digest of the whole file is
     * a different thing that happens to be the same length, so the substitution
     * was invisible: eos_dmverity_generate_table() would emit a table the
     * kernel rejects, and anything treating root_hash as an integrity token got
     * a value with none of dm-verity's properties.
     *
     * A hash device that could not be built is a failure. Reporting one is more
     * useful than inventing a hash that cannot verify anything.
     */
#else
    (void)image_path;
    (void)hash_output;
#endif

    /* `verified` says an image was checked against a known-good root hash.
     * Formatting a hash device is not that check -- it establishes the root
     * hash, it does not verify anything against it. Setting the flag here made
     * every successful create() look like a completed verification to any
     * caller that trusts the field, and eos_dmverity_verify() is the only
     * function entitled to set it. */
    return dv->root_hash[0] != '\0' ? 0 : -1;
}

int eos_dmverity_generate_table(const EosDmVerity *dv, char *table, size_t table_sz) {
    snprintf(table, table_sz,
             "0 %llu verity 1 %s %s %u %u %llu 0 %s %s %s",
             (unsigned long long)(dv->data_blocks * dv->data_block_size / 512),
             dv->data_device, dv->hash_device,
             dv->data_block_size, dv->hash_block_size,
             (unsigned long long)dv->data_blocks,
             dv->hash_algo, dv->root_hash,
             dv->salt[0] ? dv->salt : "-");
    return 0;
}

int eos_dmverity_generate_kernel_params(const EosDmVerity *dv,
                                        char *params, size_t params_sz) {
    snprintf(params, params_sz,
             "dm-mod.create=\"verity,,,ro,0 %llu verity 1 %s %s %u %u %llu 0 %s %s %s\"",
             (unsigned long long)(dv->data_blocks * dv->data_block_size / 512),
             dv->data_device, dv->hash_device,
             dv->data_block_size, dv->hash_block_size,
             (unsigned long long)dv->data_blocks,
             dv->hash_algo, dv->root_hash,
             dv->salt[0] ? dv->salt : "-");
    return 0;
}

int eos_dmverity_verify(EosDmVerity *dv, const char *image_path) {
    if (!is_path_safe(image_path) || !is_path_safe(dv->hash_device) ||
        !is_word_safe(dv->root_hash)) {
        dv->verified = 0;
        return -1;
    }
#ifndef _WIN32
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "veritysetup verify \"%s\" \"%s\" \"%s\" 2>/dev/null",
             image_path, dv->hash_device, dv->root_hash);
    int rc = system(cmd);
    dv->verified = (rc == 0);
    return rc;
#else
    (void)image_path;
    dv->verified = 0;
    return -1;
#endif
}

void eos_dmverity_dump(const EosDmVerity *dv) {
    printf("dm-verity:\n");
    printf("  Data:       %s\n", dv->data_device);
    printf("  Hash:       %s\n", dv->hash_device);
    printf("  Root hash:  %s\n", dv->root_hash[0] ? dv->root_hash : "(not computed)");
    printf("  Algorithm:  %s\n", dv->hash_algo);
    printf("  Block size: %u / %u\n", dv->data_block_size, dv->hash_block_size);
    printf("  Verified:   %s\n", dv->verified ? "yes" : "no");
}

/* ==== Linux Kernel Audit ==== */

void eos_kaudit_init(EosKaudit *ka, EosKauditState state) {
    memset(ka, 0, sizeof(*ka));
    ka->state = state;
}

int eos_kaudit_add_rule(EosKaudit *ka, const char *rule, const char *key) {
    if (ka->rule_count >= EOS_KAUDIT_MAX_RULES) return -1;
    EosKauditRule *r = &ka->rules[ka->rule_count];
    strncpy(r->rule, rule, sizeof(r->rule) - 1);
    if (key) strncpy(r->key, key, sizeof(r->key) - 1);
    r->enabled = 1;
    ka->rule_count++;
    return 0;
}

int eos_kaudit_add_watch(EosKaudit *ka, const char *path,
                         const char *permissions, const char *key) {
    char rule[256];
    snprintf(rule, sizeof(rule), "-w %s -p %s", path, permissions ? permissions : "rwxa");
    return eos_kaudit_add_rule(ka, rule, key);
}

int eos_kaudit_add_syscall(EosKaudit *ka, const char *syscall,
                           const char *filter, const char *key) {
    char rule[256];
    snprintf(rule, sizeof(rule), "-a always,exit -F arch=b64 -S %s%s%s",
             syscall, filter ? " -F " : "", filter ? filter : "");
    return eos_kaudit_add_rule(ka, rule, key);
}

int eos_kaudit_install_to_rootfs(const EosKaudit *ka, const char *rootfs_dir) {
    char path[1024];
    snprintf(path, sizeof(path), "%s%setc%saudit", rootfs_dir, PATH_SEP, PATH_SEP);
    MKDIR(path);

    snprintf(path, sizeof(path), "%s%setc%saudit%srules.d",
             rootfs_dir, PATH_SEP, PATH_SEP, PATH_SEP);
    MKDIR(path);

    snprintf(path, sizeof(path), "%s%setc%saudit%srules.d%seos.rules",
             rootfs_dir, PATH_SEP, PATH_SEP, PATH_SEP, PATH_SEP);
    return eos_kaudit_generate_rules_file(ka, path);
}

int eos_kaudit_generate_rules_file(const EosKaudit *ka, const char *output_path) {
    FILE *fp = fopen(output_path, "w");
    if (!fp) return -1;

    fprintf(fp, "# EoS Linux audit rules\n");
    fprintf(fp, "# Generated automatically — do not edit\n\n");

    if (ka->state == EOS_KAUDIT_DISABLED) {
        fprintf(fp, "-D\n-e 0\n");
    } else {
        fprintf(fp, "-D\n-b 8192\n");

        if (ka->log_exec) {
            fprintf(fp, "-a always,exit -F arch=b64 -S execve -k exec_log\n");
            fprintf(fp, "-a always,exit -F arch=b32 -S execve -k exec_log\n");
        }
        if (ka->log_net) {
            fprintf(fp, "-a always,exit -F arch=b64 -S connect -S accept -k net_log\n");
        }
        if (ka->log_file_access) {
            fprintf(fp, "-w /etc/passwd -p wa -k auth_file\n");
            fprintf(fp, "-w /etc/shadow -p wa -k auth_file\n");
            fprintf(fp, "-w /etc/group -p wa -k auth_file\n");
        }
        if (ka->log_mount) {
            fprintf(fp, "-a always,exit -F arch=b64 -S mount -S umount2 -k mount_log\n");
        }

        for (int i = 0; i < ka->rule_count; i++) {
            if (ka->rules[i].enabled) {
                fprintf(fp, "%s", ka->rules[i].rule);
                if (ka->rules[i].key[0])
                    fprintf(fp, " -k %s", ka->rules[i].key);
                fprintf(fp, "\n");
            }
        }

        if (ka->state == EOS_KAUDIT_IMMUTABLE)
            fprintf(fp, "\n-e 2\n");
        else
            fprintf(fp, "\n-e 1\n");
    }

    fclose(fp);
    return 0;
}

void eos_kaudit_dump(const EosKaudit *ka) {
    const char *state_str[] = {"disabled", "enabled", "immutable"};
    printf("Linux Audit:\n");
    printf("  State: %s\n", state_str[ka->state]);
    printf("  Rules: %d\n", ka->rule_count);
    printf("  Exec:  %s  Net: %s  Files: %s  Mount: %s\n",
           ka->log_exec ? "on" : "off", ka->log_net ? "on" : "off",
           ka->log_file_access ? "on" : "off", ka->log_mount ? "on" : "off");
}

/* ==== BusyBox ==== */

void eos_busybox_init(EosBusybox *bb) {
    memset(bb, 0, sizeof(*bb));
    strncpy(bb->version, "1.36.1", sizeof(bb->version) - 1);
    bb->use_static = 1;
    bb->install_symlinks = 1;
    strncpy(bb->defconfig, "defconfig", sizeof(bb->defconfig) - 1);
}

int eos_busybox_set_version(EosBusybox *bb, const char *version) {
    /* version is interpolated into source_dir, which is then interpolated
     * into `make -C "%s"`. Inside those double quotes a backtick is command
     * substitution, so this was an injection site with no check on it at all
     * -- and it was the *default* path, taken whenever a caller does not set
     * source_dir itself. Refuse at the boundary rather than relying on the
     * check further down, which the caller can reach around. */
    if (!bb || !is_word_safe(version)) return -1;
    strncpy(bb->version, version, sizeof(bb->version) - 1);
    bb->version[sizeof(bb->version) - 1] = '\0';
    return 0;
}

int eos_busybox_add_applet(EosBusybox *bb, const char *applet) {
    if (bb->applet_count >= EOS_BUSYBOX_MAX_APPLETS) return -1;
    strncpy(bb->applets[bb->applet_count], applet, 63);
    bb->applet_count++;
    return 0;
}

int eos_busybox_add_minimal_set(EosBusybox *bb) {
    const char *applets[] = {
        "sh", "ash", "init", "mount", "umount", "ls", "cat", "echo",
        "cp", "mv", "rm", "mkdir", "rmdir", "ln", "chmod", "chown",
        "grep", "sed", "awk", "find", "xargs", "ps", "kill", "sleep",
        "date", "hostname", "dmesg", "reboot", "poweroff", "halt",
        "login", "getty", "passwd", "su", "id", "whoami",
        "ifconfig", "route", "ping", "vi", "head", "tail", "wc",
        "sort", "uniq", "tr", "cut", "tee", "test", "true", "false"
    };
    for (size_t i = 0; i < sizeof(applets) / sizeof(applets[0]); i++)
        eos_busybox_add_applet(bb, applets[i]);
    return 0;
}

int eos_busybox_add_network_set(EosBusybox *bb) {
    const char *applets[] = {
        "wget", "nc", "telnet", "ftpget", "ftpput",
        "httpd", "udhcpc", "udhcpd", "ntpd", "ip",
        "arp", "traceroute", "nslookup", "brctl",
        "iptables", "netstat", "ss"
    };
    for (size_t i = 0; i < sizeof(applets) / sizeof(applets[0]); i++)
        eos_busybox_add_applet(bb, applets[i]);
    return 0;
}

int eos_busybox_configure(EosBusybox *bb) {
    if (!bb) return -1;
    if (!is_word_safe(bb->defconfig) || !is_word_safe(bb->cross_compile))
        return -1;

    /* Fill the default BEFORE validating it. The check used to run first, so
     * on the default path -- source_dir empty, which is what eos_busybox_init
     * leaves -- is_path_safe("") returned 1 because the loop body never ran,
     * the guard passed, and the string built from bb->version below went
     * straight into system(). Validating the value that is actually used is
     * the whole point of the guard. */
    if (!bb->source_dir[0]) {
        snprintf(bb->source_dir, sizeof(bb->source_dir),
                 ".eos/build/src/busybox-%s", bb->version);
    }
    if (!is_path_safe(bb->source_dir)) return -1;
#ifndef _WIN32
    char cmd[2048];
    int n;
    /* offset accumulated snprintf's *would-be* length, so a truncating
     * segment pushed it past sizeof(cmd): cmd + offset out of bounds and
     * sizeof(cmd) - offset underflowing to a huge size_t. Unreachable at the
     * current field widths (512 + 128 + 128 against 2048) and nothing ties
     * the two together, so it is checked rather than reasoned about. */
    int offset = snprintf(cmd, sizeof(cmd), "make -C \"%s\" %s",
                          bb->source_dir, bb->defconfig);
    if (offset < 0 || (size_t)offset >= sizeof(cmd)) return -1;
    if (bb->cross_compile[0]) {
        n = snprintf(cmd + offset, sizeof(cmd) - (size_t)offset,
                     " CROSS_COMPILE=%s", bb->cross_compile);
        if (n < 0 || (size_t)n >= sizeof(cmd) - (size_t)offset) return -1;
        offset += n;
    }
    if (bb->use_static) {
        n = snprintf(cmd + offset, sizeof(cmd) - (size_t)offset,
                     " CONFIG_STATIC=y");
        if (n < 0 || (size_t)n >= sizeof(cmd) - (size_t)offset) return -1;
    }
    return system(cmd) == 0 ? 0 : -1;
#else
    (void)bb;
    return -1;
#endif
}

int eos_busybox_build(EosBusybox *bb) {
    if (!is_path_safe(bb->source_dir) || !is_word_safe(bb->cross_compile)) return -1;
#ifndef _WIN32
    char cmd[2048];
    /* Same accumulate-the-would-be-length hazard as configure(). */
    int offset = snprintf(cmd, sizeof(cmd), "make -C \"%s\" -j4", bb->source_dir);
    if (offset < 0 || (size_t)offset >= sizeof(cmd)) return -1;
    if (bb->cross_compile[0]) {
        int n = snprintf(cmd + offset, sizeof(cmd) - (size_t)offset,
                         " CROSS_COMPILE=%s", bb->cross_compile);
        if (n < 0 || (size_t)n >= sizeof(cmd) - (size_t)offset) return -1;
    }
    return system(cmd) == 0 ? 0 : -1;
#else
    (void)bb;
    return -1;
#endif
}

int eos_busybox_install_to_rootfs(const EosBusybox *bb, const char *rootfs_dir) {
    if (!is_path_safe(bb->source_dir) || !is_path_safe(rootfs_dir)) return -1;
#ifndef _WIN32
    char cmd[2048];
    if (bb->source_dir[0]) {
        /* system()'s result was discarded here, so a rootfs with no busybox
         * in it -- no source tree, no make, a failed build -- came back 0. */
        snprintf(cmd, sizeof(cmd),
                 "make -C \"%s\" install CONFIG_PREFIX=\"%s\"",
                 bb->source_dir, rootfs_dir);
        if (system(cmd) != 0) {
            EOS_ERROR("busybox install failed (%s -> %s); the rootfs has no "
                      "busybox and its /init would not run.",
                      bb->source_dir, rootfs_dir);
            return -1;
        }
    }
#else
    if (bb->source_dir[0]) {
        EOS_ERROR("busybox cannot be installed on this platform; the rootfs "
                  "would have no busybox.");
        return -1;
    }
#endif

    /* Create /init symlink for initramfs boot */
    char path[1024];
    snprintf(path, sizeof(path), "%s%sinit", rootfs_dir, PATH_SEP);
    FILE *fp = fopen(path, "w");
    /* An unwritable /init is an initramfs that cannot boot; it was silent. */
    if (!fp) {
        EOS_ERROR("cannot write %s; the initramfs has no /init.", path);
        return -1;
    }
    fprintf(fp, "#!/bin/busybox sh\n");
    fprintf(fp, "/bin/busybox --install -s\n");
    fprintf(fp, "mount -t proc proc /proc\n");
    fprintf(fp, "mount -t sysfs sysfs /sys\n");
    fprintf(fp, "mount -t devtmpfs devtmpfs /dev\n");
    fprintf(fp, "exec /sbin/init\n");
    fclose(fp);
    return 0;
}

void eos_busybox_dump(const EosBusybox *bb) {
    printf("BusyBox:\n");
    printf("  Version:   %s\n", bb->version);
    printf("  Static:    %s\n", bb->use_static ? "yes" : "no");
    printf("  Symlinks:  %s\n", bb->install_symlinks ? "yes" : "no");
    printf("  Applets:   %d\n", bb->applet_count);
    if (bb->cross_compile[0])
        printf("  Cross:     %s\n", bb->cross_compile);
    if (bb->source_dir[0])
        printf("  Source:    %s\n", bb->source_dir);
}
