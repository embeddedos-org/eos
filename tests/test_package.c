// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eos/package.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

static int dummy_init(void) { return 0; }

/* sizeof(EosPackageSet) is 1,357,856 bytes (measured, host LP64, after
 * #148 Step 2b interned the per-package options arrays into the set's
 * string table; it was 3,979,296 before). Still far too large for a
 * plain stack local. cmd/eos/main.c stores it static for the same
 * reason. */
static void test_package_register(void) {
    static EosPackageSet set;
    memset(&set, 0, sizeof(set));
    EosPackage pkg;
    memset(&pkg, 0, sizeof(pkg));
    strncpy(pkg.name, "libfoo", EOS_MAX_NAME - 1);
    strncpy(pkg.version, "1.0.0", EOS_MAX_NAME - 1);
    pkg.build_type = EOS_BUILD_CMAKE;
    pkg.init_fn = dummy_init;
    assert(eos_package_register(&set, &pkg) == 0);
    assert(set.count == 1);
    PASS("package register");
}

static void test_package_find(void) {
    static EosPackageSet set;
    memset(&set, 0, sizeof(set));
    EosPackage pkg;
    memset(&pkg, 0, sizeof(pkg));
    strncpy(pkg.name, "libbar", EOS_MAX_NAME - 1);
    strncpy(pkg.version, "2.0.0", EOS_MAX_NAME - 1);
    pkg.build_type = EOS_BUILD_MAKE;
    eos_package_register(&set, &pkg);
    const EosPackage *found = eos_package_find(&set, "libbar");
    assert(found != NULL);
    assert(strcmp(found->version, "2.0.0") == 0);
    PASS("package find");
}

static void test_package_find_not_found(void) {
    static EosPackageSet set;
    memset(&set, 0, sizeof(set));
    const EosPackage *found = eos_package_find(&set, "nonexistent");
    assert(found == NULL);
    PASS("package find not found");
}

static void test_package_find_null(void) {
    const EosPackage *found = eos_package_find(NULL, "test");
    assert(found == NULL);
    PASS("package find null set");
}

static void test_package_register_multiple(void) {
    static EosPackageSet set;
    memset(&set, 0, sizeof(set));
    EosPackage p1, p2, p3;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    memset(&p3, 0, sizeof(p3));
    strncpy(p1.name, "a", EOS_MAX_NAME - 1);
    strncpy(p1.version, "1.0", EOS_MAX_NAME - 1);
    strncpy(p2.name, "b", EOS_MAX_NAME - 1);
    strncpy(p2.version, "2.0", EOS_MAX_NAME - 1);
    strncpy(p3.name, "c", EOS_MAX_NAME - 1);
    strncpy(p3.version, "3.0", EOS_MAX_NAME - 1);
    eos_package_register(&set, &p1);
    eos_package_register(&set, &p2);
    eos_package_register(&set, &p3);
    assert(set.count == 3);
    assert(eos_package_find(&set, "a") != NULL);
    assert(eos_package_find(&set, "b") != NULL);
    assert(eos_package_find(&set, "c") != NULL);
    PASS("package register multiple");
}

static void test_package_init_all(void) {
    static EosPackageSet set;
    memset(&set, 0, sizeof(set));
    EosPackage pkg;
    memset(&pkg, 0, sizeof(pkg));
    strncpy(pkg.name, "init_test", EOS_MAX_NAME - 1);
    strncpy(pkg.version, "1.0", EOS_MAX_NAME - 1);
    pkg.init_fn = dummy_init;
    eos_package_register(&set, &pkg);
    int r = eos_package_init_all(&set);
    assert(r == 0);
    PASS("package init all");
}

static void test_package_build_type(void) {
    static EosPackageSet set;
    memset(&set, 0, sizeof(set));
    EosPackage pkg;
    memset(&pkg, 0, sizeof(pkg));
    strncpy(pkg.name, "cmake_pkg", EOS_MAX_NAME - 1);
    strncpy(pkg.version, "1.0", EOS_MAX_NAME - 1);
    pkg.build_type = EOS_BUILD_CMAKE;
    eos_package_register(&set, &pkg);
    const EosPackage *found = eos_package_find(&set, "cmake_pkg");
    assert(found != NULL);
    assert(found->build_type == EOS_BUILD_CMAKE);
    PASS("package build type");
}


/* #148 Step 2b: conversion interns the config's options into the
 * set-owned string table; they read back through the pkg accessors,
 * and a second conversion into the same live set disposes the first
 * table instead of orphaning it. */
