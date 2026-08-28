// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eos/ota.h"
#include "eos/crypto.h"

static int g_progress = 0;
static void progress_cb(uint8_t pct, void *ctx) { g_progress = pct; (void)ctx; }


/* ---- Authenticator stubs -------------------------------------------------
 * The platform installs one of these; the OTA service must not install a
 * default that says yes. The signature bytes are opaque here -- what matters
 * is that apply() consults the callback and honours its answer. */

static int g_auth_calls;
static uint8_t g_auth_digest[32];

static int auth_accept(const uint8_t digest[32], const uint8_t *sig,
                       size_t sig_len, void *ctx) {
    (void)sig; (void)sig_len; (void)ctx;
    g_auth_calls++;
    memcpy(g_auth_digest, digest, 32);
    return 0;
}

static int auth_reject(const uint8_t digest[32], const uint8_t *sig,
                       size_t sig_len, void *ctx) {
    (void)digest; (void)sig; (void)sig_len; (void)ctx;
    g_auth_calls++;
    return -1;
}

static void sha256_of(const void *data, size_t len, uint8_t out[32]) {
    EosSha256 ctx;
    eos_sha256_init(&ctx);
    eos_sha256_update(&ctx, data, len);
    eos_sha256_final(&ctx, out);
}

/* Download an image end to end, stopping just before apply(). */
static void stage_update(const uint8_t *fw, size_t len, const uint8_t hash[32]) {
    eos_ota_source_t src;
    memset(&src, 0, sizeof(src));
    src.expected_size = (uint32_t)len;
    memcpy(src.expected_sha256, hash, 32);
    assert(eos_ota_begin(&src) == 0);
    assert(eos_ota_write_chunk(fw, len) == 0);
    assert(eos_ota_finish() == 0);
    assert(eos_ota_verify() == 0);
}

static void test_ota_init(void) {
    assert(eos_ota_init() == 0);
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_A);
    eos_ota_deinit();
    printf("[PASS] ota init\n");
}

static void test_ota_full_update(void) {
    eos_ota_init();
    uint8_t fw[64]; memset(fw, 0xAA, sizeof(fw));
    uint8_t hash[32]; EosSha256 _hctx; eos_sha256_init(&_hctx); eos_sha256_update(&_hctx, fw, sizeof(fw)); eos_sha256_final(&_hctx, hash);
    eos_ota_source_t src; memset(&src, 0, sizeof(src));
    src.expected_size = sizeof(fw);
    memcpy(src.expected_sha256, hash, 32);
    snprintf(src.version, sizeof(src.version), "2.0");
    bool avail = false;
    assert(eos_ota_check_update(&src, &avail) == 0);
    assert(avail);
    assert(eos_ota_begin(&src) == 0);
    eos_ota_set_progress_callback(progress_cb, NULL);
    assert(eos_ota_write_chunk(fw, 32) == 0);
    assert(eos_ota_write_chunk(fw + 32, 32) == 0);
    assert(eos_ota_finish() == 0);
    assert(eos_ota_verify() == 0);
    assert(eos_ota_set_authenticator(auth_accept, NULL) == 0);
    assert(eos_ota_apply() == 0);
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_B);
    eos_ota_deinit();
    printf("[PASS] ota full update\n");
}

static void test_ota_verify_fail(void) {
    eos_ota_init();
    uint8_t fw[16]; memset(fw, 0xBB, 16);
    eos_ota_source_t src; memset(&src, 0, sizeof(src));
    src.expected_size = 16;
    memset(src.expected_sha256, 0, 32);
    eos_ota_begin(&src);
    eos_ota_write_chunk(fw, 16);
    eos_ota_finish();
    assert(eos_ota_verify() == -1);
    eos_ota_deinit();
    printf("[PASS] ota verify fail\n");
}

static void test_ota_rollback(void) {
    eos_ota_init();
    uint8_t fw[8]; memset(fw, 0xCC, 8);
    uint8_t hash[32]; EosSha256 _hctx2; eos_sha256_init(&_hctx2); eos_sha256_update(&_hctx2, fw, 8); eos_sha256_final(&_hctx2, hash);
    eos_ota_source_t src; memset(&src, 0, sizeof(src));
    src.expected_size = 8;
    memcpy(src.expected_sha256, hash, 32);
    eos_ota_begin(&src);
    eos_ota_write_chunk(fw, 8);
    eos_ota_finish();
    eos_ota_verify();
    eos_ota_set_authenticator(auth_accept, NULL);
    assert(eos_ota_apply() == 0);
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_B);
    assert(eos_ota_rollback() == 0);
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_A);
    eos_ota_deinit();
    printf("[PASS] ota rollback\n");
}

