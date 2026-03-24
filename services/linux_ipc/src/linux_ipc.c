// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file linux_ipc.c
 * @brief EoS Linux IPC Emulation — AF_UNIX, eventfd, epoll, pipe
 *
 * On RTOS targets: emulated via EoS kernel queues and semaphores.
 * On Linux targets (EOS_PLATFORM_LINUX): passthrough to real syscalls.
 */

#include "eos/linux_ipc.h"
#include "eos/kernel.h"
#include "eos/hal.h"
#include <string.h>

#ifdef EOS_PLATFORM_LINUX
/* ============================================================
 * Native Linux passthrough
 * ============================================================ */
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int eos_linux_ipc_init(void)  { return 0; }
void eos_linux_ipc_deinit(void) {}

int eos_unix_socket(int domain, int type, int protocol) {
    (void)domain;
    return socket(AF_UNIX, type, protocol);
}

int eos_unix_bind(int fd, const eos_sockaddr_un_t *addr) {
    struct sockaddr_un sa;
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, addr->sun_path, sizeof(sa.sun_path) - 1);
    return bind(fd, (struct sockaddr *)&sa, sizeof(sa));
}

int eos_unix_listen(int fd, int backlog) {
    return listen(fd, backlog);
}

int eos_unix_accept(int fd, eos_sockaddr_un_t *addr) {
    struct sockaddr_un sa;
    socklen_t len = sizeof(sa);
    int cfd = accept(fd, (struct sockaddr *)&sa, &len);
    if (cfd >= 0 && addr) {
        addr->sun_family = EOS_AF_UNIX;
        strncpy(addr->sun_path, sa.sun_path, EOS_UNIX_SOCKET_NAME_MAX - 1);
        addr->sun_path[EOS_UNIX_SOCKET_NAME_MAX - 1] = '\0';
    }
    return cfd;
}

int eos_unix_connect(int fd, const eos_sockaddr_un_t *addr) {
    struct sockaddr_un sa;
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, addr->sun_path, sizeof(sa.sun_path) - 1);
    return connect(fd, (struct sockaddr *)&sa, sizeof(sa));
}

int eos_unix_send(int fd, const void *buf, size_t len, int flags) {
    return (int)send(fd, buf, len, flags);
}

int eos_unix_recv(int fd, void *buf, size_t len, int flags) {
    return (int)recv(fd, buf, len, flags);
}

int eos_unix_close(int fd) {
    return close(fd);
}

int eos_eventfd_create(unsigned int initval, int flags) {
    int eflags = 0;
    if (flags & EOS_EFD_NONBLOCK)  eflags |= EFD_NONBLOCK;
    if (flags & EOS_EFD_SEMAPHORE) eflags |= EFD_SEMAPHORE;
    return eventfd(initval, eflags);
}

int eos_eventfd_write(int fd, uint64_t value) {
    return (write(fd, &value, sizeof(value)) == sizeof(value)) ? 0 : -1;
}

int eos_eventfd_read(int fd, uint64_t *value) {
    return (read(fd, value, sizeof(*value)) == sizeof(*value)) ? 0 : -1;
}

int eos_eventfd_close(int fd) {
    return close(fd);
}

int eos_epoll_create(int size) {
    (void)size;
    return epoll_create1(0);
}

int eos_epoll_ctl(int epfd, int op, int fd, eos_epoll_event_t *event) {
    struct epoll_event ev;
    if (event) {
        ev.events  = event->events;
        ev.data.fd = event->data.fd;
    }
    int eop;
    switch (op) {
        case EOS_EPOLL_CTL_ADD: eop = EPOLL_CTL_ADD; break;
        case EOS_EPOLL_CTL_MOD: eop = EPOLL_CTL_MOD; break;
        case EOS_EPOLL_CTL_DEL: eop = EPOLL_CTL_DEL; break;
        default: return -1;
    }
    return epoll_ctl(epfd, eop, fd, event ? &ev : NULL);
}

