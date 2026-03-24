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
    tests_run = 12;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
