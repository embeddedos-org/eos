// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file linux_sysv_ipc.c
 * @brief EoS System V IPC Emulation — shmget/msgget/semget families
 *
 * On RTOS targets: emulated via EoS multicore shared memory, kernel
 * queues, and semaphores.
 * On Linux targets (EOS_PLATFORM_LINUX): passthrough to real syscalls.
 *
 * Limitations:
 *  - SEM_UNDO is not supported (semaphore undo on process exit).
 *  - Permissions (mode bits) are stored but not enforced.
 *  - IPC_PRIVATE generates sequential private keys internally.
 */

#include "eos/linux_sysv_ipc.h"
#include "eos/kernel.h"
#include "eos/hal.h"
#include <string.h>

#ifdef EOS_PLATFORM_LINUX
/* ============================================================
 * Native Linux passthrough
 * ============================================================ */
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>

int eos_sysv_ipc_init(void)  { return 0; }
void eos_sysv_ipc_deinit(void) {}

int eos_shmget(eos_key_t key, size_t size, int shmflg) {
    int flags = 0;
    if (shmflg & EOS_IPC_CREAT) flags |= IPC_CREAT;
    if (shmflg & EOS_IPC_EXCL)  flags |= IPC_EXCL;
    flags |= 0666;
    return shmget((key_t)key, size, flags);
}

void *eos_shmat(int shmid, const void *shmaddr, int shmflg) {
    int flags = 0;
    if (shmflg & EOS_SHM_RDONLY) flags |= SHM_RDONLY;
    return shmat(shmid, shmaddr, flags);
}

int eos_shmdt(const void *shmaddr) {
    return shmdt(shmaddr);
}

int eos_shmctl(int shmid, int cmd, eos_shmid_ds_t *buf) {
    int ncmd;
    switch (cmd) {
        case EOS_IPC_RMID: ncmd = IPC_RMID; break;
        case EOS_IPC_STAT: ncmd = IPC_STAT; break;
        case EOS_IPC_SET:  ncmd = IPC_SET;  break;
        default: return -1;
    }
    struct shmid_ds ds;
    int rc = shmctl(shmid, ncmd, &ds);
    if (rc == 0 && buf && cmd == EOS_IPC_STAT) {
        buf->shm_key    = (eos_key_t)0;
        buf->shm_segsz  = ds.shm_segsz;
        buf->shm_nattch = (uint32_t)ds.shm_nattch;
        buf->shm_perm   = (uint16_t)(ds.shm_perm.mode & 0xFFFF);
    }
    return rc;
}

int eos_msgget(eos_key_t key, int msgflg) {
    int flags = 0;
    if (msgflg & EOS_IPC_CREAT) flags |= IPC_CREAT;
    if (msgflg & EOS_IPC_EXCL)  flags |= IPC_EXCL;
    flags |= 0666;
    return msgget((key_t)key, flags);
}

int eos_msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg) {
    int flags = 0;
    if (msgflg & EOS_IPC_NOWAIT) flags |= IPC_NOWAIT;
    return msgsnd(msqid, msgp, msgsz, flags);
}

int eos_msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg) {
    int flags = 0;
    if (msgflg & EOS_IPC_NOWAIT) flags |= IPC_NOWAIT;
    return (int)msgrcv(msqid, msgp, msgsz, msgtyp, flags);
}

int eos_msgctl(int msqid, int cmd, eos_msqid_ds_t *buf) {
    int ncmd;
    switch (cmd) {
        case EOS_IPC_RMID: ncmd = IPC_RMID; break;
        case EOS_IPC_STAT: ncmd = IPC_STAT; break;
        case EOS_IPC_SET:  ncmd = IPC_SET;  break;
        default: return -1;
    }
    struct msqid_ds ds;
    int rc = msgctl(msqid, ncmd, &ds);
    if (rc == 0 && buf && cmd == EOS_IPC_STAT) {
        buf->msg_key    = (eos_key_t)0;
        buf->msg_qnum   = (uint32_t)ds.msg_qnum;
        buf->msg_qbytes = ds.msg_qbytes;
        buf->msg_perm   = (uint16_t)(ds.msg_perm.mode & 0xFFFF);
    }
    return rc;
}