int eos_epoll_wait(int epfd, eos_epoll_event_t *events, int maxevents,
                   int timeout_ms) {
    struct epoll_event ev[EOS_EPOLL_MAX_EVENTS];
    if (maxevents > EOS_EPOLL_MAX_EVENTS) maxevents = EOS_EPOLL_MAX_EVENTS;
    int n = epoll_wait(epfd, ev, maxevents, timeout_ms);
    for (int i = 0; i < n; i++) {
        events[i].events  = ev[i].events;
        events[i].data.fd = ev[i].data.fd;
    }
    return n;
}

int eos_epoll_close(int epfd) {
    return close(epfd);
}

int eos_pipe(int pipefd[2]) {
    return pipe(pipefd);
}

int eos_pipe2(int pipefd[2], int flags) {
    int pflags = 0;
    if (flags & EOS_O_NONBLOCK) pflags |= O_NONBLOCK;
    return pipe2(pipefd, pflags);
}

#else /* RTOS emulation via EoS kernel primitives */

/* ============================================================
 * Internal FD table
 * ============================================================ */

typedef enum {
    FD_TYPE_FREE     = 0,
    FD_TYPE_UNIX_SOCK,
    FD_TYPE_EVENTFD,
    FD_TYPE_PIPE_READ,
    FD_TYPE_PIPE_WRITE,
    FD_TYPE_EPOLL,
} fd_type_t;

typedef enum {
    SOCK_STATE_INIT      = 0,
    SOCK_STATE_BOUND,
    SOCK_STATE_LISTENING,
    SOCK_STATE_CONNECTED,
    SOCK_STATE_ACCEPTED,
} sock_state_t;

typedef struct {
    int           fd;
    char          name[EOS_UNIX_SOCKET_NAME_MAX];
    eos_queue_handle_t accept_queue;
    bool          accept_queue_valid;
} unix_bind_entry_t;

#ifndef EOS_UNIX_BIND_TABLE_MAX
#define EOS_UNIX_BIND_TABLE_MAX 16
#endif

typedef struct {
    uint16_t  tracked_fds[EOS_EPOLL_MAX_EVENTS];
    uint32_t  tracked_events[EOS_EPOLL_MAX_EVENTS];
    eos_epoll_data_t tracked_data[EOS_EPOLL_MAX_EVENTS];
    uint8_t   count;
} eos_epoll_set_t;

typedef struct {
    fd_type_t type;
    int       sock_type;
    sock_state_t state;

    eos_queue_handle_t queue;
    bool               queue_valid;

    eos_sem_handle_t sem;
    bool             sem_valid;
    uint64_t         efd_counter;
    int              efd_flags;

    int peer_fd;

    eos_epoll_set_t *epoll_set;

    int pipe_partner;
    int flags;

    char bound_name[EOS_UNIX_SOCKET_NAME_MAX];
} linux_ipc_fd_entry_t;

static linux_ipc_fd_entry_t fd_table[EOS_LINUX_IPC_MAX_FDS];
static unix_bind_entry_t    bind_table[EOS_UNIX_BIND_TABLE_MAX];
static eos_epoll_set_t      epoll_sets[EOS_LINUX_IPC_MAX_FDS];
static bool                 ipc_initialized = false;

/* ============================================================
 * Internal helpers
 * ============================================================ */

static int alloc_fd(void) {
    for (int i = 0; i < EOS_LINUX_IPC_MAX_FDS; i++) {
        if (fd_table[i].type == FD_TYPE_FREE) {
            return i;
        }
    }
    return -1;
}

static bool valid_fd(int fd) {
    return fd >= 0 && fd < EOS_LINUX_IPC_MAX_FDS &&
           fd_table[fd].type != FD_TYPE_FREE;
}

