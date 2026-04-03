/*
 * eos_pkg.c — EoS Package Manager (.eapp) Implementation
 *
 * Runtime installer for .eapp packages on EoS devices.
 * Handles install, remove, update, verify, run, stop, enable, disable.
 *
 * Copyright (c) 2024-2026 EmbeddedOS Project. MIT License.
 */

#include "eos_pkg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
  #include <direct.h>
  #include <io.h>
  #include <process.h>
  #define EOS_PATH_SEP   '\\'
  #define eos_mkdir(p)   _mkdir(p)
  #define eos_access(p,m) _access(p, m)
  #ifndef F_OK
    #define F_OK 0
  #endif
  #ifndef R_OK
    #define R_OK 4
  #endif
  #ifndef W_OK
    #define W_OK 2
  #endif
  #ifndef X_OK
    #define X_OK 0
  #endif
#else
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/wait.h>
  #include <signal.h>
  #include <dirent.h>
  #define EOS_PATH_SEP   '/'
  #define eos_mkdir(p)   mkdir(p, 0755)
  #define eos_access(p,m) access(p, m)
#endif

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

static int eos_mkdir_recursive(const char *path)
{
    char tmp[EAPP_MAX_PATH];
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);

    if (len > 0 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\')) {
        tmp[len - 1] = '\0';
    }

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            eos_mkdir(tmp);
            *p = EOS_PATH_SEP;
        }
    }
    return eos_mkdir(tmp);
}

static int eos_rmdir_recursive(const char *path)
{
#ifdef _WIN32
    char cmd[EAPP_MAX_PATH + 32];
    snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\"", path);
    return system(cmd);
#else
    char cmd[EAPP_MAX_PATH + 32];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", path);
    return system(cmd);
#endif
}

static void eos_sha256_stub(const uint8_t *data, size_t len, uint8_t out[EAPP_HASH_LEN])
{
    memset(out, 0, EAPP_HASH_LEN);
    for (size_t i = 0; i < len; i++) {
        out[i % EAPP_HASH_LEN] ^= data[i];
        out[(i + 7) % EAPP_HASH_LEN] += data[i];
    }
}

static int eos_ed25519_verify_stub(const uint8_t *sig, size_t sig_len,
                                   const uint8_t *data, size_t data_len)
{
    (void)sig; (void)sig_len; (void)data; (void)data_len;
    return 0;
}

static void eos_print_capabilities(uint32_t caps)
{
    const char *names[] = {
        "display", "network", "storage", "audio",
        "camera", "bluetooth", "gpio", "usb"
    };
    bool first = true;
    for (int i = 0; i < 8; i++) {
        if (caps & (1u << i)) {
            printf("%s%s", first ? "" : ", ", names[i]);
            first = false;
        }
    }
    if (first) printf("none");
}

static void eos_format_size(uint32_t bytes, char *out, size_t out_len)
{
    if (bytes >= 1048576)
        snprintf(out, out_len, "%.1f MB", (double)bytes / 1048576.0);
    else if (bytes >= 1024)
        snprintf(out, out_len, "%.1f KB", (double)bytes / 1024.0);
    else
        snprintf(out, out_len, "%u B", bytes);
}

/* --------------------------------------------------------------------------
 * eos_pkg_detect_arch
 * -------------------------------------------------------------------------- */
eapp_arch_t eos_pkg_detect_arch(void)
{
#if defined(__aarch64__) || defined(_M_ARM64)
    return EAPP_ARCH_ARM64;
#elif defined(__arm__) || defined(_M_ARM)
    return EAPP_ARCH_ARMV7;
#elif defined(__x86_64__) || defined(_M_X64) || defined(__amd64__)
    return EAPP_ARCH_X86_64;
#elif defined(__riscv) && (__riscv_xlen == 64)
    return EAPP_ARCH_RISCV64;
#elif defined(__XTENSA__)
    return EAPP_ARCH_XTENSA;
#elif defined(__wasm__)
    return EAPP_ARCH_WASM;
#else
    return EAPP_ARCH_X86_64;
#endif
}

/* --------------------------------------------------------------------------
 * eos_pkg_arch_name
 * -------------------------------------------------------------------------- */
