/**
 * @file ota.c
 * @brief EoS OTA Update Service — Implementation
 */

#include <eos/ota.h>
#include <string.h>

#if EOS_ENABLE_OTA

static eos_ota_status_t ota_status;
static eos_ota_progress_cb progress_cb = NULL;
static void *progress_ctx = NULL;
static bool ota_initialized = false;

int eos_ota_init(void)
{
    memset(&ota_status, 0, sizeof(ota_status));
    ota_status.state = EOS_OTA_STATE_IDLE;
    ota_status.active_slot = EOS_OTA_SLOT_A;
    ota_status.update_slot = EOS_OTA_SLOT_B;
    ota_initialized = true;
    return 0;
}

void eos_ota_deinit(void)
{
    ota_initialized = false;
}

int eos_ota_check_update(const eos_ota_source_t *source, bool *available)
{
    if (!ota_initialized || !source) return -1;
    if (available) *available = false;
    return 0;
}

int eos_ota_begin(const eos_ota_source_t *source)
{
    if (!ota_initialized || !source) return -1;
    if (ota_status.state != EOS_OTA_STATE_IDLE) return -1;

    ota_status.state = EOS_OTA_STATE_DOWNLOADING;
    ota_status.total_bytes = source->expected_size;
    ota_status.bytes_received = 0;
    ota_status.progress_pct = 0;
    strncpy(ota_status.update_version, source->version,
            sizeof(ota_status.update_version) - 1);
    return 0;
}

int eos_ota_write_chunk(const uint8_t *data, size_t len)
{
    if (!ota_initialized || !data) return -1;
    if (ota_status.state != EOS_OTA_STATE_DOWNLOADING) return -1;

    ota_status.bytes_received += (uint32_t)len;
    if (ota_status.total_bytes > 0) {
        ota_status.progress_pct =
            (uint8_t)((ota_status.bytes_received * 100) / ota_status.total_bytes);
    }
    if (progress_cb) {
        progress_cb(ota_status.progress_pct, progress_ctx);
    }
    return 0;
}

int eos_ota_finish(void)
{
    if (!ota_initialized) return -1;
    if (ota_status.state != EOS_OTA_STATE_DOWNLOADING) return -1;
    ota_status.state = EOS_OTA_STATE_VERIFYING;
    return 0;
}

int eos_ota_abort(void)
{
    if (!ota_initialized) return -1;
    ota_status.state = EOS_OTA_STATE_IDLE;
    ota_status.bytes_received = 0;
    ota_status.progress_pct = 0;
    return 0;
}

int eos_ota_verify(void)
{
    if (!ota_initialized) return -1;
    if (ota_status.state != EOS_OTA_STATE_VERIFYING) return -1;
    ota_status.state = EOS_OTA_STATE_IDLE;
    return 0;
}

int eos_ota_apply(void)
{
    if (!ota_initialized) return -1;
    ota_status.state = EOS_OTA_STATE_APPLYING;
    ota_status.state = EOS_OTA_STATE_IDLE;
    return 0;
}

int eos_ota_rollback(void)
{
    if (!ota_initialized) return -1;
    eos_ota_slot_t tmp = ota_status.active_slot;
    ota_status.active_slot = ota_status.update_slot;
    ota_status.update_slot = tmp;
    return 0;
}

int eos_ota_get_status(eos_ota_status_t *status)
{
    if (!ota_initialized || !status) return -1;
    memcpy(status, &ota_status, sizeof(*status));
    return 0;
}

int eos_ota_set_progress_callback(eos_ota_progress_cb cb, void *ctx)
{
    progress_cb = cb;
    progress_ctx = ctx;
    return 0;
}

eos_ota_slot_t eos_ota_get_active_slot(void) { return ota_status.active_slot; }

int eos_ota_set_active_slot(eos_ota_slot_t slot)
{
    ota_status.active_slot = slot;
    return 0;
}

int eos_ota_mark_slot_valid(eos_ota_slot_t slot)
{
    (void)slot;
    return 0;
}

#endif /* EOS_ENABLE_OTA */
