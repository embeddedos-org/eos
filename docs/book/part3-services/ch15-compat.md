# Chapter 15: Compatibility Layers

**Author:** Srikanth Patchava & EmbeddedOS Contributors

---

## 15.1 Overview

EoS provides three compatibility layers that map well-known APIs onto
native EoS kernel primitives. This enables porting legacy applications
and leveraging existing codebases without rewriting them from scratch.

```
+--------------------------------------------------------------+
|                   Legacy / Ported Application                |
+-------------------+------------------+-----------------------+
|  POSIX Layer      |  VxWorks Layer   |  Linux IPC Layer      |
|  pthreads, I/O,   |  taskSpawn,      |  AF_UNIX, eventfd,   |
|  signals, IPC     |  semaphores, ISR |  epoll, pipe          |
+-------------------+------------------+-----------------------+
|                    EoS Kernel API                            |
|         Tasks - Mutexes - Semaphores - Queues - IRQs         |
+--------------------------------------------------------------+
```

| Layer       | Headers                                           | Coverage                    |
|-------------|---------------------------------------------------|-----------------------------|
| POSIX       | posix_threads.h, posix_io.h, posix_ipc.h, posix_signals.h, posix_sync.h, posix_time.h | Threads, file I/O, mqueues, pipes, shm, signals |
| VxWorks     | vxworks_task.h, vxworks_sem.h, vxworks_int.h, vxworks_wd.h, vxworks_msgq.h | Tasks, semaphores, ISR, watchdog, message queues |
| Linux IPC   | linux_ipc.h, linux_sysv_ipc.h                    | AF_UNIX sockets, eventfd, epoll, pipe, SysV IPC |

---

# Part I: POSIX Compatibility

## 15.2 POSIX Threads (pthreads)

The pthreads layer (posix_threads.h) maps the standard pthread API
onto EoS kernel task primitives.

### 15.2.1 Thread Attributes

```c
typedef uint8_t pthread_t;

typedef struct {
    uint8_t  priority;      /* EoS task priority (0-31)  */
    uint32_t stack_size;    /* Stack size in bytes       */
    int      detach_state;  /* JOINABLE or DETACHED      */
} pthread_attr_t;

#define PTHREAD_CREATE_JOINABLE  0
#define PTHREAD_CREATE_DETACHED  1
#define PTHREAD_STACK_MIN        256
```

### 15.2.2 Thread Creation and Join

```c
#include <eos/posix_threads.h>

void *worker(void *arg)
{
    int id = *(int *)arg;
    printf("Worker %d running\n", id);
    return NULL;
}

void posix_threads_example(void)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    struct sched_param param = { .sched_priority = 10 };
    pthread_attr_setschedparam(&attr, &param);
    pthread_attr_setstacksize(&attr, 2048);

    pthread_t t1, t2;
    int id1 = 1, id2 = 2;

    pthread_create(&t1, &attr, worker, &id1);
    pthread_create(&t2, &attr, worker, &id2);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    pthread_attr_destroy(&attr);
}
```

### 15.2.3 Mapping Table: pthreads to EoS Kernel

| POSIX Function                  | EoS Kernel Equivalent             |
|---------------------------------|-----------------------------------|
| pthread_create                  | eos_task_create                   |
| pthread_join                    | eos_task_join                     |
| pthread_exit                    | eos_task_delete(self)             |
| pthread_self                    | eos_task_current                  |
| pthread_yield                   | eos_task_yield                    |
| pthread_attr_setschedparam      | Priority mapping                  |
| pthread_attr_setstacksize       | Stack size parameter              |

## 15.3 POSIX File I/O

The I/O layer (posix_io.h) provides open/close/read/write/lseek mapped
to eos_fs_* with file descriptors 0/1/2 reserved for console I/O.

### 15.3.1 File Descriptor Model

```
 fd 0 --- stdin  (console read)
 fd 1 --- stdout (console write)
 fd 2 --- stderr (console write)
 fd 3..15 -- filesystem files (EOS_POSIX_FD_MAX = 16)
```

### 15.3.2 I/O Operations

```c
#include <eos/posix_io.h>

void posix_io_example(void)
{
    eos_posix_io_init();

    /* Open and write */
    int fd = open("/data/log.txt",
                  EOS_O_WRONLY_IO | EOS_O_CREAT_IO | EOS_O_TRUNC_IO);
    write(fd, "Hello POSIX\n", 12);
    close(fd);

    /* Read back */
    fd = open("/data/log.txt", EOS_O_RDONLY_IO);
    char buf[64];
    ssize_t n = read(fd, buf, sizeof(buf));
    close(fd);

    /* File info */
    struct eos_stat st;
    stat("/data/log.txt", &st);
    printf("Size: %u bytes\n", st.st_size);
}
```

