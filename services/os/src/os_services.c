// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/os_services.h"
#include "eos/crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <process.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

static uint64_t get_timestamp_ms(void) {
    return (uint64_t)time(NULL) * 1000;
}

/* ---- Watchdog ---- */

void eos_watchdog_init(EosWatchdog *wd, uint32_t timeout_ms) {
    memset(wd, 0, sizeof(*wd));
    wd->timeout_ms = timeout_ms;
    wd->kick_interval_ms = timeout_ms / 2;
    wd->state = EOS_WDG_DISABLED;
    wd->auto_reboot = 1;
}

int eos_watchdog_start(EosWatchdog *wd) {
    wd->state = EOS_WDG_RUNNING;
    wd->start_time = get_timestamp_ms();
    wd->last_kick = wd->start_time;
    return 0;
}

int eos_watchdog_kick(EosWatchdog *wd) {
    if (wd->state != EOS_WDG_RUNNING) return -1;
    wd->last_kick = get_timestamp_ms();
    return 0;
}

int eos_watchdog_stop(EosWatchdog *wd) {
    wd->state = EOS_WDG_DISABLED;
    return 0;
}

int eos_watchdog_check(EosWatchdog *wd) {
    if (wd->state != EOS_WDG_RUNNING) return 0;
    uint64_t now = get_timestamp_ms();
    if (now - wd->last_kick > wd->timeout_ms) {
        wd->state = EOS_WDG_EXPIRED;
        if (wd->callback) wd->callback(wd->user_data);
        return -1;
    }
    return 0;
}

void eos_watchdog_set_callback(EosWatchdog *wd, void (*cb)(void *), void *data) {
    wd->callback = cb;
    wd->user_data = data;
}

void eos_watchdog_dump(const EosWatchdog *wd) {
    const char *states[] = {"DISABLED","RUNNING","EXPIRED","PAUSED"};
    printf("Watchdog:\n");
    printf("  Timeout:  %u ms\n", wd->timeout_ms);
    printf("  State:    %s\n", states[wd->state]);
    printf("  Reboot:   %s\n", wd->auto_reboot ? "yes" : "no");
}

/* ---- OTA Update ---- */

void eos_ota_init(EosOtaUpdate *ota) {
    memset(ota, 0, sizeof(*ota));
    ota->state = EOS_OTA_IDLE;
}

int eos_ota_set_source(EosOtaUpdate *ota, const char *url,
                       const char *expected_hash) {
    strncpy(ota->url, url, sizeof(ota->url) - 1);
    if (expected_hash)
        strncpy(ota->expected_hash, expected_hash, sizeof(ota->expected_hash) - 1);
    return 0;
}

/* ---- OTA: no shell ----
 *
 * The download and the install used to be snprintf'd into a line and handed
 * to system(): `wget -q -O "%s" "%s"` and `cp -f "%s" "%s"`. Each was guarded
 * by its own denylist, and each denylist had holes -- the URL's omitted the
 * double quote and the ampersand, so a URL carrying `" & <cmd> & "` left the
 * quoting and ran the command; the install path's omitted the backtick,
 * which is command substitution inside double quotes, so a target of
 * `x\`<cmd>\`` ran the command and then reported EOS_OTA_COMPLETE. Both were
 * demonstrated by execution in review.
 *
 * Neither step needs a shell. The fetch runs the downloader with an argv,
 * so nothing is parsed as shell syntax whatever the URL holds, and the
 * install is a file copy in C. What is left to check about the URL is only
 * what the downloader would do with it: it must be an http(s) URL made of
 * URL characters, so it cannot be mistaken for an option (`-O ...`) or a
 * local file, and it cannot smuggle a control character into a log line.
 */

static int url_is_fetchable(const char *url) {
    /* RFC 3986: unreserved, reserved (gen-delims and sub-delims), and %. */
    static const char extra[] = "-._~:/?#[]@!$&'()*+,;=%";
    const unsigned char *p;
    if (!url) return 0;
    if (strncmp(url, "https://", 8) != 0 && strncmp(url, "http://", 7) != 0) return 0;
    for (p = (const unsigned char *)url; *p; p++) {
        if (*p <= 0x20 || *p >= 0x7F) return 0;
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z') ||
              (*p >= 'A' && *p <= 'Z') || strchr(extra, (int)*p)))
            return 0;
    }
    return 1;
}

