// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file main.c
 * @brief EoS Example: Multitask RTOS — Kernel usage demo
 *
 * Creates 3 tasks: a producer, a consumer, and a monitor.
 * Producer sends data through a message queue, consumer reads it,
 * and both use a mutex to protect a shared counter.
 */

#include <eos/hal.h>
#include <eos/kernel.h>
#include <stdio.h>
#include <string.h>

#define QUEUE_CAPACITY 8

typedef struct {
    uint32_t id;
    int32_t  value;
    uint32_t timestamp;
} message_t;

static eos_mutex_handle_t g_mutex;
static eos_queue_handle_t g_queue;
static volatile uint32_t  g_shared_counter = 0;

static void producer_task(void *arg)
{
    (void)arg;
    uint32_t seq = 0;

    printf("[producer] Started\n");

    while (1) {
        message_t msg = {
            .id        = seq++,
            .value     = (int32_t)(seq * 10 + 42),
            .timestamp = eos_get_tick_ms(),
        };

        int ret = eos_queue_send(g_queue, &msg, EOS_WAIT_FOREVER);
        if (ret == EOS_KERN_OK) {
            printf("[producer] Sent msg id=%lu value=%ld\n",
                   (unsigned long)msg.id, (long)msg.value);
        }

        /* Update shared counter with mutex protection */
        eos_mutex_lock(g_mutex, EOS_WAIT_FOREVER);
        g_shared_counter++;
        eos_mutex_unlock(g_mutex);

        eos_task_delay_ms(500);
    }
}

static void consumer_task(void *arg)
{
    (void)arg;

    printf("[consumer] Started\n");

    while (1) {
        message_t msg;
        int ret = eos_queue_receive(g_queue, &msg, EOS_WAIT_FOREVER);
        if (ret == EOS_KERN_OK) {
            printf("[consumer] Received msg id=%lu value=%ld ts=%lu ms\n",
                   (unsigned long)msg.id, (long)msg.value,
                   (unsigned long)msg.timestamp);
        }

        /* Update shared counter with mutex protection */
        eos_mutex_lock(g_mutex, EOS_WAIT_FOREVER);
        g_shared_counter++;
        eos_mutex_unlock(g_mutex);
    }
}

static void monitor_task(void *arg)
{
    (void)arg;

    printf("[monitor] Started\n");

    while (1) {
        eos_task_delay_ms(2000);

        eos_mutex_lock(g_mutex, EOS_WAIT_FOREVER);
        uint32_t count = g_shared_counter;
        eos_mutex_unlock(g_mutex);

        uint32_t qcount = eos_queue_count(g_queue);

        printf("[monitor] counter=%lu queue_depth=%lu uptime=%lu ms\n",
               (unsigned long)count,
               (unsigned long)qcount,
               (unsigned long)eos_get_tick_ms());
    }
}

int main(void)
{
    eos_hal_init();
    eos_kernel_init();

    /* Create synchronization primitives */
    eos_mutex_create(&g_mutex);
    eos_queue_create(&g_queue, sizeof(message_t), QUEUE_CAPACITY);

    printf("[main] Creating tasks...\n");

    /* Create tasks with different priorities */
    eos_task_create("producer", producer_task, NULL, 2, 1024);
    eos_task_create("consumer", consumer_task, NULL, 3, 1024);
    eos_task_create("monitor",  monitor_task,  NULL, 1, 512);

    printf("[main] Starting kernel...\n");
    eos_kernel_start();

    return 0;
}