int eos_semget(eos_key_t key, int nsems, int semflg) {
    int flags = 0;
    if (semflg & EOS_IPC_CREAT) flags |= IPC_CREAT;
    if (semflg & EOS_IPC_EXCL)  flags |= IPC_EXCL;
    flags |= 0666;
    return semget((key_t)key, nsems, flags);
}

int eos_semop(int semid, eos_sembuf_t *sops, size_t nsops) {
    struct sembuf ops[EOS_SYSV_SEM_NSEMS_MAX];
    if (nsops > EOS_SYSV_SEM_NSEMS_MAX) return -1;
    for (size_t i = 0; i < nsops; i++) {
        ops[i].sem_num = sops[i].sem_num;
        ops[i].sem_op  = sops[i].sem_op;
        ops[i].sem_flg = sops[i].sem_flg;
    }
    return semop(semid, ops, nsops);
}

int eos_semctl(int semid, int semnum, int cmd, eos_semun_t arg) {
    (void)arg;
    switch (cmd) {
        case EOS_IPC_RMID: return semctl(semid, semnum, IPC_RMID);
        case EOS_GETVAL:   return semctl(semid, semnum, GETVAL);
        case EOS_SETVAL: {
            union semun { int val; } su;
            su.val = arg.val;
            return semctl(semid, semnum, SETVAL, su);
        }
        default: return -1;
    }
}

#else /* RTOS emulation via EoS kernel primitives */

#include "eos/multicore.h"

/* ============================================================
 * Internal tables
 * ============================================================ */

typedef struct {
    bool               used;
    eos_key_t          key;
    size_t             size;
    uint32_t           ref_count;
    uint16_t           perm;
    eos_shmem_region_t region;
    bool               region_valid;
} sysv_shm_entry_t;

typedef struct {
    bool               used;
    eos_key_t          key;
    eos_queue_handle_t queue;
    bool               queue_valid;
    uint16_t           perm;
} sysv_msg_entry_t;

typedef struct {
    bool              used;
    eos_key_t         key;
    uint16_t          nsems;
    eos_sem_handle_t  sems[EOS_SYSV_SEM_NSEMS_MAX];
    bool              sem_valid[EOS_SYSV_SEM_NSEMS_MAX];
    uint16_t          perm;
} sysv_sem_entry_t;

static sysv_shm_entry_t shm_table[EOS_SYSV_SHM_MAX];
static sysv_msg_entry_t msg_table[EOS_SYSV_MSG_MAX];
static sysv_sem_entry_t sem_table[EOS_SYSV_SEM_MAX];
static eos_key_t        private_key_counter = -1;
static bool             sysv_initialized = false;

/* ============================================================
 * Helpers
 * ============================================================ */

static eos_key_t next_private_key(void) {
    return private_key_counter--;
}

/* ============================================================
 * Module Init / Deinit
 * ============================================================ */

int eos_sysv_ipc_init(void) {
    if (sysv_initialized) return EOS_KERN_OK;

    memset(shm_table, 0, sizeof(shm_table));
    memset(msg_table, 0, sizeof(msg_table));
    memset(sem_table, 0, sizeof(sem_table));
    private_key_counter = -1;

    sysv_initialized = true;
    return EOS_KERN_OK;
}