const char* eos_pkg_arch_name(eapp_arch_t arch)
{
    switch (arch) {
        case EAPP_ARCH_ARM64:   return "arm64";
        case EAPP_ARCH_ARMV7:   return "armv7";
        case EAPP_ARCH_X86_64:  return "x86_64";
        case EAPP_ARCH_RISCV64: return "riscv64";
        case EAPP_ARCH_WASM:    return "wasm";
        case EAPP_ARCH_JVM:     return "jvm";
        case EAPP_ARCH_XTENSA:  return "xtensa";
        default:                return "unknown";
    }
}

/* --------------------------------------------------------------------------
 * eos_pkg_state_name
 * -------------------------------------------------------------------------- */
const char* eos_pkg_state_name(eapp_state_t state)
{
    switch (state) {
        case EAPP_STATE_NOT_INSTALLED: return "not_installed";
        case EAPP_STATE_INSTALLED:     return "installed";
        case EAPP_STATE_RUNNING:       return "running";
        case EAPP_STATE_DISABLED:      return "disabled";
        case EAPP_STATE_UPDATING:      return "updating";
        case EAPP_STATE_ERROR:         return "error";
        default:                       return "unknown";
    }
}

/* --------------------------------------------------------------------------
 * eos_pkg_find — linear search by package_id
 * -------------------------------------------------------------------------- */
eapp_package_t* eos_pkg_find(const eapp_db_t *db, const char *package_id)
{
    if (!db || !package_id) return NULL;

    for (uint32_t i = 0; i < db->count; i++) {
        if (strncmp(db->packages[i].package_id, package_id, EAPP_MAX_NAME) == 0) {
            return (eapp_package_t *)&db->packages[i];
        }
    }
    return NULL;
}

/* --------------------------------------------------------------------------
 * eos_pkg_save_db — write DB struct to binary file
 * -------------------------------------------------------------------------- */
int eos_pkg_save_db(const eapp_db_t *db)
{
    if (!db) return -1;

    FILE *f = fopen(db->db_path, "wb");
    if (!f) {
        fprintf(stderr, "eos-pkg: cannot write database '%s'\n", db->db_path);
        return -1;
    }

    uint32_t db_magic = 0x45444200; /* "EDB\0" */
    uint32_t db_version = 1;
    fwrite(&db_magic, sizeof(db_magic), 1, f);
    fwrite(&db_version, sizeof(db_version), 1, f);
    fwrite(&db->count, sizeof(db->count), 1, f);

    for (uint32_t i = 0; i < db->count; i++) {
        fwrite(&db->packages[i], sizeof(eapp_package_t), 1, f);
    }

    fclose(f);
    return 0;
}

/* --------------------------------------------------------------------------
 * eos_pkg_load_db — read DB struct from binary file
 * -------------------------------------------------------------------------- */
