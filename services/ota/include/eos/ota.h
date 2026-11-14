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
    /* Monotonic per-release number for anti-rollback (see
     * eos_ota_set_rollback_floor). 0 means the update carries no version
     * code: it applies only while no floor is provisioned. */
    uint32_t version_code;
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
    /* version_code of the active slot's image (0 if unknown -- e.g. the
     * factory image predates version codes); rollback_floor is the
     * anti-rollback floor currently enforced at apply(). eBoot reads both
     * at boot: the applied version feeds its monotonic counter (PR-EBT-2). */
    uint32_t        version_code;
    uint32_t        rollback_floor;
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
/**
 * @brief Feed one chunk of the download.
 *
 * The chunk is hashed and counted, and -- when a storage writer is installed
 * with eos_ota_set_storage_writer() -- persisted into the update slot first:
 * a chunk the writer refuses is neither hashed nor counted. Bytes that were
 * never persisted are never installable: eos_ota_apply() refuses unless the
 * persisted byte count equals the declared image size.
 */
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
 * EOS_ALLOW_STUB_CRYPTO opt-in in services/crypto. Also requires that every
 * received byte was persisted through the installed storage writer (see
 * eos_ota_set_storage_writer) and that the update's version_code is not
 * below the anti-rollback floor (see eos_ota_set_rollback_floor).
 *
 * @return 0 on success, -1 if the image is unverified, unauthenticated,
 * unpersisted, or below the anti-rollback floor.
 */
int  eos_ota_apply(void);
int  eos_ota_rollback(void);

int  eos_ota_get_status(eos_ota_status_t *status);
int  eos_ota_set_progress_callback(eos_ota_progress_cb cb, void *ctx);

eos_ota_slot_t eos_ota_get_active_slot(void);
int  eos_ota_set_active_slot(eos_ota_slot_t slot);

/**
 * @brief Persist one downloaded chunk into the update slot.
 *
 * Installed with eos_ota_set_storage_writer(); the platform's flash/slot
 * writer belongs here. Called by eos_ota_write_chunk() with the update slot
 * and the byte offset the chunk belongs at, before the chunk is hashed or
 * counted. Returning non-zero rejects the chunk: it is neither hashed nor
 * counted, so the download can never verify or apply past a failed write.
 *
 * @return 0 if the bytes were persisted, non-zero otherwise.
 */
typedef int (*eos_ota_storage_write_cb)(eos_ota_slot_t slot, uint32_t offset,
                                        const uint8_t *data, size_t len,
                                        void *ctx);

/**
 * @brief Install a storage writer, or NULL to remove it.
 *
 * Without one, eos_ota_write_chunk() still hashes and counts the download
 * but no byte is persisted anywhere -- and eos_ota_apply() refuses:
 * switching the boot slot to an image that was never written to it ships a
 * device that boots whatever the slot happened to hold before. There is no
 * opt-out: an OTA service with nowhere to write the image has nothing to
 * install.
 */
int  eos_ota_set_storage_writer(eos_ota_storage_write_cb cb, void *ctx);

/**
 * @brief Provision the anti-rollback floor (monotonic version_code).
 *
 * The platform calls this at boot with the floor its durable store holds --
 * on eBoot boards, the monotonic counter PR-EBT-2 makes fail-closed -- and
 * eos_ota_apply() then refuses any update whose version_code is below it, or
 * which carries no version_code at all. A floor of 0 means unprovisioned:
 * updates apply as before. The floor only moves up: lowering it is refused.
 * On a successful apply the floor rises to the applied version_code, so a
 * downgrade cannot be re-applied in this boot; the durable counter is
 * eBoot's to raise at next boot from the applied version this service
 * reports in eos_ota_status_t (version_code / rollback_floor).
 *
 * eos_ota_rollback() is deliberately NOT floor-gated: rollback is the
 * recovery path out of a bad new image, and the boot-time downgrade
 * decision belongs to eBoot's verifier, not to this service.
 */
int  eos_ota_set_rollback_floor(uint32_t version_code);

/**
 * @brief Confirm a slot holds a verified image (trial-boot confirmation).
 *
 * The book's first-boot flow calls this for the active slot after self-tests
 * pass. It succeeds only for a slot whose image is verified: the slot that
 * booted (verified by the bootloader's choice) or a slot apply() installed
 * (integrity + authenticator proven there). A slot whose image failed
 * verification -- never verified, failed verify, or being overwritten by a
 * new begin() -- is never markable valid, so it can never become a
 * rollback() / eos_ota_set_active_slot() target. There is no override.
 *
 * @return 0 on success, -1 if the slot holds no verified image.
 */
int  eos_ota_mark_slot_valid(eos_ota_slot_t slot);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ENABLE_OTA */
#endif /* EOS_OTA_H */
