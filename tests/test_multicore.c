// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define EOS_ENABLE_MULTICORE 1
#include "eos/eos_config.h"
#include "eos/multicore.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

static void dummy_entry(void *arg) { (void)arg; }

static void test_mc_init(void) {
    assert(eos_multicore_init(EOS_MP_SMP) == 0);
    eos_multicore_deinit();
    PASS("multicore init/deinit SMP");
}

static void test_mc_init_amp(void) {
    assert(eos_multicore_init(EOS_MP_AMP) == 0);
    eos_multicore_deinit();
    PASS("multicore init AMP");
}

static void test_core_count(void) {
    eos_multicore_init(EOS_MP_SMP);
    uint8_t count = eos_core_count();
    assert(count >= 1);
    eos_multicore_deinit();
    PASS("core count >= 1");
}

static void test_core_id(void) {
    eos_multicore_init(EOS_MP_SMP);
    uint8_t id = eos_core_id();
    assert(id == 0);
    eos_multicore_deinit();
    PASS("core id == 0 (boot core)");
}

static void test_core_get_info(void) {
    eos_multicore_init(EOS_MP_SMP);
    eos_core_info_t info;
    assert(eos_core_get_info(0, &info) == 0);
    assert(info.core_id == 0);
    assert(info.state == EOS_CORE_ONLINE);
    eos_multicore_deinit();
    PASS("core get info");
}

static void test_core_get_info_null(void) {
    eos_multicore_init(EOS_MP_SMP);
    assert(eos_core_get_info(0, NULL) != 0);
    eos_multicore_deinit();
    PASS("core get info null");
}

static void test_core_get_info_invalid_id(void) {
    eos_multicore_init(EOS_MP_SMP);
    eos_core_info_t info;
    assert(eos_core_get_info(EOS_MAX_CORES, &info) != 0);
    eos_multicore_deinit();
    PASS("core get info invalid id");
}

static void test_core_start(void) {
    eos_multicore_init(EOS_MP_SMP);
    int r = eos_core_start(1, dummy_entry, NULL);
    assert(r == 0);
    eos_core_info_t info;
    eos_core_get_info(1, &info);
    assert(info.state == EOS_CORE_ONLINE);
    eos_multicore_deinit();
    PASS("core start");
}

static void test_core_stop(void) {
    eos_multicore_init(EOS_MP_SMP);
    eos_core_start(1, dummy_entry, NULL);
    assert(eos_core_stop(1) == 0);
    eos_core_info_t info;
    eos_core_get_info(1, &info);
    assert(info.state == EOS_CORE_OFFLINE);
    eos_multicore_deinit();
    PASS("core stop");
}

static void test_spinlock_basic(void) {
    eos_spinlock_t lock = EOS_SPINLOCK_INIT;
    eos_spin_init(&lock);
    eos_spin_lock(&lock);
    eos_spin_unlock(&lock);
    PASS("spinlock basic");
}

static void test_spinlock_trylock(void) {
    eos_spinlock_t lock = EOS_SPINLOCK_INIT;
    eos_spin_init(&lock);
    assert(eos_spin_trylock(&lock) == true);
    eos_spin_unlock(&lock);
    PASS("spinlock trylock");
}

static void test_spinlock_irqsave(void) {
    eos_spinlock_t lock = EOS_SPINLOCK_INIT;
    uint32_t flags = 0;
    eos_spin_init(&lock);
    eos_spin_lock_irqsave(&lock, &flags);
    eos_spin_unlock_irqrestore(&lock, flags);
    PASS("spinlock irqsave");
}

static void test_ipi_send(void) {
    eos_multicore_init(EOS_MP_SMP);
    eos_core_start(1, dummy_entry, NULL);
    int r = eos_ipi_send(1, EOS_IPI_RESCHEDULE, 0);
    assert(r == 0);
    eos_multicore_deinit();
    PASS("ipi send");
}

static void test_ipi_broadcast(void) {
    eos_multicore_init(EOS_MP_SMP);
    int r = eos_ipi_broadcast(EOS_IPI_RESCHEDULE, 42);
    assert(r == 0);
    eos_multicore_deinit();
    PASS("ipi broadcast");
}

static void test_ipi_register_handler(void) {
    eos_multicore_init(EOS_MP_SMP);
    assert(eos_ipi_register_handler(EOS_IPI_USER, NULL, NULL) == 0);
    eos_multicore_deinit();
    PASS("ipi register handler");
}

static void test_shmem_create_close(void) {
    eos_multicore_init(EOS_MP_SMP);
    static char buf[256];
    eos_shmem_config_t cfg = { .name = "test", .base = buf, .size = sizeof(buf), .cached = false };
    eos_shmem_region_t region;
    assert(eos_shmem_create(&cfg, &region) == 0);
    assert(region.size == sizeof(buf));
    assert(eos_shmem_close(&region) == 0);
    eos_multicore_deinit();
    PASS("shmem create/close");
}