### 15.3.3 Open Flags

| Flag               | Value    | Description            |
|--------------------|----------|------------------------|
| EOS_O_RDONLY_IO    | 0x0000   | Read only              |
| EOS_O_WRONLY_IO    | 0x0001   | Write only             |
| EOS_O_RDWR_IO      | 0x0002   | Read and write         |
| EOS_O_CREAT_IO     | 0x0100   | Create if not exists   |
| EOS_O_TRUNC_IO     | 0x0200   | Truncate to zero       |
| EOS_O_APPEND_IO    | 0x0400   | Append mode            |

## 15.4 POSIX IPC

### 15.4.1 Message Queues

```c
#include <eos/posix_ipc.h>

void mq_example(void)
{
    struct mq_attr attr = {
        .mq_maxmsg  = 10,
        .mq_msgsize = 64,
    };

    mqd_t mq = mq_open("/sensor_queue",
                        EOS_O_CREAT | EOS_O_RDWR, 0, &attr);

    /* Send */
    mq_send(mq, "temperature=23.5", 17, 0);

    /* Receive */
    char buf[64];
    unsigned int prio;
    ssize_t n = mq_receive(mq, buf, sizeof(buf), &prio);

    mq_close(mq);
    mq_unlink("/sensor_queue");
}
```

### 15.4.2 Pipes

```c
void pipe_example(void)
{
    int pipefd[2];
    pipe(pipefd);   /* pipefd[0] = read, pipefd[1] = write */

    write(pipefd[1], "data", 4);

    char buf[8];
    read(pipefd[0], buf, sizeof(buf));
}
```

### 15.4.3 Shared Memory

```c
void shm_example(void)
{
    int fd = shm_open("/shared_data", EOS_O_CREAT | EOS_O_RDWR, 0);
    void *ptr = shm_mmap(fd, 4096);

    /* Use shared memory region */
    memcpy(ptr, "shared data", 12);

    shm_unlink("/shared_data");
}
```

---

# Part II: VxWorks Compatibility

## 15.5 VxWorks Task API

The VxWorks layer (vxworks_task.h) maps the classic VxWorks task
management API onto EoS kernel tasks.

### 15.5.1 Task Spawning

```c
#include <eos/vxworks_task.h>

int my_task(int arg1, int arg2, int arg3, int arg4, int arg5,
            int arg6, int arg7, int arg8, int arg9, int arg10)
{
    printf("VxWorks task running: arg1=%d\n", arg1);
    return 0;
}

void vxworks_task_example(void)
{
    TASK_ID tid = taskSpawn(
        "tMyTask",     /* name        */
        100,           /* priority    */
        VX_FP_TASK,    /* options     */
        4096,          /* stack size  */
        my_task,       /* entry point */
        42, 0, 0, 0, 0, 0, 0, 0, 0, 0  /* 10 args */
    );

    if (tid == TASK_ID_ERROR) {
        printf("Task spawn failed\n");
    }
}
```

### 15.5.2 Task Operations

```c
/* Delay by ticks (10 ms per tick default) */
taskDelay(100);   /* 1 second delay */

/* Get own task ID */
TASK_ID me = taskIdSelf();

/* Suspend / Resume */
taskSuspend(tid);
taskResume(tid);

/* Look up by name */
TASK_ID found = taskNameToId("tMyTask");

/* Lock / unlock preemption */
taskLock();
/* critical section */
taskUnlock();

/* Delete a task */
taskDelete(tid);
```

### 15.5.3 Mapping Table: VxWorks to EoS Kernel

| VxWorks Function   | EoS Kernel Equivalent          |
|--------------------|--------------------------------|
| taskSpawn          | eos_task_create                |
| taskDelete         | eos_task_delete                |
| taskSuspend        | eos_task_suspend               |
| taskResume         | eos_task_resume                |
| taskDelay          | eos_task_delay_ms              |
| taskIdSelf         | eos_task_current               |
| taskLock           | eos_irq_disable                |
| taskUnlock         | eos_irq_enable                 |

## 15.6 VxWorks Semaphores

```c
#include <eos/vxworks_sem.h>

void vxworks_sem_example(void)
{
    /* Binary semaphore (initially full) */
    SEM_ID bin = semBCreate(SEM_Q_PRIORITY, SEM_FULL);

    /* Mutex semaphore */
    SEM_ID mtx = semMCreate(SEM_Q_PRIORITY | SEM_INVERSION_SAFE);

    /* Counting semaphore (initial count = 5) */
    SEM_ID cnt = semCCreate(SEM_Q_FIFO, 5);

    /* Take with timeout (100 ticks = 1 second) */
    STATUS rc = semTake(mtx, 100);
    if (rc == OK) {
        /* critical section */
        semGive(mtx);
    }

    /* Take with no wait */
    rc = semTake(bin, NO_WAIT);

    /* Take blocking forever */
    semTake(cnt, WAIT_FOREVER);
    semGive(cnt);

    /* Cleanup */
    semDelete(bin);
    semDelete(mtx);
    semDelete(cnt);
}
```

