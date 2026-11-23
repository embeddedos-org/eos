// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file freestanding.c
 * @brief memset/memcpy for the ESP32 image
 *
 * The toolchain's newlib and libgcc are built for the windowed ABI and cannot
 * be linked into a CALL0 image, so the image links with -nostdlib. The kernel
 * and HAL need only these two functions (checked with nm on the objects).
 *
 * Build with -fno-tree-loop-distribute-patterns, or GCC turns these loops back
 * into calls to themselves.
 */

#include <stddef.h>
#include <stdint.h>

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

void *memcpy(void *restrict dst, const void *restrict src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}