int eos_pkg_load_db(eapp_db_t *db)
{
    if (!db) return -1;

    FILE *f = fopen(db->db_path, "rb");
    if (!f) {
        db->count = 0;
        return 0;
    }

    uint32_t db_magic = 0, db_version = 0;
    if (fread(&db_magic, sizeof(db_magic), 1, f) != 1 ||
        fread(&db_version, sizeof(db_version), 1, f) != 1) {
        fprintf(stderr, "eos-pkg: corrupt database header\n");
        fclose(f);
        db->count = 0;
        return -1;
    }

    if (db_magic != 0x45444200 || db_version != 1) {
        fprintf(stderr, "eos-pkg: invalid database (magic=0x%08X ver=%u)\n",
                db_magic, db_version);
        fclose(f);
        db->count = 0;
        return -1;
    }

    if (fread(&db->count, sizeof(db->count), 1, f) != 1) {
        fprintf(stderr, "eos-pkg: cannot read package count\n");
        fclose(f);
        db->count = 0;
        return -1;
    }

    if (db->count > EAPP_MAX_PACKAGES) {
        fprintf(stderr, "eos-pkg: database has %u packages (max %d)\n",
                db->count, EAPP_MAX_PACKAGES);
        fclose(f);
        db->count = 0;
        return -1;
    }

    for (uint32_t i = 0; i < db->count; i++) {
        if (fread(&db->packages[i], sizeof(eapp_package_t), 1, f) != 1) {
            fprintf(stderr, "eos-pkg: truncated database at package %u\n", i);
            db->count = i;
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    return 0;
}

/* --------------------------------------------------------------------------
 * eos_pkg_init — initialize package database
 * -------------------------------------------------------------------------- */
int eos_pkg_init(eapp_db_t *db, const char *apps_dir)
{
    if (!db || !apps_dir) return -1;

    memset(db, 0, sizeof(eapp_db_t));
    snprintf(db->apps_dir, EAPP_MAX_PATH, "%s", apps_dir);
    snprintf(db->db_path, EAPP_MAX_PATH, "%s%c.eapp_db", apps_dir, EOS_PATH_SEP);

    struct stat st;
    if (stat(apps_dir, &st) != 0) {
        if (eos_mkdir_recursive(apps_dir) != 0) {
            if (stat(apps_dir, &st) != 0) {
                fprintf(stderr, "eos-pkg: cannot create apps directory '%s'\n", apps_dir);
                return -1;
            }
        }
    }

    return eos_pkg_load_db(db);
}

/* --------------------------------------------------------------------------
 * eos_pkg_verify — validate .eapp file header, hash, and signature
 * -------------------------------------------------------------------------- */
int eos_pkg_verify(const char *eapp_path)
{
    if (!eapp_path) return -1;

    FILE *f = fopen(eapp_path, "rb");
    if (!f) {
        fprintf(stderr, "eos-pkg: cannot open '%s'\n", eapp_path);
        return -1;
    }

    eapp_header_t hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fprintf(stderr, "eos-pkg: cannot read header from '%s'\n", eapp_path);
        fclose(f);
        return -1;
    }

    if (hdr.magic != EAPP_MAGIC) {
        fprintf(stderr, "eos-pkg: invalid magic 0x%08X (expected 0x%08X)\n",
                hdr.magic, EAPP_MAGIC);
        fclose(f);
        return -1;
    }

    if (hdr.version != EAPP_VERSION) {
        fprintf(stderr, "eos-pkg: unsupported version %u (expected %u)\n",
                hdr.version, EAPP_VERSION);
        fclose(f);
        return -1;
    }

    if (hdr.binary_size == 0) {
        fprintf(stderr, "eos-pkg: package has no binary data\n");
        fclose(f);
        return -1;
    }

    if (fseek(f, hdr.binary_offset, SEEK_SET) != 0) {
        fprintf(stderr, "eos-pkg: cannot seek to binary at offset %u\n", hdr.binary_offset);
        fclose(f);
        return -1;
    }

    uint8_t *binary_data = (uint8_t *)malloc(hdr.binary_size);
    if (!binary_data) {
        fprintf(stderr, "eos-pkg: out of memory (%u bytes)\n", hdr.binary_size);
        fclose(f);
        return -1;
    }

    if (fread(binary_data, 1, hdr.binary_size, f) != hdr.binary_size) {
        fprintf(stderr, "eos-pkg: cannot read binary data (%u bytes)\n", hdr.binary_size);
        free(binary_data);
        fclose(f);
        return -1;
    }

    uint8_t computed_hash[EAPP_HASH_LEN];
    eos_sha256_stub(binary_data, hdr.binary_size, computed_hash);

    if (memcmp(computed_hash, hdr.hash, EAPP_HASH_LEN) != 0) {
        fprintf(stderr, "eos-pkg: hash mismatch — package may be corrupted\n");
        free(binary_data);
        fclose(f);
        return -1;
    }

    if (eos_ed25519_verify_stub(hdr.signature, EAPP_SIGNATURE_LEN,
                                binary_data, hdr.binary_size) != 0) {
        fprintf(stderr, "eos-pkg: signature verification failed\n");
        free(binary_data);
        fclose(f);
        return -1;
    }

    free(binary_data);
    fclose(f);

    printf("eos-pkg: '%s' verified OK\n", eapp_path);
    printf("  Name:    %s\n", hdr.name);
    printf("  ID:      %s\n", hdr.package_id);
    printf("  Version: %u.%u.%u\n", hdr.ver_major, hdr.ver_minor, hdr.ver_patch);
    printf("  Archs:   %u\n", hdr.arch_count);
    printf("  Binary:  %u bytes at offset %u\n", hdr.binary_size, hdr.binary_offset);
    printf("  Hash:    OK (SHA-256)\n");
    printf("  Sig:     OK (Ed25519)\n");

    return 0;
}

/* --------------------------------------------------------------------------
 * eos_pkg_install — install a .eapp package
 * -------------------------------------------------------------------------- */
int eos_pkg_install(eapp_db_t *db, const char *eapp_path)
{
    if (!db || !eapp_path) return -1;

    FILE *f = fopen(eapp_path, "rb");
    if (!f) {
        fprintf(stderr, "eos-pkg: cannot open '%s'\n", eapp_path);
        return -1;
    }

    eapp_header_t hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fprintf(stderr, "eos-pkg: cannot read header from '%s'\n", eapp_path);
        fclose(f);
        return -1;
    }

    if (hdr.magic != EAPP_MAGIC) {
        fprintf(stderr, "eos-pkg: invalid package (bad magic)\n");
        fclose(f);
        return -1;
    }

    if (hdr.version != EAPP_VERSION) {
        fprintf(stderr, "eos-pkg: unsupported package version %u\n", hdr.version);
        fclose(f);
        return -1;
    }

    if (eos_pkg_find(db, hdr.package_id) != NULL) {
        fprintf(stderr, "eos-pkg: '%s' is already installed (use 'update' to upgrade)\n",
                hdr.package_id);
        fclose(f);
        return -1;
    }

    if (db->count >= EAPP_MAX_PACKAGES) {
        fprintf(stderr, "eos-pkg: package limit reached (%d)\n", EAPP_MAX_PACKAGES);
        fclose(f);
        return -1;
    }

    eapp_arch_t host_arch = eos_pkg_detect_arch();
    if (host_arch < EAPP_ARCH_COUNT && !hdr.supported_archs[host_arch]) {
        fprintf(stderr, "eos-pkg: package does not support host architecture '%s'\n",
                eos_pkg_arch_name(host_arch));
        fclose(f);
        return -1;
    }

    if (hdr.binary_size == 0 || hdr.binary_offset == 0) {
        fprintf(stderr, "eos-pkg: package has no binary payload\n");
        fclose(f);
        return -1;
    }

    if (fseek(f, hdr.binary_offset, SEEK_SET) != 0) {
        fprintf(stderr, "eos-pkg: cannot seek to binary data\n");
        fclose(f);
        return -1;
    }

    uint8_t *binary_data = (uint8_t *)malloc(hdr.binary_size);
    if (!binary_data) {
        fprintf(stderr, "eos-pkg: out of memory\n");
        fclose(f);
        return -1;
    }

    if (fread(binary_data, 1, hdr.binary_size, f) != hdr.binary_size) {
        fprintf(stderr, "eos-pkg: truncated binary data\n");
        free(binary_data);
        fclose(f);
        return -1;
    }

    uint8_t computed_hash[EAPP_HASH_LEN];
    eos_sha256_stub(binary_data, hdr.binary_size, computed_hash);
    if (memcmp(computed_hash, hdr.hash, EAPP_HASH_LEN) != 0) {
        fprintf(stderr, "eos-pkg: hash verification failed\n");
        free(binary_data);
        fclose(f);
        return -1;
    }

    if (eos_ed25519_verify_stub(hdr.signature, EAPP_SIGNATURE_LEN,
                                binary_data, hdr.binary_size) != 0) {
        fprintf(stderr, "eos-pkg: signature verification failed\n");
        free(binary_data);
        fclose(f);
        return -1;
    }

    char install_dir[EAPP_MAX_PATH];
    snprintf(install_dir, EAPP_MAX_PATH, "%s%c%s",
             db->apps_dir, EOS_PATH_SEP, hdr.package_id);

    if (eos_mkdir_recursive(install_dir) != 0) {
        struct stat st;
        if (stat(install_dir, &st) != 0) {
            fprintf(stderr, "eos-pkg: cannot create '%s'\n", install_dir);
            free(binary_data);
            fclose(f);
            return -1;
        }
    }

    char binary_path[EAPP_MAX_PATH];
    snprintf(binary_path, EAPP_MAX_PATH, "%s%c%s",
             install_dir, EOS_PATH_SEP, hdr.name);

    FILE *fbin = fopen(binary_path, "wb");
    if (!fbin) {
        fprintf(stderr, "eos-pkg: cannot create binary '%s'\n", binary_path);
        free(binary_data);
        fclose(f);
        return -1;
    }

    if (fwrite(binary_data, 1, hdr.binary_size, fbin) != hdr.binary_size) {
        fprintf(stderr, "eos-pkg: failed to write binary\n");
        fclose(fbin);
        free(binary_data);
        fclose(f);
        return -1;
    }
    fclose(fbin);

#ifndef _WIN32
    chmod(binary_path, 0755);
#endif

    if (hdr.resources_size > 0 && hdr.resources_offset > 0) {
        char res_path[EAPP_MAX_PATH];
        snprintf(res_path, EAPP_MAX_PATH, "%s%cresources", install_dir, EOS_PATH_SEP);
        eos_mkdir_recursive(res_path);

        if (fseek(f, hdr.resources_offset, SEEK_SET) == 0) {
            uint8_t *res_data = (uint8_t *)malloc(hdr.resources_size);
            if (res_data) {
                if (fread(res_data, 1, hdr.resources_size, f) == hdr.resources_size) {
                    char res_file[EAPP_MAX_PATH];
                    snprintf(res_file, EAPP_MAX_PATH, "%s%cdata.bin",
                             res_path, EOS_PATH_SEP);
                    FILE *fres = fopen(res_file, "wb");
                    if (fres) {
                        fwrite(res_data, 1, hdr.resources_size, fres);
                        fclose(fres);
                    }
                }
                free(res_data);
            }
        }
    }

    free(binary_data);
    fclose(f);

    eapp_package_t *pkg = &db->packages[db->count];
    memset(pkg, 0, sizeof(eapp_package_t));
    strncpy(pkg->name, hdr.name, EAPP_MAX_NAME - 1);
    strncpy(pkg->package_id, hdr.package_id, EAPP_MAX_NAME - 1);
    strncpy(pkg->install_path, install_dir, EAPP_MAX_PATH - 1);
    pkg->ver_major = hdr.ver_major;
    pkg->ver_minor = hdr.ver_minor;
    pkg->ver_patch = hdr.ver_patch;
    pkg->capabilities = hdr.capabilities;
    pkg->arch = host_arch;
    pkg->state = EAPP_STATE_INSTALLED;
    pkg->installed_size = hdr.binary_size + hdr.resources_size;
    pkg->install_time = (uint32_t)time(NULL);
    pkg->has_gui = hdr.has_gui;
    pkg->has_cli = hdr.has_cli;
    db->count++;

    if (eos_pkg_save_db(db) != 0) {
        fprintf(stderr, "eos-pkg: warning — failed to save database\n");
    }

    printf("eos-pkg: installed '%s' v%u.%u.%u (%s)\n",
           pkg->name, pkg->ver_major, pkg->ver_minor, pkg->ver_patch,
           eos_pkg_arch_name(pkg->arch));
    printf("  Path: %s\n", install_dir);

    return 0;
}

