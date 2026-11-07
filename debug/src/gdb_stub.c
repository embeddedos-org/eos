// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/gdb_stub.h"
#include "eos/eos_config.h"
#include <string.h>
#include <stdio.h>

/* Compiled out unless EOS_ENABLE_GDB_STUB is 1 (eos_config.h defaults it
 * to 0; no product profile sets it). A production build contains no stub
 * code at all; the header stays declarative so a stale caller fails at
 * link time, loudly. */
#if EOS_ENABLE_GDB_STUB

static uint8_t hex_val(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    return 0;
}

/**
 * @brief Hex digit to value, reporting invalid input.
 *
 * hex_val() maps any non-hex byte to 0, which silently turns a malformed
 * payload into zero bytes. Callers that write the decoded result to memory
 * use this variant instead and reject the packet.
 *
 * @param c Candidate hex digit.
 * @return 0-15 for a valid digit, -1 otherwise.
 */
static int hex_val_checked(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static char hex_char(uint8_t v) {
    return "0123456789abcdef"[v & 0x0F];
}

static uint8_t checksum(const char *buf, int len) {
    uint8_t sum = 0;
    for (int i = 0; i < len; i++) sum += (uint8_t)buf[i];
    return sum;
}

static int send_packet(EosGdbStub *stub, const char *data) {
    int len = (int)strlen(data);
    uint8_t cs = checksum(data, len);
    char pkt[512];
    int n = snprintf(pkt, sizeof(pkt), "$%s#%c%c", data, hex_char(cs >> 4), hex_char(cs & 0x0F));
    if (n <= 0 || !stub->io.write) return -1;
    return stub->io.write(stub->io.ctx, (const uint8_t *)pkt, n);
}

/* The largest packet body the stub accepts. It is what qSupported reports
 * as PacketSize, and it is derived from the receive buffer so the two cannot
 * drift apart: the stub used to advertise 0x400 over a 512-byte buffer, so a
 * packet GDB was told it could send was refused every time. */
#define GDB_PKT_BUF   512
#define GDB_PKT_MAX   (GDB_PKT_BUF - 1)

/* The most memory one m/M packet may move, derived from GDB_PKT_MAX so the
 * two cannot drift apart either: an M packet is "M<addr>,<len>:" -- up to
 * 15 characters with a 32-bit address -- followed by two hex digits per
 * byte, and it has to fit the body the stub accepts. GDB sizes its bulk
 * transfers from PacketSize, so a cap below this is a request the stub
 * invited and then refused. The reply to an m packet is 2*len characters
 * and fits send_packet()'s buffer for the same reason. */
#define GDB_MEM_HDR   15
#define GDB_MEM_MAX   ((GDB_PKT_MAX - GDB_MEM_HDR) / 2)

/* recv_packet() results below zero: */
#define GDB_RECV_TRANSPORT_GONE  (-1)   /* read failed: the session is over */
#define GDB_RECV_BAD_PACKET      (-2)   /* checksum or size: '-' was sent, GDB will resend */

static int recv_packet(EosGdbStub *stub, char *buf, int maxlen) {
    if (!stub->io.read) return GDB_RECV_TRANSPORT_GONE;
    uint8_t c;
    do {
        if (stub->io.read(stub->io.ctx, &c, 1) != 1) return GDB_RECV_TRANSPORT_GONE;
    } while (c != '$');

    /* Read the body up to '#'. A body the buffer cannot hold is drained to
     * its '#' and refused with '-', so the checksum and the next packet are
     * read from where they really are. Stopping early and treating the next
     * two body bytes as the checksum left the stream out of step. */
    int i = 0, overflow = 0, drained = 0;
    for (;;) {
        if (stub->io.read(stub->io.ctx, &c, 1) != 1) return GDB_RECV_TRANSPORT_GONE;
        if (c == '#') break;
        if (i < maxlen - 1) {
            buf[i++] = (char)c;
        } else if (++drained > GDB_PKT_MAX) {
            /* Draining is for a body that is merely too long. One that never
             * ends -- a peer that keeps sending and never a '#' -- would
             * otherwise hold the exception handler forever. Give up on it
             * as a bad packet: '-' goes out, the session stays up, and the
             * '$' search above resynchronises on whatever comes next. */
            uint8_t nack = '-';
            if (stub->io.write) stub->io.write(stub->io.ctx, &nack, 1);
            return GDB_RECV_BAD_PACKET;
        } else {
            overflow = 1;
        }
    }
    buf[i] = '\0';

    uint8_t cs_chars[2];
    if (stub->io.read(stub->io.ctx, cs_chars, 2) != 2) return GDB_RECV_TRANSPORT_GONE;
    uint8_t expected = (hex_val((char)cs_chars[0]) << 4) | hex_val((char)cs_chars[1]);
    uint8_t actual = checksum(buf, i);
    int ok = !overflow && actual == expected;

    uint8_t ack = ok ? '+' : '-';
    if (stub->io.write) stub->io.write(stub->io.ctx, &ack, 1);

    return ok ? i : GDB_RECV_BAD_PACKET;
}

static void hex_encode(char *dst, const uint8_t *src, int len) {
    for (int i = 0; i < len; i++) {
        dst[i * 2] = hex_char(src[i] >> 4);
        dst[i * 2 + 1] = hex_char(src[i] & 0x0F);
    }
    dst[len * 2] = '\0';
}

static void handle_read_regs(EosGdbStub *stub) {
    char resp[256];
    hex_encode(resp, (const uint8_t *)&stub->regs, sizeof(stub->regs));
    send_packet(stub, resp);
}

static void handle_query(EosGdbStub *stub, const char *pkt) {
    if (strncmp(pkt, "qSupported", 10) == 0) {
        char supported[64];
        snprintf(supported, sizeof(supported), "PacketSize=%x;swbreak+;hwbreak+",
                 (unsigned)GDB_PKT_MAX);
        send_packet(stub, supported);
    } else if (strcmp(pkt, "qAttached") == 0) {
        send_packet(stub, "1");
    } else if (strcmp(pkt, "qTStatus") == 0) {
        send_packet(stub, "T0");
    } else if (strncmp(pkt, "qfThreadInfo", 12) == 0) {
        send_packet(stub, "m1");
    } else if (strncmp(pkt, "qsThreadInfo", 12) == 0) {
        send_packet(stub, "l");
    } else if (strncmp(pkt, "qC", 2) == 0) {
        send_packet(stub, "QC1");
    } else {
        send_packet(stub, "");
    }
}

static void handle_read_mem(EosGdbStub *stub, const char *pkt) {
    uint32_t addr = 0, len = 0;
    if (sscanf(pkt, "m%x,%x", &addr, &len) != 2 || len > GDB_MEM_MAX) {
        send_packet(stub, "E01");
        return;
    }
    uint8_t buf[GDB_MEM_MAX];
    if (eos_gdb_read_mem(addr, buf, (int)len) != 0) {
        send_packet(stub, "E02");
        return;
    }
    char resp[GDB_MEM_MAX * 2 + 1];
    hex_encode(resp, buf, (int)len);
    send_packet(stub, resp);
}

static void handle_write_mem(EosGdbStub *stub, const char *pkt) {
    uint32_t addr = 0, len = 0;
    if (sscanf(pkt, "M%x,%x:", &addr, &len) != 2 || len > GDB_MEM_MAX) {
        send_packet(stub, "E01");
        return;
    }
    const char *data = strchr(pkt, ':');
    if (!data) { send_packet(stub, "E01"); return; }
    data++;

    /* The packet must actually carry two hex digits per byte. Without this
     * check the decode loop read past the end of the received packet and
     * handed whatever followed it to eos_gdb_write_mem() at an address the
     * remote peer chose. */
    if (strlen(data) < (size_t)len * 2u) { send_packet(stub, "E01"); return; }

    uint8_t buf[GDB_MEM_MAX];
    for (uint32_t i = 0; i < len; i++) {
        int hi = hex_val_checked(data[i * 2]);
        int lo = hex_val_checked(data[i * 2 + 1]);
        if (hi < 0 || lo < 0) { send_packet(stub, "E01"); return; }
        buf[i] = (uint8_t)((hi << 4) | lo);
    }
    if (eos_gdb_write_mem(addr, buf, (int)len) != 0) {
        send_packet(stub, "E02");
        return;
    }
    send_packet(stub, "OK");
}

static void handle_set_breakpoint(EosGdbStub *stub, const char *pkt) {
    int type; uint32_t addr, kind;
    if (sscanf(pkt, "Z%d,%x,%x", &type, &addr, &kind) != 3) {
        send_packet(stub, "E01");
        return;
    }
    if (eos_gdb_add_breakpoint(stub, addr) == 0)
        send_packet(stub, "OK");
    else
        send_packet(stub, "E02");
}

static void handle_remove_breakpoint(EosGdbStub *stub, const char *pkt) {
    int type; uint32_t addr, kind;
    if (sscanf(pkt, "z%d,%x,%x", &type, &addr, &kind) != 3) {
        send_packet(stub, "E01");
        return;
    }
    if (eos_gdb_remove_breakpoint(stub, addr) == 0)
        send_packet(stub, "OK");
    else
        send_packet(stub, "E02");
}

int eos_gdb_init(EosGdbStub *stub, EosGdbTransport transport) {
    if (!stub) return -1;
    memset(stub, 0, sizeof(*stub));
    stub->transport = transport;
    stub->signal = 5;
    return 0;
}

int eos_gdb_start(EosGdbStub *stub, int port_or_baud) {
    if (!stub) return -1;
    stub->running = 1;
    stub->connected = 0;
    (void)port_or_baud;
    return 0;
}

void eos_gdb_stop(EosGdbStub *stub) {
    if (stub) {
        stub->running = 0;
        stub->connected = 0;
    }
}

void eos_gdb_set_io(EosGdbStub *stub, EosGdbIO *io) {
    if (stub && io) stub->io = *io;
}

void eos_gdb_handle_exception(EosGdbStub *stub, int signal) {
    if (!stub || !stub->running) return;
    stub->signal = signal;
    stub->connected = 1;

    char reply[16];
    snprintf(reply, sizeof(reply), "S%02x", signal & 0xFF);
    send_packet(stub, reply);

    char pkt[GDB_PKT_BUF];
    while (stub->connected) {
        int n = recv_packet(stub, pkt, sizeof(pkt));
        if (n == GDB_RECV_TRANSPORT_GONE) break;
        /* A refused packet has been answered with '-'; GDB resends it. The
         * session used to end here instead, on the first corrupt or
         * oversized packet. */
        if (n < 0) continue;
        if (n == 0) continue;

        switch (pkt[0]) {
        case '?': {
            char sr[16];
            snprintf(sr, sizeof(sr), "S%02x", stub->signal & 0xFF);
            send_packet(stub, sr);
            break;
        }
        case 'g': handle_read_regs(stub); break;
        case 'm': handle_read_mem(stub, pkt); break;
        case 'M': handle_write_mem(stub, pkt); break;
        case 'c':
            stub->connected = 0;
            send_packet(stub, "OK");
            return;
        case 's':
            send_packet(stub, "S05");
            break;
        case 'Z': handle_set_breakpoint(stub, pkt); break;
        case 'z': handle_remove_breakpoint(stub, pkt); break;
        case 'k':
            stub->running = 0;
            stub->connected = 0;
            return;
        case 'D':
            send_packet(stub, "OK");
            stub->connected = 0;
            return;
        case 'q': case 'Q': handle_query(stub, pkt); break;
        default:
            send_packet(stub, "");
            break;
        }
    }
}

int eos_gdb_add_breakpoint(EosGdbStub *stub, uint32_t addr) {
    if (!stub || stub->bp_count >= 16) return -1;
    for (int i = 0; i < stub->bp_count; i++)
        if (stub->breakpoints[i] == addr) return 0;
    stub->breakpoints[stub->bp_count++] = addr;
    return 0;
}

int eos_gdb_remove_breakpoint(EosGdbStub *stub, uint32_t addr) {
    if (!stub) return -1;
    for (int i = 0; i < stub->bp_count; i++) {
        if (stub->breakpoints[i] == addr) {
            stub->breakpoints[i] = stub->breakpoints[--stub->bp_count];
            return 0;
        }
    }
    return -1;
}

void eos_gdb_clear_breakpoints(EosGdbStub *stub) {
    if (stub) stub->bp_count = 0;
}

/* Default memory access — portable (no weak attribute for MSVC).
 *
 * Range-checked: the address must lie entirely inside a range registered
 * with eos_gdb_set_mem_ranges(); with no ranges registered nothing is
 * accessible. The cast below is still a uint32_t truncation on a 64-bit
 * host -- the stub is a firmware debug aid, not a host tool -- so the
 * in-range accept path is exercised only on target; the refusing side is
 * covered by tests/test_debug.c. */
int eos_gdb_read_mem(uint32_t addr, uint8_t *buf, int len) {
    if (!buf || !eos_gdb_mem_allowed(addr, len)) return -1;
    const uint8_t *p = (const uint8_t *)(uintptr_t)addr;
    memcpy(buf, p, (size_t)len);
    return 0;
}

int eos_gdb_write_mem(uint32_t addr, const uint8_t *buf, int len) {
    if (!buf || !eos_gdb_mem_allowed(addr, len)) return -1;
    uint8_t *p = (uint8_t *)(uintptr_t)addr;
    memcpy(p, buf, (size_t)len);
    return 0;
}

/* The ranges the stub may touch. Empty by default: nothing is accessible
 * until the integrator registers ranges explicitly. Process-wide, shared
 * by every EosGdbStub in the image (the memory hooks take no stub). */
static EosGdbMemRange gdb_mem_ranges[EOS_GDB_MAX_MEM_RANGES];
static int gdb_mem_range_count;

int eos_gdb_set_mem_ranges(const EosGdbMemRange *ranges, int count) {
    if (count < 0 || count > EOS_GDB_MAX_MEM_RANGES) return -1;
    if (count > 0 && !ranges) return -1;
    if (count > 0)
        memcpy(gdb_mem_ranges, ranges, (size_t)count * sizeof(*ranges));
    gdb_mem_range_count = count;
    return 0;
}

int eos_gdb_mem_allowed(uint32_t addr, int len) {
    if (len < 0) return 0;
    for (int i = 0; i < gdb_mem_range_count; i++) {
        uint32_t base = gdb_mem_ranges[i].base;
        uint32_t size = gdb_mem_ranges[i].size;
        /* Overflow-safe without a wider type: addr >= base, so addr - base
         * cannot underflow; (uint32_t)len <= size, so size - len cannot
         * underflow. An end address that wraps past 2^32 fails the second
         * comparison and is refused. */
        if (addr >= base && (uint32_t)len <= size &&
            (addr - base) <= (size - (uint32_t)len))
            return 1;
    }
    return 0;
}

void eos_gdb_breakpoint(void) {
#if defined(__GNUC__) && defined(__arm__)
    __asm__ volatile("bkpt #0");
#elif defined(__GNUC__) && defined(__aarch64__)
    __asm__ volatile("brk #0");
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    __asm__ volatile("int3");
#elif defined(__GNUC__) && defined(__riscv)
    __asm__ volatile("ebreak");
#elif defined(_MSC_VER)
    __debugbreak();
#else
    /* Fallback: no-op */
    (void)0;
#endif
}

int eos_gdb_is_connected(EosGdbStub *stub) {
    return stub ? stub->connected : 0;
}

#endif /* EOS_ENABLE_GDB_STUB */