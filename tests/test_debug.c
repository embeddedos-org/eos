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
    (void)stub;
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

/* ---- Scripted GDB transport ----
 *
 * Feeds crafted packets to eos_gdb_handle_exception() and captures the
 * replies, so the remote-protocol parsing can be exercised on the host.
 *
 * These cases deliberately cover only packets the stub must *reject*.
 * eos_gdb_read_mem()/eos_gdb_write_mem() cast a uint32_t straight to a
 * pointer, so any packet the stub accepts would dereference a truncated
 * address on a 64-bit host. The accept path is therefore not exercised
 * here; see the note in the pull request.
 */

#define GDB_RX_CAP 4096
#define GDB_TX_CAP 4096

static char   gdb_rx[GDB_RX_CAP];
static size_t gdb_rx_len;
static size_t gdb_rx_pos;
static char   gdb_tx[GDB_TX_CAP];
static size_t gdb_tx_len;

static int mock_read(void *ctx, uint8_t *buf, int len) {
    (void)ctx;
    int n = 0;
    while (n < len && gdb_rx_pos < gdb_rx_len) {
        buf[n++] = (uint8_t)gdb_rx[gdb_rx_pos++];
    }
    return n;
}

/* Honours the EosGdbIO.write contract: the return value must be the number of
 * bytes actually written. Silently truncating while reporting success would
 * let tx_has() miss a reply the stub really did send, hiding a failure. */
static int mock_write(void *ctx, const uint8_t *buf, int len) {
    (void)ctx;
    assert(len >= 0);
    assert(gdb_tx_len + (size_t)len < GDB_TX_CAP);
    memcpy(gdb_tx + gdb_tx_len, buf, (size_t)len);
    gdb_tx_len += (size_t)len;
    gdb_tx[gdb_tx_len] = '\0';
    return len;
}

static void gdb_reset(void) {
    memset(gdb_rx, 0, sizeof(gdb_rx));
    memset(gdb_tx, 0, sizeof(gdb_tx));
    gdb_rx_len = 0;
    gdb_rx_pos = 0;
    gdb_tx_len = 0;
}

/* Append "$<body>#<checksum>" to the scripted input. */
static void gdb_push(const char *body) {
    uint8_t sum = 0;
    size_t n = strlen(body);
    /* '$' + body + '#' + two checksum digits */
    assert(gdb_rx_len + n + 4 <= GDB_RX_CAP);
    gdb_rx[gdb_rx_len++] = '$';
    for (size_t i = 0; i < n; i++) {
        sum = (uint8_t)(sum + (uint8_t)body[i]);
        gdb_rx[gdb_rx_len++] = body[i];
    }
    gdb_rx[gdb_rx_len++] = '#';
    gdb_rx[gdb_rx_len++] = "0123456789abcdef"[(sum >> 4) & 0x0F];
    gdb_rx[gdb_rx_len++] = "0123456789abcdef"[sum & 0x0F];
}

static void gdb_run(void) {
    EosGdbStub stub;
    EosGdbIO io = { mock_read, mock_write, NULL };
    eos_gdb_init(&stub, EOS_GDB_TRANSPORT_TCP);
    eos_gdb_set_io(&stub, &io);
    eos_gdb_start(&stub, 0);
    eos_gdb_handle_exception(&stub, 5);
}

static int tx_has(const char *needle) {
    return strstr(gdb_tx, needle) != NULL;
}

/*
 * Regression: handle_write_mem() bounded len to 128 but never checked that
 * the packet carried 2*len hex digits. "M0,80:" with no payload made the
 * decode loop read 256 bytes past the end of the received packet and write
 * them to an address the peer chose.
 */
static void test_gdb_write_mem_truncated_payload(void) {
    gdb_reset();
    gdb_push("M0,80:");     /* declares 128 bytes, supplies none */
    gdb_push("D");
    gdb_run();
    assert(tx_has("E01"));
    printf("[PASS] gdb rejects M packet with truncated payload\n");
}

static void test_gdb_write_mem_short_payload(void) {
    gdb_reset();
    gdb_push("M0,4:aabb");  /* declares 4 bytes = 8 digits, supplies 4 */
    gdb_push("D");
    gdb_run();
    assert(tx_has("E01"));
    printf("[PASS] gdb rejects M packet with short payload\n");
}

/* hex_val() mapped any non-hex byte to 0, so garbage was written as zeros. */
static void test_gdb_write_mem_non_hex_payload(void) {
    gdb_reset();
    gdb_push("M0,2:zzzz");
    gdb_push("D");
    gdb_run();
    assert(tx_has("E01"));
    printf("[PASS] gdb rejects M packet with non-hex payload\n");
}

static void test_gdb_write_mem_oversized_len(void) {
    gdb_reset();
    gdb_push("M0,1000:aabb");
    gdb_push("D");
    gdb_run();
    assert(tx_has("E01"));
    printf("[PASS] gdb rejects M packet with oversized length\n");
}

static void test_gdb_query_roundtrip(void) {
    gdb_reset();
    gdb_push("qSupported");
    gdb_push("D");
    gdb_run();
    assert(tx_has("PacketSize"));
    printf("[PASS] gdb qSupported round-trip\n");
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
    (void)dump;
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
    test_gdb_write_mem_truncated_payload();
    test_gdb_write_mem_short_payload();
    test_gdb_write_mem_non_hex_payload();
    test_gdb_write_mem_oversized_len();
    test_gdb_query_roundtrip();
    test_coredump_init();
    test_coredump_capture();
    test_coredump_clear();
    test_coredump_crc();
    printf("=== ALL DEBUG TESTS PASSED ===\n");
    return 0;
}