static void test_shmem_open(void) {
    eos_multicore_init(EOS_MP_SMP);
    static char buf[128];
    eos_shmem_config_t cfg = { .name = "sh2", .base = buf, .size = sizeof(buf), .cached = false };
    eos_shmem_region_t r1, r2;
    eos_shmem_create(&cfg, &r1);
    assert(eos_shmem_open("sh2", &r2) == 0);
    eos_shmem_close(&r1);
    eos_multicore_deinit();
    PASS("shmem open");
}

static void test_shmem_flush_invalidate(void) {
    eos_multicore_init(EOS_MP_SMP);
    static char buf[64];
    eos_shmem_config_t cfg = { .name = "sh3", .base = buf, .size = sizeof(buf), .cached = true };
    eos_shmem_region_t region;
    eos_shmem_create(&cfg, &region);
    eos_shmem_flush(&region);
    eos_shmem_invalidate(&region);
    eos_shmem_close(&region);
    eos_multicore_deinit();
    PASS("shmem flush/invalidate");
}

static void test_task_affinity(void) {
    eos_multicore_init(EOS_MP_SMP);
    eos_core_mask_t mask = EOS_CORE_MASK(0) | EOS_CORE_MASK(1);
    assert(eos_task_set_affinity(0, mask) == 0);
    eos_core_mask_t got;
    assert(eos_task_get_affinity(0, &got) == 0);
    assert(got == mask);
    eos_multicore_deinit();
    PASS("task affinity set/get");
}

static void test_task_migrate(void) {
    eos_multicore_init(EOS_MP_SMP);
    assert(eos_task_migrate(0, 1) == 0);
    eos_multicore_deinit();
    PASS("task migrate");
}

static void test_atomic_add(void) {
    volatile int32_t val = 10;
    int32_t old = eos_atomic_add(&val, 5);
    assert(old == 10);
    assert(val == 15);
    PASS("atomic add");
}

static void test_atomic_sub(void) {
    volatile int32_t val = 20;
    int32_t old = eos_atomic_sub(&val, 7);
    assert(old == 20);
    assert(val == 13);
    PASS("atomic sub");
}

static void test_atomic_cas_success(void) {
    volatile int32_t val = 42;
    int32_t old = eos_atomic_cas(&val, 42, 99);
    assert(old == 42);
    assert(val == 99);
    PASS("atomic CAS success");
}

static void test_atomic_cas_fail(void) {
    volatile int32_t val = 42;
    int32_t old = eos_atomic_cas(&val, 100, 99);
    assert(old == 42);
    assert(val == 42);
    PASS("atomic CAS fail");
}

static void test_atomic_load_store(void) {
    volatile int32_t val = 0;
    eos_atomic_store(&val, 77);
    assert(eos_atomic_load(&val) == 77);
    PASS("atomic load/store");
}

static void test_memory_barriers(void) {
    eos_dmb();
    eos_dsb();
    eos_isb();
    PASS("memory barriers (no crash)");
}

static void test_rproc_init(void) {
    eos_multicore_init(EOS_MP_AMP);
    eos_rproc_config_t cfg = { .id = 0, .name = "m4", .firmware_addr = 0x08000000, .firmware_size = 65536 };
    assert(eos_rproc_init(&cfg) == 0);
    eos_multicore_deinit();
    PASS("rproc init");
}

static void test_rproc_start_stop(void) {
    eos_multicore_init(EOS_MP_AMP);
    eos_rproc_config_t cfg = { .id = 0, .name = "m4core", .firmware_addr = 0x08000000, .firmware_size = 65536 };
    eos_rproc_init(&cfg);
    assert(eos_rproc_start(0) == 0);
    assert(eos_rproc_get_state(0) == EOS_RPROC_RUNNING);
    assert(eos_rproc_stop(0) == 0);
    assert(eos_rproc_get_state(0) == EOS_RPROC_OFFLINE);
    eos_multicore_deinit();
    PASS("rproc start/stop");
}

int main(void) {
    printf("=== EoS Multicore Tests ===\n");
    test_mc_init();
    test_mc_init_amp();
    test_core_count();
    test_core_id();
    test_core_get_info();
    test_core_get_info_null();
    test_core_get_info_invalid_id();
    test_core_start();
    test_core_stop();
    test_spinlock_basic();
    test_spinlock_trylock();
    test_spinlock_irqsave();
    test_ipi_send();
    test_ipi_broadcast();
    test_ipi_register_handler();
    test_shmem_create_close();
    test_shmem_open();
    test_shmem_flush_invalidate();
    test_task_affinity();
    test_task_migrate();
    test_atomic_add();
    test_atomic_sub();
    test_atomic_cas_success();
    test_atomic_cas_fail();
    test_atomic_load_store();
    test_memory_barriers();
    test_rproc_init();
    test_rproc_start_stop();
    printf("\n=== ALL %d MULTICORE TESTS PASSED ===\n", passed);
    return 0;
}
