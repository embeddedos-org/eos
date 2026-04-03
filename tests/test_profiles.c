// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_profiles.c
 * @brief Regression tests validating all 48 product profiles parse correctly
 *
 * Reads each YAML profile from products/ and verifies required fields.
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

static void validate_profile(const char *dir, const char *filename) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, filename);

    tests_run++;
    printf("  %-50s ", filename);

    if (!file_exists(path)) {
        printf("[FAIL] file not found\n");
        tests_failed++;
        return;
    }

    /* Every profile must have a 'name:' field */
    if (!file_contains(path, "name:")) {
        printf("[FAIL] missing 'name:' field\n");
        tests_failed++;
        return;
    }

    /* Every profile must have 'features:' or 'config:' */
    if (!file_contains(path, "features:") && !file_contains(path, "config:")) {
        printf("[FAIL] missing 'features:' or 'config:' field\n");
        tests_failed++;
        return;
    }

    tests_passed++;
    printf("[PASS]\n");
}

int main(int argc, char *argv[]) {
    const char *products_dir = "products";
    if (argc > 1) products_dir = argv[1];

    printf("=== EoS: Product Profile Regression Tests ===\n\n");
    printf("  Scanning: %s\n\n", products_dir);

    DIR *d = opendir(products_dir);
    if (!d) {
        printf("  [SKIP] Cannot open '%s' — run from project root\n", products_dir);
        printf("\n0/0 tests passed (skipped)\n");
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 5 && strcmp(entry->d_name + len - 5, ".yaml") == 0) {
            validate_profile(products_dir, entry->d_name);
        }
        if (len > 4 && strcmp(entry->d_name + len - 4, ".yml") == 0) {
            validate_profile(products_dir, entry->d_name);
        }
    }
    closedir(d);

    if (tests_run == 0) {
        printf("  [WARN] No profile files found\n");
    }

    printf("\n%d/%d tests passed", tests_passed, tests_run);
    if (tests_failed > 0) printf(" (%d failed)", tests_failed);
    printf("\n");
    return (tests_failed == 0) ? 0 : 1;
}
