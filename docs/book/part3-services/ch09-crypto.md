# Chapter 9: Cryptographic Services

**Author:** Srikanth Patchava & EmbeddedOS Contributors

---

## 9.1 Overview


![Figure: EoS Platform Services — crypto, filesystem, networking, OTA, UI, security](images/services-overview.png)

The EoS cryptographic subsystem (`eos/crypto.h`) provides hash functions,
checksums, symmetric encryption, and asymmetric key operations suitable for
both build-time integrity verification and runtime security. Every algorithm
is implemented in portable C with no external dependencies — critical for
bare-metal and resource-constrained targets.

```
┌──────────────────────────────────────────────────────────┐
│                   Application Layer                      │
├──────────┬──────────┬──────────┬──────────┬──────────────┤
│ SHA-256  │ SHA-512  │   AES    │   RSA    │  ECC P-256   │
│          │          │ CBC/ECB  │ Sign/Ver │  Sign/Verify │
├──────────┴──────────┼──────────┴──────────┴──────────────┤
│   CRC-32 / CRC-64   │        Big-Integer Core           │
├──────────────────────┴───────────────────────────────────┤
│                   <eos/crypto.h>                         │
└──────────────────────────────────────────────────────────┘
```

## 9.2 Hash Functions — SHA-256

SHA-256 produces a 256-bit (32-byte) digest and is the primary hash used
throughout EoS for firmware signing, secure boot, and OTA verification.

### 9.2.1 Constants and Context

| Constant                | Value | Description              |
|-------------------------|-------|--------------------------|
| `EOS_SHA256_DIGEST_SIZE`| 32    | Digest size in bytes     |
| `EOS_SHA256_BLOCK_SIZE` | 64    | Internal block size      |
| `EOS_SHA256_HEX_SIZE`   | 65    | Hex string + NUL         |

```c
typedef struct {
    uint32_t state[8];   /* Running hash state (H0–H7) */
    uint8_t  buf[64];    /* Partial block buffer        */
    uint64_t total;      /* Total bytes hashed          */
} EosSha256;
```

### 9.2.2 Streaming API

The streaming interface processes data in arbitrarily sized chunks:

```c
EosSha256 ctx;
uint8_t digest[EOS_SHA256_DIGEST_SIZE];
char hex[EOS_SHA256_HEX_SIZE];

eos_sha256_init(&ctx);
eos_sha256_update(&ctx, block1, sizeof(block1));
eos_sha256_update(&ctx, block2, sizeof(block2));
eos_sha256_final(&ctx, digest);
eos_sha256_hex(digest, hex);

printf("SHA-256: %s\n", hex);
```

### 9.2.3 Convenience Helpers

For common one-shot operations:

```c
/* Hash a memory buffer in one call */
char hex[EOS_SHA256_HEX_SIZE];
eos_sha256_data(firmware_blob, firmware_size, hex);

/* Hash a file on the filesystem */
char file_hex[EOS_SHA256_HEX_SIZE];
if (eos_sha256_file("/flash/app.bin", file_hex) == 0) {
    printf("File hash: %s\n", file_hex);
}
```

## 9.3 Hash Functions — SHA-512

SHA-512 uses 64-bit arithmetic and produces a 512-bit (64-byte) digest.
It is preferred when higher collision resistance is needed or the target
CPU has native 64-bit ALU support.

| Constant                | Value | Description              |
|-------------------------|-------|--------------------------|
| `EOS_SHA512_DIGEST_SIZE`| 64    | Digest size in bytes     |
| `EOS_SHA512_BLOCK_SIZE` | 128   | Internal block size      |
| `EOS_SHA512_HEX_SIZE`   | 129   | Hex string + NUL         |

```c
typedef struct {
    uint64_t state[8];   /* Running hash state */
    uint8_t  buf[128];   /* Partial block      */
    uint64_t total;      /* Total bytes hashed */
} EosSha512;
```

The API mirrors SHA-256:

