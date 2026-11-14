// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
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

/* ---- Storage writer (RAM-backed stand-in for flash) ----------------------
 * apply() refuses unless every byte reached the update slot through a
 * writer, so the staged updates below install one; the tests that need
 * no writer stage manually instead. */

static uint8_t g_slot_image[2][512];
static uint32_t g_slot_image_len[2];

static int ram_writer(eos_ota_slot_t slot, uint32_t offset, const uint8_t *data,
                      size_t len, void *ctx) {
    (void)ctx;
    if (slot > EOS_OTA_SLOT_B || offset + len > sizeof(g_slot_image[0])) return -1;
    memcpy(&g_slot_image[slot][offset], data, len);
    if (offset + (uint32_t)len > g_slot_image_len[slot])
        g_slot_image_len[slot] = offset + (uint32_t)len;
    return 0;
}

static void install_ram_writer(void) {
    memset(g_slot_image, 0, sizeof(g_slot_image));
    memset(g_slot_image_len, 0, sizeof(g_slot_image_len));
    assert(eos_ota_set_storage_writer(ram_writer, NULL) == 0);
}

/* Fails every chunk after the first: the download stalls mid-image. */
static int failing_writer(eos_ota_slot_t slot, uint32_t offset, const uint8_t *data,
                          size_t len, void *ctx) {
    (void)ctx;
    if (offset > 0) return -1;
    return ram_writer(slot, offset, data, len, NULL);
}

/* Download an image end to end, stopping just before apply(). */
static void stage_update(const uint8_t *fw, size_t len, const uint8_t hash[32],
                         uint32_t version_code) {
    install_ram_writer();
    eos_ota_source_t src;
    memset(&src, 0, sizeof(src));
    src.expected_size = (uint32_t)len;
    src.version_code = version_code;
    snprintf(src.version, sizeof(src.version), "v%u", version_code);
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
    install_ram_writer();
    eos_ota_source_t src; memset(&src, 0, sizeof(src));
    src.expected_size = sizeof(fw);
    src.version_code = 2;
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
    /* The bytes actually landed in the slot, not just in the hash. */
    assert(g_slot_image_len[EOS_OTA_SLOT_B] == sizeof(fw));
    assert(memcmp(g_slot_image[EOS_OTA_SLOT_B], fw, sizeof(fw)) == 0);
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
    install_ram_writer();
    eos_ota_source_t src; memset(&src, 0, sizeof(src));
    src.expected_size = 8;
    src.version_code = 2;
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

    stage_update(fw, sizeof(fw), hash, 2);

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
    stage_update(fw, sizeof(fw), hash, 2);
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
    stage_update(fw, sizeof(fw), hash, 2);
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

    stage_update(fw, sizeof(fw), hash, 2);
    assert(eos_ota_apply() == -1);          /* no authenticator */

    assert(eos_ota_rollback() == -1);
    assert(eos_ota_set_active_slot(EOS_OTA_SLOT_B) == -1);
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_A);
    eos_ota_deinit();
    printf("[PASS] ota unapplied slot is not a rollback target\n");
}

/* After one A->B apply, slot A is the update target for the next begin().
 * It stayed marked valid while being overwritten, so a rollback mid-download
 * switched to a half-written image. */
static void test_ota_slot_being_written_is_not_a_rollback_target(void) {
    eos_ota_init();
    uint8_t fw[16]; memset(fw, 0x55, sizeof(fw));
    uint8_t hash[32]; sha256_of(fw, sizeof(fw), hash);

    stage_update(fw, sizeof(fw), hash, 2);
    eos_ota_set_authenticator(auth_accept, NULL);
    assert(eos_ota_apply() == 0);
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_B);

    eos_ota_source_t src; memset(&src, 0, sizeof(src));
    src.expected_size = sizeof(fw);
    memcpy(src.expected_sha256, hash, 32);
    assert(eos_ota_begin(&src) == 0);          /* targets slot A */
    assert(eos_ota_write_chunk(fw, 4) == 0);   /* partial */

    assert(eos_ota_rollback() == -1);
    assert(eos_ota_set_active_slot(EOS_OTA_SLOT_A) == -1);
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_B);
    eos_ota_abort();
    eos_ota_set_authenticator(NULL, NULL);
    eos_ota_set_storage_writer(NULL, NULL);
    eos_ota_deinit();
    printf("[PASS] ota slot being written is not a rollback target\n");
}

static void test_ota_write_chunk_overflow(void) {
    eos_ota_init();
    uint8_t fw[64];
    memset(fw, 0xAA, sizeof(fw));
    eos_ota_source_t src;
    memset(&src, 0, sizeof(src));
    src.expected_size = sizeof(fw);
    assert(eos_ota_begin(&src) == 0);
    assert(eos_ota_write_chunk(fw, 32) == 0);
    assert(eos_ota_write_chunk(fw, 33) == -1);

    eos_ota_status_t st;
    assert(eos_ota_get_status(&st) == 0);
    assert(st.bytes_received == 32);
    eos_ota_deinit();
    printf("[PASS] ota write chunk overflow\n");
}

