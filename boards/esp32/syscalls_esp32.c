// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file syscalls_esp32.c
 * @brief The system calls the toolchain's newlib needs, for printf and malloc
 *
 * The ESP32 image links the toolchain's newlib (sim/qemu_esp32/sim.yaml),
 * which leaves these to the system:
 *
 *  - _write: stdout and stderr go to the UART0 console; other descriptors
 *    are refused (EBADF). stdin is not connected: _read fails (ENOSYS).
 *  - _sbrk: the heap is the DRAM the linker leaves after everything else
 *    (_eos_heap_start/_eos_heap_end, esp32.ld).
 *  - __malloc_lock/__malloc_unlock: the kernel's critical section, so tasks
 *    (and both CPUs, on SMP) can allocate. stdio's own FILE locks stay
 *    newlib's no-ops: holding interrupts off for a whole UART write would
 *    stall the tick, so two tasks printing at once may interleave.
 *  - __getreent: this newlib keeps errno and stdio state in a struct _reent
 *    it asks the system for; there is one, newlib's own, shared by every
 *    task. errno is therefore shared too.
 *  - _exit: stops the system through the port's fatal path; the rest
 *    (_close, _fstat, _isatty, _lseek, _getpid, _kill) answer for the
 *    console or fail cleanly.
 */

#include <errno.h>
#include <reent.h>
#include <stdint.h>
#include <sys/stat.h>
#include "eos/arch.h"
#include "board_esp32.h"

#undef errno
extern int errno;

extern char _eos_heap_start[], _eos_heap_end[];
extern struct _reent *_impure_ptr;

struct _reent *__getreent(void)
{
    return _impure_ptr;
}

int _write(int fd, const void *buf, size_t len)
{
    if (fd != 1 && fd != 2) {
        errno = EBADF;
        return -1;
    }
    eos_esp32_console_write((const char *)buf, len);
    return (int)len;
}

int _read(int fd, void *buf, size_t len)
{
    (void)fd; (void)buf; (void)len;
    errno = ENOSYS;
    return -1;
}

void *_sbrk(ptrdiff_t incr)
{
    static char *s_brk;
    uint32_t crit = eos_port_enter_critical();
    if (s_brk == NULL) s_brk = _eos_heap_start;
    char *prev = s_brk;
    if (incr > _eos_heap_end - s_brk || incr < _eos_heap_start - s_brk) {
        eos_port_exit_critical(crit);
        errno = ENOMEM;
        return (void *)-1;
    }
    s_brk += incr;
    eos_port_exit_critical(crit);
    return prev;
}

/* Recursive: newlib takes the malloc lock again inside realloc and free. */
static uint32_t s_malloc_ps;
static uint32_t s_malloc_depth;

void __malloc_lock(struct _reent *r)
{
    (void)r;
    uint32_t ps = eos_port_enter_critical();
    if (s_malloc_depth++ == 0U) {
        s_malloc_ps = ps;
    } else {
        /* Already held: drop this level's hold (on SMP, its count on the
         * kernel lock); the outermost one keeps the section. */
        eos_port_exit_critical(ps);
    }
}

void __malloc_unlock(struct _reent *r)
{
    (void)r;
    if (s_malloc_depth == 0U) return;
    if (--s_malloc_depth == 0U) eos_port_exit_critical(s_malloc_ps);
}

int _close(int fd)
{
    (void)fd;
    errno = EBADF;
    return -1;
}

int _fstat(int fd, struct stat *st)
{
    if (fd < 0 || fd > 2) {
        errno = EBADF;
        return -1;
    }
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int fd)
{
    return fd >= 0 && fd <= 2;
}

off_t _lseek(int fd, off_t off, int whence)
{
    (void)fd; (void)off; (void)whence;
    errno = ESPIPE;
    return -1;
}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid; (void)sig;
    errno = EINVAL;
    return -1;
}

void _exit(int status)
{
    eos_esp32_console_puts("EOS FATAL exit(");
    eos_esp32_console_put_u32((uint32_t)status);
    eos_esp32_console_puts(")\n");
    for (;;) {
        __asm volatile ("rsil a2, 15\n\twaiti 15" ::: "a2");
    }
}
