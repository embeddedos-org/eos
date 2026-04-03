# EoS Security Hardening Guide

This guide provides a production security checklist for deploying EoS on embedded targets. Every item should be verified before shipping a production firmware image.

---

## 1. Disable Debug UART in Production Builds

Debug UART output can leak sensitive runtime information (memory addresses, crypto state, error details).

```c
/* eos_config.h — production profile */
#define EOS_ENABLE_DEBUG_UART   0
#define EOS_ENABLE_CONSOLE      0
#define EOS_LOG_LEVEL           EOS_LOG_NONE
```

- Ensure `EOS_ENABLE_DEBUG_UART` is `0` in all production product profiles (`products/*.yaml`).
- Verify no serial output is present on UART pins during boot and runtime.

---

## 2. Enable MPU for Memory Protection

The Memory Protection Unit (MPU) enforces access permissions on memory regions, preventing stack overflows from corrupting code or data.

```c
#define EOS_ENABLE_MPU          1
```

Configuration checklist:
- [x] Flash region: read-only, executable
- [x] RAM data region: read-write, non-executable (XN)
- [x] Peripheral region: read-write, non-executable, non-cacheable
- [x] Stack guard region: no-access (triggers fault on overflow)
- [x] Kernel memory: privileged-access only

---

## 3. Stack Canaries

Enable compiler stack protection to detect buffer overflows at runtime.

```cmake
# CMakeLists.txt — production build
target_compile_options(eos PRIVATE -fstack-protector-strong)
```

- Use `-fstack-protector-strong` (protects functions with local arrays or address-taken variables).
- For maximum security on critical modules, use `-fstack-protector-all`.
- Ensure the `__stack_chk_fail` handler triggers a system reset or enters a safe state.

---

## 4. ASLR (Linux Targets)

For EoS builds targeting Linux-hosted environments (simulation, testing):

```bash
# Verify ASLR is enabled on the host
cat /proc/sys/kernel/randomize_va_space
# Should be 2 (full randomization)

# Compile with PIE
target_compile_options(eos PRIVATE -fPIE)
target_link_options(eos PRIVATE -pie)
```

- Not applicable to bare-metal Cortex-M targets (no MMU).
- On Cortex-A targets with Linux/RTOS+MMU, enable where supported.

---

## 5. Secure Boot Chain Verification

EoS uses a multi-stage boot chain (eboot stage0 → stage1 → EoS kernel):

```
ROM → stage0 (immutable) → verify(stage1) → verify(kernel) → verify(services)
```

Requirements:
- **stage0** must be in OTP/ROM — cannot be field-updated.
- **stage1** signature is verified by stage0 using the root public key burned into OTP.
- **Kernel image** signature is verified by stage1 before jump.
- **Rollback protection**: maintain a monotonic counter in OTP/secure storage; reject images with version ≤ current.
- **Key revocation**: support at least 2 key slots so a compromised key can be revoked without bricking devices.

---

## 6. Key Rotation Policy and Procedures

| Key Type | Rotation Interval | Storage |
|----------|-------------------|---------|
| Firmware signing key | Annual or on compromise | HSM / air-gapped machine |
| TLS device certificate | 1 year validity, auto-renew | Secure element or TrustZone |
| OTA transport key | Per-session (ephemeral) | RAM only |
| Root of trust public key | Device lifetime (OTP) | OTP fuses |

Procedures:
1. Generate new key pair on HSM.
2. Sign a key rotation manifest with the **current** key.
3. Distribute the manifest via OTA; devices verify with current key, then install new key.
4. Revoke old key by incrementing the key revision counter.
5. Document the rotation event in the security audit log.

---

## 7. Least-Privilege ACL Defaults

Apply the principle of least privilege to all EoS services and tasks:

```c
/* Default service permissions — deny all, then grant */
#define EOS_DEFAULT_PERM        EOS_PERM_NONE

/* Example: OTA service needs network + flash write */
eos_service_set_permissions(ota_svc, EOS_PERM_NET | EOS_PERM_FLASH_WRITE);

/* Example: Sensor service needs only GPIO read */
eos_service_set_permissions(sensor_svc, EOS_PERM_GPIO_READ);
```

- No service should run with `EOS_PERM_ALL`.
- Kernel tasks run in privileged mode; all user services run in unprivileged mode.
- Peripheral access is gated by the MPU and permission bitmask.

---

## 8. Network Encryption

All network communications must use encryption in transit:

| Protocol | Minimum Version | Cipher Suite |
|----------|----------------|--------------|
| TLS | 1.2 (prefer 1.3) | AES-128-GCM, AES-256-GCM, ChaCha20-Poly1305 |
| DTLS | 1.2 | AES-128-CCM (constrained devices) |
| OTA transport | TLS 1.2+ | Mutual authentication required |

