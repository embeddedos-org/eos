// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_datacenter_vm.c
 * @brief VM ids in the datacenter service stay bound to their VM.
 *
 * eos_vm_create() returns an id that the caller passes back to
 * eos_vm_start/stop/pause/destroy/get. Destroying one VM must not make
 * another VM's id name a different VM, and a new VM must not get an id
 * that is already in use.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "eos/datacenter.h"

static void test_ids_survive_destroy(void) {
    EosVirtualization vm;
    eos_vm_init(&vm, 16, 32768);

    int a = eos_vm_create(&vm, "vm-a", 2, 1024);
    int b = eos_vm_create(&vm, "vm-b", 2, 1024);
    int c = eos_vm_create(&vm, "vm-c", 2, 1024);
    assert(a >= 0 && b >= 0 && c >= 0);

    assert(eos_vm_destroy(&vm, a) == 0);

    /* b and c still name vm-b and vm-c. */
    const EosVmInstance *vb = eos_vm_get(&vm, b);
    const EosVmInstance *vc = eos_vm_get(&vm, c);
    assert(vb != NULL && strcmp(vb->name, "vm-b") == 0 && vb->id == b);
    assert(vc != NULL && strcmp(vc->name, "vm-c") == 0 && vc->id == c);

    /* The destroyed id is gone. */
    assert(eos_vm_get(&vm, a) == NULL);
    assert(eos_vm_start(&vm, a) == -1);

    /* Starting vm-b by its id starts vm-b, not vm-c. */
    assert(eos_vm_start(&vm, b) == 0);
    assert(eos_vm_get(&vm, b)->state == EOS_VM_RUNNING);
    assert(eos_vm_get(&vm, c)->state == EOS_VM_STOPPED);
    printf("[PASS] vm ids survive destroying another vm\n");
}

static void test_new_id_is_unique(void) {
    EosVirtualization vm;
    eos_vm_init(&vm, 16, 32768);

    int a = eos_vm_create(&vm, "vm-a", 1, 512);
    int b = eos_vm_create(&vm, "vm-b", 1, 512);
    int c = eos_vm_create(&vm, "vm-c", 1, 512);
    assert(eos_vm_destroy(&vm, a) == 0);

    int d = eos_vm_create(&vm, "vm-d", 1, 512);
    assert(d >= 0);
    assert(d != b && d != c);

    /* Stopping vm-c by its id leaves the new vm-d alone. */
    assert(eos_vm_start(&vm, c) == 0);
    assert(eos_vm_start(&vm, d) == 0);
    assert(eos_vm_stop(&vm, c) == 0);
    assert(strcmp(eos_vm_get(&vm, c)->name, "vm-c") == 0);
    assert(eos_vm_get(&vm, c)->state == EOS_VM_STOPPED);
    assert(strcmp(eos_vm_get(&vm, d)->name, "vm-d") == 0);
    assert(eos_vm_get(&vm, d)->state == EOS_VM_RUNNING);
    printf("[PASS] a new vm gets an id no live vm has\n");
}

static void test_capacity_still_enforced(void) {
    EosVirtualization vm;
    eos_vm_init(&vm, 64, 65536);

    int first = -1;
    for (int i = 0; i < EOS_VM_MAX_INSTANCES; i++) {
        int id = eos_vm_create(&vm, "vm", 1, 256);
        assert(id >= 0);
        if (i == 0) first = id;
    }
    assert(eos_vm_create(&vm, "one-too-many", 1, 256) == -1);
    assert(eos_vm_destroy(&vm, first) == 0);
    assert(eos_vm_create(&vm, "fits-again", 1, 256) >= 0);
    assert(vm.count == EOS_VM_MAX_INSTANCES);
    printf("[PASS] instance limit still enforced after destroy\n");
}

int main(void) {
    test_ids_survive_destroy();
    test_new_id_is_unique();
    test_capacity_still_enforced();
    printf("All datacenter VM tests passed.\n");
    return 0;
}
