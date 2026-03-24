// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/config.h"
#include "eos/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void eos_config_init(EosConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->workspace.backend, "ninja", EOS_MAX_NAME - 1);
    strncpy(cfg->workspace.build_dir, ".eos/build", EOS_MAX_PATH - 1);
    strncpy(cfg->workspace.cache_dir, ".eos/cache", EOS_MAX_PATH - 1);
    cfg->system.kind = EOS_SYSTEM_LINUX;
    cfg->system.image_format = EOS_IMG_RAW;
    cfg->system.rootfs.init = EOS_INIT_BUSYBOX;
    strncpy(cfg->system.rootfs.hostname, "eos", EOS_MAX_NAME - 1);

    cfg->docs.enabled = 1;
    cfg->docs.api_docs = 1;
    cfg->docs.user_docs = 1;
    strncpy(cfg->docs.api_tool, "doxygen", EOS_MAX_NAME - 1);
    strncpy(cfg->docs.site_tool, "mkdocs", EOS_MAX_NAME - 1);
}

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

static int indent_level(const char *line) {
    int n = 0;
    while (line[n] == ' ') n++;
    return n / 2;
}

static int parse_kv(const char *line, char *key, size_t ksz, char *val, size_t vsz) {
    const char *colon = strchr(line, ':');
    if (!colon) return -1;

    size_t klen = (size_t)(colon - line);
    if (klen >= ksz) klen = ksz - 1;
    memcpy(key, line, klen);
    key[klen] = '\0';

    const char *v = colon + 1;
    while (*v && isspace((unsigned char)*v)) v++;
    strncpy(val, v, vsz - 1);
    val[vsz - 1] = '\0';

    char *t = trim(key);
    if (t != key) memmove(key, t, strlen(t) + 1);
    return 0;
}

typedef enum {
    SEC_NONE,
    SEC_PROJECT,
    SEC_WORKSPACE,
    SEC_TOOLCHAIN,
    SEC_TOOLCHAIN_LINUX,
    SEC_TOOLCHAIN_RTOS,
    SEC_LAYERS,
    SEC_PACKAGES,
    SEC_SYSTEM,
    SEC_SYSTEM_KERNEL,
    SEC_SYSTEM_ROOTFS,
    SEC_SYSTEM_RTOS,
    SEC_SYSTEM_RTOS_ENTRY,
    SEC_SYSTEM_LINUX,
    SEC_PKG_ENTRY,
    SEC_PKG_BUILD,
    SEC_PKG_DEPS,
    SEC_PKG_OPTIONS,
    SEC_DOCS
} Section;

