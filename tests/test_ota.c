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
    strcpy(src.version, "2.0");
    bool avail = false;
    assert(eos_ota_check_update(&src, &avail) == 0);
    assert(avail);
    assert(eos_ota_begin(&src) == 0);
    eos_ota_set_progress_callback(progress_cb, NULL);
    assert(eos_ota_write_chunk(fw, 32) == 0);
    assert(eos_ota_write_chunk(fw + 32, 32) == 0);
    assert(eos_ota_finish() == 0);
    assert(eos_ota_verify() == 0);
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
    eos_ota_apply();
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_B);
    assert(eos_ota_rollback() == 0);
    assert(eos_ota_get_active_slot() == EOS_OTA_SLOT_A);
    eos_ota_deinit();
    printf("[PASS] ota rollback\n");
}

static void test_ota_status(void) {
    eos_ota_init();
    eos_ota_status_t st;
    assert(eos_ota_get_status(&st) == 0);
    assert(st.active_slot == EOS_OTA_SLOT_A);
    assert(eos_ota_get_status(NULL) == -1);
    eos_ota_deinit();
    printf("[PASS] ota status\n");
}

int main(void) {
    printf("=== EoS OTA Tests ===\n");
    test_ota_init();
    test_ota_full_update();
    test_ota_verify_fail();
    test_ota_rollback();
    test_ota_status();
    printf("=== ALL OTA TESTS PASSED (5/5) ===\n");
    return 0;
}