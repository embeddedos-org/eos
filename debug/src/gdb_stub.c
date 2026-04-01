// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/gdb_stub.h"
#include <string.h>
#include <stdio.h>

static uint8_t hex_val(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    return 0;
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

static int recv_packet(EosGdbStub *stub, char *buf, int maxlen) {
    if (!stub->io.read) return -1;
    uint8_t c;
    do {
        if (stub->io.read(stub->io.ctx, &c, 1) != 1) return -1;
    } while (c != '$');

    int i = 0;
    while (i < maxlen - 1) {
        if (stub->io.read(stub->io.ctx, &c, 1) != 1) return -1;
        if (c == '#') break;
        buf[i++] = (char)c;
    }
    buf[i] = '\0';

    uint8_t cs_chars[2];
    if (stub->io.read(stub->io.ctx, cs_chars, 2) != 2) return -1;
    uint8_t expected = (hex_val((char)cs_chars[0]) << 4) | hex_val((char)cs_chars[1]);
    uint8_t actual = checksum(buf, i);

    uint8_t ack = (actual == expected) ? '+' : '-';
    if (stub->io.write) stub->io.write(stub->io.ctx, &ack, 1);

    return (actual == expected) ? i : -1;
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
        send_packet(stub, "PacketSize=400;swbreak+;hwbreak+");
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
    if (sscanf(pkt, "m%x,%x", &addr, &len) != 2 || len > 128) {
        send_packet(stub, "E01");
        return;
    }
    uint8_t buf[128];
    if (eos_gdb_read_mem(addr, buf, (int)len) != 0) {
        send_packet(stub, "E02");
        return;
    }
    char resp[257];
    hex_encode(resp, buf, (int)len);
    send_packet(stub, resp);
}

static void handle_write_mem(EosGdbStub *stub, const char *pkt) {
    uint32_t addr = 0, len = 0;
    if (sscanf(pkt, "M%x,%x:", &addr, &len) != 2 || len > 128) {
        send_packet(stub, "E01");
        return;
    }
    const char *data = strchr(pkt, ':');
    if (!data) { send_packet(stub, "E01"); return; }
    data++;
    uint8_t buf[128];
    for (uint32_t i = 0; i < len; i++)
        buf[i] = (hex_val(data[i * 2]) << 4) | hex_val(data[i * 2 + 1]);
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

    char pkt[512];
    while (stub->connected) {
        int n = recv_packet(stub, pkt, sizeof(pkt));
        if (n < 0) break;
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

/* Default memory access — portable (no weak attribute for MSVC) */
int eos_gdb_read_mem(uint32_t addr, uint8_t *buf, int len) {
    const uint8_t *p = (const uint8_t *)(uintptr_t)addr;
    memcpy(buf, p, (size_t)len);
    return 0;
}

int eos_gdb_write_mem(uint32_t addr, const uint8_t *buf, int len) {
    uint8_t *p = (uint8_t *)(uintptr_t)addr;
    memcpy(p, buf, (size_t)len);
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