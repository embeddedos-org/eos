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
 * address on a 64-bit host. The accept path is therefore exercised here
 * only from the refusing side: out-of-range memory access is answered E02
 * without dereferencing (test_gdb_memory_access_out_of_range_is_refused),
 * and the range table itself is unit-tested directly
 * (test_gdb_mem_ranges). The in-range dereference still runs only on
 * target; see the note in the pull request.
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

/* The stub advertises PacketSize in its qSupported reply, and GDB sends
 * packets up to that size. A packet the stub cannot hold must not desync
 * the stream: it is answered with '-' and the next packet still parses.
 * And a packet exactly as long as advertised must be accepted. */
static unsigned advertised_packet_size(void) {
    gdb_reset();
    gdb_push("qSupported");
    gdb_push("D");
    gdb_run();
    const char *p = strstr(gdb_tx, "PacketSize=");
    assert(p != NULL);
    unsigned size = 0;
    assert(sscanf(p + strlen("PacketSize="), "%x", &size) == 1);
    return size;
}

/* The first thing the stub writes is the stop reply "$S05#xx"; the byte
 * after it is the ack ('+' or '-') for the first scripted packet. */
static char first_ack(void) {
    const char *hash = strchr(gdb_tx, '#');
    assert(hash != NULL && strlen(hash) >= 4);
    return hash[3];
}

static void test_gdb_packet_of_the_advertised_size_is_accepted(void) {
    unsigned size = advertised_packet_size();
    assert(size >= 64 && size < GDB_RX_CAP - 8);

    char body[GDB_RX_CAP];
    memset(body, 'x', size);
    body[0] = 'q';                /* an unknown query: the reply is empty */
    body[size] = '\0';

    gdb_reset();
    gdb_push(body);
    gdb_push("D");
    gdb_run();
    /* '+' acknowledges the long packet, "$#00" is the empty reply to an
     * unknown query, and D is then handled normally. */
    assert(first_ack() == '+');
    assert(tx_has("$#00"));
    assert(tx_has("$OK#"));
    printf("[PASS] gdb packet of the advertised size is accepted\n");
}

static void test_gdb_packet_longer_than_advertised_does_not_desync(void) {
    unsigned size = advertised_packet_size();

    char body[GDB_RX_CAP];
    memset(body, 'x', size + 40);
    body[0] = 'q';
    body[size + 40] = '\0';

    gdb_reset();
    gdb_push(body);
    gdb_push("D");
    gdb_run();
    /* The oversized packet is refused with '-', nothing of it is answered,
     * and the D that follows still gets its OK: the stream stayed in sync. */
    assert(first_ack() == '-');
    assert(!tx_has("$#00"));
    assert(tx_has("$OK#"));
    printf("[PASS] gdb packet longer than advertised does not desync\n");
}

/* A packet whose checksum does not match is answered with '-', and GDB
 * resends it. The session must still be there to receive the resend: it
 * used to end on the first corrupt packet, because the dispatch loop read
 * every negative result from recv_packet() as a dead transport. */
static void test_gdb_corrupt_checksum_is_nacked_and_the_session_continues(void) {
    gdb_reset();
    const char *bad = "$qAttached#00";       /* real checksum is not 00 */
    memcpy(gdb_rx, bad, strlen(bad));
    gdb_rx_len = strlen(bad);
    gdb_push("D");
    gdb_run();
    assert(first_ack() == '-');
    assert(!tx_has("$1#"));                  /* the refused query got no reply */
    assert(tx_has("$OK#"));                  /* D still handled */
    printf("[PASS] gdb corrupt checksum is NACKed and the session continues\n");
}

/* A peer that keeps sending body bytes and never a '#' must not hold the
 * exception handler forever. The read below hands out 'x' until a cap and
 * only then reports the transport gone; the stub is expected to give up on
 * the body long before that, saying so with a '-'. The unbounded loop this
 * pins against read every byte to the cap and wrote nothing. */
#define ENDLESS_CAP 20000
static size_t endless_reads;
static size_t reads_at_first_write;