/* Run argv[0] with argv, no shell in between. 0 when it exited 0. */
static int run_argv(char *const argv[]) {
#ifdef _WIN32
    intptr_t rc = _spawnvp(_P_WAIT, argv[0], (const char *const *)argv);
    return (rc == 0) ? 0 : -1;
#else
    pid_t pid;
    int status = 0;
    if (posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ) != 0) return -1;
    if (waitpid(pid, &status, 0) != pid) return -1;
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
#endif
}

/* Create dst for writing, owner read/write only. fopen("wb") creates with
 * 0666 masked by umask, so on a permissive umask the installed image would
 * be readable -- and writable -- by every user on the host. The updater is
 * the one that writes the image and the one that reads it back, so 0600 is
 * the right mode; a consumer that needs it wider is a policy of its own. */
static FILE *create_private(const char *dst) {
#ifdef _WIN32
    return fopen(dst, "wb");
#else
    int fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR);
    FILE *f;
    if (fd < 0) return NULL;
    f = fdopen(fd, "wb");
    if (!f) close(fd);
    return f;
#endif
}

/* Copy src to dst byte for byte. 0 on success; dst is not left half-written
 * on a read or write error only in the sense that the error is reported. */
static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    FILE *out;
    unsigned char buf[8192];
    size_t n;
    int ok = 1;
    if (!in) return -1;
    out = create_private(dst);
    if (!out) { fclose(in); return -1; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    }
    if (ferror(in)) ok = 0;
    if (fclose(out) != 0) ok = 0;
    fclose(in);
    return ok ? 0 : -1;
}

int eos_ota_download(EosOtaUpdate *ota) {
    if (!ota || !url_is_fetchable(ota->url)) {
        if (ota) ota->state = EOS_OTA_FAILED;
        return -1;
    }
    ota->state = EOS_OTA_DOWNLOADING;
#ifdef _WIN32
    snprintf(ota->local_path, sizeof(ota->local_path), "%s\\eos_ota_update.bin",
             getenv("TEMP") ? getenv("TEMP") : ".");
    {
        char *const argv[] = { "curl", "-fSL", "-o", ota->local_path, ota->url, NULL };
        if (run_argv(argv) != 0) { ota->state = EOS_OTA_FAILED; return -1; }
    }
#else
    snprintf(ota->local_path, sizeof(ota->local_path), "/tmp/eos_ota_update.bin");
    {
        char *const argv[] = { "wget", "-q", "-O", ota->local_path, ota->url, NULL };
        if (run_argv(argv) != 0) { ota->state = EOS_OTA_FAILED; return -1; }
    }
#endif
    ota->state = EOS_OTA_VERIFYING;
    return 0;
}

int eos_ota_verify(EosOtaUpdate *ota) {
    ota->state = EOS_OTA_VERIFYING;
    if (eos_sha256_file(ota->local_path, ota->computed_hash) != 0) {
        ota->state = EOS_OTA_FAILED;
        return -1;
    }
    if (ota->expected_hash[0] && strcmp(ota->computed_hash, ota->expected_hash) != 0) {
        ota->state = EOS_OTA_FAILED;
        return -1;
    }
    return 0;
}

int eos_ota_install(EosOtaUpdate *ota, const char *target_path) {
    if (!ota) return -1;
    if (!target_path || !target_path[0] || !ota->local_path[0]) {
        ota->state = EOS_OTA_FAILED;
        return -1;
    }
    ota->state = EOS_OTA_INSTALLING;
    if (copy_file(ota->local_path, target_path) != 0) {
        ota->state = EOS_OTA_FAILED;
        return -1;
    }
    ota->state = EOS_OTA_COMPLETE;
    return 0;
}

int eos_ota_rollback(EosOtaUpdate *ota) {
    ota->state = EOS_OTA_ROLLBACK;
    return 0;
}

