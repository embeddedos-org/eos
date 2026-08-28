# Secure OTA — Firmware Update with Verification

Demonstrates a complete over-the-air firmware update flow: check for updates, download, check SHA-256 integrity, authenticate the image, apply to the inactive A/B slot, and reboot. Includes a watchdog task for safety.

Integrity and authenticity are separate steps. `eos_ota_verify()` compares the download against the SHA-256 the update source declared, which catches a corrupted transfer and nothing more — that digest travels with the update, so anyone able to replace the image can replace the digest. `eos_ota_apply()` then calls the authenticator registered with `eos_ota_set_authenticator()`, which is where a signature check against a provisioned public key belongs. With no authenticator installed, `apply()` refuses.

## What it demonstrates

- OTA update lifecycle: check → download → integrity → authenticate → apply → reboot
- A/B slot management for safe firmware updates
- SHA-256 integrity check before applying
- A vendor-signature authenticator gating `apply()`
- Abort on integrity or authenticity failure
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
| OTA | `eos/ota.h` | `eos_ota_init`, `eos_ota_check_update`, `eos_ota_begin`, `eos_ota_finish`, `eos_ota_verify`, `eos_ota_set_authenticator`, `eos_ota_apply`, `eos_ota_rollback` |
| Crypto | `eos/crypto.h` | SHA-256 for the integrity check; the authenticator is where signature verification goes |
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