void eos_sysv_ipc_deinit(void) {
    for (int i = 0; i < EOS_SYSV_SHM_MAX; i++) {
        if (shm_table[i].used && shm_table[i].region_valid) {
            eos_shmem_close(&shm_table[i].region);
        }
    }
    for (int i = 0; i < EOS_SYSV_MSG_MAX; i++) {
        if (msg_table[i].used && msg_table[i].queue_valid) {
            eos_queue_delete(msg_table[i].queue);
        }
    }
    for (int i = 0; i < EOS_SYSV_SEM_MAX; i++) {
        if (sem_table[i].used) {
            for (int j = 0; j < sem_table[i].nsems; j++) {
                if (sem_table[i].sem_valid[j]) {
                    eos_sem_delete(sem_table[i].sems[j]);
                }
            }
        }
    }

    memset(shm_table, 0, sizeof(shm_table));
    memset(msg_table, 0, sizeof(msg_table));
    memset(sem_table, 0, sizeof(sem_table));
    sysv_initialized = false;
}

/* ============================================================
 * Shared Memory — key_t → {eos_shmem_region, ref_count}
 * ============================================================ */

int eos_shmget(eos_key_t key, size_t size, int shmflg) {
    eos_key_t effective_key = (key == EOS_IPC_PRIVATE) ? next_private_key() : key;

    for (int i = 0; i < EOS_SYSV_SHM_MAX; i++) {
        if (shm_table[i].used && shm_table[i].key == effective_key) {
            if ((shmflg & EOS_IPC_CREAT) && (shmflg & EOS_IPC_EXCL)) {
                return -1;
            }
            return i;
        }
    }

    if (!(shmflg & EOS_IPC_CREAT) && key != EOS_IPC_PRIVATE) {
        return -1;
    }

    int slot = -1;
    for (int i = 0; i < EOS_SYSV_SHM_MAX; i++) {
        if (!shm_table[i].used) { slot = i; break; }
    }
    if (slot < 0) return -1;

    sysv_shm_entry_t *e = &shm_table[slot];
    e->used      = true;
    e->key       = effective_key;
    e->size      = size;
    e->ref_count = 0;
    e->perm      = (uint16_t)(shmflg & 0x1FF);

    char name[32];
    snprintf(name, sizeof(name), "sysv_shm_%d", slot);

    eos_shmem_config_t cfg = {
        .name   = name,
        .base   = NULL,
        .size   = size,
        .cached = false,
    };

    if (eos_shmem_create(&cfg, &e->region) == EOS_KERN_OK) {
        e->region_valid = true;
    } else {
        e->used = false;
        return -1;
    }

    return slot;
}

void *eos_shmat(int shmid, const void *shmaddr, int shmflg) {
    (void)shmaddr;
    (void)shmflg;
    if (shmid < 0 || shmid >= EOS_SYSV_SHM_MAX) return NULL;
    if (!shm_table[shmid].used || !shm_table[shmid].region_valid) return NULL;

    shm_table[shmid].ref_count++;
    return shm_table[shmid].region.addr;
}

int eos_shmdt(const void *shmaddr) {
    if (!shmaddr) return -1;

    for (int i = 0; i < EOS_SYSV_SHM_MAX; i++) {
        if (shm_table[i].used && shm_table[i].region_valid &&
            shm_table[i].region.addr == shmaddr) {
            if (shm_table[i].ref_count > 0) {
                shm_table[i].ref_count--;
            }
            return 0;
        }
    }
    return -1;
}

int eos_shmctl(int shmid, int cmd, eos_shmid_ds_t *buf) {
    if (shmid < 0 || shmid >= EOS_SYSV_SHM_MAX) return -1;
    if (!shm_table[shmid].used) return -1;

    sysv_shm_entry_t *e = &shm_table[shmid];

    switch (cmd) {
    case EOS_IPC_STAT:
        if (!buf) return -1;
        buf->shm_key    = e->key;
        buf->shm_segsz  = e->size;
        buf->shm_nattch = e->ref_count;
        buf->shm_perm   = e->perm;
        return 0;

    case EOS_IPC_RMID:
        if (e->region_valid) {
            eos_shmem_close(&e->region);
            e->region_valid = false;
        }
        memset(e, 0, sizeof(*e));
        return 0;

    case EOS_IPC_SET:
        if (!buf) return -1;
        e->perm = buf->shm_perm;
        return 0;

    default:
        return -1;
    }
}

