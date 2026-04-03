# Secure OTA — Firmware Update with Verification

Demonstrates a complete over-the-air firmware update flow: check for updates, download, verify SHA-256 integrity, apply to the inactive A/B slot, and reboot. Includes a watchdog task for safety.

## What it demonstrates

- OTA update lifecycle: check → download → verify → apply → reboot
- A/B slot management for safe firmware updates
- SHA-256 integrity verification before applying
- Rollback on verification failure
- Progress callback for download monitoring
- Watchdog task to prevent hang during update

## Update flow

```
┌─────────────┐     ┌──────────┐     ┌──────────┐     ┌─────────┐     ┌────────┐
│ Check Update├────►│ Download ├────►│  Verify  ├────►│  Apply  ├────►│ Reboot │
└─────────────┘     └──────────┘     └────┬─────┘     └─────────┘     └────────┘
                                          │ FAIL
                                     ┌────▼─────┐
                                     │ Rollback │
                                     └──────────┘
```

## Modules used

| Module | Header | Functions |
|--------|--------|-----------|
| OTA | `eos/ota.h` | `eos_ota_init`, `eos_ota_check_update`, `eos_ota_begin`, `eos_ota_finish`, `eos_ota_verify`, `eos_ota_apply`, `eos_ota_rollback` |
| Crypto | `eos/crypto.h` | Used internally by OTA verify |
| Kernel | `eos/kernel.h` | `eos_kernel_init`, `eos_task_create`, `eos_task_delay_ms` |
| OS Services | `eos/os_services.h` | Watchdog management |

## How to build

```bash
cmake -B build -DEOS_PRODUCT=gateway
cmake --build build
./build/secure-ota
```

## Expected output

```
[main] Current firmware: 1.0.0 (slot A)
[main] Starting kernel...
[ota] OTA task started, checking every 30 seconds
[watchdog] Watchdog task started
[watchdog] Feeding watchdog
[ota] Update available: 2.0.0
[ota] Download progress: 10%
[ota] Download progress: 50%
[ota] Download progress: 100%
[ota] Verifying firmware integrity...
[ota] Verification passed
[ota] Applying update...
[ota] Update applied successfully!
[ota]   Current: 1.0.0 (slot A)
[ota]   Next boot: 2.0.0 (slot B)
[ota] Rebooting in 3 seconds...
[ota] === REBOOT ===
```