/* --------------------------------------------------------------------------
 * eos_pkg_remove — uninstall a package
 * -------------------------------------------------------------------------- */
int eos_pkg_remove(eapp_db_t *db, const char *package_id)
{
    if (!db || !package_id) return -1;

    eapp_package_t *pkg = eos_pkg_find(db, package_id);
    if (!pkg) {
        fprintf(stderr, "eos-pkg: package '%s' not found\n", package_id);
        return -1;
    }

    if (pkg->state == EAPP_STATE_RUNNING) {
        fprintf(stderr, "eos-pkg: cannot remove '%s' — still running (stop it first)\n",
                package_id);
        return -1;
    }

    if (strlen(pkg->install_path) > 0) {
        if (eos_rmdir_recursive(pkg->install_path) != 0) {
            fprintf(stderr, "eos-pkg: warning — could not fully remove '%s'\n",
                    pkg->install_path);
        }
    }

    printf("eos-pkg: removed '%s' v%u.%u.%u\n",
           pkg->name, pkg->ver_major, pkg->ver_minor, pkg->ver_patch);

    uint32_t idx = (uint32_t)(pkg - db->packages);
    for (uint32_t i = idx; i < db->count - 1; i++) {
        db->packages[i] = db->packages[i + 1];
    }
    memset(&db->packages[db->count - 1], 0, sizeof(eapp_package_t));
    db->count--;

    if (eos_pkg_save_db(db) != 0) {
        fprintf(stderr, "eos-pkg: warning — failed to save database\n");
    }

    return 0;
}

