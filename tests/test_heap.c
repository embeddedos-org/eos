// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eos/mem.h"

#include <stdint.h>
#include <stdio.h>

static uint8_t heap_area[512];

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

    eos_free(allocation);
    return 0;
}
