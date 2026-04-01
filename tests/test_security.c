// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_security.c
 * @brief Unit tests for EoS security services (keystore, ACL)
 */

#include "eos/security.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        printf("  %-50s ", #name); \
        name(); \
        tests_passed++; \
        printf("[PASS]\n"); \
    } \
    static void name(void)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while(0)

TEST(test_secureboot_init) {
    EosSecureBoot sb;
    eos_secureboot_init(&sb);
    ASSERT(sb.status == EOS_BOOT_UNVERIFIED);
    ASSERT(strcmp(sb.hash_algo, "sha256") == 0);
}

TEST(test_keystore_add_find) {
    EosKeyStore ks;
    eos_keystore_init(&ks, NULL);

    uint8_t key_data[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    ASSERT(eos_keystore_add(&ks, "test-key", EOS_KEY_AES128, key_data, 16) == 0);

    const EosKeyEntry *found = eos_keystore_find(&ks, "test-key");
    ASSERT(found != NULL);
    ASSERT(found->type == EOS_KEY_AES128);
    ASSERT(found->data_len == 16);
    ASSERT(memcmp(found->data, key_data, 16) == 0);
    ASSERT(found->active == 1);
}

TEST(test_keystore_not_found) {
    EosKeyStore ks;
    eos_keystore_init(&ks, NULL);
    ASSERT(eos_keystore_find(&ks, "nope") == NULL);
}

TEST(test_keystore_multiple_keys) {
    EosKeyStore ks;
    eos_keystore_init(&ks, NULL);

    uint8_t k1[16] = {0}, k2[32] = {0};
    eos_keystore_add(&ks, "aes-key", EOS_KEY_AES128, k1, 16);
    eos_keystore_add(&ks, "aes256-key", EOS_KEY_AES256, k2, 32);

    ASSERT(ks.count == 2);
    ASSERT(eos_keystore_find(&ks, "aes-key") != NULL);
    ASSERT(eos_keystore_find(&ks, "aes256-key") != NULL);
    ASSERT(eos_keystore_find(&ks, "aes-key")->type == EOS_KEY_AES128);
    ASSERT(eos_keystore_find(&ks, "aes256-key")->type == EOS_KEY_AES256);
}

TEST(test_acl_allow_deny) {
    EosAcl acl;
    eos_acl_init(&acl);

    eos_acl_add_rule(&acl, "admin", "*", "*", EOS_ACL_ALLOW);
    eos_acl_add_rule(&acl, "guest", "/etc/shadow", "read", EOS_ACL_DENY);
    eos_acl_add_rule(&acl, "guest", "/tmp", "*", EOS_ACL_ALLOW);

    ASSERT(eos_acl_check(&acl, "admin", "/etc/passwd", "read") == EOS_ACL_ALLOW);
    ASSERT(eos_acl_check(&acl, "guest", "/etc/shadow", "read") == EOS_ACL_DENY);
    ASSERT(eos_acl_check(&acl, "guest", "/tmp", "write") == EOS_ACL_ALLOW);
}

TEST(test_acl_default_deny) {
    EosAcl acl;
    eos_acl_init(&acl);
    /* No rules — should deny */
    ASSERT(eos_acl_check(&acl, "user", "/data", "read") == EOS_ACL_DENY);
}

TEST(test_acl_wildcard) {
    EosAcl acl;
    eos_acl_init(&acl);
    eos_acl_add_rule(&acl, "*", "*", "*", EOS_ACL_ALLOW);
    ASSERT(eos_acl_check(&acl, "anyone", "/anything", "execute") == EOS_ACL_ALLOW);
}

TEST(test_acl_last_match_wins) {
    EosAcl acl;
    eos_acl_init(&acl);
    eos_acl_add_rule(&acl, "user", "/data", "read", EOS_ACL_ALLOW);
    eos_acl_add_rule(&acl, "user", "/data", "read", EOS_ACL_DENY);
    /* Last matching rule wins */
    ASSERT(eos_acl_check(&acl, "user", "/data", "read") == EOS_ACL_DENY);
}

int main(void) {
    printf("=== EoS: Security Services Unit Tests ===\n\n");
    run_test_secureboot_init();
    run_test_keystore_add_find();
    run_test_keystore_not_found();
    run_test_keystore_multiple_keys();
    run_test_acl_allow_deny();
    run_test_acl_default_deny();
    run_test_acl_wildcard();
    run_test_acl_last_match_wins();
    tests_run = 8;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