void eos_ota_dump(const EosOtaUpdate *ota) {
    const char *states[] = {"IDLE","DOWNLOADING","VERIFYING","INSTALLING","COMPLETE","FAILED","ROLLBACK"};
    printf("OTA Update:\n");
    printf("  URL:    %s\n", ota->url);
    printf("  State:  %s\n", states[ota->state]);
    if (ota->computed_hash[0]) printf("  Hash:   %s\n", ota->computed_hash);
}

/* ---- Audit Trail ---- */

void eos_audit_init(EosAuditLog *log, const char *path) {
    memset(log, 0, sizeof(*log));
    if (path) {
        strncpy(log->log_path, path, sizeof(log->log_path) - 1);
        log->persist = 1;
    }
}

int eos_audit_record(EosAuditLog *log, EosAuditLevel level,
                     const char *source, const char *message) {
    int idx;
    if (log->count < EOS_AUDIT_MAX_ENTRIES) {
        idx = log->count++;
    } else {
        idx = log->head;
        log->head = (log->head + 1) % EOS_AUDIT_MAX_ENTRIES;
    }

    EosAuditEntry *e = &log->entries[idx];
    e->timestamp = get_timestamp_ms();
    e->level = level;
    e->sequence = log->next_seq++;
    strncpy(e->source, source, sizeof(e->source) - 1);
    strncpy(e->message, message, sizeof(e->message) - 1);

    if (log->persist && log->log_path[0]) {
        FILE *fp = fopen(log->log_path, "a");
        if (fp) {
            const char *lvl_str[] = {"INFO","WARN","ERROR","SECURITY","BOOT","UPDATE"};
            fprintf(fp, "[%u] %s [%s] %s: %s\n",
                    e->sequence, "(timestamp)", lvl_str[e->level], e->source, e->message);
            fclose(fp);
        }
    }
    return 0;
}

int eos_audit_save(const EosAuditLog *log) {
    if (!log->log_path[0]) return -1;
    FILE *fp = fopen(log->log_path, "w");
    if (!fp) return -1;
    const char *lvl_str[] = {"INFO","WARN","ERROR","SECURITY","BOOT","UPDATE"};
    for (int i = 0; i < log->count; i++) {
        const EosAuditEntry *e = &log->entries[i];
        fprintf(fp, "[%u] %s [%s] %s\n",
                e->sequence, lvl_str[e->level], e->source, e->message);
    }
    fclose(fp);
    return 0;
}

int eos_audit_load(EosAuditLog *log) {
    (void)log;
    return 0;
}

void eos_audit_dump(const EosAuditLog *log, int last_n) {
    const char *lvl_str[] = {"INFO","WARN","ERROR","SECURITY","BOOT","UPDATE"};
    int start = (last_n > 0 && last_n < log->count) ? log->count - last_n : 0;
    printf("Audit Log (%d entries):\n", log->count);
    for (int i = start; i < log->count; i++) {
        const EosAuditEntry *e = &log->entries[i];
        printf("  [%u] %s [%s] %s\n",
               e->sequence, lvl_str[e->level], e->source, e->message);
    }
}

/* ---- Secure Storage ---- */

void eos_storage_init(EosSecureStorage *ss, const char *path) {
    memset(ss, 0, sizeof(*ss));
    if (path) strncpy(ss->store_path, path, sizeof(ss->store_path) - 1);
}

int eos_storage_set(EosSecureStorage *ss, const char *key,
                    const void *value, size_t len) {
    if (len > EOS_STORAGE_MAX_VALUE) return -1;

    /* Update existing */
    for (int i = 0; i < ss->count; i++) {
        if (strcmp(ss->items[i].key, key) == 0) {
            memcpy(ss->items[i].value, value, len);
            ss->items[i].value_len = len;
            return 0;
        }
    }

    if (ss->count >= EOS_STORAGE_MAX_ITEMS) return -1;
    EosStorageItem *item = &ss->items[ss->count];
    strncpy(item->key, key, sizeof(item->key) - 1);
    memcpy(item->value, value, len);
    item->value_len = len;
    item->encrypted = ss->encryption_enabled;
    ss->count++;
    return 0;
}

int eos_storage_get(const EosSecureStorage *ss, const char *key,
                    void *value, size_t *len) {
    for (int i = 0; i < ss->count; i++) {
        if (strcmp(ss->items[i].key, key) == 0) {
            if (*len < ss->items[i].value_len) return -1;
            memcpy(value, ss->items[i].value, ss->items[i].value_len);
            *len = ss->items[i].value_len;
            return 0;
        }
    }
    return -1;
}

