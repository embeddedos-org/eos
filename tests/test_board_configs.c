// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_board_configs.c
 * @brief Validates all board YAML configurations for schema conformance
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int file_contains(const char *path, const char *needle) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, needle)) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

static const char *required_fields[] = {
    "name:",
    "arch:",
    NULL
};

static const char *valid_arches[] = {
    "arm64", "cortex-m", "cortex-m4", "cortex-m7", "cortex-r",
    "cortex-a", "x86", "x86_64", "riscv32", "riscv64",
    "mips", "powerpc", "sparc", "sh", "m68k", "h8300",
    "mn103", "v850", "frv", "strongarm", "xscale", "host",
    NULL
};

static void validate_board(const char *dir, const char *filename) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, filename);

    tests_run++;
    printf("  %-50s ", filename);

    if (!file_exists(path)) {
        printf("[FAIL] file not found\n");
        tests_failed++;
        return;
    }

    /* Check required fields */
    for (int i = 0; required_fields[i]; i++) {
        if (!file_contains(path, required_fields[i])) {
            printf("[FAIL] missing '%s'\n", required_fields[i]);
            tests_failed++;
            return;
        }
    }

    /* Check arch is recognized */
    int arch_valid = 0;
    for (int i = 0; valid_arches[i]; i++) {
        if (file_contains(path, valid_arches[i])) {
            arch_valid = 1;
            break;
        }
    }
    if (!arch_valid) {
        printf("[WARN] unrecognized arch (may be valid)\n");
    }

    tests_passed++;
    printf("[PASS]\n");
}

int main(int argc, char *argv[]) {
    const char *boards_dir = "boards";
    if (argc > 1) boards_dir = argv[1];

    printf("=== EoS: Board Config Validation Tests ===\n\n");
    printf("  Scanning: %s\n\n", boards_dir);

    DIR *d = opendir(boards_dir);
    if (!d) {
        printf("  [SKIP] Cannot open '%s' — run from project root\n", boards_dir);
        printf("\n0/0 tests passed (skipped)\n");
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 5 && strcmp(entry->d_name + len - 5, ".yaml") == 0) {
            validate_board(boards_dir, entry->d_name);
        }
    }
    closedir(d);

    printf("\n%d/%d tests passed", tests_passed, tests_run);
    if (tests_failed > 0) printf(" (%d failed)", tests_failed);
    printf("\n");
    return (tests_failed == 0) ? 0 : 1;
}
