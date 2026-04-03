// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/ota.h"
#include "eos/crypto.h"
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
    int              in_progress;
    int              verified;
    eos_ota_progress_cb progress_cb;
    void            *progress_ctx;
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
    g_ota.verified = 0;
    g_ota.in_progress = 1;
    memcpy(g_ota.expected_hash, source->expected_sha256, 32);
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
    g_ota.verified = 0;
    return 0;
}

int eos_ota_verify(void) {
    if (!g_ota.initialized || g_ota.bytes_written != g_ota.total_size) return -1;
    uint8_t actual[32];
    eos_sha256_final(&g_ota.hash_ctx, actual);
    if (memcmp(actual, g_ota.expected_hash, 32) != 0) return -1;
    g_ota.verified = 1;
    g_ota.slot_valid[g_ota.update_slot] = 1;
    return 0;
}

int eos_ota_apply(void) {
    if (!g_ota.initialized || !g_ota.verified) return -1;
    g_ota.active_slot = g_ota.update_slot;
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