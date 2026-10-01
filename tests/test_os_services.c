// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_os_services.c
 * @brief Unit tests for EoS OS services (watchdog, audit, secure storage)
 */

#include "eos/os_services.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        printf("  %-50s ", #name); \
        name(); \
        tests_passed++; \
        printf("[PASS]\n"); \
    } \
    static void name(void)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while(0)

/* ---- Watchdog ---- */

TEST(test_watchdog_init) {
    EosWatchdog wd;
    eos_watchdog_init(&wd, 5000);
    ASSERT(wd.timeout_ms == 5000);
    ASSERT(wd.kick_interval_ms == 2500);
    ASSERT(wd.state == EOS_WDG_DISABLED);
    ASSERT(wd.auto_reboot == 1);
}

TEST(test_watchdog_start_kick) {
    EosWatchdog wd;
    eos_watchdog_init(&wd, 10000);
    ASSERT(eos_watchdog_start(&wd) == 0);
    ASSERT(wd.state == EOS_WDG_RUNNING);
    ASSERT(eos_watchdog_kick(&wd) == 0);
    ASSERT(eos_watchdog_check(&wd) == 0);
}

TEST(test_watchdog_stop) {
    EosWatchdog wd;
    eos_watchdog_init(&wd, 5000);
    eos_watchdog_start(&wd);
    ASSERT(eos_watchdog_stop(&wd) == 0);
    ASSERT(wd.state == EOS_WDG_DISABLED);
    ASSERT(eos_watchdog_kick(&wd) == -1);
}

static int callback_count = 0;
static void test_callback(void *data) { (void)data; callback_count++; }

TEST(test_watchdog_callback) {
    EosWatchdog wd;
    callback_count = 0;
    eos_watchdog_init(&wd, 5000);
    eos_watchdog_set_callback(&wd, test_callback, NULL);
    ASSERT(wd.callback == test_callback);
}

/* ---- Audit ---- */

TEST(test_audit_init) {
    EosAuditLog log;
    eos_audit_init(&log, NULL);
    ASSERT(log.count == 0);
    ASSERT(log.persist == 0);
}

TEST(test_audit_record) {
    EosAuditLog log;
    eos_audit_init(&log, NULL);

    ASSERT(eos_audit_record(&log, EOS_AUDIT_INFO, "test", "hello") == 0);
    ASSERT(log.count == 1);
    ASSERT(log.entries[0].level == EOS_AUDIT_INFO);
    ASSERT(strcmp(log.entries[0].source, "test") == 0);
    ASSERT(strcmp(log.entries[0].message, "hello") == 0);
}

TEST(test_audit_multiple) {
    EosAuditLog log;
    eos_audit_init(&log, NULL);

    eos_audit_record(&log, EOS_AUDIT_INFO, "sys", "boot");
    eos_audit_record(&log, EOS_AUDIT_WARN, "net", "timeout");
    eos_audit_record(&log, EOS_AUDIT_ERROR, "disk", "io error");

    ASSERT(log.count == 3);
    ASSERT(log.entries[2].level == EOS_AUDIT_ERROR);
    ASSERT(log.entries[0].sequence == 0);
    ASSERT(log.entries[2].sequence == 2);
}

/* ---- Secure Storage ---- */

TEST(test_storage_set_get) {
    EosSecureStorage ss;
    eos_storage_init(&ss, NULL);

    uint32_t val = 42;
    ASSERT(eos_storage_set(&ss, "count", &val, sizeof(val)) == 0);
    ASSERT(ss.count == 1);

    uint32_t out = 0;
    size_t len = sizeof(out);
    ASSERT(eos_storage_get(&ss, "count", &out, &len) == 0);
    ASSERT(out == 42);
    ASSERT(len == sizeof(uint32_t));
}

