# Chapter 13: OTA Updates

**Author:** Srikanth Patchava & EmbeddedOS Contributors

---

## 13.1 Overview

The EoS OTA (Over-the-Air) update service (`eos/ota.h`) provides a complete
firmware update framework with download management, SHA-256 verification,
A/B slot management, and automatic rollback. It is gated by the
`EOS_ENABLE_OTA` configuration flag.

```
+----------------------------------------------------------+
|                    OTA Server                            |
|              (firmware.bin + manifest)                   |
+------------------------+---------------------------------+
                         |  HTTPS / MQTT
+------------------------v---------------------------------+
|                  EoS OTA Service                         |
|   +----------+-----------+----------+--------------+     |
|   | Download |  Verify   |  Apply   |  Rollback    |     |
|   +----------+-----------+----------+--------------+     |
+----------------------------------------------------------+
|              Flash Partition Table                        |
|         +----------+    +----------+                     |
|         |  Slot A  |    |  Slot B  |                     |
|         | (active) |    | (update) |                     |
|         +----------+    +----------+                     |
+----------------------------------------------------------+
```

## 13.2 OTA State Machine

The update process follows a well-defined state machine:

| State                        | Value | Description                    |
|------------------------------|-------|--------------------------------|
| `EOS_OTA_STATE_IDLE`         | 0     | No update in progress          |
| `EOS_OTA_STATE_DOWNLOADING`  | 1     | Receiving firmware chunks      |
| `EOS_OTA_STATE_VERIFYING`    | 2     | Validating hash/signature      |
| `EOS_OTA_STATE_APPLYING`     | 3     | Writing to update slot         |
| `EOS_OTA_STATE_REBOOTING`    | 4     | About to reboot into new fw    |
| `EOS_OTA_STATE_ERROR`        | 5     | Error occurred                 |

```
  IDLE --> DOWNLOADING --> VERIFYING --> APPLYING --> REBOOTING
    ^           |               |             |
    |           v               v             v
    +--------- ERROR <-------- ERROR <------- ERROR
                |
                v
            ROLLBACK --> IDLE
```

## 13.3 A/B Slot Management

EoS uses a dual-slot (A/B) partitioning scheme for safe updates:

```
+--------------------------------------------------+
|                  Flash Layout                    |
+------------+----------------+--------------------+
| Bootloader |    Slot A      |    Slot B          |
|            |  (fw v1.2.0)   |  (fw v1.3.0)      |
|            |  [ACTIVE]      |  [UPDATE]          |
+------------+----------------+--------------------+
```

| Enum              | Value | Description               |
|-------------------|-------|---------------------------|
| `EOS_OTA_SLOT_A`  | 0     | First firmware slot       |
| `EOS_OTA_SLOT_B`  | 1     | Second firmware slot      |

### 13.3.1 Slot Operations

```c
/* Query which slot is currently running */
eos_ota_slot_t active = eos_ota_get_active_slot();
printf("Running from Slot %c\n",
       active == EOS_OTA_SLOT_A ? 'A' : 'B');

/* After successful boot, mark current slot as valid */
eos_ota_mark_slot_valid(active);

/* Force boot from a specific slot (e.g., for recovery) */
eos_ota_set_active_slot(EOS_OTA_SLOT_A);
```

## 13.4 Update Source Configuration

```c
typedef struct {
    char     url[256];              /* Firmware download URL       */
    char     version[32];           /* Target version string       */
    uint32_t expected_size;         /* Expected size in bytes      */
    uint8_t  expected_sha256[32];   /* Expected SHA-256 digest     */
    bool     use_tls;               /* Use HTTPS?                  */
} eos_ota_source_t;
```

## 13.5 Update Status

```c
typedef struct {
    eos_ota_state_t state;          /* Current OTA state           */
    uint32_t        bytes_received; /* Downloaded so far           */
    uint32_t        total_bytes;    /* Total firmware size         */
    uint8_t         progress_pct;   /* 0-100 progress percentage   */
    eos_ota_slot_t  active_slot;    /* Currently running slot      */
    eos_ota_slot_t  update_slot;    /* Slot being written to       */
    char            current_version[32];  /* Running fw version    */
    char            update_version[32];   /* New fw version        */
} eos_ota_status_t;
```

## 13.6 Complete OTA Workflow

### 13.6.1 Check for Updates

```c
#include <eos/ota.h>

int check_and_update(void)
{
    eos_ota_init();

    eos_ota_source_t source = {
        .url           = "https://fw.example.com/v1.3.0/firmware.bin",
        .version       = "1.3.0",
        .expected_size = 262144,
        .use_tls       = true,
    };
    /* Set expected_sha256 from manifest */
    memcpy(source.expected_sha256, manifest_hash, 32);

    bool available = false;
    int rc = eos_ota_check_update(&source, &available);
    if (rc != 0 || !available) {
        printf("No update available\n");
        return -1;
    }

    printf("Update available: v%s\n", source.version);
    return 0;
}
```

### 13.6.2 Download and Apply

