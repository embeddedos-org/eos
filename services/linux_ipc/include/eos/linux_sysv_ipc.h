// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file linux_sysv_ipc.h
 * @brief EoS System V IPC Emulation Layer
 *
 * Provides System V shared memory (shmget/shmat/shmdt/shmctl),
 * message queues (msgget/msgsnd/msgrcv/msgctl), and semaphore arrays
 * (semget/semop/semctl) on top of EoS kernel and multicore primitives.
 * On native Linux targets (EOS_PLATFORM_LINUX), these pass through
 * to real syscalls.
 */

#ifndef EOS_LINUX_SYSV_IPC_H
#define EOS_LINUX_SYSV_IPC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Configuration
 * ============================================================ */

#ifndef EOS_SYSV_SHM_MAX
#define EOS_SYSV_SHM_MAX       8
#endif

#ifndef EOS_SYSV_MSG_MAX
#define EOS_SYSV_MSG_MAX       8
#endif

#ifndef EOS_SYSV_SEM_MAX
#define EOS_SYSV_SEM_MAX       8
#endif

#ifndef EOS_SYSV_SEM_NSEMS_MAX
#define EOS_SYSV_SEM_NSEMS_MAX 8
#endif

#ifndef EOS_SYSV_MSG_SIZE_MAX
#define EOS_SYSV_MSG_SIZE_MAX  256
#endif

#ifndef EOS_SYSV_MSG_QUEUE_DEPTH
#define EOS_SYSV_MSG_QUEUE_DEPTH 16
#endif

/* ============================================================
 * IPC Flags / Commands
 * ============================================================ */

typedef int32_t eos_key_t;

#define EOS_IPC_PRIVATE 0
#define EOS_IPC_CREAT   (1 << 0)
#define EOS_IPC_EXCL    (1 << 1)
#define EOS_IPC_RMID    0
#define EOS_IPC_SET     1
#define EOS_IPC_STAT    2

/* ============================================================
 * Shared Memory
 * ============================================================ */

#define EOS_SHM_RDONLY  (1 << 0)

typedef struct {
    eos_key_t  shm_key;
    size_t     shm_segsz;
    uint32_t   shm_nattch;
    uint16_t   shm_perm;
} eos_shmid_ds_t;

int   eos_shmget(eos_key_t key, size_t size, int shmflg);
void *eos_shmat(int shmid, const void *shmaddr, int shmflg);
int   eos_shmdt(const void *shmaddr);
int   eos_shmctl(int shmid, int cmd, eos_shmid_ds_t *buf);

/* ============================================================
 * Message Queues
 * ============================================================ */

typedef struct {
    long mtype;
} eos_msgbuf_t;

typedef struct {
    eos_key_t  msg_key;
    uint32_t   msg_qnum;
    size_t     msg_qbytes;
    uint16_t   msg_perm;
} eos_msqid_ds_t;

#define EOS_IPC_NOWAIT  (1 << 2)

int eos_msgget(eos_key_t key, int msgflg);
int eos_msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg);
int eos_msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);
int eos_msgctl(int msqid, int cmd, eos_msqid_ds_t *buf);

/* ============================================================
 * Semaphore Arrays
 * ============================================================ */

typedef struct {
    uint16_t sem_num;
    int16_t  sem_op;
    int16_t  sem_flg;
} eos_sembuf_t;

typedef struct {
    eos_key_t  sem_key;
    uint16_t   sem_nsems;
    uint16_t   sem_perm;
} eos_semid_ds_t;

/* semctl commands */
#define EOS_GETVAL  3
#define EOS_SETVAL  4
#define EOS_GETALL  5
#define EOS_SETALL  6

typedef union {
    int              val;
    eos_semid_ds_t  *buf;
    uint16_t        *array;
} eos_semun_t;

int eos_semget(eos_key_t key, int nsems, int semflg);
int eos_semop(int semid, eos_sembuf_t *sops, size_t nsops);
int eos_semctl(int semid, int semnum, int cmd, eos_semun_t arg);

/* ============================================================
 * Module Init / Deinit
 * ============================================================ */

int  eos_sysv_ipc_init(void);
void eos_sysv_ipc_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_LINUX_SYSV_IPC_H */
