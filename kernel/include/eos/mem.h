// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file mem.h
 * @brief EoS memory management API
 *
 * Provides heap allocation, MPU management, and memory statistics.
 */

#ifndef EOS_MEM_H
#define EOS_MEM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Heap Allocator
 * ============================================================ */

#ifndef EOS_HEAP_SIZE
#define EOS_HEAP_SIZE   (32 * 1024)  /* Default 32 KB */
#endif

typedef struct {
    size_t total_size;
    size_t used;
    size_t free;
    size_t largest_free_block;
    uint32_t alloc_count;
    uint32_t free_count;
} eos_heap_stats_t;

/**
 * @brief Initialize the heap allocator.
 * @param heap_base  Base address of the heap memory.
 * @param heap_size  Total heap size in bytes.
 * @return 0 on success, -1 on error.
 */
int eos_heap_init(void *heap_base, size_t heap_size);

/**
 * @brief Allocate memory from the kernel heap.
 */
void *eos_malloc(size_t size);

/**
 * @brief Free a previously allocated block.
 */
void eos_free(void *ptr);

/**
 * @brief Reallocate memory (grow or shrink).
 */
void *eos_realloc(void *ptr, size_t new_size);

/**
 * @brief Allocate zero-initialized memory.
 */
void *eos_calloc(size_t nmemb, size_t size);

/**
 * @brief Get heap statistics.
 */
void eos_heap_stats(eos_heap_stats_t *stats);

/* ============================================================
 * MPU Management
 * ============================================================ */

#ifndef EOS_MPU_MAX_REGIONS
#define EOS_MPU_MAX_REGIONS  8
#endif

typedef enum {
    EOS_MEM_NO_ACCESS   = 0,
    EOS_MEM_PRIV_RW     = 1,
    EOS_MEM_PRIV_RO     = 5,
    EOS_MEM_FULL_RW     = 3,
    EOS_MEM_FULL_RO     = 6,
} eos_mem_access_t;

typedef struct {
    uint32_t base_addr;
    uint32_t size;
    eos_mem_access_t access;
    bool executable;
    bool cacheable;
    bool enabled;
} eos_mpu_region_t;

/**
 * @brief Configure an MPU region for task stack guard.
 */
int eos_mpu_set_stack_guard(uint32_t stack_bottom, uint32_t guard_size);

/**
 * @brief Configure MPU regions for task isolation.
 */
int eos_mpu_configure_task(uint8_t task_id, uint32_t stack_base,
                            uint32_t stack_size, uint32_t code_base,
                            uint32_t code_size);

/**
 * @brief Enable the MPU.
 */
int eos_mpu_enable(void);

/**
 * @brief Disable the MPU.
 */
int eos_mpu_disable(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_MEM_H */
