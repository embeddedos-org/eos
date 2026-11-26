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

/* A NULL path must fail the boot, not crash it. eos_secureboot_verify_image()
 * already refused an absent public key near the end of the function, but the
 * three strncpy() calls at the top ran first, so the refusal was unreachable
 * and the call died with SIGSEGV instead. Without this case nothing in the
 * suite ever passes NULL, which is how it survived. */
TEST(test_secureboot_null_paths) {
    EosSecureBoot sb;

    eos_secureboot_init(&sb);
    ASSERT(eos_secureboot_verify_image(&sb, NULL, "sig", "key") == -1);
    ASSERT(sb.status == EOS_BOOT_FAILED);

    eos_secureboot_init(&sb);
    ASSERT(eos_secureboot_verify_image(&sb, "img", NULL, "key") == -1);
    ASSERT(sb.status == EOS_BOOT_FAILED);

    eos_secureboot_init(&sb);
    ASSERT(eos_secureboot_verify_image(&sb, "img", "sig", NULL) == -1);
    ASSERT(sb.status == EOS_BOOT_FAILED);

    ASSERT(eos_secureboot_verify_image(NULL, "img", "sig", "key") == -1);
}

TEST(test_secureboot_dump_null) {
    eos_secureboot_dump(NULL);
    ASSERT(1);
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

TEST(test_keystore_rejects_overlong_id) {
    EosKeyStore ks;
    eos_keystore_init(&ks, NULL);
    uint8_t key_data[16] = {0};
    char id[EOS_KEY_MAX_ID + 1];
    memset(id, 'k', sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';

    ASSERT(eos_keystore_add(&ks, id, EOS_KEY_AES128, key_data,
                            sizeof(key_data)) == -1);
    ASSERT(ks.count == 0);
    ASSERT(eos_keystore_find(&ks, id) == NULL);
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

/* A rule field that does not fit used to be cut short by strncpy() and the
 * rule stored anyway. eos_acl_check() compares the whole string, so the stored
 * rule no longer named what the caller wrote: an overlong DENY never matched
 * (and the caller was told it was added), and an overlong ALLOW granted the
 * shorter resource its truncated copy happened to spell. */
TEST(test_acl_rejects_overlong_fields) {
    EosAcl acl;
    EosAclRule r;
    char subject[sizeof(r.subject) + 1];
    char resource[sizeof(r.resource) + 1];
    char permission[sizeof(r.permission) + 1];

    eos_acl_init(&acl);

    /* One byte too long for each field in turn. */
    memset(subject, 's', sizeof(r.subject));
    subject[sizeof(r.subject)] = '\0';
    memset(resource, 'r', sizeof(r.resource));
    resource[sizeof(r.resource)] = '\0';
    memset(permission, 'p', sizeof(r.permission));
    permission[sizeof(r.permission)] = '\0';

    ASSERT(eos_acl_add_rule(&acl, subject, "/data", "read", EOS_ACL_DENY) == -1);
    ASSERT(eos_acl_add_rule(&acl, "user", resource, "read", EOS_ACL_DENY) == -1);
    ASSERT(eos_acl_add_rule(&acl, "user", "/data", permission, EOS_ACL_DENY) == -1);
    ASSERT(acl.count == 0);

    /* The longest value that fits is still accepted and matched in full. */
    subject[sizeof(r.subject) - 1] = '\0';
    resource[sizeof(r.resource) - 1] = '\0';
    permission[sizeof(r.permission) - 1] = '\0';
    ASSERT(eos_acl_add_rule(&acl, subject, resource, permission, EOS_ACL_ALLOW) == 0);
    ASSERT(acl.count == 1);
    ASSERT(eos_acl_check(&acl, subject, resource, permission) == EOS_ACL_ALLOW);
}

TEST(test_acl_overlong_allow_does_not_grant_prefix) {
    EosAcl acl;
    EosAclRule r;
    char longer[sizeof(r.resource) + 1];
    char prefix[sizeof(r.resource)];

    memset(longer, 'r', sizeof(r.resource));
    longer[sizeof(r.resource)] = '\0';
    memcpy(prefix, longer, sizeof(prefix) - 1);
    prefix[sizeof(prefix) - 1] = '\0';

    eos_acl_init(&acl);
    eos_acl_add_rule(&acl, "user", longer, "read", EOS_ACL_ALLOW);

    /* Nothing allowed "user" to read the shorter resource. */
    ASSERT(eos_acl_check(&acl, "user", prefix, "read") == EOS_ACL_DENY);
}

int main(void) {
    printf("=== EoS: Security Services Unit Tests ===\n\n");
    run_test_secureboot_init();
    run_test_secureboot_null_paths();
    run_test_secureboot_dump_null();
    run_test_keystore_add_find();
    run_test_keystore_rejects_overlong_id();
    run_test_keystore_not_found();
    run_test_keystore_multiple_keys();
    run_test_acl_allow_deny();
    run_test_acl_default_deny();
    run_test_acl_wildcard();
    run_test_acl_last_match_wins();
    run_test_acl_rejects_overlong_fields();
    run_test_acl_overlong_allow_does_not_grant_prefix();
    tests_run = 13;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
