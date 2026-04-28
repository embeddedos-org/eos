// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file heap.c
 * @brief Best-fit heap allocator with coalescing for EoS kernel
 *
 * Simple embedded heap allocator using a free-list with best-fit strategy
 * and immediate coalescing of adjacent free blocks.
 */

#include "eos/mem.h"
#include <string.h>

/* Block header — prepended to every allocation.
 * Padded to 8-byte boundary so returned pointers are always 8-byte aligned. */
typedef struct block_header {
    size_t size;                    /* Block size (including header) */
    struct block_header *next;      /* Next free block (only valid when free) */
    uint8_t free;                   /* 1 = free, 0 = allocated */
    uint8_t _pad[7]; /* Pad header to 16 bytes for 8-byte aligned returns */
} block_header_t;

#define HEADER_SIZE     (sizeof(block_header_t))
#define MIN_BLOCK_SIZE  (HEADER_SIZE + 8)  /* Minimum usable allocation */
#define ALIGN_UP(x, a)  (((x) + (a) - 1) & ~((a) - 1))

static block_header_t *free_list = NULL;
static void *heap_start = NULL;
static size_t heap_total = 0;

/* Statistics */
static eos_heap_stats_t stats;

int eos_heap_init(void *heap_base, size_t heap_size)
{
    if (!heap_base || heap_size < MIN_BLOCK_SIZE) return -1;

    /* Align base */
    uintptr_t aligned = ALIGN_UP((uintptr_t)heap_base, 8);
    size_t adjustment = aligned - (uintptr_t)heap_base;
    if (adjustment >= heap_size) return -1;
    heap_size -= adjustment;

    heap_start = (void *)aligned;
    heap_total = heap_size;

    /* Initialize single free block spanning entire heap */
    free_list = (block_header_t *)heap_start;
    free_list->size = heap_size;
    free_list->next = NULL;
    free_list->free = 1;

    memset(&stats, 0, sizeof(stats));
    stats.total_size = heap_size;
    stats.free = heap_size - HEADER_SIZE;
    stats.largest_free_block = heap_size - HEADER_SIZE;

    return 0;
}

void *eos_malloc(size_t size)
{
    if (size == 0 || !free_list) return NULL;

    /* Align allocation size to 8 bytes */
    size = ALIGN_UP(size, 8);
    size_t total_needed = size + HEADER_SIZE;

    /* Best-fit search */
    block_header_t *best = NULL;
    block_header_t *current = free_list;

    while (current) {
        if (current->free && current->size >= total_needed) {
            if (!best || current->size < best->size) {
                best = current;
                if (best->size == total_needed) break;  /* Perfect fit */
            }
        }
        current = current->next;
    }

    if (!best) return NULL;  /* Out of memory */

    /* Split block if remainder is large enough */
    if (best->size >= total_needed + MIN_BLOCK_SIZE) {
        block_header_t *split = (block_header_t *)((uint8_t *)best + total_needed);
        split->size = best->size - total_needed;
        split->next = best->next;
        split->free = 1;
        best->size = total_needed;
        best->next = split;
    }

    best->free = 0;

    stats.used += best->size;
    stats.free = stats.total_size - stats.used;
    stats.alloc_count++;

    return (void *)((uint8_t *)best + HEADER_SIZE);
}

/**
 * @brief Coalesce adjacent free blocks starting from the given block.
 */
static void coalesce(block_header_t *block)
{
    while (block->next && block->next->free) {
        block->size += block->next->size;
        block->next = block->next->next;
    }
}

void eos_free(void *ptr)
{
    if (!ptr) return;

    block_header_t *block = (block_header_t *)((uint8_t *)ptr - HEADER_SIZE);

    /* Sanity check: pointer should be within heap */
    if ((uintptr_t)block < (uintptr_t)heap_start ||
        (uintptr_t)block >= (uintptr_t)heap_start + heap_total) {
        return;
    }

    if (block->free) return;  /* Double-free protection */

    block->free = 1;

    stats.used -= block->size;
    stats.free = stats.total_size - stats.used;
    stats.free_count++;

    /* Coalesce forward */
    coalesce(block);

    /* Coalesce backward — find predecessor */
    block_header_t *prev = free_list;
    if (prev != block) {
        while (prev && prev->next != block) {
            prev = prev->next;
        }
        if (prev && prev->free) {
            coalesce(prev);
        }
    }
}

void *eos_realloc(void *ptr, size_t new_size)
{
    if (!ptr) return eos_malloc(new_size);
    if (new_size == 0) { eos_free(ptr); return NULL; }

    block_header_t *block = (block_header_t *)((uint8_t *)ptr - HEADER_SIZE);
    size_t current_usable = block->size - HEADER_SIZE;

    if (new_size <= current_usable) return ptr;  /* Fits in current block */

    /* Allocate new block, copy, free old */
    void *new_ptr = eos_malloc(new_size);
    if (!new_ptr) return NULL;

    memcpy(new_ptr, ptr, current_usable);
    eos_free(ptr);
    return new_ptr;
}

void *eos_calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    if (nmemb != 0 && total / nmemb != size) return NULL;  /* Overflow */
    void *ptr = eos_malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void eos_heap_stats(eos_heap_stats_t *out)
{
    if (!out) return;

    /* Recalculate largest free block */
    size_t largest = 0;
    block_header_t *current = free_list;
    while (current) {
        if (current->free && current->size > largest) {
            largest = current->size;
        }
        current = current->next;
    }
    stats.largest_free_block = largest > HEADER_SIZE ? largest - HEADER_SIZE : 0;

    memcpy(out, &stats, sizeof(stats));
}
