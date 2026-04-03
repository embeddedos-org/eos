// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file os_services.h
 * @brief EoS Enhanced OS Services
 *
 * Runtime services for embedded targets: watchdog management,
 * OTA firmware updates, audit trail logging, secure storage,
 * and image/file integrity verification.
 */

#ifndef EOS_OS_SERVICES_H
#define EOS_OS_SERVICES_H

#include <stdint.h>
#include <stddef.h>

/* ---- Watchdog Management ---- */

typedef enum {
    EOS_WDG_DISABLED,
    EOS_WDG_RUNNING,
    EOS_WDG_EXPIRED,
    EOS_WDG_PAUSED
} EosWatchdogState;

typedef struct {
    uint32_t timeout_ms;
    uint32_t kick_interval_ms;
    EosWatchdogState state;
    uint64_t last_kick;
    uint64_t start_time;
    int auto_reboot;
    void (*callback)(void *user_data);
    void *user_data;
} EosWatchdog;

void eos_watchdog_init(EosWatchdog *wd, uint32_t timeout_ms);
int  eos_watchdog_start(EosWatchdog *wd);
int  eos_watchdog_kick(EosWatchdog *wd);
int  eos_watchdog_stop(EosWatchdog *wd);
int  eos_watchdog_check(EosWatchdog *wd);
void eos_watchdog_set_callback(EosWatchdog *wd, void (*cb)(void *), void *data);
void eos_watchdog_dump(const EosWatchdog *wd);

/* ---- OTA Update Service ---- */

typedef enum {
    EOS_OTA_IDLE,
    EOS_OTA_DOWNLOADING,
    EOS_OTA_VERIFYING,
    EOS_OTA_INSTALLING,
    EOS_OTA_COMPLETE,
    EOS_OTA_FAILED,
    EOS_OTA_ROLLBACK
} EosOtaState;

typedef struct {
    char url[1024];
    char local_path[512];
    char expected_hash[129];
    char computed_hash[129];
    char current_version[128];
    char target_version[128];
    EosOtaState state;
    size_t total_bytes;
    size_t downloaded_bytes;
    int verify_signature;
    char sig_url[1024];
    char pubkey_path[512];
} EosOtaUpdate;

void eos_ota_init(EosOtaUpdate *ota);
int  eos_ota_set_source(EosOtaUpdate *ota, const char *url,
                        const char *expected_hash);
int  eos_ota_download(EosOtaUpdate *ota);
int  eos_ota_verify(EosOtaUpdate *ota);
int  eos_ota_install(EosOtaUpdate *ota, const char *target_path);
int  eos_ota_rollback(EosOtaUpdate *ota);
void eos_ota_dump(const EosOtaUpdate *ota);

/* ---- Audit Trail ---- */

#define EOS_AUDIT_MAX_ENTRIES 256
#define EOS_AUDIT_MSG_LEN     256

typedef enum {
    EOS_AUDIT_INFO,
    EOS_AUDIT_WARN,
    EOS_AUDIT_ERROR,
    EOS_AUDIT_SECURITY,
    EOS_AUDIT_BOOT,
    EOS_AUDIT_UPDATE
} EosAuditLevel;

typedef struct {
    uint64_t timestamp;
    EosAuditLevel level;
    char source[64];
    char message[EOS_AUDIT_MSG_LEN];
    uint32_t sequence;
} EosAuditEntry;

typedef struct {
    EosAuditEntry entries[EOS_AUDIT_MAX_ENTRIES];
    int count;
    int head;
    uint32_t next_seq;
    char log_path[512];
    int persist;
} EosAuditLog;

void eos_audit_init(EosAuditLog *log, const char *path);
int  eos_audit_record(EosAuditLog *log, EosAuditLevel level,
                      const char *source, const char *message);
int  eos_audit_save(const EosAuditLog *log);
int  eos_audit_load(EosAuditLog *log);
void eos_audit_dump(const EosAuditLog *log, int last_n);

/* ---- Secure Storage ---- */

#define EOS_STORAGE_MAX_ITEMS 64
#define EOS_STORAGE_MAX_VALUE 1024

typedef struct {
    char key[128];
    uint8_t value[EOS_STORAGE_MAX_VALUE];
    size_t value_len;
    int encrypted;
} EosStorageItem;

typedef struct {
    EosStorageItem items[EOS_STORAGE_MAX_ITEMS];
    int count;
    char store_path[512];
    int encryption_enabled;
    uint8_t encryption_key[32];
} EosSecureStorage;

void eos_storage_init(EosSecureStorage *ss, const char *path);
int  eos_storage_set(EosSecureStorage *ss, const char *key,
                     const void *value, size_t len);
int  eos_storage_get(const EosSecureStorage *ss, const char *key,
                     void *value, size_t *len);
int  eos_storage_delete(EosSecureStorage *ss, const char *key);
int  eos_storage_save(const EosSecureStorage *ss);
int  eos_storage_load(EosSecureStorage *ss);
void eos_storage_set_encryption(EosSecureStorage *ss,
                                const uint8_t key[32]);
void eos_storage_dump(const EosSecureStorage *ss);

/* ---- Integrity Check ---- */

typedef struct {
    char path[512];
    char expected_sha256[65];
    char computed_sha256[65];
    uint32_t expected_crc32;
    uint32_t computed_crc32;
    int sha256_ok;
    int crc32_ok;
} EosIntegrityResult;

int eos_integrity_check_sha256(const char *path, const char *expected_hash,
                               EosIntegrityResult *result);
int eos_integrity_check_crc32(const char *path, uint32_t expected_crc,
                              EosIntegrityResult *result);
void eos_integrity_dump(const EosIntegrityResult *result);

#endif /* EOS_OS_SERVICES_H */