```c
EosSha512 ctx;
uint8_t digest[EOS_SHA512_DIGEST_SIZE];
char hex[EOS_SHA512_HEX_SIZE];

eos_sha512_init(&ctx);
eos_sha512_update(&ctx, data, len);
eos_sha512_final(&ctx, digest);
eos_sha512_hex(digest, hex);
```

### 9.3.1 SHA-256 vs SHA-512 Comparison

| Property          | SHA-256       | SHA-512       |
|-------------------|---------------|---------------|
| Digest size       | 32 bytes      | 64 bytes      |
| Block size        | 64 bytes      | 128 bytes     |
| Word size         | 32-bit        | 64-bit        |
| Rounds            | 64            | 80            |
| Context RAM       | ~112 bytes    | ~216 bytes    |
| Best for          | Cortex-M      | 64-bit cores  |

## 9.4 Cyclic Redundancy Checks — CRC-32 / CRC-64

CRC functions provide fast error-detection checksums for data integrity
without the computational overhead of cryptographic hashes.

### 9.4.1 API

```c
/* Incremental CRC-32 */
uint32_t crc = 0;
crc = eos_crc32(crc, chunk1, len1);
crc = eos_crc32(crc, chunk2, len2);
printf("CRC-32: 0x%08X\n", crc);

/* File CRC-32 */
uint32_t file_crc = eos_crc32_file("/flash/config.bin");

/* CRC-64 for large datasets */
uint64_t crc64 = 0;
crc64 = eos_crc64(crc64, data, data_len);
```

### 9.4.2 When to Use CRC vs SHA

| Criterion          | CRC-32/64          | SHA-256            |
|--------------------|--------------------|--------------------|
| Speed              | Very fast          | Moderate           |
| Security           | None (detectable)  | Cryptographic      |
| Use case           | Bus errors, flash  | Firmware signing   |
| Collision resist.  | Weak               | Strong             |

## 9.5 Symmetric Encryption — AES

EoS provides AES-128 and AES-256 in both ECB (single-block) and CBC
(chained) modes.

### 9.5.1 Constants

| Constant              | Value | Description               |
|-----------------------|-------|---------------------------|
| `EOS_AES_BLOCK_SIZE`  | 16    | Block size in bytes       |
| `EOS_AES128_KEY_SIZE` | 16    | AES-128 key size          |
| `EOS_AES256_KEY_SIZE` | 32    | AES-256 key size          |
| `EOS_AES_MAX_ROUNDS`  | 14    | Max key-schedule rounds   |

### 9.5.2 Key Expansion

```c
typedef struct {
    uint32_t rk[60];   /* Expanded round keys */
    int nr;            /* Number of rounds    */
} EosAesCtx;
```

Initialize the context once, then encrypt/decrypt multiple blocks:

```c
EosAesCtx aes;
uint8_t key[EOS_AES256_KEY_SIZE] = { /* 32 bytes */ };

eos_aes_init(&aes, key, 256);   /* 128 or 256 */
```

### 9.5.3 ECB Mode (Single Block)

ECB encrypts each 16-byte block independently — suitable only for
single-block operations like key wrapping:

```c
uint8_t plaintext[16] = "Hello, AES-256!";
uint8_t ciphertext[16];
uint8_t decrypted[16];

eos_aes_encrypt_block(&aes, plaintext, ciphertext);
eos_aes_decrypt_block(&aes, ciphertext, decrypted);
```

> **Warning:** Never use ECB for multi-block data — identical plaintext
> blocks produce identical ciphertext, leaking patterns.

### 9.5.4 CBC Mode (Chained Blocks)

CBC chains blocks through an IV, providing semantic security:

```c
uint8_t iv[16] = { /* random 16 bytes */ };
uint8_t data[64];    /* must be multiple of 16 */
uint8_t enc[64], dec[64];

eos_aes_cbc_encrypt(&aes, iv, data, enc, sizeof(data));
eos_aes_cbc_decrypt(&aes, iv, enc,  dec, sizeof(enc));
```