```c
int perform_update(const eos_ota_source_t *source)
{
    /* Begin the update -- sets state to DOWNLOADING */
    int rc = eos_ota_begin(source);
    if (rc != 0) return rc;

    /* Download in chunks (typically via HTTP GET) */
    eos_http_response_t resp;
    rc = eos_http_get(source->url, &resp);
    if (rc != 0) {
        eos_ota_abort();
        return rc;
    }

    /* Feed chunks to OTA writer */
    size_t offset = 0;
    while (offset < resp.body_len) {
        size_t chunk = (resp.body_len - offset > 4096)
                       ? 4096 : resp.body_len - offset;
        eos_ota_write_chunk(resp.body + offset, chunk);
        offset += chunk;
    }
    eos_http_response_free(&resp);

    /* Finalize download */
    rc = eos_ota_finish();
    if (rc != 0) return rc;

    /* Verify integrity (SHA-256) */
    rc = eos_ota_verify();
    if (rc != 0) {
        printf("Verification failed -- aborting\n");
        eos_ota_abort();
        return rc;
    }

    /* Apply: set update slot as active */
    rc = eos_ota_apply();
    if (rc != 0) return rc;

    printf("Update applied -- rebooting...\n");
    /* System reboot triggered by eos_ota_apply */
    return 0;
}
```

### 13.6.3 Progress Monitoring

```c
void progress_handler(uint8_t pct, void *ctx)
{
    printf("OTA progress: %u%%\n", pct);
    /* Update LED, display, or MQTT telemetry */
}

/* Register callback */
eos_ota_set_progress_callback(progress_handler, NULL);
```

### 13.6.4 Status Query

```c
eos_ota_status_t status;
eos_ota_get_status(&status);

printf("State:    %d\n", status.state);
printf("Progress: %u%%\n", status.progress_pct);
printf("Current:  v%s (Slot %c)\n",
       status.current_version,
       status.active_slot == EOS_OTA_SLOT_A ? 'A' : 'B');
printf("Update:   v%s (Slot %c)\n",
       status.update_version,
       status.update_slot == EOS_OTA_SLOT_A ? 'A' : 'B');
```

## 13.7 Rollback

If the new firmware fails self-tests after reboot, the system can
roll back to the previous version:

```c
void first_boot_check(void)
{
    /* Run self-tests */
    if (selftest_passed()) {
        /* Mark current slot as valid */
        eos_ota_slot_t active = eos_ota_get_active_slot();
        eos_ota_mark_slot_valid(active);
        printf("Boot validated\n");
    } else {
        printf("Self-test failed -- rolling back\n");
        eos_ota_rollback();
        /* Reboots into previous slot */
    }
}
```

### 13.7.1 Rollback Safety Flow

```
  New firmware boots
         |
         v
  +--------------+     YES    +--------------------+
  | Self-tests   |---------->| mark_slot_valid()  |
  | passed?      |           | Commit new version |
  +------+-------+           +--------------------+
         | NO
         v
  +--------------+
  | eos_ota_     |
  | rollback()   |---> Reboot into previous slot
  +--------------+
```

## 13.8 OTA with MQTT Trigger

```c
void on_ota_command(const char *topic, const uint8_t *payload,
                    size_t len, void *ctx)
{
    /* Parse JSON manifest */
    eos_ota_source_t source;
    parse_ota_manifest(payload, len, &source);

    /* Perform update in background task */
    perform_update(&source);
}

void setup_ota_listener(eos_mqtt_handle_t mqtt)
{
    eos_mqtt_subscribe(mqtt, "device/ota/update", 1,
                       on_ota_command, NULL);
}
```

## 13.9 Security Considerations

1. **Always use TLS** -- Set `use_tls = true` for production downloads
   to prevent man-in-the-middle attacks.
2. **Verify before apply** -- Never call `eos_ota_apply()` without a
   successful `eos_ota_verify()` return.
3. **Anti-rollback** -- Maintain a monotonic version counter in OTP
   memory to prevent downgrade attacks.
4. **Signed manifests** -- Verify the update manifest signature before
   trusting the `expected_sha256` field.

## 13.10 API Reference Summary

| Function                        | Description                      |
|---------------------------------|----------------------------------|
| `eos_ota_init`                  | Initialize OTA subsystem         |
| `eos_ota_deinit`                | Shutdown OTA subsystem           |
| `eos_ota_check_update`          | Check if update is available     |
| `eos_ota_begin`                 | Start a new update               |
| `eos_ota_write_chunk`           | Write firmware data chunk        |
| `eos_ota_finish`                | Finalize download                |
| `eos_ota_abort`                 | Abort current update             |
| `eos_ota_verify`                | Verify firmware integrity        |
| `eos_ota_apply`                 | Apply update and reboot          |
| `eos_ota_rollback`              | Rollback to previous firmware    |
| `eos_ota_get_status`            | Query current OTA status         |
| `eos_ota_set_progress_callback` | Register progress callback       |
| `eos_ota_get_active_slot`       | Get currently active slot        |
| `eos_ota_set_active_slot`       | Force a specific slot active     |
| `eos_ota_mark_slot_valid`       | Mark slot as valid after boot    |

---

*Next: [Chapter 14 -- Sensor and Motor Frameworks](ch14-sensor-motor.md)*
