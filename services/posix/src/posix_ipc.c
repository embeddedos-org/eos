// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file posix_ipc.c
 * @brief POSIX IPC implementation for EoS
 */

#include <eos/posix_ipc.h>
#include <eos/kernel.h>
#include <eos/hal.h>
#include <string.h>

#if defined(EOS_ENABLE_MULTICORE) && EOS_ENABLE_MULTICORE
#include <eos/multicore.h>
#endif

#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef ENOMEM
#define ENOMEM 12
#endif
#ifndef ENOENT
#define ENOENT  2
#endif
#ifndef EAGAIN
#define EAGAIN 11
#endif
#ifndef ENOSYS
#define ENOSYS 38
#endif

/* ============================================================
 * Named Message Queue Registry
 * ============================================================ */

typedef struct {
    bool                active;
    char                name[EOS_POSIX_MQ_NAME_MAX];
    eos_queue_handle_t  handle;
    size_t              msg_size;
    uint32_t            capacity;
    int                 flags;
} posix_mq_entry_t;

static posix_mq_entry_t mq_registry[EOS_POSIX_MQ_MAX];
static bool mq_registry_initialized = false;

static void mq_registry_init_once(void) {
    if (!mq_registry_initialized) {
        memset(mq_registry, 0, sizeof(mq_registry));
        mq_registry_initialized = true;
    }
}

static int mq_find_by_name(const char *name) {
    for (int i = 0; i < EOS_POSIX_MQ_MAX; i++) {
        if (mq_registry[i].active &&
            strncmp(mq_registry[i].name, name, EOS_POSIX_MQ_NAME_MAX) == 0) {
            return i;
        }
    }
    return -1;
}

static int mq_alloc_slot(void) {
    for (int i = 0; i < EOS_POSIX_MQ_MAX; i++) {
        if (!mq_registry[i].active) return i;
    }
    return -1;
}

/* ============================================================
 * mq_open — variadic, expects (name, oflag) or
 *           (name, oflag, mode, struct mq_attr*)
 * ============================================================ */

#include <stdarg.h>

mqd_t mq_open(const char *name, int oflag, ...) {
    if (!name) return (mqd_t)-1;

    mq_registry_init_once();

    int idx = mq_find_by_name(name);

    if (idx >= 0) {
        /* Already exists — return existing descriptor */
        mq_registry[idx].flags = oflag;
        return (mqd_t)idx;
    }

    /* Not found — must have O_CREAT */
    if (!(oflag & EOS_O_CREAT)) return (mqd_t)-1;

    va_list ap;
    va_start(ap, oflag);
    (void)va_arg(ap, uint32_t);          /* mode — unused */
    struct mq_attr *attr = va_arg(ap, struct mq_attr *);
    va_end(ap);

    if (!attr || attr->mq_maxmsg <= 0 || attr->mq_msgsize <= 0)
        return (mqd_t)-1;

    int slot = mq_alloc_slot();
    if (slot < 0) return (mqd_t)-1;

    eos_queue_handle_t qh;
    if (eos_queue_create(&qh, (size_t)attr->mq_msgsize,
                         (uint32_t)attr->mq_maxmsg) != EOS_KERN_OK) {
        return (mqd_t)-1;
    }

    posix_mq_entry_t *e = &mq_registry[slot];
    e->active   = true;
    e->handle   = qh;
    e->msg_size = (size_t)attr->mq_msgsize;
    e->capacity = (uint32_t)attr->mq_maxmsg;
    e->flags    = oflag;
    strncpy(e->name, name, EOS_POSIX_MQ_NAME_MAX - 1);
    e->name[EOS_POSIX_MQ_NAME_MAX - 1] = '\0';

    return (mqd_t)slot;
}

int mq_close(mqd_t mqdes) {
    /* Close does not destroy — just mark flags as closed */
    if (mqdes < 0 || mqdes >= EOS_POSIX_MQ_MAX) return -1;
    if (!mq_registry[mqdes].active) return -1;
    return 0;
}

int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len,
            unsigned int msg_prio) {
    (void)msg_prio;
    if (mqdes < 0 || mqdes >= EOS_POSIX_MQ_MAX) return -1;
    posix_mq_entry_t *e = &mq_registry[mqdes];
    if (!e->active) return -1;
    if (msg_len > e->msg_size) return -1;

    uint32_t timeout = (e->flags & EOS_O_NONBLOCK) ? EOS_NO_WAIT
                                                    : EOS_WAIT_FOREVER;
    int rc = eos_queue_send(e->handle, msg_ptr, timeout);
    if (rc == EOS_KERN_OK)   return 0;
    if (rc == EOS_KERN_FULL) return -1; /* EAGAIN */
    return -1;
}

ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len,
                   unsigned int *msg_prio) {
    if (msg_prio) *msg_prio = 0;
    if (mqdes < 0 || mqdes >= EOS_POSIX_MQ_MAX) return -1;
    posix_mq_entry_t *e = &mq_registry[mqdes];
    if (!e->active) return -1;
    if (msg_len < e->msg_size) return -1;

    uint32_t timeout = (e->flags & EOS_O_NONBLOCK) ? EOS_NO_WAIT
                                                    : EOS_WAIT_FOREVER;
    int rc = eos_queue_receive(e->handle, msg_ptr, timeout);
    if (rc == EOS_KERN_OK) return (ssize_t)e->msg_size;
    return -1;
}

