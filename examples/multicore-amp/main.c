// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file main.c
 * @brief EoS Example: Multicore AMP — Dual-core application
 *
 * Starts a secondary core, sets up shared memory for IPC,
 * and uses spinlocks for synchronization. Demonstrates
 * asymmetric multiprocessing with EoS.
 */

#include <eos/hal.h>
#include <eos/kernel.h>
#include <eos/multicore.h>
#include <stdio.h>
#include <string.h>

#define SHMEM_SIZE 4096

typedef struct {
    volatile uint32_t    write_idx;
    volatile uint32_t    read_idx;
    volatile uint32_t    count;
    eos_spinlock_t       lock;
    volatile uint8_t     data[SHMEM_SIZE - 16];
} shared_ringbuf_t;

static eos_shmem_region_t g_shmem;
static shared_ringbuf_t  *g_ring;

static void ringbuf_init(shared_ringbuf_t *ring)
{
    ring->write_idx = 0;
    ring->read_idx  = 0;
    ring->count     = 0;
    eos_spin_init(&ring->lock);
    memset((void *)ring->data, 0, sizeof(ring->data));
}

static int ringbuf_write(shared_ringbuf_t *ring, uint8_t value)
{
    eos_spin_lock(&ring->lock);

    if (ring->count >= sizeof(ring->data)) {
        eos_spin_unlock(&ring->lock);
        return -1;  /* full */
    }

    ring->data[ring->write_idx] = value;
    ring->write_idx = (ring->write_idx + 1) % sizeof(ring->data);
    ring->count++;

    eos_spin_unlock(&ring->lock);
    return 0;
}

static int ringbuf_read(shared_ringbuf_t *ring, uint8_t *value)
{
    eos_spin_lock(&ring->lock);

    if (ring->count == 0) {
        eos_spin_unlock(&ring->lock);
        return -1;  /* empty */
    }

    *value = ring->data[ring->read_idx];
    ring->read_idx = (ring->read_idx + 1) % sizeof(ring->data);
    ring->count--;

    eos_spin_unlock(&ring->lock);
    return 0;
}

static void ipi_handler(uint8_t from_core, uint32_t data, void *ctx)
{
    (void)ctx;
    printf("[core-%u] IPI received from core %u: data=0x%08lx\n",
           eos_core_id(), from_core, (unsigned long)data);
}

static void secondary_core_main(void *arg)
{
    (void)arg;

    uint8_t core = eos_core_id();
    printf("[core-%u] Secondary core started\n", core);

    /* Read messages from shared memory and process them */
    uint32_t processed = 0;

    while (processed < 20) {
        uint8_t value;
        if (ringbuf_read(g_ring, &value) == 0) {
            printf("[core-%u] Read value: %u (processed=%lu)\n",
                   core, value, (unsigned long)processed);
            processed++;

            /* Send IPI back to primary with processed count */
            eos_ipi_send(0, EOS_IPI_USER, processed);
        }
        eos_delay_ms(100);
    }

    printf("[core-%u] Secondary core done (processed %lu messages)\n",
           core, (unsigned long)processed);
}

static void producer_task(void *arg)
{
    (void)arg;

    printf("[core-0] Producer task started\n");

    for (uint32_t i = 0; i < 20; i++) {
        uint8_t value = (uint8_t)(i + 1);

        if (ringbuf_write(g_ring, value) == 0) {
            printf("[core-0] Wrote value: %u\n", value);
        } else {
            printf("[core-0] Ring buffer full, retrying...\n");
            i--;
        }

        eos_task_delay_ms(200);
    }

    printf("[core-0] Producer done\n");
}

int main(void)
{
    eos_hal_init();
    eos_kernel_init();

    printf("=== EoS Multicore AMP Demo ===\n");
    printf("[core-0] Primary core running, %u cores available\n",
           eos_core_count());

    /* Initialize multicore in AMP mode */
    eos_multicore_init(EOS_MP_AMP);

    /* Create shared memory region */
    eos_shmem_config_t shmem_cfg = {
        .name   = "ipc_ring",
        .base   = NULL,  /* let OS allocate */
        .size   = sizeof(shared_ringbuf_t),
        .cached = false,
    };
    eos_shmem_create(&shmem_cfg, &g_shmem);
    g_ring = (shared_ringbuf_t *)g_shmem.addr;
    ringbuf_init(g_ring);

    printf("[core-0] Shared memory created: %zu bytes at %p\n",
           g_shmem.size, g_shmem.addr);

    /* Register IPI handler */
    eos_ipi_register_handler(EOS_IPI_USER, ipi_handler, NULL);

    /* Start secondary core */
    printf("[core-0] Starting core 1...\n");
    eos_core_start(1, secondary_core_main, NULL);

    /* Create producer task on primary core */
    eos_task_create("producer", producer_task, NULL, 2, 1024);

    printf("[core-0] Starting kernel...\n");
    eos_kernel_start();

    /* Cleanup */
    eos_shmem_close(&g_shmem);
    eos_multicore_deinit();

    printf("=== Multicore demo complete ===\n");
    return 0;
}