TEST(test_storage_overwrite) {
    EosSecureStorage ss;
    eos_storage_init(&ss, NULL);

    uint32_t v1 = 10, v2 = 20;
    eos_storage_set(&ss, "x", &v1, sizeof(v1));
    eos_storage_set(&ss, "x", &v2, sizeof(v2));

    uint32_t out = 0;
    size_t len = sizeof(out);
    eos_storage_get(&ss, "x", &out, &len);
    ASSERT(out == 20);
    ASSERT(ss.count == 1);
}

TEST(test_storage_delete) {
    EosSecureStorage ss;
    eos_storage_init(&ss, NULL);

    uint32_t v = 99;
    eos_storage_set(&ss, "temp", &v, sizeof(v));
    ASSERT(ss.count == 1);
    ASSERT(eos_storage_delete(&ss, "temp") == 0);
    ASSERT(ss.count == 0);

    size_t len = sizeof(v);
    ASSERT(eos_storage_get(&ss, "temp", &v, &len) == -1);
}

TEST(test_storage_not_found) {
    EosSecureStorage ss;
    eos_storage_init(&ss, NULL);

    uint32_t v = 0;
    size_t len = sizeof(v);
    ASSERT(eos_storage_get(&ss, "nope", &v, &len) == -1);
}

TEST(test_storage_string) {
    EosSecureStorage ss;
    eos_storage_init(&ss, NULL);

    const char *msg = "hello storage";
    eos_storage_set(&ss, "msg", msg, strlen(msg) + 1);

    char out[64] = {0};
    size_t len = sizeof(out);
    ASSERT(eos_storage_get(&ss, "msg", out, &len) == 0);
    ASSERT(strcmp(out, "hello storage") == 0);
}

/* ---- OTA: no shell between the caller's strings and the system ---- */

/* The two shell-injection probes the review executed against the old code,
 * as data. The URL one used to leave `wget -q -O "%s" "%s"`'s quoting and
 * run the command; the target one used backticks, which are substitution
 * inside double quotes, and reported EOS_OTA_COMPLETE afterwards. */
#define OTA_SENTINEL_DL  "eos_ota_probe_download"
#define OTA_SENTINEL_IN  "eos_ota_probe_install"

/* Fixtures are created 0600, not fopen("wb")'s 0666-masked-by-umask: the
 * same rule the code under test follows, and the one CodeQL holds this
 * repository's tests to as well. */
static FILE *create_fixture(const char *path) {
#ifdef _WIN32
    return fopen(path, "wb");
#else
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    FILE *f;
    if (fd < 0) return NULL;
    f = fdopen(fd, "wb");
    if (!f) close(fd);
    return f;
#endif
}

static int file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

TEST(test_ota_download_refuses_a_url_that_is_not_an_http_url) {
    EosOtaUpdate ota;
    const char *bad[] = {
        /* the probe from the review; the sentinel name is spelled out so
         * the compiler does not read the adjacent literals as one entry */
        "http://example.invalid/fw.bin\" & touch eos_ota_probe_download & \"",
        "-Oeos_ota_probe_download",               /* would be read as an option */
        "file:///etc/passwd",                     /* not a download */
        "ftp://example.invalid/fw.bin",
        "http://example.invalid/fw`id`",          /* not a URL character */
        "http://example.invalid/fw bin",          /* space */
        "http://example.invalid/fw\nbin",         /* control character */
        "",
    };
    remove(OTA_SENTINEL_DL);
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        eos_ota_init(&ota);
        eos_ota_set_source(&ota, bad[i], NULL);
        ASSERT(eos_ota_download(&ota) == -1);
        ASSERT(ota.state == EOS_OTA_FAILED);
    }
    ASSERT(!file_exists(OTA_SENTINEL_DL));
}

/* A URL made of URL characters -- including the sub-delims a signed URL
 * carries -- passes the check; the refusal above is about shape, not about
 * characters that were dangerous only to a shell. */