static void test_ota_write_chunk_wrap(void) {
    eos_ota_init();
    uint8_t fw[64];
    memset(fw, 0xAA, sizeof(fw));
    eos_ota_source_t src;
    memset(&src, 0, sizeof(src));
    src.expected_size = sizeof(fw);
    assert(eos_ota_begin(&src) == 0);
    /* One accepted byte so bytes_written + SIZE_MAX wraps on the old add. */
    assert(eos_ota_write_chunk(fw, 1) == 0);
    assert(eos_ota_write_chunk(fw, SIZE_MAX) == -1);

    eos_ota_status_t st;
    assert(eos_ota_get_status(&st) == 0);
    assert(st.bytes_received == 1);
    eos_ota_deinit();
    printf("[PASS] ota write chunk wrap\n");
}

/* bytes_written * 100 was computed in uint32_t, which wraps once
 * bytes_written passes UINT32_MAX / 100 (~42.9 MB). Halfway through a
 * 100,000,000-byte image the old arithmetic gave
 * (5,000,000,000 mod 2^32) / 100,000,000 = 7 instead of 50. */
static void test_ota_progress_large_image(void) {
    static uint8_t chunk[1000000];
    eos_ota_init();
    memset(chunk, 0x5A, sizeof(chunk));
    eos_ota_source_t src;
    memset(&src, 0, sizeof(src));
    src.expected_size = 100000000u;
    g_progress = -1;
    assert(eos_ota_set_progress_callback(progress_cb, NULL) == 0);
    assert(eos_ota_begin(&src) == 0);
    for (int i = 0; i < 50; i++)
        assert(eos_ota_write_chunk(chunk, sizeof(chunk)) == 0);

    assert(g_progress == 50);
    eos_ota_status_t st;
    assert(eos_ota_get_status(&st) == 0);
    assert(st.bytes_received == 50000000u);
    assert(st.progress_pct == 50);

    eos_ota_set_progress_callback(NULL, NULL);
    eos_ota_deinit();
    printf("[PASS] ota progress large image\n");
}

/* mark_slot_valid() is the trial-boot confirmation, not a back door: only a
 * slot holding a verified image can be marked valid. */
static void test_ota_mark_slot_valid_refuses_unverified_slot(void) {
    eos_ota_init();
    uint8_t fw[16]; memset(fw, 0x77, sizeof(fw));
    uint8_t hash[32]; sha256_of(fw, sizeof(fw), hash);

    assert(eos_ota_mark_slot_valid(EOS_OTA_SLOT_B) == -1);  /* never verified */
    assert(eos_ota_mark_slot_valid(EOS_OTA_SLOT_A) == 0);   /* the booted image */

    stage_update(fw, sizeof(fw), hash, 2);
    assert(eos_ota_mark_slot_valid(EOS_OTA_SLOT_B) == -1);  /* verified, not applied */
    assert(eos_ota_set_active_slot(EOS_OTA_SLOT_B) == -1);
    assert(eos_ota_rollback() == -1);

    assert(eos_ota_set_authenticator(auth_accept, NULL) == 0);
    assert(eos_ota_apply() == 0);
    assert(eos_ota_mark_slot_valid(EOS_OTA_SLOT_B) == 0);   /* now installed */

    /* A failed verify leaves the target slot unverified even though it was
     * valid before the new download began. */
    eos_ota_source_t src; memset(&src, 0, sizeof(src));
    src.expected_size = sizeof(fw);
    src.version_code = 3;
    memset(src.expected_sha256, 0, 32);                     /* wrong digest */
    assert(eos_ota_begin(&src) == 0);                       /* targets slot A */
    assert(eos_ota_write_chunk(fw, sizeof(fw)) == 0);
    assert(eos_ota_finish() == 0);
    assert(eos_ota_verify() == -1);
    assert(eos_ota_mark_slot_valid(EOS_OTA_SLOT_A) == -1);
    assert(eos_ota_rollback() == -1);

    eos_ota_set_authenticator(NULL, NULL);
    eos_ota_set_storage_writer(NULL, NULL);
    eos_ota_deinit();
    printf("[PASS] ota mark slot valid refuses unverified slot\n");
}

/* Hashing bytes is not installing them: without a storage writer no byte
 * reaches the update slot, and apply() must refuse even a fully verified,
 * authenticated image. Installing a writer afterwards does not rescue the
 * download -- it has to be staged again through the writer. */
