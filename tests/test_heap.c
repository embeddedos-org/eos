// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eos/mem.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t heap_area[512];

/* Memory that never came from this heap. */
static uint8_t foreign[128];

int main(void)
{
    eos_heap_stats_t stats;

    /* ---------------------------------------------------------
     * 1. Heap initialization
     * --------------------------------------------------------- */
    if (eos_heap_init(heap_area, sizeof(heap_area)) != 0) {
        fprintf(stderr, "heap initialization failed\n");
        return 1;
    }

    /* ---------------------------------------------------------
     * 2. Overflow protection
     * --------------------------------------------------------- */
    if (eos_malloc(SIZE_MAX) != NULL) {
        fprintf(stderr, "SIZE_MAX allocation must be rejected\n");
        return 1;
    }

    if (eos_malloc(SIZE_MAX - 3) != NULL) {
        fprintf(stderr, "alignment-overflow allocation must be rejected\n");
        return 1;
    }

    /* ---------------------------------------------------------
     * 3. Basic allocation and alignment
     * --------------------------------------------------------- */
    void *a = eos_malloc(32);

    if (a == NULL) {
        fprintf(stderr, "32-byte allocation failed\n");
        return 1;
    }

    if (((uintptr_t)a % 8) != 0) {
        fprintf(stderr, "allocation is not 8-byte aligned\n");
        return 1;
    }

    /* ---------------------------------------------------------
     * 4. Write/read allocated memory
     * --------------------------------------------------------- */
    memset(a, 0xAA, 32);

    uint8_t *bytes = (uint8_t *)a;

    for (int i = 0; i < 32; i++) {
        if (bytes[i] != 0xAA) {
            fprintf(stderr, "allocated memory read/write failed\n");
            return 1;
        }
    }

    /* ---------------------------------------------------------
     * 5. Multiple allocations
     * --------------------------------------------------------- */
    void *b = eos_malloc(64);
    void *c = eos_malloc(16);

    if (b == NULL || c == NULL) {
        fprintf(stderr, "multiple allocations failed\n");
        return 1;
    }

    if (a == b || a == c || b == c) {
        fprintf(stderr, "allocations overlap\n");
        return 1;
    }

    /* ---------------------------------------------------------
     * 6. Free one block and allocate again
     * --------------------------------------------------------- */
    eos_free(b);

    void *d = eos_malloc(48);

    if (d == NULL) {
        fprintf(stderr, "allocation after free failed\n");
        return 1;
    }

    /* ---------------------------------------------------------
     * 7. calloc
     * --------------------------------------------------------- */
    uint8_t *zero = (uint8_t *)eos_calloc(16, sizeof(uint8_t));

    if (zero == NULL) {
        fprintf(stderr, "calloc failed\n");
        return 1;
    }

    for (int i = 0; i < 16; i++) {
        if (zero[i] != 0) {
            fprintf(stderr, "calloc memory is not zeroed\n");
            return 1;
        }
    }

    /* ---------------------------------------------------------
     * 8. realloc data preservation
     * --------------------------------------------------------- */
    uint8_t *r = (uint8_t *)eos_malloc(16);

    if (r == NULL) {
        fprintf(stderr, "realloc setup allocation failed\n");
        return 1;
    }

    for (int i = 0; i < 16; i++) {
        r[i] = (uint8_t)i;
    }

    uint8_t *r2 = (uint8_t *)eos_realloc(r, 64);

    if (r2 == NULL) {
        fprintf(stderr, "realloc growth failed\n");
        return 1;
    }

    for (int i = 0; i < 16; i++) {
        if (r2[i] != (uint8_t)i) {
            fprintf(stderr, "realloc did not preserve data\n");
            return 1;
        }
    }

    /* ---------------------------------------------------------
     * 9. Invalid realloc inputs
     *
     * PR #77 added validation so realloc rejects pointers that
     * do not belong to the heap and malformed block headers.
     * --------------------------------------------------------- */

    /* A pointer that never came from this heap must be rejected. */
    memset(foreign, 0, sizeof(foreign));

    if (eos_realloc(foreign + 64, 16) != NULL) {
        fprintf(stderr, "realloc of a non-heap pointer must be rejected\n");
        return 1;
    }

    /*
     * A header smaller than HEADER_SIZE must be rejected rather than
     * allowing size arithmetic to underflow.
     */
    memset(foreign, 0, sizeof(foreign));
    ((size_t *)foreign)[0] = 8;

    if (eos_realloc(foreign + 16, 4096) != NULL) {
        fprintf(stderr, "realloc must reject a header smaller than itself\n");
        return 1;
    }

    /* Freeing foreign memory remains a safe no-op. */
    eos_free(foreign + 64);

    /* ---------------------------------------------------------
     * 10. Free everything and verify heap statistics
     * --------------------------------------------------------- */
    eos_free(a);
    eos_free(c);
    eos_free(d);
    eos_free(zero);
    eos_free(r2);

    eos_heap_stats(&stats);

    if (stats.used != 0) {
        fprintf(stderr, "heap still reports used memory: %zu\n",
                stats.used);
        return 1;
    }

    if (stats.free == 0) {
        fprintf(stderr, "heap reports no free memory\n");
        return 1;
    }

    printf("heap tests passed\n");
    return 0;
}