TEST(test_ota_url_check_accepts_url_characters) {
    /* Reached through eos_ota_download() on a host with no downloader on the
     * PATH the state is DOWNLOADING or FAILED, never a refusal-before-start:
     * the refusal leaves local_path empty, a started fetch fills it. */
    EosOtaUpdate ota;
    eos_ota_init(&ota);
    eos_ota_set_source(&ota, "https://example.invalid/fw.bin?X-Sig=a%2Fb&Expires=1;v=2", NULL);
    (void)eos_ota_download(&ota);
    ASSERT(ota.local_path[0] != '\0');
}

TEST(test_ota_install_copies_the_file_and_runs_nothing) {
    EosOtaUpdate ota;
    const char payload[] = "firmware bytes\x00\x01\x02 end";
    const char *target = "eos_ota_test_target`touch " OTA_SENTINEL_IN "`";
    FILE *f;
    char back[64];
    size_t n;

    remove(OTA_SENTINEL_IN);
    remove(target);
    eos_ota_init(&ota);
    snprintf(ota.local_path, sizeof(ota.local_path), "%s", "eos_ota_test_source.bin");
    f = create_fixture(ota.local_path);
    ASSERT(f != NULL);
    ASSERT(fwrite(payload, 1, sizeof(payload), f) == sizeof(payload));
    fclose(f);

    /* The backticks are just characters in a file name now. */
    ASSERT(eos_ota_install(&ota, target) == 0);
    ASSERT(ota.state == EOS_OTA_COMPLETE);
    ASSERT(!file_exists(OTA_SENTINEL_IN));
    f = fopen(target, "rb");
    ASSERT(f != NULL);
    n = fread(back, 1, sizeof(back), f);
#ifndef _WIN32
    {
        /* The installed image is owner read/write only, whatever the umask.
         * Read the mode off the open descriptor, not the path: a path
         * stat'ed here and removed below is a check-then-use CodeQL
         * rightly flags, even in a test. */
        struct stat st;
        ASSERT(fstat(fileno(f), &st) == 0);
        ASSERT((st.st_mode & 077) == 0);
    }
#endif
    fclose(f);
    ASSERT(n == sizeof(payload));
    ASSERT(memcmp(back, payload, sizeof(payload)) == 0);

    remove(target);
    remove(ota.local_path);
}

TEST(test_ota_install_refuses_a_missing_source_or_target) {
    EosOtaUpdate ota;
    eos_ota_init(&ota);
    ASSERT(eos_ota_install(&ota, "eos_ota_never_written") == -1);   /* no local_path */
    ASSERT(ota.state == EOS_OTA_FAILED);
    ASSERT(!file_exists("eos_ota_never_written"));

    eos_ota_init(&ota);
    snprintf(ota.local_path, sizeof(ota.local_path), "%s", "eos_ota_does_not_exist.bin");
    ASSERT(eos_ota_install(&ota, "eos_ota_never_written") == -1);
    ASSERT(ota.state == EOS_OTA_FAILED);
    ASSERT(!file_exists("eos_ota_never_written"));

    eos_ota_init(&ota);
    ASSERT(eos_ota_install(&ota, NULL) == -1);
    ASSERT(eos_ota_install(&ota, "") == -1);
}

int main(void) {
    printf("=== EoS: OS Services Unit Tests ===\n\n");
    run_test_watchdog_init();
    run_test_watchdog_start_kick();
    run_test_watchdog_stop();
    run_test_watchdog_callback();
    run_test_audit_init();
    run_test_audit_record();
    run_test_audit_multiple();
    run_test_storage_set_get();
    run_test_storage_overwrite();
    run_test_storage_delete();
    run_test_storage_not_found();
    run_test_storage_string();
    run_test_ota_download_refuses_a_url_that_is_not_an_http_url();
    run_test_ota_url_check_accepts_url_characters();
    run_test_ota_install_copies_the_file_and_runs_nothing();
    run_test_ota_install_refuses_a_missing_source_or_target();
    tests_run = 16;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