/* ============================================================
 * Message Queues — key_t → eos_queue_handle
 * msgsnd prepends long mtype to data.
 * ============================================================ */

int eos_msgget(eos_key_t key, int msgflg) {
    eos_key_t effective_key = (key == EOS_IPC_PRIVATE) ? next_private_key() : key;

    for (int i = 0; i < EOS_SYSV_MSG_MAX; i++) {
        if (msg_table[i].used && msg_table[i].key == effective_key) {
            if ((msgflg & EOS_IPC_CREAT) && (msgflg & EOS_IPC_EXCL)) {
                return -1;
            }
            return i;
        }
    }

    if (!(msgflg & EOS_IPC_CREAT) && key != EOS_IPC_PRIVATE) {
        return -1;
    }

    int slot = -1;
    for (int i = 0; i < EOS_SYSV_MSG_MAX; i++) {
        if (!msg_table[i].used) { slot = i; break; }
    }
    if (slot < 0) return -1;

    sysv_msg_entry_t *e = &msg_table[slot];
    e->used = true;
    e->key  = effective_key;
    e->perm = (uint16_t)(msgflg & 0x1FF);

    size_t item_size = sizeof(long) + EOS_SYSV_MSG_SIZE_MAX;
    if (eos_queue_create(&e->queue, item_size,
                         EOS_SYSV_MSG_QUEUE_DEPTH) == EOS_KERN_OK) {
        e->queue_valid = true;
    } else {
        e->used = false;
        return -1;
    }

    return slot;
}

int eos_msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg) {
    if (msqid < 0 || msqid >= EOS_SYSV_MSG_MAX) return -1;
    if (!msg_table[msqid].used || !msg_table[msqid].queue_valid) return -1;
    if (!msgp) return -1;
    if (msgsz > EOS_SYSV_MSG_SIZE_MAX) return -1;

    uint8_t buf[sizeof(long) + EOS_SYSV_MSG_SIZE_MAX];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, msgp, sizeof(long) + msgsz);

    uint32_t timeout = (msgflg & EOS_IPC_NOWAIT) ? EOS_NO_WAIT : EOS_WAIT_FOREVER;

    if (eos_queue_send(msg_table[msqid].queue, buf, timeout) != EOS_KERN_OK) {
        return -1;
    }
    return 0;
}

int eos_msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg) {
    if (msqid < 0 || msqid >= EOS_SYSV_MSG_MAX) return -1;
    if (!msg_table[msqid].used || !msg_table[msqid].queue_valid) return -1;
    if (!msgp) return -1;

    uint8_t buf[sizeof(long) + EOS_SYSV_MSG_SIZE_MAX];
    uint32_t timeout = (msgflg & EOS_IPC_NOWAIT) ? EOS_NO_WAIT : EOS_WAIT_FOREVER;

    if (msgtyp == 0) {
        if (eos_queue_receive(msg_table[msqid].queue, buf,
                              timeout) != EOS_KERN_OK) {
            return -1;
        }
    } else {
        uint32_t start = eos_get_tick_ms();
        bool found = false;
        while (!found) {
            if (eos_queue_receive(msg_table[msqid].queue, buf,
                                  EOS_NO_WAIT) == EOS_KERN_OK) {
                long mtype;
                memcpy(&mtype, buf, sizeof(long));

                bool match = false;
                if (msgtyp > 0 && mtype == msgtyp) match = true;
                if (msgtyp < 0 && mtype <= -msgtyp) match = true;

                if (match) {
                    found = true;
                } else {
                    eos_queue_send(msg_table[msqid].queue, buf, EOS_NO_WAIT);
                    if (timeout == EOS_NO_WAIT) return -1;
                    eos_delay_ms(1);
                }
            } else {
                if (timeout == EOS_NO_WAIT) return -1;
                eos_delay_ms(1);
            }

            if (timeout != EOS_WAIT_FOREVER) {
                uint32_t elapsed = eos_get_tick_ms() - start;
                if (elapsed >= timeout) return -1;
            }
        }
    }

    size_t copy_sz = msgsz < EOS_SYSV_MSG_SIZE_MAX ? msgsz : EOS_SYSV_MSG_SIZE_MAX;
    memcpy(msgp, buf, sizeof(long) + copy_sz);
    return (int)copy_sz;
}