```
┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐
│   IV    │   │  P[0]   │   │  P[1]   │   │  P[2]   │
└────┬────┘   └────┬────┘   └────┬────┘   └────┬────┘
     │  XOR ◄──────┘             │              │
     ▼                           ▼              ▼
 ┌───────┐    ┌──── XOR ◄──┐  ┌──── XOR ◄──┐
 │  AES  │    │   │  AES  │ │  │   │  AES  │ │
 └───┬───┘    └───┬───────┘ │  └───┬───────┘ │
     │            │         │      │         │
     ▼            ▼         │      ▼         │
   C[0] ─────────►──────────┘    C[1] ──────►┘
```

## 9.6 Asymmetric Cryptography — RSA

EoS includes a minimal RSA implementation for firmware signing and
verification with SHA-256 digests.

### 9.6.1 Key Structure

```c
#define EOS_RSA_MAX_KEY_BYTES 512

typedef struct {
    uint8_t n[EOS_RSA_MAX_KEY_BYTES];   /* Modulus           */
    uint8_t e[EOS_RSA_MAX_KEY_BYTES];   /* Public exponent   */
    uint8_t d[EOS_RSA_MAX_KEY_BYTES];   /* Private exponent  */
    int key_bits;                        /* 2048 or 4096      */
    int has_private;                     /* 1 if d is valid   */
} EosRsaKey;
```

### 9.6.2 Sign and Verify

```c
EosRsaKey key;
/* ... load key from keystore ... */

uint8_t hash[32];
eos_sha256_data(firmware, fw_len, NULL);
/* compute raw digest into hash[] */

uint8_t sig[512];
size_t sig_len = sizeof(sig);

/* Sign (requires private key) */
int rc = eos_rsa_sign_sha256(&key, hash, sig, &sig_len);

/* Verify (public key only) */
rc = eos_rsa_verify_sha256(&key, hash, sig, sig_len);
if (rc == 0) {
    printf("Signature valid\n");
}
```

## 9.7 Elliptic Curve Cryptography — ECDSA P-256

ECC provides equivalent security to RSA with much smaller keys,
making it ideal for constrained devices.

### 9.7.1 Key Structure

```c
typedef struct {
    uint8_t pub[64];    /* Uncompressed public key (x, y) */
    uint8_t priv[32];   /* Private scalar                 */
    int curve;          /* Curve identifier (P-256 = 0)   */
} EosEccKey;
```

### 9.7.2 Sign and Verify

```c
EosEccKey ecc_key;
/* ... load or generate key ... */

uint8_t hash[32];
uint8_t sig[72];   /* DER-encoded ECDSA signature */
size_t sig_len = sizeof(sig);

int rc = eos_ecc_sign(&ecc_key, hash, 32, sig, &sig_len);

rc = eos_ecc_verify(&ecc_key, hash, 32, sig, sig_len);
```

### 9.7.3 RSA vs ECC Comparison

| Property          | RSA-2048      | ECDSA P-256    |
|-------------------|---------------|----------------|
| Key size (bits)   | 2048          | 256            |
| Signature size    | 256 bytes     | ~64 bytes      |
| RAM usage         | ~2 KB         | ~200 bytes     |
| Verify speed      | Moderate      | Faster         |
| Standard          | PKCS#1 v2.1   | FIPS 186-4     |

## 9.8 Algorithm Selection Guide

