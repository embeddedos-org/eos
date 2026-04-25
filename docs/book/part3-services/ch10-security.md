# Chapter 10: Security Services

**Author:** Srikanth Patchava & EmbeddedOS Contributors

---

## 10.1 Overview

The EoS security subsystem (`eos/security.h`) provides four pillars of
embedded security: **secure boot**, **firmware signing**, **key management**,
and **access control**. Together these services form a chain of trust from
power-on through runtime operation.

```
┌────────────────────────────────────────────────────────────┐
│                     Application                            │
├────────────┬────────────┬──────────────┬───────────────────┤
│ Secure Boot│  Firmware  │  Key Store   │  Access Control   │
│ Verify     │  Signing   │  (AES/RSA/   │  (ACL Rules)      │
│            │            │   ECC keys)  │                   │
├────────────┴────────────┴──────────────┴───────────────────┤
│                    <eos/security.h>                         │
├────────────────────────────────────────────────────────────┤
│                    <eos/crypto.h>                           │
└────────────────────────────────────────────────────────────┘
```

## 10.2 Secure Boot

Secure boot verifies the integrity and authenticity of firmware images
before they execute. The EoS boot verifier checks a cryptographic hash
and validates a digital signature against a trusted public key.

### 10.2.1 Boot Status States

| State                  | Value | Description                        |
|------------------------|-------|------------------------------------|
| `EOS_BOOT_UNVERIFIED`  | 0     | Image has not been checked         |
| `EOS_BOOT_VERIFIED`    | 1     | Image passed verification          |
| `EOS_BOOT_FAILED`      | 2     | Verification failed                |
| `EOS_BOOT_SKIPPED`     | 3     | Verification intentionally skipped |

### 10.2.2 Secure Boot Structure

```c
typedef struct {
    char image_path[512];      /* Path to firmware image       */
    char sig_path[512];        /* Path to detached signature   */
    char pubkey_path[512];     /* Path to public key file      */
    char hash_algo[32];        /* Hash algorithm name          */
    EosBootStatus status;      /* Current verification status  */
    char hash[129];            /* Computed hash (hex string)   */
} EosSecureBoot;
```

### 10.2.3 Verification Workflow

```c
#include <eos/security.h>

int boot_verify(void)
{
    EosSecureBoot sb;
    eos_secureboot_init(&sb);

    int rc = eos_secureboot_verify_image(
        &sb,
        "/flash/app.bin",          /* firmware image   */
        "/flash/app.bin.sig",      /* detached sig     */
        "/flash/boot_pubkey.pem"   /* trusted pub key  */
    );

    eos_secureboot_dump(&sb);      /* print status     */

    if (sb.status == EOS_BOOT_VERIFIED) {
        printf("Boot: image verified — hash: %s\n", sb.hash);
        return 0;
    }
    printf("Boot: VERIFICATION FAILED\n");
    return -1;
}
```

### 10.2.4 Boot Verification Flow

```
  Power-On
     │
     ▼
┌──────────────┐
│ Load image   │
│ from flash   │
└──────┬───────┘
       │
       ▼
┌──────────────┐     ┌──────────────┐
│ Compute hash │────▶│ Load .sig    │
│ (SHA-256)    │     │ from flash   │
└──────┬───────┘     └──────┬───────┘
       │                    │
       ▼                    ▼
┌──────────────────────────────────┐
│   eos_secureboot_verify_image()  │
│   RSA/ECC verify(hash, sig, pk) │
└──────────────┬───────────────────┘
       ┌───────┴───────┐
       ▼               ▼
   VERIFIED          FAILED
   Jump to app    Halt / fallback
```

## 10.3 Firmware Signing

The firmware signing API generates signed firmware packages for
distribution and OTA updates.

### 10.3.1 Signed Firmware Structure

```c
typedef struct {
    char input_path[512];      /* Source firmware path         */
    char output_path[512];     /* Signed output path          */
    char key_path[512];        /* Private key path            */
    char hash[129];            /* Computed SHA-256 hash        */
    char signature[1024];      /* Generated signature bytes    */
    size_t sig_len;            /* Signature length             */
    int signed_ok;             /* 1 if signing succeeded       */
} EosSignedFirmware;
```

### 10.3.2 Signing a Firmware Image

