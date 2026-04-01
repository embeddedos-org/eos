// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eos/package.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

static int dummy_init(void) { return 0; }

static void test_package_register(void) {
    EosPackageSet set;
    memset(&set, 0, sizeof(set));
    EosPackage pkg;
    memset(&pkg, 0, sizeof(pkg));
    pkg.name = "libfoo";
    pkg.version = "1.0.0";
    pkg.build_type = EOS_BUILD_CMAKE;
    pkg.init_fn = dummy_init;
    assert(eos_package_register(&set, &pkg) == 0);
    assert(set.count == 1);
    PASS("package register");
}

static void test_package_find(void) {
    EosPackageSet set;
    memset(&set, 0, sizeof(set));
    EosPackage pkg;
    memset(&pkg, 0, sizeof(pkg));
    pkg.name = "libbar";
    pkg.version = "2.0.0";
    pkg.build_type = EOS_BUILD_MAKE;
    eos_package_register(&set, &pkg);
    const EosPackage *found = eos_package_find(&set, "libbar");
    assert(found != NULL);
    assert(strcmp(found->version, "2.0.0") == 0);
    PASS("package find");
}

static void test_package_find_not_found(void) {
    EosPackageSet set;
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
    EosPackageSet set;
    memset(&set, 0, sizeof(set));
    EosPackage p1 = {0}; p1.name = "a"; p1.version = "1.0";
    EosPackage p2 = {0}; p2.name = "b"; p2.version = "2.0";
    EosPackage p3 = {0}; p3.name = "c"; p3.version = "3.0";
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
    EosPackageSet set;
    memset(&set, 0, sizeof(set));
    EosPackage pkg = {0};
    pkg.name = "init_test";
    pkg.version = "1.0";
    pkg.init_fn = dummy_init;
    eos_package_register(&set, &pkg);
    int r = eos_package_init_all(&set);
    assert(r == 0);
    PASS("package init all");
}

static void test_package_build_type(void) {
    EosPackageSet set;
    memset(&set, 0, sizeof(set));
    EosPackage pkg = {0};
    pkg.name = "cmake_pkg";
    pkg.version = "1.0";
    pkg.build_type = EOS_BUILD_CMAKE;
    eos_package_register(&set, &pkg);
    const EosPackage *found = eos_package_find(&set, "cmake_pkg");
    assert(found != NULL);
    assert(found->build_type == EOS_BUILD_CMAKE);
    PASS("package build type");
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
    printf("\n=== ALL %d PACKAGE TESTS PASSED ===\n", passed);
    return 0;
}
