// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file main.c
 * @brief EoS Example: POSIX App — POSIX compatibility demo
 *
 * Demonstrates EoS POSIX compatibility layer: pthread_create,
 * semaphores, message queues, and sleep — all running on EoS.
 */

#include <eos/hal.h>
#include <eos/posix_threads.h>
#include <eos/posix_sync.h>
#include <eos/posix_ipc.h>
#include <eos/posix_time.h>
#include <stdio.h>
#include <string.h>

#define MQ_NAME      "/sensor_queue"
#define MQ_MAX_MSG   8
#define MQ_MSG_SIZE  64

static eos_sem_t  g_sem;
static eos_mqd_t  g_mq;

static void *writer_thread(void *arg)
{
    int thread_id = *(int *)arg;
    char msg[MQ_MSG_SIZE];

    printf("[writer-%d] Started\n", thread_id);

    for (int i = 0; i < 5; i++) {
        snprintf(msg, sizeof(msg), "msg from thread %d, seq %d", thread_id, i);

        /* Wait for permission to write */
        eos_sem_wait(&g_sem);

        int ret = eos_mq_send(g_mq, msg, strlen(msg) + 1, 0);
        if (ret == 0) {
            printf("[writer-%d] Sent: %s\n", thread_id, msg);
        }

        /* Signal that write is done */
        eos_sem_post(&g_sem);

        eos_sleep(1);
    }

    printf("[writer-%d] Done\n", thread_id);
    return NULL;
}

static void *reader_thread(void *arg)
{
    (void)arg;
    char buf[MQ_MSG_SIZE];

    printf("[reader] Started\n");

    for (int i = 0; i < 10; i++) {
        unsigned int prio = 0;
        ssize_t n = eos_mq_receive(g_mq, buf, sizeof(buf), &prio);
        if (n > 0) {
            printf("[reader] Received (%zd bytes): %s\n", n, buf);
        }
    }

    printf("[reader] Done\n");
    return NULL;
}

int main(void)
{
    eos_hal_init();

    printf("=== EoS POSIX Compatibility Demo ===\n\n");

    /* Initialize semaphore (binary semaphore, initially available) */
    eos_sem_init(&g_sem, 0, 1);

    /* Open message queue */
    struct eos_mq_attr attr = {
        .mq_maxmsg  = MQ_MAX_MSG,
        .mq_msgsize = MQ_MSG_SIZE,
    };
    g_mq = eos_mq_open(MQ_NAME, O_CREAT | O_RDWR, 0644, &attr);
    if (g_mq == (eos_mqd_t)-1) {
        printf("[main] Failed to create message queue\n");
        return 1;
    }

    /* Create threads */
    eos_pthread_t writer1, writer2, reader;
    int id1 = 1, id2 = 2;

    eos_pthread_create(&reader,  NULL, reader_thread, NULL);
    eos_pthread_create(&writer1, NULL, writer_thread, &id1);
    eos_pthread_create(&writer2, NULL, writer_thread, &id2);

    printf("[main] Threads created, waiting for completion...\n\n");

    /* Wait for all threads to finish */
    eos_pthread_join(writer1, NULL);
    eos_pthread_join(writer2, NULL);
    eos_pthread_join(reader, NULL);

    /* Cleanup */
    eos_mq_close(g_mq);
    eos_mq_unlink(MQ_NAME);
    eos_sem_destroy(&g_sem);

    printf("\n=== POSIX demo complete ===\n");
    return 0;
}
