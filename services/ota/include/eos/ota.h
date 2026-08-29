// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file ota.h
 * @brief EoS OTA (Over-the-Air) Update Service
 *
 * Firmware update framework supporting download, verification,
 * A/B slot management, and rollback.
 */

#ifndef EOS_OTA_H
#define EOS_OTA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <eos/eos_config.h>

#if EOS_ENABLE_OTA

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EOS_OTA_STATE_IDLE       = 0,
    EOS_OTA_STATE_DOWNLOADING = 1,
    EOS_OTA_STATE_VERIFYING  = 2,
    EOS_OTA_STATE_APPLYING   = 3,
    EOS_OTA_STATE_REBOOTING  = 4,
    EOS_OTA_STATE_ERROR      = 5,
} eos_ota_state_t;

typedef enum {
    EOS_OTA_SLOT_A = 0,
    EOS_OTA_SLOT_B = 1,
} eos_ota_slot_t;

typedef struct {
    char     url[256];
    char     version[32];
    uint32_t expected_size;
    uint8_t  expected_sha256[32];
    bool     use_tls;

    /* Signature over the image, handed to the authenticator callback. May be
     * empty: expected_sha256 alone proves only that the download was not
     * corrupted in transit, never who produced it. */
    const uint8_t *signature;
    size_t         signature_len;
} eos_ota_source_t;

typedef struct {
    eos_ota_state_t state;
    uint32_t        bytes_received;
    uint32_t        total_bytes;
    uint8_t         progress_pct;
    eos_ota_slot_t  active_slot;
    eos_ota_slot_t  update_slot;
    char            current_version[32];
    char            update_version[32];
} eos_ota_status_t;

typedef void (*eos_ota_progress_cb)(uint8_t progress_pct, void *ctx);

/**
 * @brief Authenticity check for a downloaded image.
 *
 * Called by eos_ota_apply() with the SHA-256 the service computed over the
 * bytes it actually received -- not the one the update source declared -- plus
 * whatever signature material accompanied the update. The platform installs
 * one of these with eos_ota_set_authenticator(); it is where a public key and
 * a real signature verifier belong (see services/security).
 *
 * @return 0 if the image is authentic, non-zero otherwise.
 */
typedef int (*eos_ota_authenticator_cb)(const uint8_t digest[32],
                                        const uint8_t *signature,
                                        size_t signature_len,
                                        void *ctx);

int  eos_ota_init(void);
void eos_ota_deinit(void);

int  eos_ota_check_update(const eos_ota_source_t *source, bool *available);
int  eos_ota_begin(const eos_ota_source_t *source);
int  eos_ota_write_chunk(const uint8_t *data, size_t len);
int  eos_ota_finish(void);
int  eos_ota_abort(void);

/**
 * @brief Check the downloaded image against the source's declared SHA-256.
 *
 * This is an integrity check only. The digest it compares against travels with
 * the update, so anyone able to substitute the image can substitute the digest
 * too. Passing this does not mean the image is trusted -- eos_ota_apply()
 * enforces that separately.
 *
 * @return 0 if the bytes received hash to the declared digest.
 */
int  eos_ota_verify(void);

/**
 * @brief Install an authenticity callback, or NULL to remove it.
 *
 * Must be called before eos_ota_apply(). Without one, apply() refuses: an
 * unauthenticated image is not installable in a default build.
 */
int  eos_ota_set_authenticator(eos_ota_authenticator_cb cb, void *ctx);

/**
 * @brief Switch the active slot to the downloaded image.
 *
 * Requires that eos_ota_verify() passed and that the installed authenticator
 * accepted the image. A build with no authenticator installed is refused
 * unless it defines EOS_ALLOW_UNSIGNED_OTA, which mirrors the
 * EOS_ALLOW_STUB_CRYPTO opt-in in services/crypto.
 *
 * @return 0 on success, -1 if the image is unverified or unauthenticated.
 */
int  eos_ota_apply(void);
int  eos_ota_rollback(void);

int  eos_ota_get_status(eos_ota_status_t *status);
int  eos_ota_set_progress_callback(eos_ota_progress_cb cb, void *ctx);

eos_ota_slot_t eos_ota_get_active_slot(void);
int  eos_ota_set_active_slot(eos_ota_slot_t slot);
int  eos_ota_mark_slot_valid(eos_ota_slot_t slot);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ENABLE_OTA */
#endif /* EOS_OTA_H */
