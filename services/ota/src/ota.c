// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/ota.h"
#include "eos/crypto.h"
#include "eos/log.h"
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
    uint8_t          slot_verified[2]; /* slot holds an integrity+auth proven image */
    uint32_t         bytes_persisted;  /* bytes actually written to the update slot */
    eos_ota_storage_write_cb storage_cb;
    void            *storage_ctx;
    uint32_t         rollback_floor;       /* anti-rollback floor (0 = unprovisioned) */
    uint32_t         current_version_code; /* version_code of the active slot image */
    uint32_t         update_version_code;  /* version_code of the in-progress update */
    char             current_version[32];
    char             update_version[32];
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
    /* The booted image is verified by the bootloader having chosen it. */
    g_ota.slot_verified[0] = 1;
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
    /* The slot is about to be overwritten: it stops being a rollback or
     * set_active_slot() target now, not when the new image is applied. */
    g_ota.slot_valid[g_ota.update_slot] = 0;
    g_ota.slot_verified[g_ota.update_slot] = 0;
    g_ota.total_size = source->expected_size;
    g_ota.bytes_written = 0;
    g_ota.bytes_persisted = 0;
    g_ota.integrity_ok = 0;
    g_ota.authenticated = 0;
    g_ota.in_progress = 1;
    memcpy(g_ota.expected_hash, source->expected_sha256, 32);
    memset(g_ota.actual_hash, 0, sizeof(g_ota.actual_hash));
    g_ota.signature = source->signature;
    g_ota.signature_len = source->signature_len;
    g_ota.update_version_code = source->version_code;
    memcpy(g_ota.update_version, source->version, sizeof(g_ota.update_version));
    eos_sha256_init(&g_ota.hash_ctx);
    return 0;
}

int eos_ota_write_chunk(const uint8_t *data, size_t len) {
    if (!g_ota.initialized || !g_ota.in_progress || !data) return -1;
    /* Compare against remaining space. Adding len can wrap and accept a huge chunk. */
    if (g_ota.bytes_written > g_ota.total_size ||
        len > (size_t)(g_ota.total_size - g_ota.bytes_written))
        return -1;
    if (g_ota.storage_cb) {
        /* Persist first: a chunk the writer refuses is neither hashed nor
         * counted, so the download can never verify or apply past it. */
        if (g_ota.storage_cb(g_ota.update_slot, g_ota.bytes_written,
                             data, len, g_ota.storage_ctx) != 0)
            return -1;
    }
    eos_sha256_update(&g_ota.hash_ctx, data, len);
    g_ota.bytes_written += (uint32_t)len;
    if (g_ota.storage_cb) g_ota.bytes_persisted += (uint32_t)len;
    if (g_ota.progress_cb && g_ota.total_size > 0) {
        /* 64-bit: bytes_written * 100 wraps uint32_t above ~42.9 MB. */
        uint8_t pct = (uint8_t)(((uint64_t)g_ota.bytes_written * 100u) / g_ota.total_size);
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
    g_ota.bytes_persisted = 0;
    g_ota.integrity_ok = 0;
    g_ota.authenticated = 0;
    g_ota.update_version_code = 0;
    memset(g_ota.update_version, 0, sizeof(g_ota.update_version));
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

    /* The bytes were hashed, but hashing is not installing: unless every
     * byte reached the update slot through the storage writer, the slot
     * still holds whatever it held before, and switching the boot to it
     * would boot that stale image under the new image's name. */
    if (g_ota.total_size == 0 || g_ota.bytes_persisted != g_ota.total_size)
        return -1;

    /* Anti-rollback: once a floor is provisioned, an update below it -- or
     * one that carries no version code to order against it -- is refused.
     * The durable floor lives in eBoot (PR-EBT-2); this is the RAM mirror. */
    if (g_ota.rollback_floor != 0 &&
        (g_ota.update_version_code == 0 ||
         g_ota.update_version_code < g_ota.rollback_floor))
        return -1;

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
        EOS_ERROR("ota: refusing to apply an update with no authenticator "
                  "installed; expected_sha256 travels with the update and "
                  "proves nothing about its origin. Call "
                  "eos_ota_set_authenticator(), or build with "
                  "EOS_ALLOW_UNSIGNED_OTA.");
        return -1;
#endif
    }

    g_ota.active_slot = g_ota.update_slot;
    g_ota.slot_valid[g_ota.update_slot] = 1;
    g_ota.slot_verified[g_ota.update_slot] = 1;
    g_ota.current_version_code = g_ota.update_version_code;
    memcpy(g_ota.current_version, g_ota.update_version,
           sizeof(g_ota.current_version));
    /* The floor rises to what just applied: a downgrade cannot be
     * re-applied in this boot. eBoot raises its durable counter from the
     * version this service now reports (see eos_ota_get_status). */
    if (g_ota.update_version_code > g_ota.rollback_floor)
        g_ota.rollback_floor = g_ota.update_version_code;
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
        status->progress_pct = (uint8_t)(((uint64_t)g_ota.bytes_written * 100u) /
                                         g_ota.total_size);
    status->state = g_ota.in_progress ? EOS_OTA_STATE_DOWNLOADING : EOS_OTA_STATE_IDLE;
    memcpy(status->current_version, g_ota.current_version,
           sizeof(status->current_version));
    memcpy(status->update_version, g_ota.update_version,
           sizeof(status->update_version));
    status->version_code = g_ota.current_version_code;
    status->rollback_floor = g_ota.rollback_floor;
    return 0;
}

int eos_ota_set_progress_callback(eos_ota_progress_cb cb, void *ctx) {
    g_ota.progress_cb = cb;
    g_ota.progress_ctx = ctx;
    return 0;
}

int eos_ota_set_storage_writer(eos_ota_storage_write_cb cb, void *ctx) {
    if (!g_ota.initialized) return -1;
    g_ota.storage_cb = cb;
    g_ota.storage_ctx = ctx;
    return 0;
}

int eos_ota_set_rollback_floor(uint32_t version_code) {
    if (!g_ota.initialized) return -1;
    /* The floor only moves up. */
    if (version_code < g_ota.rollback_floor) return -1;
    g_ota.rollback_floor = version_code;
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
    /* No public bypass: only a slot holding a verified image -- the one
     * that booted, or one apply() installed -- can be marked valid. */
    if (!g_ota.slot_verified[slot]) return -1;
    g_ota.slot_valid[slot] = 1;
    return 0;
}

#endif /* EOS_ENABLE_OTA */