```
                    ┌──────────────────────┐
                    │  What do you need?   │
                    └──────────┬───────────┘
              ┌────────────────┼────────────────┐
              ▼                ▼                ▼
        ┌──────────┐    ┌──────────┐    ┌──────────────┐
        │ Integrity│    │ Encrypt  │    │  Authenticate│
        │  Check   │    │  Data    │    │  / Sign      │
        └────┬─────┘    └────┬─────┘    └──────┬───────┘
             │               │                 │
      ┌──────┴──────┐       │          ┌───────┴───────┐
      ▼             ▼       ▼          ▼               ▼
  ┌───────┐   ┌────────┐ ┌─────┐  ┌───────┐     ┌─────────┐
  │CRC-32 │   │SHA-256 │ │ AES │  │  RSA  │     │ECC P-256│
  │(fast) │   │(secure)│ │ CBC │  │(compat)│    │(compact)│
  └───────┘   └────────┘ └─────┘  └───────┘     └─────────┘
```

## 9.9 Complete Example: Firmware Integrity Pipeline

```c
#include <eos/crypto.h>
#include <stdio.h>

int verify_firmware(const char *fw_path, const EosRsaKey *pub_key)
{
    /* Step 1: Compute SHA-256 of firmware image */
    char hex[EOS_SHA256_HEX_SIZE];
    if (eos_sha256_file(fw_path, hex) != 0) {
        return -1;
    }
    printf("Firmware SHA-256: %s\n", hex);

    /* Step 2: Also verify CRC for transport errors */
    uint32_t crc = eos_crc32_file(fw_path);
    printf("CRC-32: 0x%08X\n", crc);

    /* Step 3: Verify RSA signature */
    uint8_t digest[EOS_SHA256_DIGEST_SIZE];
    EosSha256 ctx;
    eos_sha256_init(&ctx);
    /* ... feed firmware data ... */
    eos_sha256_final(&ctx, digest);

    uint8_t sig[512];
    size_t sig_len = 256;
    /* ... load sig from .sig file ... */

    if (eos_rsa_verify_sha256(pub_key, digest, sig, sig_len) != 0) {
        printf("ERROR: Signature verification failed!\n");
        return -2;
    }

    printf("Firmware verified successfully.\n");
    return 0;
}
```

## 9.10 Security Considerations

1. **Key storage** — Never store private keys in plaintext flash. Use the
   keystore service (Chapter 10) with encrypted storage.
2. **Side channels** — The software AES implementation is not constant-time
   on all architectures. For high-security targets, consider hardware AES.
3. **Random numbers** — EoS does not currently bundle a CSPRNG. Seed keys
   from a hardware entropy source (TRNG) where available.
4. **Algorithm agility** — Use ECC P-256 for new designs. RSA is provided
   for backward compatibility with existing PKI infrastructure.

## 9.11 API Reference Summary

| Function                   | Description                          |
|----------------------------|--------------------------------------|
| `eos_sha256_init`          | Initialize SHA-256 context           |
| `eos_sha256_update`        | Feed data into hash                  |
| `eos_sha256_final`         | Finalize and output digest           |
| `eos_sha256_hex`           | Convert digest to hex string         |
| `eos_sha256_data`          | One-shot hash of buffer              |
| `eos_sha256_file`          | One-shot hash of file                |
| `eos_sha512_init/update/final` | SHA-512 streaming API            |
| `eos_sha512_hex`           | SHA-512 digest to hex                |
| `eos_crc32`                | Incremental CRC-32                   |
| `eos_crc32_file`           | CRC-32 of a file                     |
| `eos_crc64`                | Incremental CRC-64                   |
| `eos_aes_init`             | Expand AES key schedule              |
| `eos_aes_encrypt_block`    | Encrypt single 16-byte block         |
| `eos_aes_decrypt_block`    | Decrypt single 16-byte block         |
| `eos_aes_cbc_encrypt`      | AES-CBC encryption                   |
| `eos_aes_cbc_decrypt`      | AES-CBC decryption                   |
| `eos_rsa_sign_sha256`      | RSA-SHA256 signature                 |
| `eos_rsa_verify_sha256`    | RSA-SHA256 verification              |
| `eos_ecc_sign`             | ECDSA signature                      |
| `eos_ecc_verify`           | ECDSA verification                   |

---

*Next: [Chapter 10 — Security Services](ch10-security.md)*
