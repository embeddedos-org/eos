// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_datacenter.c
 * @brief RAID, load-balancer and failover bookkeeping in the datacenter service.
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
    if (!arr) {
        printf("  FAIL: out of memory\n");
        exit(1);
    }
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

/* IP_HASH fell back to backend 0 when the hashed backend was down, whether
 * or not 0 was healthy; RANDOM ignored health altogether. */
static void test_lb_skips_unhealthy_backends(void)
{
    printf("test_lb_skips_unhealthy_backends:\n");
    static EosLoadBalancer lb;
    const char *clients[] = {"192.168.1.1", "192.168.1.2", "192.168.1.3",
                             "10.0.0.7", "172.16.4.20", NULL};

    eos_lb_init(&lb, EOS_LB_IP_HASH);
    eos_lb_add_backend(&lb, "b0", 80, 1);
    eos_lb_add_backend(&lb, "b1", 80, 1);
    lb.backends[0].healthy = 0;
    int all_healthy = 1;
    for (int i = 0; clients[i]; i++)
        if (eos_lb_select(&lb, clients[i]) != 1) all_healthy = 0;
    ASSERT(all_healthy, "ip_hash: every client lands on the healthy backend");
    ASSERT(lb.backends[0].active_conns == 0, "ip_hash: no connection on the unhealthy backend");
    lb.backends[1].healthy = 0;
    ASSERT(eos_lb_select(&lb, clients[0]) == -1, "ip_hash: no healthy backend selects none");

    eos_lb_init(&lb, EOS_LB_RANDOM);
    eos_lb_add_backend(&lb, "b0", 80, 1);
    eos_lb_add_backend(&lb, "b1", 80, 1);
    eos_lb_add_backend(&lb, "b2", 80, 1);
    lb.backends[0].healthy = 0;
    lb.backends[2].healthy = 0;
    srand(1);
    int random_healthy = 1;
    for (int i = 0; i < 64; i++)
        if (eos_lb_select(&lb, NULL) != 1) random_healthy = 0;
    ASSERT(random_healthy, "random: only the healthy backend is selected");
    lb.backends[1].healthy = 0;
    ASSERT(eos_lb_select(&lb, NULL) == -1, "random: no healthy backend selects none");
}

/* Failback demoted whichever node primary_idx named, unconditionally. */
static void test_failover_failback_states(void)
{
    printf("test_failover_failback_states:\n");
    static EosFailoverCluster fc;

    /* Both nodes fail, then the primary recovers: the stand-in stays FAILED. */
    eos_failover_init(&fc, EOS_FAILOVER_ACTIVE_STANDBY, 1000);
    eos_failover_add_node(&fc, "n0", "10.0.0.1", 1);
    eos_failover_add_node(&fc, "n1", "10.0.0.2", 0);
    ASSERT(eos_failover_trigger(&fc, 0) == 0, "n1 takes over from n0");
    ASSERT(eos_failover_trigger(&fc, 1) == -1, "n1 fails with no standby left");
    eos_failover_failback(&fc, 0);
    ASSERT(fc.nodes[0].state == EOS_NODE_ACTIVE, "recovered primary is active");
    ASSERT(fc.nodes[1].state == EOS_NODE_FAILED, "failed stand-in is not made a standby");
    ASSERT(fc.primary_idx == 0, "primary_idx names the recovered primary");

    /* A lone primary fails and recovers: it must not demote itself. */
    eos_failover_init(&fc, EOS_FAILOVER_ACTIVE_STANDBY, 1000);
    eos_failover_add_node(&fc, "n0", "10.0.0.1", 1);
    ASSERT(eos_failover_trigger(&fc, 0) == -1, "lone primary fails with no standby");
    eos_failover_failback(&fc, 0);
    ASSERT(fc.nodes[0].state == EOS_NODE_ACTIVE, "lone primary is active after failback");

    /* The ordinary case still hands back: the active stand-in becomes standby. */
    eos_failover_init(&fc, EOS_FAILOVER_ACTIVE_STANDBY, 1000);
    eos_failover_add_node(&fc, "n0", "10.0.0.1", 1);
    eos_failover_add_node(&fc, "n1", "10.0.0.2", 0);
    eos_failover_trigger(&fc, 0);
    eos_failover_failback(&fc, 0);
    ASSERT(fc.nodes[0].state == EOS_NODE_ACTIVE, "primary active after normal failback");
    ASSERT(fc.nodes[1].state == EOS_NODE_STANDBY, "stand-in returns to standby");
}

int main(void)
{
    printf("=== EoS Datacenter Tests ===\n\n");
    test_raid_rebuild_rejects_bad_ids();
    test_raid6_capacity_small_array();
    test_lb_skips_unhealthy_backends();
    test_failover_failback_states();
    printf("\n%d/%d assertions passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