```c
EosSignedFirmware sf;
int rc = eos_firmware_sign(
    &sf,
    "/build/firmware.bin",       /* input image     */
    "/keys/signing_key.pem",     /* private key     */
    "/release/firmware.signed"   /* output package  */
);

if (rc == 0 && sf.signed_ok) {
    printf("Signed: hash=%s sig_len=%zu\n", sf.hash, sf.sig_len);
}
```

### 10.3.3 Verifying a Signed Firmware

```c
int rc = eos_firmware_verify_sig(
    "/release/firmware.signed",
    "/release/firmware.sig",
    "/keys/public_key.pem"
);
/* rc == 0 means valid */
```

## 10.4 Key Management (Keystore)

The keystore provides persistent, typed storage for cryptographic keys.
It supports AES, RSA, and ECC key types with a simple ID-based lookup.

### 10.4.1 Supported Key Types

| Enum                | Algorithm   | Typical Key Size  |
|---------------------|-------------|-------------------|
| `EOS_KEY_AES128`    | AES-128     | 16 bytes          |
| `EOS_KEY_AES256`    | AES-256     | 32 bytes          |
| `EOS_KEY_RSA2048`   | RSA 2048    | ~256 bytes (n)    |
| `EOS_KEY_RSA4096`   | RSA 4096    | ~512 bytes (n)    |
| `EOS_KEY_ECC_P256`  | ECDSA P-256 | 32 bytes (priv)   |
| `EOS_KEY_ECC_P384`  | ECDSA P-384 | 48 bytes (priv)   |

### 10.4.2 Key Entry and Store

```c
typedef struct {
    char id[EOS_KEY_MAX_ID];       /* Key identifier string */
    EosKeyType type;               /* Algorithm type        */
    uint8_t data[512];             /* Raw key material      */
    size_t data_len;               /* Actual key length     */
    int active;                    /* 1 = active, 0 = revoked */
} EosKeyEntry;

typedef struct {
    EosKeyEntry keys[EOS_KEY_MAX_KEYS];  /* Key slots (max 32) */
    int count;                            /* Current key count  */
    char store_path[512];                 /* Persistent storage */
} EosKeyStore;
```

### 10.4.3 Keystore Operations

```c
#include <eos/security.h>

void keystore_example(void)
{
    EosKeyStore ks;
    eos_keystore_init(&ks, "/flash/keystore.bin");

    /* Load existing keys from flash */
    eos_keystore_load(&ks);

    /* Add a new AES-256 key */
    uint8_t aes_key[32] = { /* key material */ };
    eos_keystore_add(&ks, "sensor-encryption",
                     EOS_KEY_AES256, aes_key, 32);

    /* Add an ECC P-256 private key */
    uint8_t ecc_priv[32] = { /* private scalar */ };
    eos_keystore_add(&ks, "ota-signing",
                     EOS_KEY_ECC_P256, ecc_priv, 32);

    /* Look up a key by ID */
    const EosKeyEntry *entry = eos_keystore_find(&ks, "sensor-encryption");
    if (entry && entry->active) {
        printf("Found key: type=%d len=%zu\n", entry->type, entry->data_len);
    }

    /* Persist to flash */
    eos_keystore_save(&ks);

    /* Debug dump */
    eos_keystore_dump(&ks);
}
```

### 10.4.4 Keystore Limits

| Constant            | Value | Description                  |
|---------------------|-------|------------------------------|
| `EOS_KEY_MAX_ID`    | 64    | Max key identifier length    |
| `EOS_KEY_MAX_KEYS`  | 32    | Max keys in one store        |

## 10.5 Access Control (ACL)

The ACL subsystem implements a subject–resource–permission model for
controlling access to system resources at runtime.

### 10.5.1 ACL Rule Model

```
┌───────────┐     ┌──────────────┐     ┌────────────┐
│  Subject   │────▶│   Resource   │────▶│ Permission │──▶ ALLOW/DENY
│ "app:gps"  │     │ "/dev/uart1" │     │  "write"   │
└───────────┘     └──────────────┘     └────────────┘
```

Each ACL rule is a tuple of (subject, resource, permission, action):

```c
typedef struct {
    char subject[128];       /* Who: task name, module ID   */
    char resource[256];      /* What: device path, API name */
    char permission[64];     /* How: "read", "write", "exec"*/
    EosAclAction action;     /* EOS_ACL_ALLOW or DENY       */
} EosAclRule;
```