static int mock_read_endless(void *ctx, uint8_t *buf, int len) {
    (void)ctx;
    int n = 0;
    while (n < len && endless_reads < ENDLESS_CAP) {
        buf[n++] = (endless_reads == 0) ? '$' : 'x';
        endless_reads++;
    }
    return n;
}

/* The stop reply ($S05#..) goes out before anything is read; the write to
 * time is the first ack byte, which is the stub's verdict on the body. */
static int mock_write_first(void *ctx, const uint8_t *buf, int len) {
    if (reads_at_first_write == 0 && len == 1 && (buf[0] == '-' || buf[0] == '+'))
        reads_at_first_write = endless_reads;
    return mock_write(ctx, buf, len);
}

static void test_gdb_body_that_never_ends_is_abandoned(void) {
    gdb_reset();
    endless_reads = 0;
    reads_at_first_write = 0;
    EosGdbStub stub;
    EosGdbIO io = { mock_read_endless, mock_write_first, NULL };
    eos_gdb_init(&stub, EOS_GDB_TRANSPORT_TCP);
    eos_gdb_set_io(&stub, &io);
    eos_gdb_start(&stub, 0);
    eos_gdb_handle_exception(&stub, 5);          /* returns: the cap ends the transport */

    assert(reads_at_first_write > 0);            /* an ack was written before the cap at all */
    char verdict = first_ack();                  /* the ack after the stop reply */
    unsigned size = advertised_packet_size();    /* separate scripted session */
    assert(reads_at_first_write <= 2u * size + 8u);  /* ... after at most the buffer plus one more body's worth */
    assert(verdict == '-');                      /* and it was a refusal */
    printf("[PASS] gdb body that never ends is abandoned with a NACK\n");
}

/* The advertised PacketSize is what GDB sizes an M transfer from, so the
 * memory cap has to be derived from it (tests/unit/test_gdb_stub_limits.py
 * pins the derivation in the source). The accept path cannot run here --
 * the memory hooks dereference the 32-bit address on a 64-bit host, as the
 * note at the top of this file says -- so this pins the boundary from the
 * refusing side: one byte more than the packet can carry is E01, and the
 * cap is where the packet size says it is, not at the old literal 128. */
static void test_gdb_memory_transfer_one_past_the_cap_is_refused(void) {
    unsigned size = advertised_packet_size();
    unsigned cap = (size - 15) / 2;              /* "M<addr>,<len>:" is at most 15 chars */
    assert(cap > 128);                           /* the old literal cap, well below what the packet carries */
    char body[64];
    gdb_reset();
    snprintf(body, sizeof(body), "M0,%x:aabb", cap + 1);
    gdb_push(body);
    gdb_push("D");
    gdb_run();
    assert(first_ack() == '+');                  /* well-formed packet ... */
    assert(tx_has("E01"));                       /* ... refused by the cap */
    printf("[PASS] gdb memory transfer one past the cap is refused\n");
}

/* With no ranges registered -- the default -- every address is refused,
 * and the hooks refuse without dereferencing, so this is safe on a 64-bit
 * host. The stub's memory access is deny-by-default: an image that
 * enables the stub but registers no ranges cannot be read or written. */
static void test_gdb_mem_allowed_default_deny(void) {
    assert(eos_gdb_set_mem_ranges(NULL, 0) == 0);
    assert(!eos_gdb_mem_allowed(0x00000000u, 4));
    assert(!eos_gdb_mem_allowed(0x20000000u, 4));
    assert(!eos_gdb_mem_allowed(0xFFFFFFFFu, 4));
    assert(!eos_gdb_mem_allowed(0x20000000u, 0));
    uint8_t buf[4] = {0};
    assert(eos_gdb_read_mem(0x20000000u, buf, 4) != 0);
    assert(eos_gdb_write_mem(0x20000000u, buf, 4) != 0);
    assert(eos_gdb_read_mem(0x20000000u, NULL, 4) != 0);
    assert(eos_gdb_write_mem(0x20000000u, NULL, 4) != 0);
    printf("[PASS] gdb memory access is denied with no ranges registered\n");
}