static void test_ota_apply_refuses_unpersisted_chunks(void) {
    eos_ota_init();
    uint8_t fw[32]; memset(fw, 0xDD, sizeof(fw));
    uint8_t hash[32]; sha256_of(fw, sizeof(fw), hash);

    g_auth_calls = 0;
    assert(eos_ota_set_authenticator(auth_accept, NULL) == 0);

    eos_ota_source_t src; memset(&src, 0, sizeof(src));
    src.expected_size = sizeof(fw);
    src.version_code = 2;
    memcpy(src.expected_sha256, hash, 32);
    assert(eos_ota_begin(&src) == 0);
    assert(eos_ota_write_chunk(fw, sizeof(fw)) == 0);
    assert(eos_ota_finish() == 0);
    assert(eos_ota_verify() == 0);

    assert(eos_ota_apply() == -1);          /* nothing was persisted */
    assert(g_auth_calls == 0);              /* not even consulted */
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_A);

    install_ram_writer();
    assert(eos_ota_apply() == -1);          /* writer installed late: no rescue */
    assert(g_auth_calls == 0);

    stage_update(fw, sizeof(fw), hash, 2);
    assert(eos_ota_apply() == 0);
    assert(g_auth_calls == 1);
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_B);
    assert(g_slot_image_len[EOS_OTA_SLOT_B] == sizeof(fw));
    assert(memcmp(g_slot_image[EOS_OTA_SLOT_B], fw, sizeof(fw)) == 0);

    eos_ota_set_authenticator(NULL, NULL);
    eos_ota_set_storage_writer(NULL, NULL);
    eos_ota_deinit();
    printf("[PASS] ota apply refuses unpersisted chunks\n");
}

/* A writer that fails mid-download stalls the image: the failed chunk is
 * neither hashed nor counted, so verify and apply both refuse. */
static void test_ota_writer_failure_blocks_apply(void) {
    eos_ota_init();
    uint8_t fw[32]; memset(fw, 0x5A, sizeof(fw));
    uint8_t hash[32]; sha256_of(fw, sizeof(fw), hash);

    assert(eos_ota_set_storage_writer(failing_writer, NULL) == 0);
    eos_ota_source_t src; memset(&src, 0, sizeof(src));
    src.expected_size = sizeof(fw);
    src.version_code = 2;
    memcpy(src.expected_sha256, hash, 32);
    assert(eos_ota_begin(&src) == 0);
    assert(eos_ota_write_chunk(fw, 16) == 0);
    assert(eos_ota_write_chunk(fw + 16, 16) == -1);   /* writer refuses */

    eos_ota_status_t st;
    assert(eos_ota_get_status(&st) == 0);
    assert(st.bytes_received == 16);

    assert(eos_ota_finish() == 0);
    assert(eos_ota_verify() == -1);
    assert(eos_ota_set_authenticator(auth_accept, NULL) == 0);
    assert(eos_ota_apply() == -1);
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_A);

    eos_ota_set_authenticator(NULL, NULL);
    eos_ota_set_storage_writer(NULL, NULL);
    eos_ota_deinit();
    printf("[PASS] ota writer failure blocks apply\n");
}

/* The anti-rollback floor: once provisioned (from eBoot's monotonic counter,
 * PR-EBT-2), apply() refuses anything below it and anything unversioned, and
 * the floor rises to each applied version. The floor itself never lowers. */
static void test_ota_rollback_floor(void) {
    eos_ota_init();
    uint8_t fw[16]; memset(fw, 0xA5, sizeof(fw));
    uint8_t hash[32]; sha256_of(fw, sizeof(fw), hash);

    assert(eos_ota_set_rollback_floor(5) == 0);
    assert(eos_ota_set_authenticator(auth_accept, NULL) == 0);

    stage_update(fw, sizeof(fw), hash, 3);      /* below the floor */
    assert(eos_ota_apply() == -1);
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_A);

    stage_update(fw, sizeof(fw), hash, 5);      /* at the floor */
    assert(eos_ota_apply() == 0);
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_B);
    eos_ota_status_t st;
    assert(eos_ota_get_status(&st) == 0);
    assert(st.version_code == 5);
    assert(st.rollback_floor == 5);
    assert(strcmp(st.current_version, "v5") == 0);

    stage_update(fw, sizeof(fw), hash, 6);      /* newer: floor rises */
    assert(eos_ota_apply() == 0);
    assert(eos_ota_get_status(&st) == 0);
    assert(st.version_code == 6);
    assert(st.rollback_floor == 6);

    stage_update(fw, sizeof(fw), hash, 5);      /* older than the new floor */
    assert(eos_ota_apply() == -1);

    stage_update(fw, sizeof(fw), hash, 0);      /* unversioned vs a set floor */
    assert(eos_ota_apply() == -1);

    assert(eos_ota_set_rollback_floor(3) == -1); /* the floor never lowers */
    assert(eos_ota_get_status(&st) == 0);
    assert(st.rollback_floor == 6);

    eos_ota_set_authenticator(NULL, NULL);
    eos_ota_set_storage_writer(NULL, NULL);
    eos_ota_deinit();
    printf("[PASS] ota rollback floor\n");
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
    test_ota_slot_being_written_is_not_a_rollback_target();
    test_ota_write_chunk_overflow();
    test_ota_write_chunk_wrap();
    test_ota_progress_large_image();
    test_ota_mark_slot_valid_refuses_unverified_slot();
    test_ota_apply_refuses_unpersisted_chunks();
    test_ota_writer_failure_blocks_apply();
    test_ota_rollback_floor();
    printf("=== ALL OTA TESTS PASSED (17/17) ===\n");
    return 0;
}