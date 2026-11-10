// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_datacenter.c
 * @brief RAID bookkeeping in the datacenter service.
 */

#include "eos/datacenter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(expr, msg)                                         \
    do {                                                          \
        tests_run++;                                              \
        if (!(expr)) {                                            \
            printf("  FAIL: %s (line %d)\n", msg, __LINE__);      \
        } else {                                                  \
            tests_passed++;                                       \
            printf("  PASS: %s\n", msg);                          \
        }                                                         \
    } while (0)

/* eos_raid_rebuild() indexed disks[] with ids it had only checked for < 0,
 * so an id past the array wrote outside it. The array is allocated to its
 * exact size so a sanitizer build reports any stray write precisely. */
static void test_raid_rebuild_rejects_bad_ids(void)
{
    printf("test_raid_rebuild_rejects_bad_ids:\n");
    EosRaidArray *arr = malloc(sizeof(*arr));
    eos_raid_init(arr, EOS_RAID_1, 2);
    ASSERT(eos_raid_add_disk(arr, 1000, "d0") == 0, "disk 0 added");
    ASSERT(eos_raid_add_disk(arr, 1000, "d1") == 1, "disk 1 added");
    ASSERT(eos_raid_remove_disk(arr, 0) == 0, "disk 0 failed");

    ASSERT(eos_raid_rebuild(arr, 0, 2) == -1, "spare id == disk_count rejected");
    ASSERT(eos_raid_rebuild(arr, 0, 1000) == -1, "spare id far past the array rejected");
    ASSERT(eos_raid_rebuild(arr, EOS_RAID_MAX_DISKS, 1) == -1, "failed id past the array rejected");
    ASSERT(eos_raid_rebuild(arr, 1, 1) == -1, "failed id == spare id rejected");
    ASSERT(arr->degraded == 1, "rejected rebuilds leave the array degraded");

    ASSERT(eos_raid_rebuild(arr, 0, 1) == 0, "valid rebuild succeeds");
    ASSERT(arr->degraded == 0, "valid rebuild clears degraded");
    free(arr);
}

/* RAID 6 keeps two parity disks; below three disks nothing is usable. The
 * capacity used to be computed with (uint64_t)(disk_count - 2), which wraps
 * for a single disk. */
static void test_raid6_capacity_small_array(void)
{
    printf("test_raid6_capacity_small_array:\n");
    EosRaidArray arr;
    eos_raid_init(&arr, EOS_RAID_6, 4);
    eos_raid_add_disk(&arr, 1000, "d0");
    ASSERT(arr.usable_capacity == 0, "one disk: no usable capacity");
    eos_raid_add_disk(&arr, 1000, "d1");
    ASSERT(arr.usable_capacity == 0, "two disks: no usable capacity");
    eos_raid_add_disk(&arr, 1000, "d2");
    ASSERT(arr.usable_capacity == 1000, "three disks: one disk of data");
    eos_raid_add_disk(&arr, 1000, "d3");
    ASSERT(arr.usable_capacity == 2000, "four disks: two disks of data");
}

int main(void)
{
    printf("=== EoS Datacenter Tests ===\n\n");
    test_raid_rebuild_rejects_bad_ids();
    test_raid6_capacity_small_array();
    printf("\n%d/%d assertions passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
