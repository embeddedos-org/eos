// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file posix_ipc.h
 * @brief POSIX IPC (message queues, pipes, shared memory) for EoS
 *
 * Maps mq_*, pipe(), shm_open/shm_unlink onto EoS kernel and multicore APIs.
 */

#ifndef EOS_POSIX_IPC_H
#define EOS_POSIX_IPC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Message Queue
 * ============================================================ */

#define EOS_POSIX_MQ_MAX         8
#define EOS_POSIX_MQ_NAME_MAX   16

typedef int mqd_t;

struct mq_attr {
    long mq_flags;     /* 0 or O_NONBLOCK */
    long mq_maxmsg;    /* max messages in queue */
    long mq_msgsize;   /* max message size in bytes */
    long mq_curmsgs;   /* current messages (output only) */
};

/* Simplified oflag defines */
#define EOS_O_CREAT    0x0100
#define EOS_O_RDONLY   0x0000
#define EOS_O_WRONLY   0x0001
#define EOS_O_RDWR     0x0002
#define EOS_O_NONBLOCK 0x0800

mqd_t mq_open(const char *name, int oflag, ...);
int   mq_close(mqd_t mqdes);
int   mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len,
              unsigned int msg_prio);
ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len,
                   unsigned int *msg_prio);
int   mq_unlink(const char *name);

/* ============================================================
 * Pipe
 * ============================================================ */

/**
 * Create a unidirectional pipe using an EoS message queue.
 * pipefd[0] = read end, pipefd[1] = write end.
 * Both descriptors reference the same underlying queue.
 */
int pipe(int pipefd[2]);

/* ============================================================
 * Shared Memory (requires EOS_ENABLE_MULTICORE)
 * ============================================================ */

#define EOS_POSIX_SHM_MAX       4
#define EOS_POSIX_SHM_NAME_MAX 16

int shm_open(const char *name, int oflag, uint32_t mode);
int shm_unlink(const char *name);

/**
 * Map a shared memory descriptor to its base address.
 * Returns pointer or NULL on failure.
 */
void *shm_mmap(int shm_fd, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* EOS_POSIX_IPC_H */