int eos_msgctl(int msqid, int cmd, eos_msqid_ds_t *buf) {
    if (msqid < 0 || msqid >= EOS_SYSV_MSG_MAX) return -1;
    if (!msg_table[msqid].used) return -1;

    sysv_msg_entry_t *e = &msg_table[msqid];

    switch (cmd) {
    case EOS_IPC_STAT:
        if (!buf) return -1;
        buf->msg_key    = e->key;
        buf->msg_qnum   = e->queue_valid ? eos_queue_count(e->queue) : 0;
        buf->msg_qbytes = (size_t)(EOS_SYSV_MSG_SIZE_MAX * EOS_SYSV_MSG_QUEUE_DEPTH);
        buf->msg_perm   = e->perm;
        return 0;

    case EOS_IPC_RMID:
        if (e->queue_valid) {
            eos_queue_delete(e->queue);
            e->queue_valid = false;
        }
        memset(e, 0, sizeof(*e));
        return 0;

    case EOS_IPC_SET:
        if (!buf) return -1;
        e->perm = buf->msg_perm;
        return 0;

    default:
        return -1;
    }
}

/* ============================================================
 * Semaphore Arrays — key_t → {eos_sem_handle array, nsems}
 * semop iterates operations. No SEM_UNDO support.
 * ============================================================ */

int eos_semget(eos_key_t key, int nsems, int semflg) {
    if (nsems < 0 || nsems > EOS_SYSV_SEM_NSEMS_MAX) return -1;

    eos_key_t effective_key = (key == EOS_IPC_PRIVATE) ? next_private_key() : key;

    for (int i = 0; i < EOS_SYSV_SEM_MAX; i++) {
        if (sem_table[i].used && sem_table[i].key == effective_key) {
            if ((semflg & EOS_IPC_CREAT) && (semflg & EOS_IPC_EXCL)) {
                return -1;
            }
            return i;
        }
    }

    if (!(semflg & EOS_IPC_CREAT) && key != EOS_IPC_PRIVATE) {
        return -1;
    }

    int slot = -1;
    for (int i = 0; i < EOS_SYSV_SEM_MAX; i++) {
        if (!sem_table[i].used) { slot = i; break; }
    }
    if (slot < 0) return -1;

    sysv_sem_entry_t *e = &sem_table[slot];
    e->used  = true;
    e->key   = effective_key;
    e->nsems = (uint16_t)nsems;
    e->perm  = (uint16_t)(semflg & 0x1FF);

    for (int i = 0; i < nsems; i++) {
        if (eos_sem_create(&e->sems[i], 0, UINT32_MAX) == EOS_KERN_OK) {
            e->sem_valid[i] = true;
        } else {
            for (int j = 0; j < i; j++) {
                if (e->sem_valid[j]) eos_sem_delete(e->sems[j]);
            }
            e->used = false;
            return -1;
        }
    }

    return slot;
}

