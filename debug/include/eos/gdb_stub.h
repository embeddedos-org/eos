// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_GDB_STUB_H
#define EOS_GDB_STUB_H

#include <stdint.h>

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

/* Memory access for GDB */
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