### 15.6.1 Semaphore Types

| Factory        | Type     | EoS Mapping       | Use Case             |
|----------------|----------|--------------------|----------------------|
| semBCreate     | Binary   | eos_sem (max=1)    | Event signaling      |
| semMCreate     | Mutex    | eos_mutex          | Mutual exclusion     |
| semCCreate     | Counting | eos_sem (max=N)    | Resource pools       |

---

# Part III: Linux IPC Compatibility

## 15.7 AF_UNIX Socket Emulation

The Linux IPC layer (linux_ipc.h) emulates AF_UNIX sockets, eventfd,
epoll, and pipes using EoS kernel queues and semaphores.

### 15.7.1 Socket Address

```c
typedef struct {
    uint16_t sun_family;                          /* EOS_AF_UNIX     */
    char     sun_path[EOS_UNIX_SOCKET_NAME_MAX];  /* Socket name     */
} eos_sockaddr_un_t;
```

### 15.7.2 Stream Socket Example

```c
#include <eos/linux_ipc.h>

void unix_server(void)
{
    eos_linux_ipc_init();

    int fd = eos_unix_socket(EOS_AF_UNIX, EOS_SOCK_STREAM, 0);

    eos_sockaddr_un_t addr = { .sun_family = EOS_AF_UNIX };
    strncpy(addr.sun_path, "/tmp/eos.sock", EOS_UNIX_SOCKET_NAME_MAX);

    eos_unix_bind(fd, &addr);
    eos_unix_listen(fd, EOS_UNIX_SOCKET_BACKLOG);

    eos_sockaddr_un_t client_addr;
    int client = eos_unix_accept(fd, &client_addr);

    char buf[256];
    int n = eos_unix_recv(client, buf, sizeof(buf), 0);
    eos_unix_send(client, "ACK", 3, 0);

    eos_unix_close(client);
    eos_unix_close(fd);
}
```

## 15.8 eventfd Emulation

```c
void eventfd_example(void)
{
    int efd = eos_eventfd_create(0, 0);

    /* Signal from ISR or another task */
    eos_eventfd_write(efd, 1);

    /* Wait for signal */
    uint64_t val;
    eos_eventfd_read(efd, &val);
    printf("Event received: %llu\n", val);

    eos_eventfd_close(efd);
}
```

| Flag                | Description                        |
|---------------------|------------------------------------|
| EOS_EFD_NONBLOCK    | Non-blocking read/write            |
| EOS_EFD_SEMAPHORE   | Semaphore mode (decrement by 1)    |

## 15.9 epoll Emulation

```c
void epoll_example(void)
{
    int epfd = eos_epoll_create(10);

    int sock_fd = eos_unix_socket(EOS_AF_UNIX, EOS_SOCK_STREAM, 0);

    eos_epoll_event_t ev = {
        .events = EOS_EPOLLIN,
        .data.fd = sock_fd,
    };
    eos_epoll_ctl(epfd, EOS_EPOLL_CTL_ADD, sock_fd, &ev);

    /* Wait for events */
    eos_epoll_event_t events[4];
    int n = eos_epoll_wait(epfd, events, 4, 5000);

    for (int i = 0; i < n; i++) {
        if (events[i].events & EOS_EPOLLIN) {
            printf("Data ready on fd %d\n", events[i].data.fd);
        }
    }

    eos_epoll_close(epfd);
}
```

### 15.9.1 epoll Events

| Event           | Description              |
|-----------------|--------------------------|
| EOS_EPOLLIN     | Ready for reading        |
| EOS_EPOLLOUT    | Ready for writing        |
| EOS_EPOLLERR    | Error condition          |
| EOS_EPOLLHUP    | Hang up                  |
| EOS_EPOLLET     | Edge-triggered mode      |

## 15.10 Pipe Emulation

```c
void linux_pipe_example(void)
{
    int pipefd[2];
    eos_pipe(pipefd);

    /* Non-blocking pipe */
    int nb_pipefd[2];
    eos_pipe2(nb_pipefd, EOS_O_NONBLOCK);

    /* Write end to Read end */
    eos_unix_send(pipefd[1], "hello", 5, 0);

    char buf[16];
    eos_unix_recv(pipefd[0], buf, sizeof(buf), 0);
}
```

## 15.11 Configuration Constants