int mq_unlink(const char *name) {
    if (!name) return -1;
    mq_registry_init_once();

    int idx = mq_find_by_name(name);
    if (idx < 0) return -1;

    eos_queue_delete(mq_registry[idx].handle);
    mq_registry[idx].active = false;
    return 0;
}

/* ============================================================
 * Pipe — implemented as a 1-byte-item message queue
 * ============================================================ */

static int pipe_next_id = 0;

int pipe(int pipefd[2]) {
    if (!pipefd) return -1;

    mq_registry_init_once();

    int slot = mq_alloc_slot();
    if (slot < 0) return -1;

    eos_queue_handle_t qh;
    /* Pipe: item_size=1, capacity=64 bytes buffer */
    if (eos_queue_create(&qh, 1, 64) != EOS_KERN_OK) return -1;

    posix_mq_entry_t *e = &mq_registry[slot];
    e->active   = true;
    e->handle   = qh;
    e->msg_size = 1;
    e->capacity = 64;
    e->flags    = 0;
    snprintf(e->name, EOS_POSIX_MQ_NAME_MAX, "_pipe_%d", pipe_next_id++);

    /* Both ends reference the same queue descriptor */
    pipefd[0] = slot;           /* read end */
    pipefd[1] = slot | 0x80;   /* write end — high bit marks direction */
    return 0;
}

/* ============================================================
 * Shared Memory (requires EOS_ENABLE_MULTICORE)
 * ============================================================ */

#if defined(EOS_ENABLE_MULTICORE) && EOS_ENABLE_MULTICORE

typedef struct {
    bool   active;
    char   name[EOS_POSIX_SHM_NAME_MAX];
    eos_shmem_region_t region;
} posix_shm_entry_t;

static posix_shm_entry_t shm_table[EOS_POSIX_SHM_MAX];
static bool shm_table_initialized = false;

static void shm_table_init_once(void) {
    if (!shm_table_initialized) {
        memset(shm_table, 0, sizeof(shm_table));
        shm_table_initialized = true;
    }
}

int shm_open(const char *name, int oflag, uint32_t mode) {
    (void)mode;
    if (!name) return -1;
    shm_table_init_once();

    /* Check if already open */
    for (int i = 0; i < EOS_POSIX_SHM_MAX; i++) {
        if (shm_table[i].active &&
            strncmp(shm_table[i].name, name, EOS_POSIX_SHM_NAME_MAX) == 0) {
            return i;
        }
    }

    if (!(oflag & EOS_O_CREAT)) return -1;

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < EOS_POSIX_SHM_MAX; i++) {
        if (!shm_table[i].active) { slot = i; break; }
    }
    if (slot < 0) return -1;

    eos_shmem_config_t cfg = {
        .name   = name,
        .base   = NULL,
        .size   = 0,
        .cached = false,
    };

    if (eos_shmem_create(&cfg, &shm_table[slot].region) != EOS_KERN_OK) {
        /* Try opening existing */
        if (eos_shmem_open(name, &shm_table[slot].region) != EOS_KERN_OK)
            return -1;
    }

    shm_table[slot].active = true;
    strncpy(shm_table[slot].name, name, EOS_POSIX_SHM_NAME_MAX - 1);
    shm_table[slot].name[EOS_POSIX_SHM_NAME_MAX - 1] = '\0';
    return slot;
}

int shm_unlink(const char *name) {
    if (!name) return -1;
    shm_table_init_once();

    for (int i = 0; i < EOS_POSIX_SHM_MAX; i++) {
        if (shm_table[i].active &&
            strncmp(shm_table[i].name, name, EOS_POSIX_SHM_NAME_MAX) == 0) {
            eos_shmem_close(&shm_table[i].region);
            shm_table[i].active = false;
            return 0;
        }
    }
    return -1;
}

void *shm_mmap(int shm_fd, size_t size) {
    (void)size;
    if (shm_fd < 0 || shm_fd >= EOS_POSIX_SHM_MAX) return NULL;
    if (!shm_table[shm_fd].active) return NULL;
    return shm_table[shm_fd].region.addr;
}

#else /* !EOS_ENABLE_MULTICORE */

int shm_open(const char *name, int oflag, uint32_t mode) {
    (void)name; (void)oflag; (void)mode;
    return -1; /* ENOSYS */
}

int shm_unlink(const char *name) {
    (void)name;
    return -1;
}

void *shm_mmap(int shm_fd, size_t size) {
    (void)shm_fd; (void)size;
    return NULL;
}

#endif /* EOS_ENABLE_MULTICORE */