Configuration:
```c
#define EOS_TLS_MIN_VERSION     EOS_TLS_1_2
#define EOS_TLS_REQUIRE_MUTUAL_AUTH  1
#define EOS_TLS_CERT_VERIFY     1  /* Always verify server certificate */
```

- Disable SSLv3, TLS 1.0, TLS 1.1.
- Pin certificates or use a trusted CA bundle stored in secure storage.
- Log TLS handshake failures for security monitoring.

---

## 9. Firmware Signing and Verification

All firmware images must be cryptographically signed before distribution:

### Signing (build server / HSM)
```bash
# Sign firmware image with Ed25519 key
eos-sign --key firmware-signing.key --image eos-firmware.bin --output eos-firmware.bin.sig

# Generate signed OTA package
eos-ota-pack --image eos-firmware.bin --sig eos-firmware.bin.sig --version 1.2.3
```

### Verification (on device)
```c
/* eboot stage1 verifies kernel image before boot */
if (eos_image_verify(image_addr, image_len, signature, pub_key) != EOS_OK) {
    eos_boot_panic("Image verification failed");
}
```

- Use Ed25519 or ECDSA-P256 for firmware signatures.
- Include image version, build timestamp, and target board in the signed metadata.
- Reject images that fail verification — never boot unsigned code in production.

---

## 10. Disable JTAG/SWD in Production

Debug interfaces (JTAG, SWD) allow full device memory access and must be disabled in production:

```c
/* Disable debug port via option bytes / fuse bits */
#if defined(EOS_PRODUCTION_BUILD)
    eos_hal_disable_debug_port();  /* Sets JTAG/SWD lock bits */
#endif
```

- On STM32: set `RDP Level 1` (minimum) or `RDP Level 2` (permanent, irreversible).
- On nRF: enable `APPROTECT`.
- Document the debug lock level in the board configuration.
- Verify debug port is inaccessible with a probe after flashing production firmware.

---

## 11. Secure Key Storage

Cryptographic keys and secrets must never be stored in plaintext flash:

| Storage Method | Security Level | Use Case |
|----------------|---------------|----------|
| Secure Element (SE) | Highest | Device identity keys, TLS private keys |
| TrustZone secure partition | High | Session keys, OTA decryption keys |
| Encrypted flash (AES-wrapped) | Medium | Configuration secrets, API tokens |
| Plaintext flash | **Prohibited** | — |

Implementation:
```c
/* Store key in secure element */
eos_secure_store_key(KEY_SLOT_TLS, tls_private_key, key_len);

/* Retrieve key — never leaves SE for signing operations */
eos_secure_sign(KEY_SLOT_TLS, hash, hash_len, signature, &sig_len);
```

- Use hardware RNG (`eos_hal_random()`) for all key generation.
- Zeroize key material in RAM after use (`eos_secure_memzero()`).
- Never log or print key material, even in debug builds.

---

## 12. Runtime Integrity Checks

Detect tampering or corruption during operation:

### Code integrity
```c
/* Periodic CRC check of flash contents against known-good value */
eos_integrity_check_flash(EOS_FLASH_CODE_START, EOS_FLASH_CODE_SIZE, expected_crc);
```

### Stack integrity
```c
/* Stack watermark monitoring — detect near-overflow conditions */
uint32_t remaining = eos_task_stack_remaining(current_task);
if (remaining < EOS_STACK_CRITICAL_THRESHOLD) {
    eos_log_error("Stack critical: %u bytes remaining", remaining);
    eos_system_reset(EOS_RESET_STACK_OVERFLOW);
}
```

### Watchdog
```c
/* Hardware watchdog — resets device if firmware hangs */
#define EOS_ENABLE_WATCHDOG     1
#define EOS_WATCHDOG_TIMEOUT_MS 5000
```

### Anomaly detection
- Monitor task execution time — alert if a task takes >2× its WCET.
- Monitor heap fragmentation — alert if free memory drops below threshold.
- Log all reset reasons for post-mortem analysis.

---

## Production Build Checklist

Use this checklist before releasing a production firmware image:

- [ ] `EOS_ENABLE_DEBUG_UART` is `0`
- [ ] `EOS_ENABLE_CONSOLE` is `0`
- [ ] `EOS_ENABLE_MPU` is `1`
- [ ] `-fstack-protector-strong` is in compiler flags
- [ ] Secure boot chain is enabled and tested
- [ ] Firmware image is signed with production key
- [ ] JTAG/SWD debug port is locked
- [ ] All keys stored in secure element or encrypted
- [ ] TLS 1.2+ enforced for all network connections
- [ ] No service runs with `EOS_PERM_ALL`
- [ ] Watchdog is enabled
- [ ] Runtime integrity checks are active
- [ ] `EOS_LOG_LEVEL` is `EOS_LOG_ERROR` or `EOS_LOG_NONE`
- [ ] Rollback protection counter is set correctly
- [ ] SBOM is generated and archived for this build
