// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file security.h
 * @brief EoS Security Services
 *
 * Provides secure boot verification, signed firmware generation,
 * key management, and access control for embedded targets.
 */

#ifndef EOS_SECURITY_H
#define EOS_SECURITY_H

#include <stdint.h>
#include <stddef.h>

/* ---- Secure Boot ---- */

typedef enum {
    EOS_BOOT_UNVERIFIED,
    EOS_BOOT_VERIFIED,
    EOS_BOOT_FAILED,
    EOS_BOOT_SKIPPED
} EosBootStatus;

typedef struct {
    char image_path[512];
    char sig_path[512];
    char pubkey_path[512];
    char hash_algo[32];
    EosBootStatus status;
    char hash[129];
} EosSecureBoot;

void eos_secureboot_init(EosSecureBoot *sb);
int  eos_secureboot_verify_image(EosSecureBoot *sb, const char *image_path,
                                 const char *sig_path, const char *pubkey_path);
void eos_secureboot_dump(const EosSecureBoot *sb);

/* ---- Signed Firmware ---- */

typedef struct {
    char input_path[512];
    char output_path[512];
    char key_path[512];
    char hash[129];
    char signature[1024];
    size_t sig_len;
    int signed_ok;
} EosSignedFirmware;

int eos_firmware_sign(EosSignedFirmware *sf, const char *fw_path,
                      const char *key_path, const char *output_path);
int eos_firmware_verify_sig(const char *fw_path, const char *sig_path,
                            const char *pubkey_path);

/* ---- Key Management ---- */

#define EOS_KEY_MAX_ID   64
#define EOS_KEY_MAX_KEYS 32

typedef enum {
    EOS_KEY_AES128,
    EOS_KEY_AES256,
    EOS_KEY_RSA2048,
    EOS_KEY_RSA4096,
    EOS_KEY_ECC_P256,
    EOS_KEY_ECC_P384
} EosKeyType;

typedef struct {
    char id[EOS_KEY_MAX_ID];
    EosKeyType type;
    uint8_t data[512];
    size_t data_len;
    int active;
} EosKeyEntry;

typedef struct {
    EosKeyEntry keys[EOS_KEY_MAX_KEYS];
    int count;
    char store_path[512];
} EosKeyStore;

void eos_keystore_init(EosKeyStore *ks, const char *path);
int  eos_keystore_add(EosKeyStore *ks, const char *id, EosKeyType type,
                      const uint8_t *data, size_t len);
const EosKeyEntry *eos_keystore_find(const EosKeyStore *ks, const char *id);
int  eos_keystore_save(const EosKeyStore *ks);
int  eos_keystore_load(EosKeyStore *ks);
void eos_keystore_dump(const EosKeyStore *ks);

/* ---- Access Control ---- */

#define EOS_ACL_MAX_RULES 64

typedef enum {
    EOS_ACL_ALLOW,
    EOS_ACL_DENY
} EosAclAction;

typedef struct {
    char subject[128];
    char resource[256];
    char permission[64];
    EosAclAction action;
} EosAclRule;

typedef struct {
    EosAclRule rules[EOS_ACL_MAX_RULES];
    int count;
} EosAcl;

void eos_acl_init(EosAcl *acl);
int  eos_acl_add_rule(EosAcl *acl, const char *subject, const char *resource,
                      const char *permission, EosAclAction action);
EosAclAction eos_acl_check(const EosAcl *acl, const char *subject,
                           const char *resource, const char *permission);
void eos_acl_dump(const EosAcl *acl);

#endif /* EOS_SECURITY_H */