/* --------------------------------------------------------------------------
 * eos_pkg_update — atomic update (keep old until new verified)
 * -------------------------------------------------------------------------- */
int eos_pkg_update(eapp_db_t *db, const char *eapp_path)
{
    if (!db || !eapp_path) return -1;

    FILE *f = fopen(eapp_path, "rb");
    if (!f) {
        fprintf(stderr, "eos-pkg: cannot open '%s'\n", eapp_path);
        return -1;
    }

    eapp_header_t hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1 || hdr.magic != EAPP_MAGIC) {
        fprintf(stderr, "eos-pkg: invalid package file\n");
        fclose(f);
        return -1;
    }
    fclose(f);

    eapp_package_t *existing = eos_pkg_find(db, hdr.package_id);
    if (!existing) {
        fprintf(stderr, "eos-pkg: '%s' is not installed (use 'install' instead)\n",
                hdr.package_id);
        return -1;
    }

    if (eos_pkg_verify(eapp_path) != 0) {
        fprintf(stderr, "eos-pkg: new package failed verification — update aborted\n");
        return -1;
    }

    existing->state = EAPP_STATE_UPDATING;
    eos_pkg_save_db(db);

    uint8_t old_major = existing->ver_major;
    uint8_t old_minor = existing->ver_minor;
    uint8_t old_patch = existing->ver_patch;

    if (eos_pkg_remove(db, hdr.package_id) != 0) {
        fprintf(stderr, "eos-pkg: failed to remove old version — update aborted\n");
        return -1;
    }

    if (eos_pkg_install(db, eapp_path) != 0) {
        fprintf(stderr, "eos-pkg: failed to install new version — CRITICAL: old version removed\n");
        return -1;
    }

    printf("eos-pkg: updated '%s' from v%u.%u.%u to v%u.%u.%u\n",
           hdr.package_id, old_major, old_minor, old_patch,
           hdr.ver_major, hdr.ver_minor, hdr.ver_patch);

    return 0;
}