| Constant                       | Default | Description                    |
|--------------------------------|---------|--------------------------------|
| EOS_LINUX_IPC_MAX_FDS          | 32      | Max file descriptors           |
| EOS_UNIX_SOCKET_NAME_MAX       | 64      | Max socket path length         |
| EOS_UNIX_SOCKET_BACKLOG        | 4       | Default listen backlog         |
| EOS_UNIX_SOCKET_MSG_SIZE       | 256     | Max message size               |
| EOS_UNIX_SOCKET_QUEUE_DEPTH    | 16      | Internal queue depth           |
| EOS_EPOLL_MAX_EVENTS           | 16      | Max epoll events               |
| EOS_PIPE_QUEUE_DEPTH           | 64      | Pipe internal queue depth      |

## 15.12 Porting Strategy

```
  +------------------------------+
  |   Existing Application       |
  |   (POSIX / VxWorks / Linux)  |
  +--------------+---------------+
                 |
  +--------------v---------------+
  |   Step 1: Include EoS compat |
  |   headers instead of OS ones |
  +--------------+---------------+
                 |
  +--------------v---------------+
  |   Step 2: Link compat layer  |
  |   (posix / vxworks / linux)  |
  +--------------+---------------+
                 |
  +--------------v---------------+
  |   Step 3: Compile and test   |
  |   -- most code works as-is  |
  +------------------------------+
```

## 15.13 API Reference Summary

### POSIX APIs

| Function          | Header              | Description               |
|-------------------|---------------------|---------------------------|
| pthread_create    | posix_threads.h     | Create thread             |
| pthread_join      | posix_threads.h     | Wait for thread           |
| pthread_exit      | posix_threads.h     | Exit current thread       |
| pthread_self      | posix_threads.h     | Get thread ID             |
| pthread_yield     | posix_threads.h     | Yield CPU                 |
| open              | posix_io.h          | Open file                 |
| close             | posix_io.h          | Close file descriptor     |
| read              | posix_io.h          | Read from fd              |
| write             | posix_io.h          | Write to fd               |
| lseek             | posix_io.h          | Seek in file              |
| stat / fstat      | posix_io.h          | File information          |
| mq_open           | posix_ipc.h         | Open message queue        |
| mq_send           | posix_ipc.h         | Send message              |
| mq_receive        | posix_ipc.h         | Receive message           |
| pipe              | posix_ipc.h         | Create pipe               |
| shm_open          | posix_ipc.h         | Open shared memory        |

### VxWorks APIs

| Function          | Header              | Description               |
|-------------------|---------------------|---------------------------|
| taskSpawn         | vxworks_task.h      | Create task               |
| taskDelete        | vxworks_task.h      | Delete task               |
| taskSuspend       | vxworks_task.h      | Suspend task              |
| taskResume        | vxworks_task.h      | Resume task               |
| taskDelay         | vxworks_task.h      | Delay in ticks            |
| taskIdSelf        | vxworks_task.h      | Get current task ID       |
| semBCreate        | vxworks_sem.h       | Create binary semaphore   |
| semMCreate        | vxworks_sem.h       | Create mutex semaphore    |
| semCCreate        | vxworks_sem.h       | Create counting semaphore |
| semTake           | vxworks_sem.h       | Acquire semaphore         |
| semGive           | vxworks_sem.h       | Release semaphore         |
| semDelete         | vxworks_sem.h       | Delete semaphore          |

### Linux IPC APIs

| Function              | Header          | Description                |
|-----------------------|-----------------|----------------------------|
| eos_unix_socket       | linux_ipc.h     | Create AF_UNIX socket      |
| eos_unix_bind         | linux_ipc.h     | Bind socket to path        |
| eos_unix_listen       | linux_ipc.h     | Listen for connections     |
| eos_unix_accept       | linux_ipc.h     | Accept connection          |
| eos_unix_connect      | linux_ipc.h     | Connect to socket          |
| eos_unix_send         | linux_ipc.h     | Send data                  |
| eos_unix_recv         | linux_ipc.h     | Receive data               |
| eos_unix_close        | linux_ipc.h     | Close socket               |
| eos_eventfd_create    | linux_ipc.h     | Create eventfd             |
| eos_eventfd_write     | linux_ipc.h     | Signal eventfd             |
| eos_eventfd_read      | linux_ipc.h     | Wait on eventfd            |
| eos_epoll_create      | linux_ipc.h     | Create epoll instance      |
| eos_epoll_ctl         | linux_ipc.h     | Add/modify/delete fd       |
| eos_epoll_wait        | linux_ipc.h     | Wait for events            |
| eos_pipe              | linux_ipc.h     | Create pipe                |
| eos_pipe2             | linux_ipc.h     | Create pipe with flags     |
| eos_linux_ipc_init    | linux_ipc.h     | Init IPC subsystem         |
| eos_linux_ipc_deinit  | linux_ipc.h     | Shutdown IPC subsystem     |

---

*Next: [Chapter 16 -- UI Framework](ch16-ui.md)*
