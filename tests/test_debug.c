// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eos/gdb_stub.h"
#include "eos/coredump.h"

static void test_gdb_init(void) {
    EosGdbStub stub;
    assert(eos_gdb_init(&stub, EOS_GDB_TRANSPORT_TCP) == 0);
    assert(stub.signal == 5);
    assert(stub.bp_count == 0);
    assert(!eos_gdb_is_connected(&stub));
    assert(eos_gdb_init(NULL, EOS_GDB_TRANSPORT_TCP) == -1);
    printf("[PASS] gdb init\n");
}

static void test_gdb_breakpoints(void) {
    EosGdbStub stub;
    eos_gdb_init(&stub, EOS_GDB_TRANSPORT_TCP);
    assert(eos_gdb_add_breakpoint(&stub, 0x08000100) == 0);
    assert(eos_gdb_add_breakpoint(&stub, 0x08000200) == 0);
    assert(stub.bp_count == 2);
    assert(eos_gdb_add_breakpoint(&stub, 0x08000100) == 0);  /* dup ok */
    assert(stub.bp_count == 2);
    assert(eos_gdb_remove_breakpoint(&stub, 0x08000100) == 0);
    assert(stub.bp_count == 1);
    assert(eos_gdb_remove_breakpoint(&stub, 0xDEAD) == -1);
    eos_gdb_clear_breakpoints(&stub);
    assert(stub.bp_count == 0);
    printf("[PASS] gdb breakpoints\n");
}

static void test_gdb_start_stop(void) {
    EosGdbStub stub;
    eos_gdb_init(&stub, EOS_GDB_TRANSPORT_UART);
    assert(eos_gdb_start(&stub, 115200) == 0);
    assert(stub.running == 1);
    eos_gdb_stop(&stub);
    assert(stub.running == 0);
    printf("[PASS] gdb start/stop\n");
}

static void test_coredump_init(void) {
    assert(eos_coredump_init(EOS_DUMP_TARGET_RAM) == 0);
    assert(!eos_coredump_exists());
    printf("[PASS] coredump init\n");
}

static void test_coredump_capture(void) {
    eos_coredump_init(EOS_DUMP_TARGET_RAM);
    EosCrashRegs regs; memset(&regs, 0, sizeof(regs));
    regs.pc = 0x08001234;
    regs.lr = 0x08001000;
    regs.msp = 0x20001000;
    assert(eos_coredump_capture(EOS_CRASH_HARDFAULT, &regs) == 0);
    assert(eos_coredump_exists());
    EosCoreDump dump;
    assert(eos_coredump_load(&dump) == 0);
    assert(dump.magic == EOS_COREDUMP_MAGIC);
    assert(dump.reason == EOS_CRASH_HARDFAULT);
    assert(dump.regs.pc == 0x08001234);
    assert(eos_coredump_validate(&dump));
    printf("[PASS] coredump capture + validate\n");
}

static void test_coredump_clear(void) {
    eos_coredump_init(EOS_DUMP_TARGET_RAM);
    EosCrashRegs regs = {0};
    eos_coredump_capture(EOS_CRASH_ASSERT, &regs);
    assert(eos_coredump_exists());
    eos_coredump_clear();
    assert(!eos_coredump_exists());
    printf("[PASS] coredump clear\n");
}

static void test_coredump_crc(void) {
    EosCoreDump dump;
    memset(&dump, 0, sizeof(dump));
    dump.magic = EOS_COREDUMP_MAGIC;
    dump.version = 1;
    dump.crc32 = eos_coredump_crc32(&dump);
    assert(eos_coredump_validate(&dump));
    dump.regs.pc = 1;  /* corrupt */
    assert(!eos_coredump_validate(&dump));
    printf("[PASS] coredump CRC validation\n");
}

int main(void) {
    printf("=== EoS Debug Tests ===\n");
    test_gdb_init();
    test_gdb_breakpoints();
    test_gdb_start_stop();
    test_coredump_init();
    test_coredump_capture();
    test_coredump_clear();
    test_coredump_crc();
    printf("=== ALL DEBUG TESTS PASSED ===\n");
    return 0;
}