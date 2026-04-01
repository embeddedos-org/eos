// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_drivers.c
 * @brief Unit tests for EoS driver registration framework
 */

#include <eos/drivers.h>
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

/* Mock driver ops */
static int mock_drv_init_called = 0;
static int mock_drv_open_called = 0;

static int mock_drv_init(struct eos_driver *d) { (void)d; mock_drv_init_called++; return 0; }
static void mock_drv_deinit(struct eos_driver *d) { (void)d; }
static int mock_drv_open(struct eos_driver *d) { (void)d; mock_drv_open_called++; return 0; }
static int mock_drv_close(struct eos_driver *d) { (void)d; return 0; }
static int mock_drv_read(struct eos_driver *d, void *b, size_t l) { (void)d; (void)b; (void)l; return 0; }
static int mock_drv_write(struct eos_driver *d, const void *b, size_t l) { (void)d; (void)b; (void)l; return 0; }

static const eos_driver_ops_t mock_ops = {
    .init = mock_drv_init, .deinit = mock_drv_deinit,
    .open = mock_drv_open, .close = mock_drv_close,
    .read = mock_drv_read, .write = mock_drv_write,
};

static eos_driver_t uart0_drv = {
    .name = "uart0", .type = EOS_DRIVER_UART, .instance = 0, .ops = &mock_ops,
};

static eos_driver_t spi0_drv = {
    .name = "spi0", .type = EOS_DRIVER_SPI, .instance = 0, .ops = &mock_ops,
};

TEST(test_register) {
    mock_drv_init_called = 0;
    ASSERT(eos_driver_register(&uart0_drv) == 0);
    ASSERT(eos_driver_count() >= 1);
}

TEST(test_find_by_name) {
    eos_driver_register(&uart0_drv);
    eos_driver_t *d = eos_driver_find("uart0");
    ASSERT(d != NULL);
    ASSERT(d->type == EOS_DRIVER_UART);
    ASSERT(d->instance == 0);
}

TEST(test_find_by_type) {
    eos_driver_register(&uart0_drv);
    eos_driver_register(&spi0_drv);
    eos_driver_t *d = eos_driver_find_by_type(EOS_DRIVER_SPI, 0);
    ASSERT(d != NULL);
    ASSERT(strcmp(d->name, "spi0") == 0);
}

TEST(test_find_nonexistent) {
    ASSERT(eos_driver_find("i2c99") == NULL);
    ASSERT(eos_driver_find(NULL) == NULL);
}

TEST(test_init_all) {
    mock_drv_init_called = 0;
    eos_driver_register(&uart0_drv);
    eos_driver_register(&spi0_drv);
    int errors = eos_driver_init_all();
    ASSERT(errors == 0);
    ASSERT(mock_drv_init_called >= 2);
    ASSERT(uart0_drv.state == EOS_DRV_STATE_READY);
}

TEST(test_open_close_lifecycle) {
    mock_drv_open_called = 0;
    eos_driver_register(&uart0_drv);
    eos_driver_init_all();

    ASSERT(uart0_drv.state == EOS_DRV_STATE_READY);
    ASSERT(eos_driver_open(&uart0_drv) == 0);
    ASSERT(uart0_drv.state == EOS_DRV_STATE_ACTIVE);
    ASSERT(mock_drv_open_called >= 1);

    ASSERT(eos_driver_close(&uart0_drv) == 0);
    ASSERT(uart0_drv.state == EOS_DRV_STATE_READY);
}

TEST(test_open_before_init_fails) {
    eos_driver_t fresh = {
        .name = "fresh", .type = EOS_DRIVER_GPIO, .instance = 0,
        .ops = &mock_ops, .state = EOS_DRV_STATE_UNINIT,
    };
    eos_driver_register(&fresh);
    ASSERT(eos_driver_open(&fresh) != 0);
}

TEST(test_register_null) {
    ASSERT(eos_driver_register(NULL) != 0);
}

int main(void) {
    printf("=== EoS: Driver Framework Unit Tests ===\n\n");
    run_test_register();
    run_test_find_by_name();
    run_test_find_by_type();
    run_test_find_nonexistent();
    run_test_init_all();
    run_test_open_close_lifecycle();
    run_test_open_before_init_fails();
    run_test_register_null();
    tests_run = 8;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
