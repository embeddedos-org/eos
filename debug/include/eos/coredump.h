// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_COREDUMP_H
#define EOS_COREDUMP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EOS_CRASH_HARDFAULT    = 0,
    EOS_CRASH_MEMFAULT     = 1,
    EOS_CRASH_BUSFAULT     = 2,
    EOS_CRASH_USAGEFAULT   = 3,
    EOS_CRASH_ASSERT       = 4,
    EOS_CRASH_WATCHDOG     = 5,
    EOS_CRASH_STACK_OVF    = 6,
    EOS_CRASH_UNKNOWN      = 0xFF
} EosCrashReason;

typedef struct {
    uint32_t r[16];
    uint32_t xpsr;
    uint32_t msp;
    uint32_t psp;
    uint32_t lr;
    uint32_t pc;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t mmfar;
    uint32_t bfar;
} EosCrashRegs;

typedef struct {
    uint32_t       magic;       /* 0xC0DEDEAD */
    uint32_t       version;
    EosCrashReason reason;
    uint32_t       timestamp;
    EosCrashRegs   regs;
    uint8_t        stack[256];
    uint32_t       stack_size;
    char           task_name[32];
    uint32_t       uptime_ms;
    uint32_t       crc32;
} EosCoreDump;

#define EOS_COREDUMP_MAGIC 0xC0DEDEAD

typedef enum {
    EOS_DUMP_TARGET_FLASH  = 0,
    EOS_DUMP_TARGET_UART   = 1,
    EOS_DUMP_TARGET_RAM    = 2
} EosDumpTarget;

/* Initialize core dump subsystem */
int  eos_coredump_init(EosDumpTarget target);

/* Capture current state — call from fault handler */
int  eos_coredump_capture(EosCrashReason reason, const EosCrashRegs *regs);

/* Save dump to configured target */
int  eos_coredump_save(const EosCoreDump *dump);

/* Load last saved dump */
int  eos_coredump_load(EosCoreDump *dump);

/* Print dump to serial/stderr */
void eos_coredump_print(const EosCoreDump *dump);

/* Check if a valid dump exists */
int  eos_coredump_exists(void);

/* Clear saved dump */
void eos_coredump_clear(void);

/* CRC32 validation */
uint32_t eos_coredump_crc32(const EosCoreDump *dump);
int      eos_coredump_validate(const EosCoreDump *dump);

/* Set flash address for dump storage (bare-metal) */
void eos_coredump_set_flash_addr(uint32_t addr, uint32_t size);

/* Set UART output for dump (bare-metal) */
void eos_coredump_set_uart(void (*write_fn)(const char *buf, int len));

#ifdef __cplusplus
}
#endif

#endif /* EOS_COREDUMP_H */