static void test_package_from_config_carries_options(void) {
    static EosPackageSet set;
    static EosConfig cfg;

    FILE *fp = fopen("test_pkg_convert.yaml", "w");
    assert(fp != NULL);
    fprintf(fp, "project:\n  name: convert-project\n  version: 1.0.0\n\n");
    fprintf(fp, "packages:\n");
    fprintf(fp, "  - name: alpha\n    version: 1.0.0\n");
    fprintf(fp, "    build:\n      type: cmake\n");
    fprintf(fp, "    options:\n      opt0: val0\n      opt1: val1\n");
    fprintf(fp, "  - name: beta\n    version: 2.0.0\n");
    fprintf(fp, "    build:\n      type: make\n");
    fclose(fp);

    memset(&cfg, 0, sizeof(cfg));
    assert(eos_config_load(&cfg, "test_pkg_convert.yaml") == EOS_OK);
    assert(cfg.package_count == 2);

    memset(&set, 0, sizeof(set));
    assert(eos_package_set_from_config(&set, &cfg) == EOS_OK);
    assert(set.count == 2);
    assert(eos_pkg_option_count(&set.packages[0]) == 2);
    const char *v0 = eos_pkg_option_get(&set.packages[0], "opt0");
    const char *v1 = eos_pkg_option_get(&set.packages[0], "opt1");
    assert(v0 && strcmp(v0, "val0") == 0);
    assert(v1 && strcmp(v1, "val1") == 0);
    assert(eos_pkg_option_count(&set.packages[1]) == 0);
    assert(eos_pkg_option_get(&set.packages[1], "opt0") == NULL);

    /* Second conversion into the live set: the old table is disposed,
     * not orphaned, and the new contents are what read back. */
    assert(eos_package_set_from_config(&set, &cfg) == EOS_OK);
    assert(set.count == 2);
    v0 = eos_pkg_option_get(&set.packages[0], "opt0");
    assert(v0 && strcmp(v0, "val0") == 0);

    eos_package_set_dispose(&set);
    assert(set.options_table == NULL);
    assert(eos_pkg_option_count(&set.packages[0]) == 0);
    eos_config_dispose(&cfg);
    remove("test_pkg_convert.yaml");
    PASS("package from_config carries options (set-owned table)");
}

/* #148 Step 2b: the standalone writer fills its table to the
 * EOS_MAX_OPTIONS cap and the next add fails with EOS_ERR_OVERFLOW
 * \u2014 the package-side exhaustion assertion (the loader side was
 * Step 2a's, in test_config.c). */
static void test_pkg_option_add_cap(void) {
    EosPackage pkg;
    memset(&pkg, 0, sizeof(pkg));
    char key[EOS_MAX_NAME], value[EOS_MAX_NAME];
    for (int i = 0; i < EOS_MAX_OPTIONS; i++) {
        snprintf(key, sizeof(key), "k%d", i);
        snprintf(value, sizeof(value), "v%d", i);
        assert(eos_pkg_option_add(&pkg, key, value) == EOS_OK);
    }
    assert(eos_pkg_option_count(&pkg) == EOS_MAX_OPTIONS);
    assert(eos_pkg_option_add(&pkg, "one-too-many", "x") == EOS_ERR_OVERFLOW);
    assert(eos_pkg_option_count(&pkg) == EOS_MAX_OPTIONS);
    const char *last = eos_pkg_option_get(&pkg, "k31");
    assert(last && strcmp(last, "v31") == 0);
    /* A borrowed (set-owned) table refuses the writer. */
    static EosPackageSet set;
    memset(&set, 0, sizeof(set));
    set.packages[0].options_table = "borrowed";
    assert(eos_pkg_option_add(&set.packages[0], "k", "v") == EOS_ERR_INVALID);
    eos_pkg_options_dispose(&pkg);
    assert(eos_pkg_option_count(&pkg) == 0);
    assert(pkg.options_table == NULL);
    PASS("pkg option_add cap and ownership rules");
}

/* #148 Step 2b size guard: the set must stay within the measured
 * post-2b size. Ratchet down only, in the commit that shrinks it
 * (the test_config_size.c rule, applied to EosPackageSet). */
static void test_package_set_size_guard(void) {
    assert(sizeof(EosPackageSet) <= 1360000u);
    PASS("package set size guard");
}

int main(void) {
    printf("=== EoS Package Tests ===\n");
    test_package_register();
    test_package_find();
    test_package_find_not_found();
    test_package_find_null();
    test_package_register_multiple();
    test_package_init_all();
    test_package_build_type();
    test_package_from_config_carries_options();
    test_pkg_option_add_cap();
    test_package_set_size_guard();
    printf("\n=== ALL %d PACKAGE TESTS PASSED ===\n", passed);
    return 0;
}
