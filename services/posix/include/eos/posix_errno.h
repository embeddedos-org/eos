// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file posix_errno.h
 * @brief errno constants for the EoS POSIX compatibility layer
 *
 * One place for the errno values this layer returns, because five places was
 * one too many (#270). Every source under services/posix/src/ carried its own
 * `#ifndef EINVAL / #define EINVAL 22` block, posix_io.h defined a sixth set
 * unguarded, and posix_threads.c put its block halfway down the file — below
 * the pthread_attr_* functions that already used EINVAL. That file therefore
 * could not compile at all, and nothing noticed, because EOS_ENABLE_POSIX
 * defaults OFF and no CI job turned it on.
 *
 * The layer is built both hosted and freestanding. <errno.h> is a hosted
 * header: glibc and newlib both ship it, a strictly freestanding toolchain
 * need not. So it is included when the compiler says it exists, and the
 * constants are defined only for whatever that did not provide. Guarded
 * `#define`s, never unconditional ones: ENOSYS is 38 on Linux and 129 on
 * MSVC, and redefining a libc macro to a different value is a hard error.
 */

#ifndef EOS_POSIX_ERRNO_H
#define EOS_POSIX_ERRNO_H

#if defined(__has_include)
#  if __has_include(<errno.h>)
#    include <errno.h>
#  endif
#else
/* Pre-C23 compilers without __has_include: both toolchains this layer is
 * built with (gcc/glibc hosted, arm-none-eabi/newlib cross) ship <errno.h>. */
#  include <errno.h>
#endif

/* Linux/POSIX values; used only where the libc above defined nothing. */
#ifndef EPERM
#define EPERM     1
#endif
#ifndef ENOENT
#define ENOENT    2
#endif
#ifndef ESRCH
#define ESRCH     3
#endif
#ifndef EINTR
#define EINTR     4
#endif
#ifndef EBADF
#define EBADF     9
#endif
#ifndef EAGAIN
#define EAGAIN   11
#endif
#ifndef ENOMEM
#define ENOMEM   12
#endif
#ifndef EBUSY
#define EBUSY    16
#endif
#ifndef EINVAL
#define EINVAL   22
#endif
#ifndef EMFILE
#define EMFILE   24
#endif
#ifndef ERANGE
#define ERANGE   34
#endif
#ifndef EDEADLK
#define EDEADLK  35
#endif
#ifndef ENOSYS
#define ENOSYS   38
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT 110
#endif

#endif /* EOS_POSIX_ERRNO_H */
