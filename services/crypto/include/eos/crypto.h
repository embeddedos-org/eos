// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file crypto.h
 * @brief EoS Cryptographic Services
 *
 * Provides hash, checksum, symmetric encryption, and asymmetric
 * key operations for both build-time integrity and runtime use.
 */

#ifndef EOS_CRYPTO_H
#define EOS_CRYPTO_H

#include <stdint.h>
#include <stddef.h>

/* ---- SHA-256 ---- */

#define EOS_SHA256_DIGEST_SIZE  32
#define EOS_SHA256_BLOCK_SIZE   64
#define EOS_SHA256_HEX_SIZE     65

typedef struct {
    uint32_t state[8];
    uint8_t  buf[64];
    uint64_t total;
} EosSha256;

void eos_sha256_init(EosSha256 *ctx);
void eos_sha256_update(EosSha256 *ctx, const void *data, size_t len);
void eos_sha256_final(EosSha256 *ctx, uint8_t digest[EOS_SHA256_DIGEST_SIZE]);
void eos_sha256_hex(const uint8_t digest[EOS_SHA256_DIGEST_SIZE],
                    char hex[EOS_SHA256_HEX_SIZE]);
void eos_sha256_data(const void *data, size_t len,
                     char hex[EOS_SHA256_HEX_SIZE]);
int  eos_sha256_file(const char *path, char hex[EOS_SHA256_HEX_SIZE]);

/* ---- SHA-512 ---- */

#define EOS_SHA512_DIGEST_SIZE  64
#define EOS_SHA512_BLOCK_SIZE   128
#define EOS_SHA512_HEX_SIZE     129

typedef struct {
    uint64_t state[8];
    uint8_t  buf[128];
    uint64_t total;
} EosSha512;

void eos_sha512_init(EosSha512 *ctx);
void eos_sha512_update(EosSha512 *ctx, const void *data, size_t len);
void eos_sha512_final(EosSha512 *ctx, uint8_t digest[EOS_SHA512_DIGEST_SIZE]);
void eos_sha512_hex(const uint8_t digest[EOS_SHA512_DIGEST_SIZE],
                    char hex[EOS_SHA512_HEX_SIZE]);

/* ---- CRC32 / CRC64 ---- */

uint32_t eos_crc32(uint32_t crc, const void *data, size_t len);
uint32_t eos_crc32_file(const char *path);
uint64_t eos_crc64(uint64_t crc, const void *data, size_t len);

/* ---- AES-128 / AES-256 ---- */

#define EOS_AES_BLOCK_SIZE  16
#define EOS_AES128_KEY_SIZE 16
#define EOS_AES256_KEY_SIZE 32
#define EOS_AES_MAX_ROUNDS  14

typedef struct {
    uint32_t rk[60];
    int nr;
} EosAesCtx;

void eos_aes_init(EosAesCtx *ctx, const uint8_t *key, int key_bits);
void eos_aes_encrypt_block(const EosAesCtx *ctx,
                           const uint8_t in[EOS_AES_BLOCK_SIZE],
                           uint8_t out[EOS_AES_BLOCK_SIZE]);
void eos_aes_decrypt_block(const EosAesCtx *ctx,
                           const uint8_t in[EOS_AES_BLOCK_SIZE],
                           uint8_t out[EOS_AES_BLOCK_SIZE]);
void eos_aes_cbc_encrypt(const EosAesCtx *ctx, const uint8_t *iv,
                         const uint8_t *in, uint8_t *out, size_t len);
void eos_aes_cbc_decrypt(const EosAesCtx *ctx, const uint8_t *iv,
                         const uint8_t *in, uint8_t *out, size_t len);

/* ---- RSA (key generation stubs + sign/verify) ---- */

#define EOS_RSA_MAX_KEY_BYTES 512

typedef struct {
    uint8_t n[EOS_RSA_MAX_KEY_BYTES];
    uint8_t e[EOS_RSA_MAX_KEY_BYTES];
    uint8_t d[EOS_RSA_MAX_KEY_BYTES];
    int key_bits;
    int has_private;
} EosRsaKey;

int eos_rsa_sign_sha256(const EosRsaKey *key, const uint8_t hash[32],
                        uint8_t *sig, size_t *sig_len);
int eos_rsa_verify_sha256(const EosRsaKey *key, const uint8_t hash[32],
                          const uint8_t *sig, size_t sig_len);

/* ---- ECC (placeholder interface) ---- */

typedef struct {
    uint8_t pub[64];
    uint8_t priv[32];
    int curve;
} EosEccKey;

int eos_ecc_sign(const EosEccKey *key, const uint8_t *hash, size_t hash_len,
                 uint8_t *sig, size_t *sig_len);
int eos_ecc_verify(const EosEccKey *key, const uint8_t *hash, size_t hash_len,
                   const uint8_t *sig, size_t sig_len);

#endif /* EOS_CRYPTO_H */