### 10.5.2 ACL Configuration

```c
#include <eos/security.h>

void configure_acl(void)
{
    EosAcl acl;
    eos_acl_init(&acl);

    /* Allow GPS app to read UART */
    eos_acl_add_rule(&acl, "app:gps", "/dev/uart1",
                     "read", EOS_ACL_ALLOW);

    /* Allow OTA service to write flash */
    eos_acl_add_rule(&acl, "svc:ota", "/dev/flash",
                     "write", EOS_ACL_ALLOW);

    /* Deny untrusted modules from accessing keys */
    eos_acl_add_rule(&acl, "app:*", "/keystore",
                     "read", EOS_ACL_DENY);

    /* Check access at runtime */
    EosAclAction result = eos_acl_check(
        &acl, "app:gps", "/dev/uart1", "read"
    );

    if (result == EOS_ACL_ALLOW) {
        printf("Access granted\n");
    } else {
        printf("Access denied\n");
    }

    eos_acl_dump(&acl);    /* Print all rules */
}
```

### 10.5.3 ACL Limits

| Constant             | Value | Description             |
|----------------------|-------|-------------------------|
| `EOS_ACL_MAX_RULES`  | 64    | Maximum rules per ACL   |

## 10.6 Audit and Integrity

While EoS does not include a dedicated audit log service, the building
blocks enable audit-style integrity monitoring:

```c
/* Periodic integrity check example */
void integrity_monitor(void)
{
    char current_hash[EOS_SHA256_HEX_SIZE];
    eos_sha256_file("/flash/app.bin", current_hash);

    /* Compare against known-good hash stored at build time */
    extern const char expected_hash[];
    if (strcmp(current_hash, expected_hash) != 0) {
        printf("ALERT: firmware tampered!\n");
        /* Trigger secure reboot or lockdown */
    }
}
```

## 10.7 Security Architecture — Putting It All Together

```
  ┌──────────────────────────────────────────────────────┐
  │                     Boot ROM                         │
  │              (immutable root of trust)               │
  └─────────────────────┬────────────────────────────────┘
                        │ verify
  ┌─────────────────────▼────────────────────────────────┐
  │              Bootloader (Stage 1)                    │
  │     eos_secureboot_verify_image(stage2)              │
  └─────────────────────┬────────────────────────────────┘
                        │ verify
  ┌─────────────────────▼────────────────────────────────┐
  │              Application (Stage 2)                   │
  │     eos_keystore_load() → keys available             │
  │     eos_acl_init()      → access policy active       │
  └──────────────────────────────────────────────────────┘
```

## 10.8 Best Practices

1. **Root of trust** — Store the boot verification public key in OTP
   (one-time programmable) memory or ROM, never in writable flash.
2. **Key rotation** — Use the keystore `active` flag to phase out old
   keys. Add a new key, mark the old one inactive, then `eos_keystore_save`.
3. **Least privilege** — Start with `EOS_ACL_DENY` as default and
   explicitly allow only required access paths.
4. **Defense in depth** — Combine secure boot (authenticity) with
   runtime integrity checks (periodic SHA-256 verification).
5. **Secure erasure** — Zero key material in RAM after use with
   `memset(key, 0, sizeof(key))` to limit exposure windows.

## 10.9 API Reference Summary

| Function                          | Description                        |
|-----------------------------------|------------------------------------|
| `eos_secureboot_init`             | Initialize secure boot context     |
| `eos_secureboot_verify_image`     | Verify image hash + signature      |
| `eos_secureboot_dump`             | Print boot verification status     |
| `eos_firmware_sign`               | Sign a firmware image              |
| `eos_firmware_verify_sig`         | Verify firmware signature          |
| `eos_keystore_init`              | Initialize keystore                |
| `eos_keystore_add`               | Add key to store                   |
| `eos_keystore_find`              | Look up key by ID                  |
| `eos_keystore_save`              | Persist keystore to flash          |
| `eos_keystore_load`              | Load keystore from flash           |
| `eos_keystore_dump`              | Print all key entries              |
| `eos_acl_init`                   | Initialize ACL                     |
| `eos_acl_add_rule`              | Add access control rule            |
| `eos_acl_check`                 | Check subject-resource permission  |
| `eos_acl_dump`                  | Print all ACL rules                |

---

*Next: [Chapter 11 — Networking](ch11-networking.md)*