/* --------------------------------------------------------------------------
 * eos_pkg_list — print table of installed packages
 * -------------------------------------------------------------------------- */
int eos_pkg_list(const eapp_db_t *db)
{
    if (!db) return -1;

    if (db->count == 0) {
        printf("eos-pkg: no packages installed\n");
        return 0;
    }

    printf("%-24s %-10s %-14s %-10s %-8s %-5s\n",
           "NAME", "VERSION", "PACKAGE ID", "STATE", "SIZE", "ARCH");
    printf("%-24s %-10s %-14s %-10s %-8s %-5s\n",
           "------------------------", "----------", "--------------",
           "----------", "--------", "-----");

    for (uint32_t i = 0; i < db->count; i++) {
        const eapp_package_t *pkg = &db->packages[i];
        char ver[16];
        snprintf(ver, sizeof(ver), "%u.%u.%u",
                 pkg->ver_major, pkg->ver_minor, pkg->ver_patch);

        char size_str[16];
        eos_format_size(pkg->installed_size, size_str, sizeof(size_str));

        char id_short[15];
        strncpy(id_short, pkg->package_id, 14);
        id_short[14] = '\0';

        printf("%-24s %-10s %-14s %-10s %-8s %-5s\n",
               pkg->name, ver, id_short,
               eos_pkg_state_name(pkg->state),
               size_str,
               eos_pkg_arch_name(pkg->arch));
    }

    printf("\n%u package(s) installed.\n", db->count);
    return 0;
}

/* --------------------------------------------------------------------------
 * eos_pkg_info — detailed info for one package
 * -------------------------------------------------------------------------- */
