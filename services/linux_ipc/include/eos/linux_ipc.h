// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file linux_ipc.h
 * @brief EoS Linux IPC Emulation Layer
 *
 * Provides AF_UNIX socket, eventfd, epoll, and pipe emulation on top of
 * EoS kernel primitives (queues, semaphores). On native Linux targets
 * (EOS_PLATFORM_LINUX), these pass through to real syscalls.
 */

#ifndef EOS_LINUX_IPC_H
#define EOS_LINUX_IPC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Configuration
 * ============================================================ */

#ifndef EOS_LINUX_IPC_MAX_FDS
#define EOS_LINUX_IPC_MAX_FDS       32
#endif

#ifndef EOS_UNIX_SOCKET_NAME_MAX
#define EOS_UNIX_SOCKET_NAME_MAX    64
#endif

#ifndef EOS_UNIX_SOCKET_BACKLOG
#define EOS_UNIX_SOCKET_BACKLOG     4
#endif

#ifndef EOS_UNIX_SOCKET_MSG_SIZE
#define EOS_UNIX_SOCKET_MSG_SIZE    256
#endif

#ifndef EOS_UNIX_SOCKET_QUEUE_DEPTH
#define EOS_UNIX_SOCKET_QUEUE_DEPTH 16
#endif

#ifndef EOS_EPOLL_MAX_EVENTS
#define EOS_EPOLL_MAX_EVENTS        16
#endif

#ifndef EOS_PIPE_QUEUE_DEPTH
#define EOS_PIPE_QUEUE_DEPTH        64
#endif

/* ============================================================
 * Socket Address
 * ============================================================ */

typedef struct {
    uint16_t sun_family;
    char     sun_path[EOS_UNIX_SOCKET_NAME_MAX];
} eos_sockaddr_un_t;

#define EOS_AF_UNIX     1
#define EOS_SOCK_STREAM 1
#define EOS_SOCK_DGRAM  2

/* ============================================================
 * AF_UNIX Socket Emulation
 * ============================================================ */

int eos_unix_socket(int domain, int type, int protocol);
int eos_unix_bind(int fd, const eos_sockaddr_un_t *addr);
int eos_unix_listen(int fd, int backlog);
int eos_unix_accept(int fd, eos_sockaddr_un_t *addr);
int eos_unix_connect(int fd, const eos_sockaddr_un_t *addr);
int eos_unix_send(int fd, const void *buf, size_t len, int flags);
int eos_unix_recv(int fd, void *buf, size_t len, int flags);
int eos_unix_close(int fd);

/* ============================================================
 * eventfd Emulation
 * ============================================================ */

#define EOS_EFD_NONBLOCK    (1 << 0)
#define EOS_EFD_SEMAPHORE   (1 << 1)

int eos_eventfd_create(unsigned int initval, int flags);
int eos_eventfd_write(int fd, uint64_t value);
int eos_eventfd_read(int fd, uint64_t *value);
int eos_eventfd_close(int fd);

/* ============================================================
 * epoll Emulation
 * ============================================================ */

#define EOS_EPOLLIN     (1U << 0)
#define EOS_EPOLLOUT    (1U << 1)
#define EOS_EPOLLERR    (1U << 2)
#define EOS_EPOLLHUP    (1U << 3)
#define EOS_EPOLLET     (1U << 31)

#define EOS_EPOLL_CTL_ADD   1
#define EOS_EPOLL_CTL_MOD   2
#define EOS_EPOLL_CTL_DEL   3

typedef union {
    void     *ptr;
    int       fd;
    uint32_t  u32;
    uint64_t  u64;
} eos_epoll_data_t;

typedef struct {
    uint32_t         events;
    eos_epoll_data_t data;
} eos_epoll_event_t;

int eos_epoll_create(int size);
int eos_epoll_ctl(int epfd, int op, int fd, eos_epoll_event_t *event);
int eos_epoll_wait(int epfd, eos_epoll_event_t *events, int maxevents,
                   int timeout_ms);
int eos_epoll_close(int epfd);

/* ============================================================
 * Pipe Emulation
 * ============================================================ */

#define EOS_O_NONBLOCK  (1 << 0)

int eos_pipe(int pipefd[2]);
int eos_pipe2(int pipefd[2], int flags);

/* ============================================================
 * Module Init / Deinit
 * ============================================================ */

int  eos_linux_ipc_init(void);
void eos_linux_ipc_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_LINUX_IPC_H */
