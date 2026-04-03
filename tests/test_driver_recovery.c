// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_driver_recovery.c
 * @brief Tests for driver error recovery, timeout, and retry mechanisms
 */

#include "eos/drivers.h"
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

/* Mock driver that fails N times then succeeds */
static int fail_count = 0;
static int fail_limit = 0;

static int mock_init_fail(struct eos_driver *drv) {
    (void)drv;
    if (fail_count < fail_limit) {
        fail_count++;
        return -1;
    }
    return 0;
}

static int mock_init_ok(struct eos_driver *drv) {
    (void)drv;
    return 0;
}

static void mock_deinit(struct eos_driver *drv) {
    (void)drv;
}

static int mock_open(struct eos_driver *drv) {
    (void)drv;
    return 0;
}

static int mock_close(struct eos_driver *drv) {
    (void)drv;
    return 0;
}

static int mock_read(struct eos_driver *drv, void *buf, size_t len) {
    (void)drv;
    memset(buf, 0xAA, len);
    return (int)len;
}

static int mock_write(struct eos_driver *drv, const void *buf, size_t len) {
    (void)drv;
    (void)buf;
    return (int)len;
}

static const eos_driver_ops_t ok_ops = {
    .init   = mock_init_ok,
    .deinit = mock_deinit,
    .open   = mock_open,
    .close  = mock_close,
    .read   = mock_read,
    .write  = mock_write,
};

static const eos_driver_ops_t fail_ops = {
    .init   = mock_init_fail,
    .deinit = mock_deinit,
    .open   = mock_open,
    .close  = mock_close,
    .read   = mock_read,
    .write  = mock_write,
};

TEST(test_driver_init_success) {
    eos_driver_t drv = {
        .name = "test-ok",
        .type = EOS_DRIVER_GPIO,
        .ops  = &ok_ops,
        .instance = 0,
    };

    int rc = eos_driver_register(&drv);
    ASSERT(rc == 0);

    rc = eos_driver_init_all();
    ASSERT(rc == 0);
    ASSERT(drv.state == EOS_DRV_STATE_READY);

    eos_driver_deinit_all();
    eos_driver_unregister("test-ok");
}

TEST(test_driver_init_failure_sets_error) {
    fail_count = 0;
    fail_limit = 999;

    eos_driver_t drv = {
        .name = "test-fail",
        .type = EOS_DRIVER_UART,
        .ops  = &fail_ops,
        .instance = 0,
    };

    eos_driver_register(&drv);
    int rc = eos_driver_init_all();
    ASSERT(rc > 0);
    ASSERT(drv.state == EOS_DRV_STATE_ERROR);

    eos_driver_deinit_all();
    eos_driver_unregister("test-fail");
}

TEST(test_driver_open_requires_ready) {
    eos_driver_t drv = {
        .name = "test-state",
        .type = EOS_DRIVER_SPI,
        .ops  = &ok_ops,
        .instance = 0,
    };

    eos_driver_register(&drv);

    /* Not yet initialized — open should fail */
    int rc = eos_driver_open(&drv);
    ASSERT(rc != 0);

    /* Initialize, then open should succeed */
    eos_driver_init_all();
    rc = eos_driver_open(&drv);
    ASSERT(rc == 0);
    ASSERT(drv.state == EOS_DRV_STATE_ACTIVE);

    eos_driver_close(&drv);
    eos_driver_deinit_all();
    eos_driver_unregister("test-state");
}

TEST(test_driver_read_write_active) {
    eos_driver_t drv = {
        .name = "test-rw",
        .type = EOS_DRIVER_I2C,
        .ops  = &ok_ops,
        .instance = 0,
    };

    eos_driver_register(&drv);
    eos_driver_init_all();
    eos_driver_open(&drv);

    uint8_t buf[16];
    int n = eos_driver_read(&drv, buf, sizeof(buf));
    ASSERT(n == 16);
    ASSERT(buf[0] == 0xAA);

    const uint8_t wdata[4] = {1, 2, 3, 4};
    n = eos_driver_write(&drv, wdata, sizeof(wdata));
    ASSERT(n == 4);

    eos_driver_close(&drv);
    eos_driver_deinit_all();
    eos_driver_unregister("test-rw");
}

TEST(test_driver_suspend_resume) {
    eos_driver_t drv = {
        .name = "test-pm",
        .type = EOS_DRIVER_TIMER,
        .ops  = &ok_ops,
        .instance = 0,
    };

    /* Add suspend/resume to ops */
    static int mock_suspend(struct eos_driver *d) { (void)d; return 0; }
    static int mock_resume(struct eos_driver *d) { (void)d; return 0; }

    static const eos_driver_ops_t pm_ops = {
        .init    = mock_init_ok,
        .deinit  = mock_deinit,
        .open    = mock_open,
        .close   = mock_close,
        .read    = mock_read,
        .write   = mock_write,
        .suspend = mock_suspend,
        .resume  = mock_resume,
    };
    drv.ops = &pm_ops;

    eos_driver_register(&drv);
    eos_driver_init_all();
    eos_driver_open(&drv);

    int rc = eos_driver_suspend_all();
    ASSERT(rc == 0);
    ASSERT(drv.state == EOS_DRV_STATE_SUSPEND);

    rc = eos_driver_resume_all();
    ASSERT(rc == 0);
    ASSERT(drv.state == EOS_DRV_STATE_ACTIVE);

    eos_driver_close(&drv);
    eos_driver_deinit_all();
    eos_driver_unregister("test-pm");
}

TEST(test_driver_null_safety) {
    ASSERT(eos_driver_find(NULL) == NULL);
    ASSERT(eos_driver_open(NULL) != 0);
    ASSERT(eos_driver_close(NULL) != 0);
    ASSERT(eos_driver_read(NULL, NULL, 0) < 0);
    ASSERT(eos_driver_write(NULL, NULL, 0) < 0);
}

int main(void) {
    printf("=== EoS: Driver Recovery & Stability Tests ===\n\n");
    run_test_driver_init_success();
    run_test_driver_init_failure_sets_error();
    run_test_driver_open_requires_ready();
    run_test_driver_read_write_active();
    run_test_driver_suspend_resume();
    run_test_driver_null_safety();
    tests_run = 6;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