static int find_bind_entry(const char *name) {
    for (int i = 0; i < EOS_UNIX_BIND_TABLE_MAX; i++) {
        if (bind_table[i].fd >= 0 && strcmp(bind_table[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int alloc_bind_entry(void) {
    for (int i = 0; i < EOS_UNIX_BIND_TABLE_MAX; i++) {
        if (bind_table[i].fd < 0) {
            return i;
        }
    }
    return -1;
}

static void release_fd(int fd) {
    if (!valid_fd(fd)) return;

    linux_ipc_fd_entry_t *e = &fd_table[fd];
    if (e->queue_valid) {
        eos_queue_delete(e->queue);
        e->queue_valid = false;
    }
    if (e->sem_valid) {
        eos_sem_delete(e->sem);
        e->sem_valid = false;
    }
    if (e->epoll_set) {
        e->epoll_set->count = 0;
        e->epoll_set = NULL;
    }

    for (int i = 0; i < EOS_UNIX_BIND_TABLE_MAX; i++) {
        if (bind_table[i].fd == fd) {
            if (bind_table[i].accept_queue_valid) {
                eos_queue_delete(bind_table[i].accept_queue);
                bind_table[i].accept_queue_valid = false;
            }
            bind_table[i].fd = -1;
            bind_table[i].name[0] = '\0';
            break;
        }
    }

    memset(e, 0, sizeof(*e));
    e->type = FD_TYPE_FREE;
    e->peer_fd = -1;
    e->pipe_partner = -1;
}

/* ============================================================
 * Module Init / Deinit
 * ============================================================ */

int eos_linux_ipc_init(void) {
    if (ipc_initialized) return EOS_KERN_OK;

    memset(fd_table, 0, sizeof(fd_table));
    memset(bind_table, 0, sizeof(bind_table));
    memset(epoll_sets, 0, sizeof(epoll_sets));

    for (int i = 0; i < EOS_LINUX_IPC_MAX_FDS; i++) {
        fd_table[i].type = FD_TYPE_FREE;
        fd_table[i].peer_fd = -1;
        fd_table[i].pipe_partner = -1;
    }

    for (int i = 0; i < EOS_UNIX_BIND_TABLE_MAX; i++) {
        bind_table[i].fd = -1;
    }

    ipc_initialized = true;
    return EOS_KERN_OK;
}

void eos_linux_ipc_deinit(void) {
    for (int i = 0; i < EOS_LINUX_IPC_MAX_FDS; i++) {
        if (fd_table[i].type != FD_TYPE_FREE) {
            release_fd(i);
        }
    }
    ipc_initialized = false;
}

/* ============================================================
 * AF_UNIX Socket Emulation
 * ============================================================ */

int eos_unix_socket(int domain, int type, int protocol) {
    (void)protocol;
    if (domain != EOS_AF_UNIX) return -1;
    if (type != EOS_SOCK_STREAM && type != EOS_SOCK_DGRAM) return -1;

    int fd = alloc_fd();
    if (fd < 0) return -1;

    linux_ipc_fd_entry_t *e = &fd_table[fd];
    e->type      = FD_TYPE_UNIX_SOCK;
    e->sock_type = type;
    e->state     = SOCK_STATE_INIT;
    e->peer_fd   = -1;

    if (eos_queue_create(&e->queue, EOS_UNIX_SOCKET_MSG_SIZE,
                         EOS_UNIX_SOCKET_QUEUE_DEPTH) == EOS_KERN_OK) {
        e->queue_valid = true;
    } else {
        fd_table[fd].type = FD_TYPE_FREE;
        return -1;
    }

    return fd;
}

int eos_unix_bind(int fd, const eos_sockaddr_un_t *addr) {
    if (!valid_fd(fd) || !addr) return -1;
    if (fd_table[fd].type != FD_TYPE_UNIX_SOCK) return -1;

    if (find_bind_entry(addr->sun_path) >= 0) return -1;

    int bi = alloc_bind_entry();
    if (bi < 0) return -1;

    bind_table[bi].fd = fd;
    strncpy(bind_table[bi].name, addr->sun_path,
            EOS_UNIX_SOCKET_NAME_MAX - 1);
    bind_table[bi].name[EOS_UNIX_SOCKET_NAME_MAX - 1] = '\0';
    bind_table[bi].accept_queue_valid = false;

    strncpy(fd_table[fd].bound_name, addr->sun_path,
            EOS_UNIX_SOCKET_NAME_MAX - 1);
    fd_table[fd].bound_name[EOS_UNIX_SOCKET_NAME_MAX - 1] = '\0';
    fd_table[fd].state = SOCK_STATE_BOUND;

    return 0;
}

int eos_unix_listen(int fd, int backlog) {
    if (!valid_fd(fd)) return -1;
    if (fd_table[fd].type != FD_TYPE_UNIX_SOCK) return -1;
    if (fd_table[fd].state != SOCK_STATE_BOUND) return -1;

    if (backlog <= 0) backlog = EOS_UNIX_SOCKET_BACKLOG;

    for (int i = 0; i < EOS_UNIX_BIND_TABLE_MAX; i++) {
        if (bind_table[i].fd == fd) {
            if (!bind_table[i].accept_queue_valid) {
                if (eos_queue_create(&bind_table[i].accept_queue,
                                     sizeof(int),
                                     (uint32_t)backlog) == EOS_KERN_OK) {
                    bind_table[i].accept_queue_valid = true;
                } else {
                    return -1;
                }
            }
            break;
        }
    }

    fd_table[fd].state = SOCK_STATE_LISTENING;
    return 0;
}

int eos_unix_accept(int fd, eos_sockaddr_un_t *addr) {
    if (!valid_fd(fd)) return -1;
    if (fd_table[fd].type != FD_TYPE_UNIX_SOCK) return -1;
    if (fd_table[fd].state != SOCK_STATE_LISTENING) return -1;

    int bi = -1;
    for (int i = 0; i < EOS_UNIX_BIND_TABLE_MAX; i++) {
        if (bind_table[i].fd == fd) { bi = i; break; }
    }
    if (bi < 0 || !bind_table[bi].accept_queue_valid) return -1;

    int client_fd;
    if (eos_queue_receive(bind_table[bi].accept_queue, &client_fd,
                          EOS_WAIT_FOREVER) != EOS_KERN_OK) {
        return -1;
    }

    int accept_fd = alloc_fd();
    if (accept_fd < 0) return -1;

    linux_ipc_fd_entry_t *ae = &fd_table[accept_fd];
    ae->type      = FD_TYPE_UNIX_SOCK;
    ae->sock_type = fd_table[fd].sock_type;
    ae->state     = SOCK_STATE_ACCEPTED;
    ae->peer_fd   = client_fd;

    if (eos_queue_create(&ae->queue, EOS_UNIX_SOCKET_MSG_SIZE,
                         EOS_UNIX_SOCKET_QUEUE_DEPTH) == EOS_KERN_OK) {
        ae->queue_valid = true;
    } else {
        ae->type = FD_TYPE_FREE;
        return -1;
    }

    if (valid_fd(client_fd)) {
        fd_table[client_fd].peer_fd = accept_fd;
    }

    if (addr) {
        addr->sun_family = EOS_AF_UNIX;
        strncpy(addr->sun_path, fd_table[fd].bound_name,
                EOS_UNIX_SOCKET_NAME_MAX - 1);
        addr->sun_path[EOS_UNIX_SOCKET_NAME_MAX - 1] = '\0';
    }

    return accept_fd;
}

int eos_unix_connect(int fd, const eos_sockaddr_un_t *addr) {
    if (!valid_fd(fd) || !addr) return -1;
    if (fd_table[fd].type != FD_TYPE_UNIX_SOCK) return -1;

    int bi = find_bind_entry(addr->sun_path);
    if (bi < 0) return -1;

    int server_fd = bind_table[bi].fd;
    if (!valid_fd(server_fd)) return -1;
    if (fd_table[server_fd].state != SOCK_STATE_LISTENING) return -1;
    if (!bind_table[bi].accept_queue_valid) return -1;

    if (eos_queue_send(bind_table[bi].accept_queue, &fd,
                       EOS_NO_WAIT) != EOS_KERN_OK) {
        return -1;
    }

    fd_table[fd].state = SOCK_STATE_CONNECTED;
    return 0;
}

int eos_unix_send(int fd, const void *buf, size_t len, int flags) {
    (void)flags;
    if (!valid_fd(fd) || !buf) return -1;
    if (fd_table[fd].type != FD_TYPE_UNIX_SOCK) return -1;
    if (fd_table[fd].state != SOCK_STATE_CONNECTED &&
        fd_table[fd].state != SOCK_STATE_ACCEPTED) return -1;

    int peer = fd_table[fd].peer_fd;
    if (!valid_fd(peer) || !fd_table[peer].queue_valid) return -1;

    uint8_t msg[EOS_UNIX_SOCKET_MSG_SIZE];
    size_t send_len = len < EOS_UNIX_SOCKET_MSG_SIZE ? len : EOS_UNIX_SOCKET_MSG_SIZE;
    memset(msg, 0, sizeof(msg));
    memcpy(msg, buf, send_len);

    uint32_t timeout = (fd_table[fd].flags & EOS_O_NONBLOCK) ?
                       EOS_NO_WAIT : EOS_WAIT_FOREVER;

    if (eos_queue_send(fd_table[peer].queue, msg, timeout) != EOS_KERN_OK) {
        return -1;
    }
    return (int)send_len;
}

int eos_unix_recv(int fd, void *buf, size_t len, int flags) {
    (void)flags;
    if (!valid_fd(fd) || !buf) return -1;
    if (fd_table[fd].type != FD_TYPE_UNIX_SOCK) return -1;
    if (!fd_table[fd].queue_valid) return -1;

    uint8_t msg[EOS_UNIX_SOCKET_MSG_SIZE];
    uint32_t timeout = (fd_table[fd].flags & EOS_O_NONBLOCK) ?
                       EOS_NO_WAIT : EOS_WAIT_FOREVER;

    if (eos_queue_receive(fd_table[fd].queue, msg, timeout) != EOS_KERN_OK) {
        return -1;
    }

    size_t copy_len = len < EOS_UNIX_SOCKET_MSG_SIZE ? len : EOS_UNIX_SOCKET_MSG_SIZE;
    memcpy(buf, msg, copy_len);
    return (int)copy_len;
}

int eos_unix_close(int fd) {
    if (!valid_fd(fd)) return -1;

    if (fd_table[fd].type == FD_TYPE_UNIX_SOCK) {
        int peer = fd_table[fd].peer_fd;
        if (valid_fd(peer)) {
            fd_table[peer].peer_fd = -1;
        }
    }

    release_fd(fd);
    return 0;
}

/* ============================================================
 * eventfd Emulation — backed by EoS semaphore (counter mode)
 * ============================================================ */

int eos_eventfd_create(unsigned int initval, int flags) {
    int fd = alloc_fd();
    if (fd < 0) return -1;

    linux_ipc_fd_entry_t *e = &fd_table[fd];
    e->type        = FD_TYPE_EVENTFD;
    e->efd_counter = initval;
    e->efd_flags   = flags;

    if (eos_sem_create(&e->sem, initval, UINT32_MAX) == EOS_KERN_OK) {
        e->sem_valid = true;
    } else {
        e->type = FD_TYPE_FREE;
        return -1;
    }

    return fd;
}

int eos_eventfd_write(int fd, uint64_t value) {
    if (!valid_fd(fd)) return -1;
    if (fd_table[fd].type != FD_TYPE_EVENTFD) return -1;
    if (!fd_table[fd].sem_valid) return -1;

    for (uint64_t i = 0; i < value; i++) {
        if (eos_sem_post(fd_table[fd].sem) != EOS_KERN_OK) {
            return -1;
        }
    }
    fd_table[fd].efd_counter += value;
    return 0;
}

int eos_eventfd_read(int fd, uint64_t *value) {
    if (!valid_fd(fd) || !value) return -1;
    if (fd_table[fd].type != FD_TYPE_EVENTFD) return -1;
    if (!fd_table[fd].sem_valid) return -1;

    linux_ipc_fd_entry_t *e = &fd_table[fd];
    uint32_t timeout = (e->efd_flags & EOS_EFD_NONBLOCK) ?
                       EOS_NO_WAIT : EOS_WAIT_FOREVER;

    if (e->efd_flags & EOS_EFD_SEMAPHORE) {
        if (eos_sem_wait(e->sem, timeout) != EOS_KERN_OK) {
            return -1;
        }
        *value = 1;
        if (e->efd_counter > 0) e->efd_counter--;
    } else {
        if (eos_sem_wait(e->sem, timeout) != EOS_KERN_OK) {
            return -1;
        }
        uint32_t count = eos_sem_get_count(e->sem);
        while (count > 0) {
            if (eos_sem_wait(e->sem, EOS_NO_WAIT) != EOS_KERN_OK) break;
            count = eos_sem_get_count(e->sem);
        }
        *value = e->efd_counter;
        e->efd_counter = 0;
    }

    return 0;
}

int eos_eventfd_close(int fd) {
    if (!valid_fd(fd)) return -1;
    if (fd_table[fd].type != FD_TYPE_EVENTFD) return -1;
    release_fd(fd);
    return 0;
}

/* ============================================================
 * epoll Emulation — polling EoS queue/sem readiness
 * ============================================================ */

int eos_epoll_create(int size) {
    (void)size;
    int fd = alloc_fd();
    if (fd < 0) return -1;

    linux_ipc_fd_entry_t *e = &fd_table[fd];
    e->type = FD_TYPE_EPOLL;
    e->epoll_set = &epoll_sets[fd];
    memset(e->epoll_set, 0, sizeof(eos_epoll_set_t));

    return fd;
}

int eos_epoll_ctl(int epfd, int op, int fd, eos_epoll_event_t *event) {
    if (!valid_fd(epfd) || fd_table[epfd].type != FD_TYPE_EPOLL) return -1;
    if (!valid_fd(fd)) return -1;

    eos_epoll_set_t *set = fd_table[epfd].epoll_set;
    if (!set) return -1;

    switch (op) {
    case EOS_EPOLL_CTL_ADD:
        if (set->count >= EOS_EPOLL_MAX_EVENTS) return -1;
        if (!event) return -1;
        set->tracked_fds[set->count]    = (uint16_t)fd;
        set->tracked_events[set->count] = event->events;
        set->tracked_data[set->count]   = event->data;
        set->count++;
        return 0;

    case EOS_EPOLL_CTL_MOD:
        if (!event) return -1;
        for (uint8_t i = 0; i < set->count; i++) {
            if (set->tracked_fds[i] == (uint16_t)fd) {
                set->tracked_events[i] = event->events;
                set->tracked_data[i]   = event->data;
                return 0;
            }
        }
        return -1;

    case EOS_EPOLL_CTL_DEL:
        for (uint8_t i = 0; i < set->count; i++) {
            if (set->tracked_fds[i] == (uint16_t)fd) {
                for (uint8_t j = i; j < set->count - 1; j++) {
                    set->tracked_fds[j]    = set->tracked_fds[j + 1];
                    set->tracked_events[j] = set->tracked_events[j + 1];
                    set->tracked_data[j]   = set->tracked_data[j + 1];
                }
                set->count--;
                return 0;
            }
        }
        return -1;

    default:
        return -1;
    }
}

static uint32_t poll_fd_readiness(int fd) {
    if (!valid_fd(fd)) return EOS_EPOLLERR;

    linux_ipc_fd_entry_t *e = &fd_table[fd];
    uint32_t revents = 0;

    switch (e->type) {
    case FD_TYPE_UNIX_SOCK:
        if (e->queue_valid && eos_queue_count(e->queue) > 0)
            revents |= EOS_EPOLLIN;
        if (e->peer_fd >= 0 && valid_fd(e->peer_fd) &&
            fd_table[e->peer_fd].queue_valid &&
            !eos_queue_is_full(fd_table[e->peer_fd].queue))
            revents |= EOS_EPOLLOUT;
        if (e->state == SOCK_STATE_LISTENING) {
            for (int i = 0; i < EOS_UNIX_BIND_TABLE_MAX; i++) {
                if (bind_table[i].fd == fd && bind_table[i].accept_queue_valid &&
                    eos_queue_count(bind_table[i].accept_queue) > 0) {
                    revents |= EOS_EPOLLIN;
                    break;
                }
            }
        }
        break;

    case FD_TYPE_EVENTFD:
        if (e->sem_valid && eos_sem_get_count(e->sem) > 0)
            revents |= EOS_EPOLLIN;
        revents |= EOS_EPOLLOUT;
        break;

    case FD_TYPE_PIPE_READ:
        if (e->queue_valid && eos_queue_count(e->queue) > 0)
            revents |= EOS_EPOLLIN;
        break;

    case FD_TYPE_PIPE_WRITE:
        if (e->pipe_partner >= 0 && valid_fd(e->pipe_partner) &&
            fd_table[e->pipe_partner].queue_valid &&
            !eos_queue_is_full(fd_table[e->pipe_partner].queue))
            revents |= EOS_EPOLLOUT;
        break;

    default:
        revents |= EOS_EPOLLERR;
        break;
    }

    return revents;
}

int eos_epoll_wait(int epfd, eos_epoll_event_t *events, int maxevents,
                   int timeout_ms) {
    if (!valid_fd(epfd) || fd_table[epfd].type != FD_TYPE_EPOLL) return -1;
    if (!events || maxevents <= 0) return -1;

    eos_epoll_set_t *set = fd_table[epfd].epoll_set;
    if (!set) return -1;

    uint32_t start_tick = eos_get_tick_ms();
    int nready = 0;

    for (;;) {
        nready = 0;
        for (uint8_t i = 0; i < set->count && nready < maxevents; i++) {
            int tfd = set->tracked_fds[i];
            uint32_t wanted = set->tracked_events[i] & ~EOS_EPOLLET;
            uint32_t ready  = poll_fd_readiness(tfd) & wanted;

            if (ready != 0) {
                events[nready].events = ready;
                events[nready].data   = set->tracked_data[i];
                nready++;
            }
        }

        if (nready > 0) return nready;

        if (timeout_ms == 0) return 0;

        if (timeout_ms > 0) {
            uint32_t elapsed = eos_get_tick_ms() - start_tick;
            if (elapsed >= (uint32_t)timeout_ms) return 0;
        }

        eos_delay_ms(1);
    }
}

int eos_epoll_close(int epfd) {
    if (!valid_fd(epfd)) return -1;
    if (fd_table[epfd].type != FD_TYPE_EPOLL) return -1;
    release_fd(epfd);
    return 0;
}

/* ============================================================
 * Pipe Emulation — backed by EoS queue (item_size=1)
 * ============================================================ */

int eos_pipe2(int pipefd[2], int flags) {
    if (!pipefd) return -1;

    int rfd = alloc_fd();
    if (rfd < 0) return -1;

    int wfd = alloc_fd();
    if (wfd < 0) {
        fd_table[rfd].type = FD_TYPE_FREE;
        return -1;
    }

    linux_ipc_fd_entry_t *re = &fd_table[rfd];
    linux_ipc_fd_entry_t *we = &fd_table[wfd];

    re->type = FD_TYPE_PIPE_READ;
    we->type = FD_TYPE_PIPE_WRITE;

    if (eos_queue_create(&re->queue, 1, EOS_PIPE_QUEUE_DEPTH) != EOS_KERN_OK) {
        re->type = FD_TYPE_FREE;
        we->type = FD_TYPE_FREE;
        return -1;
    }
    re->queue_valid = true;

    re->pipe_partner = wfd;
    we->pipe_partner = rfd;
    re->flags = flags;
    we->flags = flags;

    pipefd[0] = rfd;
    pipefd[1] = wfd;
    return 0;
}

int eos_pipe(int pipefd[2]) {
    return eos_pipe2(pipefd, 0);
}

#endif /* EOS_PLATFORM_LINUX */
