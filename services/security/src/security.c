// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/security.h"
#include "eos/crypto.h"
#include <stdio.h>
#include <string.h>

/* ---- Secure Boot ---- */

void eos_secureboot_init(EosSecureBoot *sb) {
    memset(sb, 0, sizeof(*sb));
    strncpy(sb->hash_algo, "sha256", sizeof(sb->hash_algo) - 1);
    sb->status = EOS_BOOT_UNVERIFIED;
}

int eos_secureboot_verify_image(EosSecureBoot *sb, const char *image_path,
                                const char *sig_path, const char *pubkey_path) {
    strncpy(sb->image_path, image_path, sizeof(sb->image_path) - 1);
    strncpy(sb->sig_path, sig_path, sizeof(sb->sig_path) - 1);
    strncpy(sb->pubkey_path, pubkey_path, sizeof(sb->pubkey_path) - 1);

    /* Hash the image */
    char hex[EOS_SHA256_HEX_SIZE];
    if (eos_sha256_file(image_path, hex) != 0) {
        sb->status = EOS_BOOT_FAILED;
        return -1;
    }
    strncpy(sb->hash, hex, sizeof(sb->hash) - 1);

    /* Read signature file */
    FILE *fp = fopen(sig_path, "rb");
    if (!fp) {
        sb->status = EOS_BOOT_FAILED;
        return -1;
    }
    uint8_t sig_buf[512];
    size_t sig_len = fread(sig_buf, 1, sizeof(sig_buf), fp);
    fclose(fp);

    /* Verify using RSA stub (in production: load public key and verify) */
    uint8_t digest[32];
    EosSha256 ctx;
    eos_sha256_init(&ctx);
    /* Re-hash for binary digest */
    FILE *img = fopen(image_path, "rb");
    if (!img) { sb->status = EOS_BOOT_FAILED; return -1; }
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), img)) > 0)
        eos_sha256_update(&ctx, buf, n);
    fclose(img);
    eos_sha256_final(&ctx, digest);

    EosRsaKey pub_key;
    memset(&pub_key, 0, sizeof(pub_key));
    pub_key.key_bits = 2048;

    if (eos_rsa_verify_sha256(&pub_key, digest, sig_buf, sig_len) == 0) {
        sb->status = EOS_BOOT_VERIFIED;
        return 0;
    }

    sb->status = EOS_BOOT_FAILED;
    return -1;
}

void eos_secureboot_dump(const EosSecureBoot *sb) {
    const char *status_str[] = {"UNVERIFIED", "VERIFIED", "FAILED", "SKIPPED"};
    printf("Secure Boot:\n");
    printf("  Image:  %s\n", sb->image_path);
    printf("  Hash:   %s (%s)\n", sb->hash[0] ? sb->hash : "(none)", sb->hash_algo);
    printf("  Status: %s\n", status_str[sb->status]);
}

/* ---- Signed Firmware ---- */

int eos_firmware_sign(EosSignedFirmware *sf, const char *fw_path,
                      const char *key_path, const char *output_path) {
    memset(sf, 0, sizeof(*sf));
    strncpy(sf->input_path, fw_path, sizeof(sf->input_path) - 1);
    strncpy(sf->key_path, key_path, sizeof(sf->key_path) - 1);
    strncpy(sf->output_path, output_path, sizeof(sf->output_path) - 1);

    /* Hash the firmware */
    uint8_t digest[32];
    EosSha256 ctx;
    eos_sha256_init(&ctx);
    FILE *fp = fopen(fw_path, "rb");
    if (!fp) return -1;
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        eos_sha256_update(&ctx, buf, n);
    fclose(fp);
    eos_sha256_final(&ctx, digest);

    char hex[EOS_SHA256_HEX_SIZE];
    eos_sha256_hex(digest, hex);
    strncpy(sf->hash, hex, sizeof(sf->hash) - 1);

    /* Sign with RSA stub */
    EosRsaKey priv_key;
    memset(&priv_key, 0, sizeof(priv_key));
    priv_key.key_bits = 2048;
    priv_key.has_private = 1;
    (void)key_path;

    uint8_t sig[512];
    size_t sig_len;
    if (eos_rsa_sign_sha256(&priv_key, digest, sig, &sig_len) != 0)
        return -1;

    /* Write signature to output */
    fp = fopen(output_path, "wb");
    if (!fp) return -1;
    fwrite(sig, 1, sig_len, fp);
    fclose(fp);

    sf->sig_len = sig_len;
    sf->signed_ok = 1;
    return 0;
}