static void test_ota_status(void) {
    eos_ota_init();
    eos_ota_status_t st;
    (void)st;
    assert(eos_ota_get_status(&st) == 0);
    assert(st.active_slot == EOS_OTA_SLOT_A);
    assert(eos_ota_get_status(NULL) == -1);
    eos_ota_deinit();
    printf("[PASS] ota status\n");
}


/* expected_sha256 arrives with the update, so an attacker who swaps the image
 * swaps the digest too. Passing eos_ota_verify() must not be enough to install. */
static void test_ota_apply_refuses_without_authenticator(void) {
    eos_ota_init();
    uint8_t fw[32]; memset(fw, 0xDD, sizeof(fw));
    uint8_t hash[32]; sha256_of(fw, sizeof(fw), hash);

    stage_update(fw, sizeof(fw), hash);

    assert(eos_ota_apply() == -1);
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_A);
    eos_ota_deinit();
    printf("[PASS] ota apply refuses without authenticator\n");
}

static void test_ota_apply_honours_a_rejecting_authenticator(void) {
    eos_ota_init();
    uint8_t fw[32]; memset(fw, 0xEE, sizeof(fw));
    uint8_t hash[32]; sha256_of(fw, sizeof(fw), hash);

    g_auth_calls = 0;
    stage_update(fw, sizeof(fw), hash);
    assert(eos_ota_set_authenticator(auth_reject, NULL) == 0);

    assert(eos_ota_apply() == -1);
    assert(g_auth_calls == 1);
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_A);
    eos_ota_deinit();
    printf("[PASS] ota apply honours a rejecting authenticator\n");
}

/* The authenticator has to see the digest the service computed over the bytes
 * it received, not the one the update declared -- otherwise the signature
 * would cover an attacker-chosen value. */
static void test_ota_authenticator_sees_the_computed_digest(void) {
    eos_ota_init();
    uint8_t fw[48]; memset(fw, 0x5A, sizeof(fw));
    uint8_t hash[32]; sha256_of(fw, sizeof(fw), hash);

    g_auth_calls = 0;
    memset(g_auth_digest, 0, sizeof(g_auth_digest));
    stage_update(fw, sizeof(fw), hash);
    assert(eos_ota_set_authenticator(auth_accept, NULL) == 0);
    assert(eos_ota_apply() == 0);

    assert(g_auth_calls == 1);
    assert(memcmp(g_auth_digest, hash, 32) == 0);
    eos_ota_deinit();
    printf("[PASS] ota authenticator sees the computed digest\n");
}

/* A slot that was downloaded and hashed but never applied still holds a
 * half-installed image. Marking it valid at verify() time made it a legal
 * rollback target. */
static void test_ota_unapplied_slot_is_not_a_rollback_target(void) {
    eos_ota_init();
    uint8_t fw[16]; memset(fw, 0x77, sizeof(fw));
    uint8_t hash[32]; sha256_of(fw, sizeof(fw), hash);

    stage_update(fw, sizeof(fw), hash);
    assert(eos_ota_apply() == -1);          /* no authenticator */

    assert(eos_ota_rollback() == -1);
    assert(eos_ota_set_active_slot(EOS_OTA_SLOT_B) == -1);
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_A);
    eos_ota_deinit();
    printf("[PASS] ota unapplied slot is not a rollback target\n");
}

int main(void) {
    printf("=== EoS OTA Tests ===\n");
    test_ota_init();
    test_ota_full_update();
    test_ota_verify_fail();
    test_ota_rollback();
    test_ota_status();
    test_ota_apply_refuses_without_authenticator();
    test_ota_apply_honours_a_rejecting_authenticator();
    test_ota_authenticator_sees_the_computed_digest();
    test_ota_unapplied_slot_is_not_a_rollback_target();
    printf("=== ALL OTA TESTS PASSED (9/9) ===\n");
    return 0;
}