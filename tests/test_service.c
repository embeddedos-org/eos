// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eos/service.h"

static int g_svc_started = 0;
static int svc_start(void *ctx) { g_svc_started++; (void)ctx; return 0; }
static void svc_stop(void *ctx) { g_svc_started--; (void)ctx; }
static int svc_health(void *ctx) { (void)ctx; return 0; }
static int svc_fail_start(void *ctx) { (void)ctx; return -1; }

static void test_svc_init(void) {
    EosSvcManager mgr;
    assert(eos_svc_init(&mgr) == 0);
    assert(eos_svc_init(NULL) == -1);
    printf("[PASS] service init\n");
}

static void test_svc_register(void) {
    EosSvcManager mgr; eos_svc_init(&mgr);
    EosSvcOps ops = { .start = svc_start, .stop = svc_stop, .health = svc_health };
    assert(eos_svc_register(&mgr, "svc1", &ops) == 0);
    assert(eos_svc_register(&mgr, "svc1", &ops) == -1);  /* dup */
    assert(eos_svc_find(&mgr, "svc1") != NULL);
    assert(eos_svc_find(&mgr, "nope") == NULL);
    printf("[PASS] service register\n");
}

static void test_svc_start_stop(void) {
    EosSvcManager mgr; eos_svc_init(&mgr);
    EosSvcOps ops = { .start = svc_start, .stop = svc_stop };
    eos_svc_register(&mgr, "test", &ops);
    g_svc_started = 0;
    assert(eos_svc_start(&mgr, "test") == 0);
    assert(g_svc_started == 1);
    assert(eos_svc_get_state(&mgr, "test") == EOS_SVC_RUNNING);
    assert(eos_svc_stop(&mgr, "test") == 0);
    assert(g_svc_started == 0);
    assert(eos_svc_get_state(&mgr, "test") == EOS_SVC_STOPPED);
    printf("[PASS] service start/stop\n");
}

static void test_svc_dependencies(void) {
    EosSvcManager mgr; eos_svc_init(&mgr);
    EosSvcOps ops = { .start = svc_start, .stop = svc_stop };
    eos_svc_register(&mgr, "db", &ops);
    eos_svc_register(&mgr, "app", &ops);
    eos_svc_add_dependency(&mgr, "app", "db");
    /* app can't start without db */
    assert(eos_svc_start(&mgr, "app") == -1);
    eos_svc_start(&mgr, "db");
    assert(eos_svc_start(&mgr, "app") == 0);
    printf("[PASS] service dependencies\n");
}

static void test_svc_start_all(void) {
    EosSvcManager mgr; eos_svc_init(&mgr);
    EosSvcOps ops = { .start = svc_start, .stop = svc_stop };
    eos_svc_register(&mgr, "a", &ops);
    eos_svc_register(&mgr, "b", &ops);
    eos_svc_register(&mgr, "c", &ops);
    g_svc_started = 0;
    eos_svc_start_all(&mgr);
    assert(g_svc_started == 3);
    eos_svc_stop_all(&mgr);
    assert(g_svc_started == 0);
    printf("[PASS] service start_all/stop_all\n");
}

static void test_svc_restart(void) {
    EosSvcManager mgr; eos_svc_init(&mgr);
    EosSvcOps ops = { .start = svc_start, .stop = svc_stop };
    eos_svc_register(&mgr, "rs", &ops);
    g_svc_started = 0;
    eos_svc_start(&mgr, "rs");
    assert(g_svc_started == 1);
    eos_svc_restart(&mgr, "rs");
    assert(g_svc_started == 1);
    printf("[PASS] service restart\n");
}

int main(void) {
    printf("=== EoS Service Manager Tests ===\n");
    test_svc_init();
    test_svc_register();
    test_svc_start_stop();
    test_svc_dependencies();
    test_svc_start_all();
    test_svc_restart();
    printf("=== ALL SERVICE TESTS PASSED ===\n");
    return 0;
}