static void test_gdb_mem_ranges(void) {
    EosGdbMemRange ranges[2] = {
        { 0x20000000u, 0x1000u },   /* SRAM */
        { 0x08000000u, 0x8000u },   /* flash */
    };
    assert(eos_gdb_set_mem_ranges(ranges, 2) == 0);
    assert(eos_gdb_mem_allowed(0x20000000u, 4));        /* start of range */
    assert(eos_gdb_mem_allowed(0x20000FFCu, 4));        /* last word */
    assert(!eos_gdb_mem_allowed(0x20001000u, 4));       /* one past the end */
    assert(!eos_gdb_mem_allowed(0x20000FFDu, 4));       /* straddles the end */
    assert(!eos_gdb_mem_allowed(0x1FFFFFFFu, 4));       /* just below the range */
    assert(eos_gdb_mem_allowed(0x08000000u, 0x8000));   /* whole flash range */
    assert(!eos_gdb_mem_allowed(0x08000000u, 0x8001));  /* one byte over */
    assert(!eos_gdb_mem_allowed(0x20000000u, -1));      /* negative length */
    assert(!eos_gdb_mem_allowed(0x10000000u, 4));       /* between the ranges */
    /* A range whose end wraps past 2^32 must not admit the wrapped bytes. */
    EosGdbMemRange wrap = { 0xFFFFFF00u, 0x100u };
    assert(eos_gdb_set_mem_ranges(&wrap, 1) == 0);
    assert(eos_gdb_mem_allowed(0xFFFFFF00u, 0x100));    /* exactly the range */
    assert(!eos_gdb_mem_allowed(0xFFFFFFF0u, 0x20));    /* end wraps to 0x10 */
    assert(!eos_gdb_mem_allowed(0x00000000u, 4));       /* the wrapped address */
    /* Bad registrations are refused and leave the table alone. */
    assert(eos_gdb_set_mem_ranges(NULL, 1) == -1);
    assert(eos_gdb_set_mem_ranges(ranges, EOS_GDB_MAX_MEM_RANGES + 1) == -1);
    assert(eos_gdb_set_mem_ranges(ranges, -1) == -1);
    assert(eos_gdb_mem_allowed(0xFFFFFF00u, 0x100));    /* still the wrap range */
    /* Clearing restores default-deny. */
    assert(eos_gdb_set_mem_ranges(NULL, 0) == 0);
    assert(!eos_gdb_mem_allowed(0x20000000u, 4));
    printf("[PASS] gdb memory ranges\n");
}

/* With no ranges registered, an m/M packet that passes the length and
 * payload checks reaches eos_gdb_read_mem()/eos_gdb_write_mem() and is
 * refused with E02 -- without dereferencing the address, so this runs
 * safely on a 64-bit host. This is the accept path's refusing side: the
 * packet is well-formed, the length fits, and still nothing is read or
 * written. */
static void test_gdb_memory_access_out_of_range_is_refused(void) {
    assert(eos_gdb_set_mem_ranges(NULL, 0) == 0);
    gdb_reset();
    gdb_push("m20000000,4");
    gdb_push("D");
    gdb_run();
    assert(tx_has("E02"));
    gdb_reset();
    gdb_push("M20000000,4:deadbeef");
    gdb_push("D");
    gdb_run();
    assert(tx_has("E02"));
    printf("[PASS] gdb refuses out-of-range memory access\n");
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
    test_gdb_packet_of_the_advertised_size_is_accepted();
    test_gdb_packet_longer_than_advertised_does_not_desync();
    test_gdb_corrupt_checksum_is_nacked_and_the_session_continues();
    test_gdb_body_that_never_ends_is_abandoned();
    test_gdb_memory_transfer_one_past_the_cap_is_refused();
    test_gdb_mem_allowed_default_deny();
    test_gdb_mem_ranges();
    test_gdb_memory_access_out_of_range_is_refused();
    test_coredump_init();
    test_coredump_capture();
    test_coredump_clear();
    test_coredump_crc();
    printf("=== ALL DEBUG TESTS PASSED ===\n");
    return 0;
}