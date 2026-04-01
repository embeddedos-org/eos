// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file main.c
 * @brief EoS Example: Secure OTA — Firmware update with verification
 *
 * Demonstrates the OTA update flow: check for update, download,
 * verify SHA-256 signature, apply to inactive slot, and reboot.
 * Uses crypto services for integrity and watchdog for safety.
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

        /* Step 4: Verify firmware integrity (SHA-256 + signature) */
        printf("[ota] Verifying firmware integrity...\n");
        ret = eos_ota_verify();
        if (ret != 0) {
            printf("[ota] Verification FAILED: %d — aborting\n", ret);
            eos_ota_abort();
            continue;
        }
        printf("[ota] Verification passed\n");

        /* Step 5: Apply update (mark inactive slot as next boot) */
        printf("[ota] Applying update...\n");
        ret = eos_ota_apply();
        if (ret != 0) {
            printf("[ota] Apply failed: %d\n", ret);
            eos_ota_rollback();
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