EosResult eos_config_load(EosConfig *cfg, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        EOS_ERROR("Cannot open config: %s", path);
        return EOS_ERR_IO;
    }

    eos_config_init(cfg);

    char line[1024];
    Section section = SEC_NONE;
    int pkg_idx = -1;
    int rtos_idx = -1;

    while (fgets(line, sizeof(line), fp)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        nl = strchr(line, '\r');
        if (nl) *nl = '\0';

        char *trimmed = trim(line);
        if (trimmed[0] == '\0' || trimmed[0] == '#') continue;

        int indent = indent_level(line);
        char key[EOS_MAX_NAME] = {0};
        char val[EOS_MAX_PATH] = {0};

        /* list item: "- something" */
        if (trimmed[0] == '-') {
            char *item = trim(trimmed + 1);

            if (section == SEC_LAYERS) {
                if (cfg->layer_count < EOS_MAX_LAYERS) {
                    strncpy(cfg->layers[cfg->layer_count], item, EOS_MAX_PATH - 1);
                    cfg->layer_count++;
                }
            } else if (section == SEC_PACKAGES ||
                       section == SEC_PKG_ENTRY ||
                       section == SEC_PKG_BUILD ||
                       section == SEC_PKG_OPTIONS) {
                if (parse_kv(item, key, sizeof(key), val, sizeof(val)) == 0 &&
                    strcmp(key, "name") == 0) {
                    pkg_idx = cfg->package_count;
                    if (pkg_idx < EOS_MAX_PACKAGES) {
                        strncpy(cfg->packages[pkg_idx].name, val, EOS_MAX_NAME - 1);
                        cfg->package_count++;
                        section = SEC_PKG_ENTRY;
                    }
                }
            } else if (section == SEC_PKG_DEPS && pkg_idx >= 0) {
                if (cfg->packages[pkg_idx].dep_count < EOS_MAX_DEPS) {
                    strncpy(cfg->packages[pkg_idx].deps[cfg->packages[pkg_idx].dep_count],
                            item, EOS_MAX_NAME - 1);
                    cfg->packages[pkg_idx].dep_count++;
                }
            } else if (section == SEC_SYSTEM_RTOS || section == SEC_SYSTEM_RTOS_ENTRY) {
                /* "- provider: freertos" starts a new RTOS entry */
                if (parse_kv(item, key, sizeof(key), val, sizeof(val)) == 0 &&
                    strcmp(key, "provider") == 0) {
                    rtos_idx = cfg->system.rtos_count;
                    if (rtos_idx < EOS_MAX_RTOS) {
                        cfg->system.rtos[rtos_idx].provider = eos_rtos_provider_from_str(val);
                        cfg->system.rtos_count++;
                        section = SEC_SYSTEM_RTOS_ENTRY;
                    }
                }
            }
            continue;
        }

        if (parse_kv(trimmed, key, sizeof(key), val, sizeof(val)) != 0) continue;

        /* top-level sections */
        if (indent == 0) {
            if (strcmp(key, "project") == 0)       { section = SEC_PROJECT; continue; }
            if (strcmp(key, "workspace") == 0)      { section = SEC_WORKSPACE; continue; }
            if (strcmp(key, "toolchain") == 0)      { section = SEC_TOOLCHAIN; continue; }
            if (strcmp(key, "layers") == 0)         { section = SEC_LAYERS; continue; }
            if (strcmp(key, "packages") == 0)       { section = SEC_PACKAGES; continue; }
            if (strcmp(key, "system") == 0)         { section = SEC_SYSTEM; continue; }
            if (strcmp(key, "docs") == 0)           { section = SEC_DOCS; continue; }
        }

        /* sub-sections */
        switch (section) {
        case SEC_PROJECT:
            if (strcmp(key, "name") == 0)    strncpy(cfg->project.name, val, EOS_MAX_NAME - 1);
            if (strcmp(key, "version") == 0) strncpy(cfg->project.version, val, EOS_MAX_NAME - 1);
            break;
        case SEC_WORKSPACE:
            if (strcmp(key, "backend") == 0)   strncpy(cfg->workspace.backend, val, EOS_MAX_NAME - 1);
            if (strcmp(key, "build_dir") == 0) strncpy(cfg->workspace.build_dir, val, EOS_MAX_PATH - 1);
            if (strcmp(key, "cache_dir") == 0) strncpy(cfg->workspace.cache_dir, val, EOS_MAX_PATH - 1);
            break;
        case SEC_TOOLCHAIN:
            if (strcmp(key, "target") == 0) strncpy(cfg->toolchain.target, val, EOS_MAX_NAME - 1);
            if (strcmp(key, "linux") == 0)  { section = SEC_TOOLCHAIN_LINUX; continue; }
            if (strcmp(key, "rtos") == 0)   { section = SEC_TOOLCHAIN_RTOS; continue; }
            break;
        case SEC_TOOLCHAIN_LINUX:
            if (strcmp(key, "target") == 0) strncpy(cfg->toolchain.target, val, EOS_MAX_NAME - 1);
            if (indent <= 1 && strcmp(key, "rtos") == 0) { section = SEC_TOOLCHAIN_RTOS; continue; }
            break;
        case SEC_TOOLCHAIN_RTOS:
            if (strcmp(key, "target") == 0) strncpy(cfg->toolchain.rtos_target, val, EOS_MAX_NAME - 1);
            break;
        case SEC_SYSTEM:
            if (strcmp(key, "kind") == 0)     cfg->system.kind = eos_system_kind_from_str(val);
            if (strcmp(key, "kernel") == 0)   { section = SEC_SYSTEM_KERNEL; continue; }
            if (strcmp(key, "rootfs") == 0)   { section = SEC_SYSTEM_ROOTFS; continue; }
            if (strcmp(key, "rtos") == 0)     { section = SEC_SYSTEM_RTOS; continue; }
            if (strcmp(key, "linux") == 0)    { section = SEC_SYSTEM_LINUX; continue; }
            if (strcmp(key, "image_format") == 0) {
                if (strcmp(val, "raw") == 0)       cfg->system.image_format = EOS_IMG_RAW;
                else if (strcmp(val, "qcow2") == 0) cfg->system.image_format = EOS_IMG_QCOW2;
                else if (strcmp(val, "iso") == 0)   cfg->system.image_format = EOS_IMG_ISO;
                else if (strcmp(val, "tar") == 0)   cfg->system.image_format = EOS_IMG_TAR;
            }
            if (strcmp(key, "output") == 0) strncpy(cfg->system.output, val, EOS_MAX_PATH - 1);
            break;
        case SEC_SYSTEM_LINUX:
            if (strcmp(key, "provider") == 0) {
                strncpy(cfg->system.kernel.provider, val, EOS_MAX_NAME - 1);
                strncpy(cfg->system.rootfs.provider, val, EOS_MAX_NAME - 1);
            }
            if (strcmp(key, "kernel") == 0)   { section = SEC_SYSTEM_KERNEL; continue; }
            if (strcmp(key, "rootfs") == 0)   { section = SEC_SYSTEM_ROOTFS; continue; }
            break;
        case SEC_SYSTEM_KERNEL:
            if (strcmp(key, "provider") == 0)  strncpy(cfg->system.kernel.provider, val, EOS_MAX_NAME - 1);
            if (strcmp(key, "defconfig") == 0) strncpy(cfg->system.kernel.defconfig, val, EOS_MAX_PATH - 1);
            if (indent <= 1 && strcmp(key, "rootfs") == 0) { section = SEC_SYSTEM_ROOTFS; continue; }
            break;
        case SEC_SYSTEM_ROOTFS:
            if (strcmp(key, "provider") == 0)  strncpy(cfg->system.rootfs.provider, val, EOS_MAX_NAME - 1);
            if (strcmp(key, "hostname") == 0)  strncpy(cfg->system.rootfs.hostname, val, EOS_MAX_NAME - 1);
            if (strcmp(key, "init") == 0) {
                if (strcmp(val, "sysvinit") == 0)       cfg->system.rootfs.init = EOS_INIT_SYSVINIT;
                else if (strcmp(val, "systemd") == 0)    cfg->system.rootfs.init = EOS_INIT_SYSTEMD;
                else if (strcmp(val, "busybox") == 0)    cfg->system.rootfs.init = EOS_INIT_BUSYBOX;
                else                                    cfg->system.rootfs.init = EOS_INIT_NONE;
            }
            break;
        case SEC_SYSTEM_RTOS:
            /* Single RTOS config (system.kind: rtos) */
            if (strcmp(key, "provider") == 0) {
                rtos_idx = 0;
                cfg->system.rtos[0].provider = eos_rtos_provider_from_str(val);
                if (cfg->system.rtos_count == 0) cfg->system.rtos_count = 1;
                section = SEC_SYSTEM_RTOS_ENTRY;
            }
            break;
        case SEC_SYSTEM_RTOS_ENTRY:
            if (rtos_idx >= 0 && rtos_idx < EOS_MAX_RTOS) {
                EosRtosConfig *rc = &cfg->system.rtos[rtos_idx];
                if (strcmp(key, "provider") == 0) rc->provider = eos_rtos_provider_from_str(val);
                if (strcmp(key, "board") == 0)    strncpy(rc->board, val, EOS_MAX_NAME - 1);
                if (strcmp(key, "core") == 0)     strncpy(rc->core, val, EOS_MAX_NAME - 1);
                if (strcmp(key, "entry") == 0)    strncpy(rc->entry, val, EOS_MAX_PATH - 1);
                if (strcmp(key, "output") == 0)   strncpy(rc->output, val, EOS_MAX_PATH - 1);
                if (strcmp(key, "format") == 0)   rc->format = eos_firmware_format_from_str(val);
            }
            break;
        case SEC_PKG_ENTRY:
            if (strcmp(key, "version") == 0) strncpy(cfg->packages[pkg_idx].version, val, EOS_MAX_NAME - 1);
            if (strcmp(key, "source") == 0)  strncpy(cfg->packages[pkg_idx].source, val, EOS_MAX_URL - 1);
            if (strcmp(key, "hash") == 0)    strncpy(cfg->packages[pkg_idx].hash, val, EOS_HASH_LEN - 1);
            if (strcmp(key, "build") == 0)   { section = SEC_PKG_BUILD; continue; }
            if (strcmp(key, "deps") == 0)    { section = SEC_PKG_DEPS; continue; }
            break;
        case SEC_PKG_BUILD:
            if (strcmp(key, "type") == 0) {
                cfg->packages[pkg_idx].build_type = eos_build_type_from_str(val);
            }
            if (indent <= 2 && strcmp(key, "options") == 0) { section = SEC_PKG_OPTIONS; continue; }
            if (indent <= 2 && strcmp(key, "version") == 0) {
                section = SEC_PKG_ENTRY;
                strncpy(cfg->packages[pkg_idx].version, val, EOS_MAX_NAME - 1);
            }
            break;
        case SEC_PKG_OPTIONS:
            if (pkg_idx >= 0 && cfg->packages[pkg_idx].option_count < EOS_MAX_OPTIONS) {
                int oi = cfg->packages[pkg_idx].option_count;
                strncpy(cfg->packages[pkg_idx].options[oi].key, key, EOS_MAX_NAME - 1);
                strncpy(cfg->packages[pkg_idx].options[oi].value, val, EOS_MAX_PATH - 1);
                cfg->packages[pkg_idx].option_count++;
            }
            break;
        case SEC_DOCS:
            if (strcmp(key, "enabled") == 0)    cfg->docs.enabled = (strcmp(val, "false") != 0 && strcmp(val, "0") != 0);
            if (strcmp(key, "api_docs") == 0)   cfg->docs.api_docs = (strcmp(val, "false") != 0 && strcmp(val, "0") != 0);
            if (strcmp(key, "user_docs") == 0)  cfg->docs.user_docs = (strcmp(val, "false") != 0 && strcmp(val, "0") != 0);
            if (strcmp(key, "output_dir") == 0) strncpy(cfg->docs.output_dir, val, EOS_MAX_PATH - 1);
            if (strcmp(key, "api_tool") == 0)   strncpy(cfg->docs.api_tool, val, EOS_MAX_NAME - 1);
            if (strcmp(key, "api_config") == 0) strncpy(cfg->docs.api_config, val, EOS_MAX_PATH - 1);
            if (strcmp(key, "site_tool") == 0)  strncpy(cfg->docs.site_tool, val, EOS_MAX_NAME - 1);
            if (strcmp(key, "site_config") == 0) strncpy(cfg->docs.site_config, val, EOS_MAX_PATH - 1);
            if (strcmp(key, "title") == 0)      strncpy(cfg->docs.title, val, EOS_MAX_NAME - 1);
            if (strcmp(key, "logo") == 0)       strncpy(cfg->docs.logo, val, EOS_MAX_PATH - 1);
            break;
        default:
            break;
        }
    }

    fclose(fp);
    EOS_INFO("Loaded config: %s (%s v%s)", path, cfg->project.name, cfg->project.version);
    return EOS_OK;
}

