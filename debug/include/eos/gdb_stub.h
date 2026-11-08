// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_GDB_STUB_H
#define EOS_GDB_STUB_H

#include <stdint.h>

/**
 * @file gdb_stub.h
 * @brief In-firmware GDB remote-protocol stub.
 *
 * GATE: the stub is compiled only when EOS_ENABLE_GDB_STUB is 1
 * (include/eos/eos_config.h defaults it to 0; no product profile sets
 * it). A build with the flag off contains no stub code at all, and
 * anything that calls these functions fails to link -- loudly, not
 * silently. A test build enables the flag (CMakeLists.txt) so the suite
 * can exercise the stub.
 *
 * AUTHENTICATION: there is no authentication. Any peer that can reach
 * the transport can read and write any registered memory range, set
 * breakpoints, and resume execution. Enable the stub only on development
 * images over a trusted link (point-to-point UART or loopback). Never
 * enable it in a shipped/production image, and never expose the TCP
 * transport on a network.
 *
 * MEMORY ACCESS: reads and writes are additionally range-checked. The
 * integrator must register the ranges the stub may touch with
 * eos_gdb_set_mem_ranges(); with no ranges registered -- the default --
 * every access is refused.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EOS_GDB_TRANSPORT_UART = 0,
    EOS_GDB_TRANSPORT_TCP  = 1
} EosGdbTransport;

typedef struct {
    int  (*read)(void *ctx, uint8_t *buf, int len);
    int  (*write)(void *ctx, const uint8_t *buf, int len);
    void *ctx;
} EosGdbIO;

typedef struct {
    uint32_t r[16];    /* general registers (ARM: r0-r15) */
    uint32_t cpsr;     /* status register */
    uint32_t pc;       /* program counter */
    uint32_t sp;       /* stack pointer */
    uint32_t lr;       /* link register */
} EosGdbRegs;

typedef struct {
    EosGdbTransport transport;
    EosGdbIO        io;
    EosGdbRegs      regs;
    int             running;
    int             connected;
    uint32_t        breakpoints[16];
    int             bp_count;
    int             signal;
} EosGdbStub;

/* Initialize GDB stub */
int  eos_gdb_init(EosGdbStub *stub, EosGdbTransport transport);

/* Start listening for GDB connections */
int  eos_gdb_start(EosGdbStub *stub, int port_or_baud);

/* Stop GDB stub */
void eos_gdb_stop(EosGdbStub *stub);

/* Handle a debug exception — call from fault handler */
void eos_gdb_handle_exception(EosGdbStub *stub, int signal);

/* Set IO callbacks (UART or TCP) */
void eos_gdb_set_io(EosGdbStub *stub, EosGdbIO *io);

/* Breakpoint management */
int  eos_gdb_add_breakpoint(EosGdbStub *stub, uint32_t addr);
int  eos_gdb_remove_breakpoint(EosGdbStub *stub, uint32_t addr);
void eos_gdb_clear_breakpoints(EosGdbStub *stub);

/* Memory access for GDB -- range-checked (see eos_gdb_set_mem_ranges). */

/* A contiguous address range the stub may read and write. Ranges are
 * process-wide: they are shared by every EosGdbStub in the image. */
typedef struct {
    uint32_t base;   /* first address in the range */
    uint32_t size;   /* range length in bytes */
} EosGdbMemRange;

/* Most ranges one image needs to expose (RAM, flash, peripherals). */
#define EOS_GDB_MAX_MEM_RANGES 8

/* Register the memory ranges the stub may access. With no ranges
 * registered -- the default -- every read and write is refused.
 * Returns 0 on success, -1 on a bad count or a NULL table. Pass count 0
 * (ranges may be NULL) to clear the table and restore default-deny. A
 * failed call leaves the previously registered ranges untouched. */
int eos_gdb_set_mem_ranges(const EosGdbMemRange *ranges, int count);

/* 1 if [addr, addr+len) lies entirely inside one registered range, else 0.
 * The check is overflow-safe: an end address that wraps past 2^32 is
 * refused. len < 0 is refused. */
int eos_gdb_mem_allowed(uint32_t addr, int len);

int  eos_gdb_read_mem(uint32_t addr, uint8_t *buf, int len);
int  eos_gdb_write_mem(uint32_t addr, const uint8_t *buf, int len);

/* Trigger a breakpoint programmatically */
void eos_gdb_breakpoint(void);

/* Check if debugger is connected */
int  eos_gdb_is_connected(EosGdbStub *stub);

#ifdef __cplusplus
}
#endif

#endif /* EOS_GDB_STUB_H */