int eos_firmware_verify_sig(const char *fw_path, const char *sig_path,
                            const char *pubkey_path) {
    EosSecureBoot sb;
    eos_secureboot_init(&sb);
    return eos_secureboot_verify_image(&sb, fw_path, sig_path, pubkey_path);
}

/* ---- Key Management ---- */

void eos_keystore_init(EosKeyStore *ks, const char *path) {
    memset(ks, 0, sizeof(*ks));
    if (path) strncpy(ks->store_path, path, sizeof(ks->store_path) - 1);
}

int eos_keystore_add(EosKeyStore *ks, const char *id, EosKeyType type,
                     const uint8_t *data, size_t len) {
    if (ks->count >= EOS_KEY_MAX_KEYS) return -1;
    if (len > sizeof(ks->keys[0].data)) return -1;

    EosKeyEntry *e = &ks->keys[ks->count];
    strncpy(e->id, id, EOS_KEY_MAX_ID - 1);
    e->type = type;
    memcpy(e->data, data, len);
    e->data_len = len;
    e->active = 1;
    ks->count++;
    return 0;
}

const EosKeyEntry *eos_keystore_find(const EosKeyStore *ks, const char *id) {
    for (int i = 0; i < ks->count; i++) {
        if (strcmp(ks->keys[i].id, id) == 0 && ks->keys[i].active)
            return &ks->keys[i];
    }
    return NULL;
}

int eos_keystore_save(const EosKeyStore *ks) {
    if (!ks->store_path[0]) return -1;
    FILE *fp = fopen(ks->store_path, "wb");
    if (!fp) return -1;
    fprintf(fp, "# EoS KeyStore v1\ncount: %d\n", ks->count);
    for (int i = 0; i < ks->count; i++) {
        const EosKeyEntry *e = &ks->keys[i];
        fprintf(fp, "key:\n  id: %s\n  type: %d\n  len: %zu\n  active: %d\n",
                e->id, e->type, e->data_len, e->active);
    }
    fclose(fp);
    return 0;
}

int eos_keystore_load(EosKeyStore *ks) {
    if (!ks->store_path[0]) return -1;
    FILE *fp = fopen(ks->store_path, "rb");
    if (!fp) return -1;
    fclose(fp);
    /* Simplified — full implementation would parse the key store file */
    return 0;
}

void eos_keystore_dump(const EosKeyStore *ks) {
    const char *type_names[] = {"AES-128","AES-256","RSA-2048","RSA-4096","ECC-P256","ECC-P384"};
    printf("KeyStore: %s (%d keys)\n", ks->store_path, ks->count);
    for (int i = 0; i < ks->count; i++) {
        const EosKeyEntry *e = &ks->keys[i];
        printf("  [%s] type=%s len=%zu %s\n",
               e->id, type_names[e->type], e->data_len,
               e->active ? "active" : "inactive");
    }
}

/* ---- Access Control ---- */

void eos_acl_init(EosAcl *acl) {
    memset(acl, 0, sizeof(*acl));
}

int eos_acl_add_rule(EosAcl *acl, const char *subject, const char *resource,
                     const char *permission, EosAclAction action) {
    if (acl->count >= EOS_ACL_MAX_RULES) return -1;
    EosAclRule *r = &acl->rules[acl->count];
    strncpy(r->subject, subject, sizeof(r->subject) - 1);
    strncpy(r->resource, resource, sizeof(r->resource) - 1);
    strncpy(r->permission, permission, sizeof(r->permission) - 1);
    r->action = action;
    acl->count++;
    return 0;
}

EosAclAction eos_acl_check(const EosAcl *acl, const char *subject,
                           const char *resource, const char *permission) {
    for (int i = acl->count - 1; i >= 0; i--) {
        const EosAclRule *r = &acl->rules[i];
        int subj_match = (strcmp(r->subject, "*") == 0 || strcmp(r->subject, subject) == 0);
        int res_match = (strcmp(r->resource, "*") == 0 || strcmp(r->resource, resource) == 0);
        int perm_match = (strcmp(r->permission, "*") == 0 || strcmp(r->permission, permission) == 0);
        if (subj_match && res_match && perm_match)
            return r->action;
    }
    return EOS_ACL_DENY;
}

void eos_acl_dump(const EosAcl *acl) {
    printf("Access Control (%d rules):\n", acl->count);
    for (int i = 0; i < acl->count; i++) {
        const EosAclRule *r = &acl->rules[i];
        printf("  [%s] %s -> %s : %s\n",
               r->action == EOS_ACL_ALLOW ? "ALLOW" : "DENY",
               r->subject, r->resource, r->permission);
    }
}