void eos_config_dump(const EosConfig *cfg) {
    printf("Project: %s v%s\n", cfg->project.name, cfg->project.version);
    printf("Workspace:\n");
    printf("  backend:   %s\n", cfg->workspace.backend);
    printf("  build_dir: %s\n", cfg->workspace.build_dir);
    printf("  cache_dir: %s\n", cfg->workspace.cache_dir);

    printf("Toolchain:\n");
    if (cfg->toolchain.target[0])
        printf("  target: %s\n", cfg->toolchain.target);
    if (cfg->toolchain.rtos_target[0])
        printf("  rtos:   %s\n", cfg->toolchain.rtos_target);

    printf("Layers (%d):\n", cfg->layer_count);
    for (int i = 0; i < cfg->layer_count; i++) {
        printf("  - %s\n", cfg->layers[i]);
    }

    printf("Packages (%d):\n", cfg->package_count);
    for (int i = 0; i < cfg->package_count; i++) {
        printf("  - %s %s [%s]\n", cfg->packages[i].name, cfg->packages[i].version,
               eos_build_type_str(cfg->packages[i].build_type));
    }

    printf("System:\n");
    printf("  kind: %s\n", eos_system_kind_str(cfg->system.kind));

    if (cfg->system.kind == EOS_SYSTEM_LINUX || cfg->system.kind == EOS_SYSTEM_HYBRID) {
        printf("  kernel provider: %s\n", cfg->system.kernel.provider);
        printf("  rootfs provider: %s\n", cfg->system.rootfs.provider);
    }

    if (cfg->system.kind == EOS_SYSTEM_RTOS || cfg->system.kind == EOS_SYSTEM_HYBRID) {
        printf("  RTOS targets (%d):\n", cfg->system.rtos_count);
        for (int i = 0; i < cfg->system.rtos_count; i++) {
            const EosRtosConfig *rc = &cfg->system.rtos[i];
            printf("    [%d] provider: %s\n", i, eos_rtos_provider_str(rc->provider));
            if (rc->board[0])  printf("        board:    %s\n", rc->board);
            if (rc->core[0])   printf("        core:     %s\n", rc->core);
            if (rc->entry[0])  printf("        entry:    %s\n", rc->entry);
            if (rc->output[0]) printf("        output:   %s\n", rc->output);
        }
    }

    printf("Docs:\n");
    printf("  enabled:    %s\n", cfg->docs.enabled ? "yes" : "no");
    if (cfg->docs.enabled) {
        printf("  api_docs:   %s (tool: %s)\n", cfg->docs.api_docs ? "yes" : "no", cfg->docs.api_tool);
        printf("  user_docs:  %s (tool: %s)\n", cfg->docs.user_docs ? "yes" : "no", cfg->docs.site_tool);
        if (cfg->docs.output_dir[0])
            printf("  output_dir: %s\n", cfg->docs.output_dir);
    }
}