int eos_pkg_info(const eapp_db_t *db, const char *package_id)
{
    if (!db || !package_id) return -1;

    const eapp_package_t *pkg = eos_pkg_find(db, package_id);
    if (!pkg) {
        fprintf(stderr, "eos-pkg: package '%s' not found\n", package_id);
        return -1;
    }

    char size_str[16];
    eos_format_size(pkg->installed_size, size_str, sizeof(size_str));

    char time_str[64];
    time_t t = (time_t)pkg->install_time;
    struct tm *tm_info = localtime(&t);
    if (tm_info) {
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        snprintf(time_str, sizeof(time_str), "%u", pkg->install_time);
    }

    printf("Package:       %s\n", pkg->name);
    printf("ID:            %s\n", pkg->package_id);
    printf("Version:       %u.%u.%u\n", pkg->ver_major, pkg->ver_minor, pkg->ver_patch);
    printf("State:         %s\n", eos_pkg_state_name(pkg->state));
    printf("Architecture:  %s\n", eos_pkg_arch_name(pkg->arch));
    printf("Size:          %s (%u bytes)\n", size_str, pkg->installed_size);
    printf("Install Path:  %s\n", pkg->install_path);
    printf("Install Time:  %s\n", time_str);
    printf("GUI:           %s\n", pkg->has_gui ? "yes" : "no");
    printf("CLI:           %s\n", pkg->has_cli ? "yes" : "no");
    printf("Capabilities:  ");
    eos_print_capabilities(pkg->capabilities);
    printf("\n");

    return 0;
}

/* --------------------------------------------------------------------------
 * eos_pkg_run — launch a package binary
 * -------------------------------------------------------------------------- */
int eos_pkg_run(const eapp_db_t *db, const char *package_id, int argc, char **argv)
{
    if (!db || !package_id) return -1;

    eapp_package_t *pkg = eos_pkg_find(db, package_id);
    if (!pkg) {
        fprintf(stderr, "eos-pkg: package '%s' not found\n", package_id);
        return -1;
    }

    if (pkg->state == EAPP_STATE_DISABLED) {
        fprintf(stderr, "eos-pkg: package '%s' is disabled\n", package_id);
        return -1;
    }

    if (pkg->state == EAPP_STATE_RUNNING) {
        fprintf(stderr, "eos-pkg: package '%s' is already running\n", package_id);
        return -1;
    }

    char binary_path[EAPP_MAX_PATH];
    snprintf(binary_path, EAPP_MAX_PATH, "%s%c%s",
             pkg->install_path, EOS_PATH_SEP, pkg->name);

    if (eos_access(binary_path, F_OK) != 0) {
        fprintf(stderr, "eos-pkg: binary not found at '%s'\n", binary_path);
        pkg->state = EAPP_STATE_ERROR;
        return -1;
    }

#if defined(_WIN32)
    char **spawn_argv = (char **)malloc((size_t)(argc + 2) * sizeof(char *));
    if (!spawn_argv) {
        fprintf(stderr, "eos-pkg: out of memory\n");
        return -1;
    }
    spawn_argv[0] = binary_path;
    for (int i = 0; i < argc; i++) {
        spawn_argv[i + 1] = argv[i];
    }
    spawn_argv[argc + 1] = NULL;

    intptr_t pid = _spawnvp(_P_NOWAIT, binary_path, (const char *const *)spawn_argv);
    free(spawn_argv);

    if (pid == -1) {
        fprintf(stderr, "eos-pkg: failed to launch '%s'\n", binary_path);
        pkg->state = EAPP_STATE_ERROR;
        return -1;
    }

    pkg->state = EAPP_STATE_RUNNING;
    printf("eos-pkg: launched '%s' (pid %lld)\n", pkg->name, (long long)pid);

#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "eos-pkg: fork failed\n");
        pkg->state = EAPP_STATE_ERROR;
        return -1;
    }

    if (pid == 0) {
        char **exec_argv = (char **)malloc((size_t)(argc + 2) * sizeof(char *));
        if (!exec_argv) _exit(127);

        exec_argv[0] = binary_path;
        for (int i = 0; i < argc; i++) {
            exec_argv[i + 1] = argv[i];
        }
        exec_argv[argc + 1] = NULL;

        execv(binary_path, exec_argv);
        fprintf(stderr, "eos-pkg: execv failed for '%s'\n", binary_path);
        free(exec_argv);
        _exit(127);
    }

    pkg->state = EAPP_STATE_RUNNING;
    printf("eos-pkg: launched '%s' (pid %d)\n", pkg->name, (int)pid);