int eos_storage_delete(EosSecureStorage *ss, const char *key) {
    for (int i = 0; i < ss->count; i++) {
        if (strcmp(ss->items[i].key, key) == 0) {
            memmove(&ss->items[i], &ss->items[i+1],
                    sizeof(EosStorageItem) * (size_t)(ss->count - i - 1));
            ss->count--;
            return 0;
        }
    }
    return -1;
}

int eos_storage_save(const EosSecureStorage *ss) {
    if (!ss->store_path[0]) return -1;
    FILE *fp = fopen(ss->store_path, "wb");
    if (!fp) return -1;
    fwrite(&ss->count, sizeof(int), 1, fp);
    for (int i = 0; i < ss->count; i++) {
        fwrite(&ss->items[i], sizeof(EosStorageItem), 1, fp);
    }
    fclose(fp);
    return 0;
}

int eos_storage_load(EosSecureStorage *ss) {
    if (!ss->store_path[0]) return -1;
    FILE *fp = fopen(ss->store_path, "rb");
    if (!fp) return -1;
    fread(&ss->count, sizeof(int), 1, fp);
    if (ss->count > EOS_STORAGE_MAX_ITEMS) ss->count = EOS_STORAGE_MAX_ITEMS;
    for (int i = 0; i < ss->count; i++) {
        fread(&ss->items[i], sizeof(EosStorageItem), 1, fp);
    }
    fclose(fp);
    return 0;
}

void eos_storage_set_encryption(EosSecureStorage *ss, const uint8_t key[32]) {
    memcpy(ss->encryption_key, key, 32);
    ss->encryption_enabled = 1;
}

void eos_storage_dump(const EosSecureStorage *ss) {
    printf("Secure Storage: %s (%d items)\n", ss->store_path, ss->count);
    for (int i = 0; i < ss->count; i++) {
        printf("  [%s] %zu bytes %s\n",
               ss->items[i].key, ss->items[i].value_len,
               ss->items[i].encrypted ? "(encrypted)" : "");
    }
}

/* ---- Integrity Check ---- */

int eos_integrity_check_sha256(const char *path, const char *expected_hash,
                               EosIntegrityResult *result) {
    memset(result, 0, sizeof(*result));
    strncpy(result->path, path, sizeof(result->path) - 1);
    strncpy(result->expected_sha256, expected_hash, sizeof(result->expected_sha256) - 1);

    if (eos_sha256_file(path, result->computed_sha256) != 0) {
        result->sha256_ok = 0;
        return -1;
    }

    result->sha256_ok = (strcmp(result->computed_sha256, expected_hash) == 0);
    return result->sha256_ok ? 0 : -1;
}

int eos_integrity_check_crc32(const char *path, uint32_t expected_crc,
                              EosIntegrityResult *result) {
    memset(result, 0, sizeof(*result));
    strncpy(result->path, path, sizeof(result->path) - 1);
    result->expected_crc32 = expected_crc;

    /* A file that cannot be read is not a file that matched. eos_crc32_file()
     * returns 0 on a failed read, which is also the CRC of an empty file, so
     * comparing its result directly let a missing file pass whenever the
     * caller expected a CRC of 0. eos_integrity_check_sha256() above already
     * treats an unreadable file as a failure. */
    if (eos_crc32_file_ex(path, &result->computed_crc32) != 0) {
        result->crc32_ok = 0;
        return -1;
    }

    result->crc32_ok = (result->computed_crc32 == expected_crc);
    return result->crc32_ok ? 0 : -1;
}

void eos_integrity_dump(const EosIntegrityResult *result) {
    printf("Integrity Check: %s\n", result->path);
    if (result->expected_sha256[0]) {
        printf("  SHA-256: %s (%s)\n", result->computed_sha256,
               result->sha256_ok ? "OK" : "MISMATCH");
    }
    if (result->expected_crc32) {
        printf("  CRC-32:  0x%08x (%s)\n", result->computed_crc32,
               result->crc32_ok ? "OK" : "MISMATCH");
    }
}
