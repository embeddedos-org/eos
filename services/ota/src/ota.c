// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/ota.h"
#include "eos/crypto.h"
#include <stdio.h>
#include <string.h>

#if EOS_ENABLE_OTA

static struct {
    int              initialized;
    eos_ota_slot_t   active_slot;
    eos_ota_slot_t   update_slot;
    EosSha256        hash_ctx;
    uint32_t         bytes_written;
    uint32_t         total_size;
    uint8_t          expected_hash[32];
    uint8_t          actual_hash[32];
    const uint8_t   *signature;
    size_t           signature_len;
    int              in_progress;
    int              integrity_ok;   /* bytes received hash to expected_hash */
    int              authenticated;  /* the authenticator accepted the image */
    eos_ota_progress_cb progress_cb;
    void            *progress_ctx;
    eos_ota_authenticator_cb auth_cb;
    void            *auth_ctx;
    uint8_t          slot_valid[2];
} g_ota;

int eos_ota_init(void) {
    memset(&g_ota, 0, sizeof(g_ota));
    g_ota.active_slot = EOS_OTA_SLOT_A;
    g_ota.slot_valid[0] = 1;
    g_ota.initialized = 1;
    return 0;
}

void eos_ota_deinit(void) {
    if (g_ota.in_progress) eos_ota_abort();
    g_ota.initialized = 0;
}

int eos_ota_check_update(const eos_ota_source_t *source, bool *available) {
    if (!g_ota.initialized || !source || !available) return -1;
    *available = (source->expected_size > 0);
    return 0;
}

int eos_ota_begin(const eos_ota_source_t *source) {
    if (!g_ota.initialized || !source || g_ota.in_progress) return -1;
    g_ota.update_slot = (g_ota.active_slot == EOS_OTA_SLOT_A) ? EOS_OTA_SLOT_B : EOS_OTA_SLOT_A;
    g_ota.total_size = source->expected_size;
    g_ota.bytes_written = 0;
    g_ota.integrity_ok = 0;
    g_ota.authenticated = 0;
    g_ota.in_progress = 1;
    memcpy(g_ota.expected_hash, source->expected_sha256, 32);
    memset(g_ota.actual_hash, 0, sizeof(g_ota.actual_hash));
    g_ota.signature = source->signature;
    g_ota.signature_len = source->signature_len;
    eos_sha256_init(&g_ota.hash_ctx);
    return 0;
}

int eos_ota_write_chunk(const uint8_t *data, size_t len) {
    if (!g_ota.initialized || !g_ota.in_progress || !data) return -1;
    if (g_ota.bytes_written + len > g_ota.total_size) return -1;
    eos_sha256_update(&g_ota.hash_ctx, data, len);
    g_ota.bytes_written += (uint32_t)len;
    if (g_ota.progress_cb && g_ota.total_size > 0) {
        uint8_t pct = (uint8_t)((g_ota.bytes_written * 100) / g_ota.total_size);
        g_ota.progress_cb(pct, g_ota.progress_ctx);
    }
    return 0;
}

int eos_ota_finish(void) {
    if (!g_ota.initialized || !g_ota.in_progress) return -1;
    g_ota.in_progress = 0;
    return 0;
}

int eos_ota_abort(void) {
    g_ota.in_progress = 0;
    g_ota.bytes_written = 0;
    g_ota.integrity_ok = 0;
    g_ota.authenticated = 0;
    return 0;
}

int eos_ota_verify(void) {
    if (!g_ota.initialized || g_ota.bytes_written != g_ota.total_size) return -1;
    if (g_ota.integrity_ok) return 0;  /* the hash context is single-use */

    eos_sha256_final(&g_ota.hash_ctx, g_ota.actual_hash);
    if (memcmp(g_ota.actual_hash, g_ota.expected_hash, 32) != 0) return -1;

    /* Integrity only. The slot is not marked valid here: until apply() runs it
     * holds a half-installed image, and marking it would make it a legal
     * eos_ota_rollback() / eos_ota_set_active_slot() target. */
    g_ota.integrity_ok = 1;
    return 0;
}

int eos_ota_set_authenticator(eos_ota_authenticator_cb cb, void *ctx) {
    if (!g_ota.initialized) return -1;
    g_ota.auth_cb = cb;
    g_ota.auth_ctx = ctx;
    return 0;
}

int eos_ota_apply(void) {
    if (!g_ota.initialized || !g_ota.integrity_ok) return -1;

    if (g_ota.auth_cb) {
        /* The authenticator is given the digest this service computed, never
         * the one the update declared. */
        if (g_ota.auth_cb(g_ota.actual_hash, g_ota.signature,
                          g_ota.signature_len, g_ota.auth_ctx) != 0) {
            g_ota.authenticated = 0;
            return -1;
        }
        g_ota.authenticated = 1;
    } else {
#ifdef EOS_ALLOW_UNSIGNED_OTA
        g_ota.authenticated = 1;
#else
        fprintf(stderr,
                "eos-ota: refusing to apply an update with no authenticator "
                "installed; expected_sha256 travels with the update and proves "
                "nothing about its origin. Call eos_ota_set_authenticator(), "
                "or build with EOS_ALLOW_UNSIGNED_OTA.\n");
        return -1;
#endif
    }

    g_ota.active_slot = g_ota.update_slot;
    g_ota.slot_valid[g_ota.update_slot] = 1;
    return 0;
}

int eos_ota_rollback(void) {
    if (!g_ota.initialized) return -1;
    eos_ota_slot_t other = (g_ota.active_slot == EOS_OTA_SLOT_A) ? EOS_OTA_SLOT_B : EOS_OTA_SLOT_A;
    if (!g_ota.slot_valid[other]) return -1;
    g_ota.active_slot = other;
    return 0;
}

int eos_ota_get_status(eos_ota_status_t *status) {
    if (!g_ota.initialized || !status) return -1;
    memset(status, 0, sizeof(*status));
    status->active_slot = g_ota.active_slot;
    status->update_slot = g_ota.update_slot;
    status->bytes_received = g_ota.bytes_written;
    status->total_bytes = g_ota.total_size;
    if (g_ota.total_size > 0)
        status->progress_pct = (uint8_t)((g_ota.bytes_written * 100) / g_ota.total_size);
    status->state = g_ota.in_progress ? EOS_OTA_STATE_DOWNLOADING : EOS_OTA_STATE_IDLE;
    return 0;
}

int eos_ota_set_progress_callback(eos_ota_progress_cb cb, void *ctx) {
    g_ota.progress_cb = cb;
    g_ota.progress_ctx = ctx;
    return 0;
}

eos_ota_slot_t eos_ota_get_active_slot(void) { return g_ota.active_slot; }

int eos_ota_set_active_slot(eos_ota_slot_t slot) {
    if (slot > EOS_OTA_SLOT_B || !g_ota.slot_valid[slot]) return -1;
    g_ota.active_slot = slot;
    return 0;
}

int eos_ota_mark_slot_valid(eos_ota_slot_t slot) {
    if (slot > EOS_OTA_SLOT_B) return -1;
    g_ota.slot_valid[slot] = 1;
    return 0;
}

#endif /* EOS_ENABLE_OTA */