int eos_semop(int semid, eos_sembuf_t *sops, size_t nsops) {
    if (semid < 0 || semid >= EOS_SYSV_SEM_MAX) return -1;
    if (!sem_table[semid].used) return -1;
    if (!sops || nsops == 0) return -1;

    sysv_sem_entry_t *e = &sem_table[semid];

    for (size_t i = 0; i < nsops; i++) {
        uint16_t num = sops[i].sem_num;
        int16_t  op  = sops[i].sem_op;

        if (num >= e->nsems || !e->sem_valid[num]) return -1;

        if (op > 0) {
            for (int16_t j = 0; j < op; j++) {
                if (eos_sem_post(e->sems[num]) != EOS_KERN_OK) {
                    return -1;
                }
            }
        } else if (op < 0) {
            int16_t abs_op = (int16_t)(-op);
            uint32_t timeout = (sops[i].sem_flg & EOS_IPC_NOWAIT) ?
                               EOS_NO_WAIT : EOS_WAIT_FOREVER;
            for (int16_t j = 0; j < abs_op; j++) {
                if (eos_sem_wait(e->sems[num], timeout) != EOS_KERN_OK) {
                    return -1;
                }
            }
        } else {
            /* op == 0: wait until semaphore value is 0 */
            uint32_t timeout = (sops[i].sem_flg & EOS_IPC_NOWAIT) ?
                               EOS_NO_WAIT : EOS_WAIT_FOREVER;
            uint32_t start = eos_get_tick_ms();
            while (eos_sem_get_count(e->sems[num]) != 0) {
                if (timeout == EOS_NO_WAIT) return -1;
                if (timeout != EOS_WAIT_FOREVER) {
                    uint32_t elapsed = eos_get_tick_ms() - start;
                    if (elapsed >= timeout) return -1;
                }
                eos_delay_ms(1);
            }
        }
    }

    return 0;
}

int eos_semctl(int semid, int semnum, int cmd, eos_semun_t arg) {
    if (semid < 0 || semid >= EOS_SYSV_SEM_MAX) return -1;
    if (!sem_table[semid].used) return -1;

    sysv_sem_entry_t *e = &sem_table[semid];

    switch (cmd) {
    case EOS_IPC_RMID:
        for (int i = 0; i < e->nsems; i++) {
            if (e->sem_valid[i]) {
                eos_sem_delete(e->sems[i]);
                e->sem_valid[i] = false;
            }
        }
        memset(e, 0, sizeof(*e));
        return 0;

    case EOS_IPC_STAT:
        if (!arg.buf) return -1;
        arg.buf->sem_key   = e->key;
        arg.buf->sem_nsems = e->nsems;
        arg.buf->sem_perm  = e->perm;
        return 0;

    case EOS_GETVAL:
        if (semnum < 0 || semnum >= e->nsems) return -1;
        if (!e->sem_valid[semnum]) return -1;
        return (int)eos_sem_get_count(e->sems[semnum]);

    case EOS_SETVAL: {
        if (semnum < 0 || semnum >= e->nsems) return -1;
        if (!e->sem_valid[semnum]) return -1;
        /* Delete and recreate with new initial value */
        eos_sem_delete(e->sems[semnum]);
        e->sem_valid[semnum] = false;
        if (eos_sem_create(&e->sems[semnum], (uint32_t)arg.val,
                           UINT32_MAX) == EOS_KERN_OK) {
            e->sem_valid[semnum] = true;
            return 0;
        }
        return -1;
    }

    case EOS_GETALL:
        if (!arg.array) return -1;
        for (int i = 0; i < e->nsems; i++) {
            if (e->sem_valid[i]) {
                arg.array[i] = (uint16_t)eos_sem_get_count(e->sems[i]);
            } else {
                arg.array[i] = 0;
            }
        }
        return 0;

    case EOS_SETALL:
        if (!arg.array) return -1;
        for (int i = 0; i < e->nsems; i++) {
            if (e->sem_valid[i]) {
                eos_sem_delete(e->sems[i]);
                e->sem_valid[i] = false;
            }
            if (eos_sem_create(&e->sems[i], (uint32_t)arg.array[i],
                               UINT32_MAX) == EOS_KERN_OK) {
                e->sem_valid[i] = true;
            } else {
                return -1;
            }
        }
        return 0;

    default:
        return -1;
    }
}

#endif /* EOS_PLATFORM_LINUX */