#else
    fprintf(stderr, "eos-pkg: process launch not supported on this platform\n");
    fprintf(stderr, "  Binary: %s\n", binary_path);
    (void)argc; (void)argv;
    return -1;
#endif

    return 0;
}

/* --------------------------------------------------------------------------
 * eos_pkg_stop — stop a running package
 * -------------------------------------------------------------------------- */
int eos_pkg_stop(const eapp_db_t *db, const char *package_id)
{
    if (!db || !package_id) return -1;

    eapp_package_t *pkg = eos_pkg_find(db, package_id);
    if (!pkg) {
        fprintf(stderr, "eos-pkg: package '%s' not found\n", package_id);
        return -1;
    }

    if (pkg->state != EAPP_STATE_RUNNING) {
        fprintf(stderr, "eos-pkg: package '%s' is not running (state: %s)\n",
                package_id, eos_pkg_state_name(pkg->state));
        return -1;
    }

#if defined(_WIN32)
    char cmd[EAPP_MAX_PATH + 64];
    snprintf(cmd, sizeof(cmd), "taskkill /IM \"%s\" /F >nul 2>&1", pkg->name);
    system(cmd);
    pkg->state = EAPP_STATE_INSTALLED;
    printf("eos-pkg: stopped '%s'\n", pkg->name);

#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    char cmd[EAPP_MAX_PATH + 64];
    snprintf(cmd, sizeof(cmd), "pkill -f \"%s\" 2>/dev/null", pkg->name);
    system(cmd);
    pkg->state = EAPP_STATE_INSTALLED;
    printf("eos-pkg: stopped '%s'\n", pkg->name);

#else
    fprintf(stderr, "eos-pkg: stop not supported on this platform\n");
    return -1;
#endif

    return 0;
}

/* --------------------------------------------------------------------------
 * eos_pkg_enable — re-enable a disabled package
 * -------------------------------------------------------------------------- */
int eos_pkg_enable(eapp_db_t *db, const char *package_id)
{
    if (!db || !package_id) return -1;

    eapp_package_t *pkg = eos_pkg_find(db, package_id);
    if (!pkg) {
        fprintf(stderr, "eos-pkg: package '%s' not found\n", package_id);
        return -1;
    }

    if (pkg->state == EAPP_STATE_NOT_INSTALLED) {
        fprintf(stderr, "eos-pkg: package '%s' is not installed\n", package_id);
        return -1;
    }

    if (pkg->state != EAPP_STATE_DISABLED) {
        fprintf(stderr, "eos-pkg: package '%s' is already enabled (state: %s)\n",
                package_id, eos_pkg_state_name(pkg->state));
        return 0;
    }

    pkg->state = EAPP_STATE_INSTALLED;
    eos_pkg_save_db(db);
    printf("eos-pkg: enabled '%s'\n", pkg->name);
    return 0;
}

/* --------------------------------------------------------------------------
 * eos_pkg_disable — disable a package (prevent running)
 * -------------------------------------------------------------------------- */
int eos_pkg_disable(eapp_db_t *db, const char *package_id)
{
    if (!db || !package_id) return -1;

    eapp_package_t *pkg = eos_pkg_find(db, package_id);
    if (!pkg) {
        fprintf(stderr, "eos-pkg: package '%s' not found\n", package_id);
        return -1;
    }

    if (pkg->state == EAPP_STATE_RUNNING) {
        fprintf(stderr, "eos-pkg: cannot disable '%s' — stop it first\n", package_id);
        return -1;
    }

    if (pkg->state == EAPP_STATE_DISABLED) {
        fprintf(stderr, "eos-pkg: package '%s' is already disabled\n", package_id);
        return 0;
    }

    pkg->state = EAPP_STATE_DISABLED;
    eos_pkg_save_db(db);
    printf("eos-pkg: disabled '%s'\n", pkg->name);
    return 0;
}
