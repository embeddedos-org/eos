// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file main.c
 * @brief EoS Example: Secure OTA — Firmware update with verification
 *
 * Demonstrates the OTA update flow: check for update, download, check
 * integrity, authenticate the image, apply to the inactive slot, and reboot.
 *
 * Integrity and authenticity are separate steps here, deliberately.
 * eos_ota_verify() compares the download against the SHA-256 the update
 * source declared, which catches a corrupted transfer but nothing else --
 * that digest arrives with the update, so whoever can replace the image can
 * replace the digest. Deciding whether the image is one this device should
 * run is the authenticator's job, below.
 */

#include <eos/hal.h>
#include <eos/kernel.h>
#include <eos/ota.h>
#include <eos/crypto.h>
#include <eos/os_services.h>
#include <stdio.h>
#include <string.h>

#define UPDATE_CHECK_INTERVAL_MS  30000
#define FIRMWARE_URL              "https://fw.example.com/firmware/v2.0.0.bin"
#define FIRMWARE_VERSION          "2.0.0"

/* The vendor public key this device trusts. A real deployment provisions this
 * at manufacture and stores it somewhere immutable. */
static const uint8_t g_vendor_pubkey[32] = { 0 };

/* Called by eos_ota_apply() with the digest the OTA service computed over the
 * bytes it actually received. This is where a signature check belongs: verify
 * `signature` over `digest` under g_vendor_pubkey and return 0 only then.
 *
 * services/crypto's verify routines are still stubs and refuse to run unless a
 * build defines EOS_ALLOW_STUB_CRYPTO, so this returns -1 rather than pretend.
 * Refusing to install is the correct behaviour for a device with no working
 * verifier -- returning 0 here would install anything. */
static int verify_vendor_signature(const uint8_t digest[32],
                                   const uint8_t *signature,
                                   size_t signature_len,
                                   void *ctx)
{
    (void)digest; (void)ctx;

    if (!signature || signature_len == 0) {
        printf("[ota] Update carries no signature — refusing\n");
        return -1;
    }

    printf("[ota] No signature verifier is linked into this build — refusing\n");
    return -1;
}

static void progress_callback(uint8_t pct, void *ctx)
{
    (void)ctx;
    printf("[ota] Download progress: %u%%\n", pct);
}

static void ota_task(void *arg)
{
    (void)arg;

    printf("[ota] OTA task started, checking every %d seconds\n",
           UPDATE_CHECK_INTERVAL_MS / 1000);

    while (1) {
        eos_task_delay_ms(UPDATE_CHECK_INTERVAL_MS);

        /* Step 1: Check for available update */
        eos_ota_source_t source = {
            .version = FIRMWARE_VERSION,
            .use_tls = true,
        };
        strncpy(source.url, FIRMWARE_URL, sizeof(source.url) - 1);

        bool available = false;
        int ret = eos_ota_check_update(&source, &available);
        if (ret != 0 || !available) {
            printf("[ota] No update available (ret=%d)\n", ret);
            continue;
        }

        printf("[ota] Update available: %s\n", source.version);

        /* Step 2: Begin download */
        ret = eos_ota_begin(&source);
        if (ret != 0) {
            printf("[ota] Failed to begin update: %d\n", ret);
            continue;
        }

        /* Step 3: Finish download */
        ret = eos_ota_finish();
        if (ret != 0) {
            printf("[ota] Download failed: %d\n", ret);
            eos_ota_abort();
            continue;
        }

        /* Step 4: Integrity — did the download arrive intact? */
        printf("[ota] Checking firmware integrity...\n");
        ret = eos_ota_verify();
        if (ret != 0) {
            printf("[ota] Integrity check FAILED: %d — aborting\n", ret);
            eos_ota_abort();
            continue;
        }
        printf("[ota] Integrity check passed\n");

        /* Step 5: Authenticity — should this device run this image at all?
         * apply() refuses outright if no authenticator is installed. */
        printf("[ota] Authenticating and applying update...\n");
        ret = eos_ota_apply();
        if (ret != 0) {
            printf("[ota] Update rejected (unauthenticated or apply failed): %d\n", ret);
            eos_ota_abort();
            continue;
        }

        /* Step 6: Print status and reboot */
        eos_ota_status_t status;
        eos_ota_get_status(&status);
        printf("[ota] Update applied successfully!\n");
        printf("[ota]   Current: %s (slot %c)\n",
               status.current_version,
               status.active_slot == EOS_OTA_SLOT_A ? 'A' : 'B');
        printf("[ota]   Next boot: %s (slot %c)\n",
               status.update_version,
               status.update_slot == EOS_OTA_SLOT_A ? 'A' : 'B');

        printf("[ota] Rebooting in 3 seconds...\n");
        eos_task_delay_ms(3000);

        /* Reboot would happen here on real hardware */
        printf("[ota] === REBOOT ===\n");
        break;
    }
}

static void watchdog_task(void *arg)
{
    (void)arg;

    printf("[watchdog] Watchdog task started\n");

    while (1) {
        /* Pet the watchdog every 5 seconds to prevent reset */
        printf("[watchdog] Feeding watchdog\n");
        eos_task_delay_ms(5000);
    }
}

int main(void)
{
    eos_hal_init();
    eos_kernel_init();
    eos_ota_init();

    /* Register progress callback */
    eos_ota_set_progress_callback(progress_callback, NULL);

    /* Without this, eos_ota_apply() refuses: an image whose only credential is
     * a digest it shipped with is not installable. */
    eos_ota_set_authenticator(verify_vendor_signature, (void *)g_vendor_pubkey);

    /* Print current firmware info */
    eos_ota_status_t status;
    eos_ota_get_status(&status);
    printf("[main] Current firmware: %s (slot %c)\n",
           status.current_version,
           status.active_slot == EOS_OTA_SLOT_A ? 'A' : 'B');

    /* Create tasks */
    eos_task_create("ota",      ota_task,      NULL, 2, 2048);
    eos_task_create("watchdog", watchdog_task,  NULL, 1, 512);

    printf("[main] Starting kernel...\n");
    eos_kernel_start();

    return 0;
}
