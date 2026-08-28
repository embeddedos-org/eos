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
    if (eos_heap_init(heap_area, sizeof(heap_area)) != 0) {
        fprintf(stderr, "heap initialization failed\n");
        return 1;
    }

    if (eos_malloc(SIZE_MAX) != NULL) {
        fprintf(stderr, "SIZE_MAX allocation must be rejected\n");
        return 1;
    }

    if (eos_malloc(SIZE_MAX - 3) != NULL) {
        fprintf(stderr, "alignment-overflow allocation must be rejected\n");
        return 1;
    }

    void *allocation = eos_malloc(32);
    if (allocation == NULL) {
        fprintf(stderr, "valid allocation failed after rejected requests\n");
        return 1;
    }

    /* Growing a live allocation must move it and preserve the contents. */
    memset(allocation, 0x5A, 32);
    void *grown = eos_realloc(allocation, 96);
    if (grown == NULL) {
        fprintf(stderr, "growing realloc failed\n");
        return 1;
    }
    for (int i = 0; i < 32; i++) {
        if (((uint8_t *)grown)[i] != 0x5A) {
            fprintf(stderr, "realloc did not preserve the payload\n");
            return 1;
        }
    }

    /* eos_free() range-checks its argument. eos_realloc() did not, so a
     * pointer that never came from this heap was handed straight back as if
     * it had been resized. */
    memset(foreign, 0, sizeof(foreign));
    if (eos_realloc(foreign + 64, 16) != NULL) {
        fprintf(stderr, "realloc of a non-heap pointer must be rejected\n");
        return 1;
    }

    /* A header claiming a size below HEADER_SIZE underflowed
     * `block->size - HEADER_SIZE` to nearly SIZE_MAX, so every growth request
     * looked already satisfied and realloc returned the short buffer. */
    memset(foreign, 0, sizeof(foreign));
    ((size_t *)foreign)[0] = 8;
    if (eos_realloc(foreign + 16, 4096) != NULL) {
        fprintf(stderr, "realloc must reject a header smaller than itself\n");
        return 1;
    }

    /* The same validation must not make eos_free() reject real pointers. */
    void *second = eos_malloc(48);
    if (second == NULL) {
        fprintf(stderr, "allocation after realloc failed\n");
        return 1;
    }
    eos_free(second);

    /* Freeing foreign memory stays a silent no-op, not a crash. */
    eos_free(foreign + 64);

    eos_free(grown);
    return 0;
}
