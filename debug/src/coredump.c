// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/coredump.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

static EosDumpTarget g_target = EOS_DUMP_TARGET_RAM;
static EosCoreDump g_ram_dump;
static int g_dump_valid = 0;
static uint32_t g_flash_addr = 0;
static uint32_t g_flash_size = 0;
static void (*g_uart_fn)(const char *buf, int len) = NULL;

static uint32_t crc32_byte(uint32_t crc, uint8_t b) {
    crc ^= b;
    for (int i = 0; i < 8; i++)
        crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
    return crc;
}

uint32_t eos_coredump_crc32(const EosCoreDump *dump) {
    if (!dump) return 0;
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *p = (const uint8_t *)dump;
    size_t len = sizeof(*dump) - sizeof(dump->crc32);
    for (size_t i = 0; i < len; i++)
        crc = crc32_byte(crc, p[i]);
    return crc ^ 0xFFFFFFFF;
}

int eos_coredump_validate(const EosCoreDump *dump) {
    if (!dump) return 0;
    if (dump->magic != EOS_COREDUMP_MAGIC) return 0;
    return dump->crc32 == eos_coredump_crc32(dump);
}

int eos_coredump_init(EosDumpTarget target) {
    g_target = target;
    g_dump_valid = 0;
    memset(&g_ram_dump, 0, sizeof(g_ram_dump));
    return 0;
}

int eos_coredump_capture(EosCrashReason reason, const EosCrashRegs *regs) {
    EosCoreDump dump;
    memset(&dump, 0, sizeof(dump));
    dump.magic = EOS_COREDUMP_MAGIC;
    dump.version = 1;
    dump.reason = reason;
    if (regs) dump.regs = *regs;

    /* Capture stack — copy from current SP */
    uint32_t sp = regs ? regs->msp : 0;
    if (sp != 0) {
        uint32_t copy_len = sizeof(dump.stack);
        const uint8_t *sp_ptr = (const uint8_t *)(uintptr_t)sp;
        memcpy(dump.stack, sp_ptr, copy_len);
        dump.stack_size = copy_len;
    }

    dump.crc32 = eos_coredump_crc32(&dump);
    return eos_coredump_save(&dump);
}

int eos_coredump_save(const EosCoreDump *dump) {
    if (!dump) return -1;
    switch (g_target) {
    case EOS_DUMP_TARGET_RAM:
        memcpy(&g_ram_dump, dump, sizeof(g_ram_dump));
        g_dump_valid = 1;
        return 0;
    case EOS_DUMP_TARGET_FLASH:
        if (g_flash_addr == 0) return -1;
        /* Flash write is platform-specific — store in RAM as fallback */
        memcpy(&g_ram_dump, dump, sizeof(g_ram_dump));
        g_dump_valid = 1;
        return 0;
    case EOS_DUMP_TARGET_UART:
        eos_coredump_print(dump);
        memcpy(&g_ram_dump, dump, sizeof(g_ram_dump));
        g_dump_valid = 1;
        return 0;
    default:
        return -1;
    }
}

int eos_coredump_load(EosCoreDump *dump) {
    if (!dump || !g_dump_valid) return -1;
    memcpy(dump, &g_ram_dump, sizeof(*dump));
    return eos_coredump_validate(dump) ? 0 : -1;
}

static void dump_line(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n < 0) return;
    if (g_uart_fn) {
        g_uart_fn(buf, n);
        g_uart_fn("\n", 1);
    }
    fprintf(stderr, "%s\n", buf);
}

static const char *reason_str(EosCrashReason r) {
    switch (r) {
    case EOS_CRASH_HARDFAULT:  return "HardFault";
    case EOS_CRASH_MEMFAULT:   return "MemManage";
    case EOS_CRASH_BUSFAULT:   return "BusFault";
    case EOS_CRASH_USAGEFAULT: return "UsageFault";
    case EOS_CRASH_ASSERT:     return "Assert";
    case EOS_CRASH_WATCHDOG:   return "Watchdog";
    case EOS_CRASH_STACK_OVF:  return "StackOverflow";
    default:                   return "Unknown";
    }
}

void eos_coredump_print(const EosCoreDump *dump) {
    if (!dump) return;
    dump_line("========== EoS CORE DUMP ==========");
    dump_line("Reason:    %s (0x%02X)", reason_str(dump->reason), dump->reason);
    dump_line("Version:   %u", dump->version);
    dump_line("Uptime:    %u ms", dump->uptime_ms);
    if (dump->task_name[0])
        dump_line("Task:      %s", dump->task_name);
    dump_line("--- Registers ---");
    dump_line("PC:   0x%08X   LR:  0x%08X", dump->regs.pc, dump->regs.lr);
    dump_line("SP:   0x%08X   MSP: 0x%08X", dump->regs.r[13], dump->regs.msp);
    dump_line("PSP:  0x%08X   xPSR:0x%08X", dump->regs.psp, dump->regs.xpsr);
    dump_line("CFSR: 0x%08X   HFSR:0x%08X", dump->regs.cfsr, dump->regs.hfsr);
    dump_line("MMFAR:0x%08X   BFAR:0x%08X", dump->regs.mmfar, dump->regs.bfar);
    for (int i = 0; i < 13; i += 4) {
        dump_line("R%-2d: 0x%08X  R%-2d: 0x%08X  R%-2d: 0x%08X  R%-2d: 0x%08X",
                  i, dump->regs.r[i], i+1, dump->regs.r[i+1],
                  i+2, dump->regs.r[i+2], i+3, dump->regs.r[i+3]);
    }
    if (dump->stack_size > 0) {
        dump_line("--- Stack (%u bytes) ---", dump->stack_size);
        for (uint32_t i = 0; i < dump->stack_size && i < 64; i += 16) {
            char hex[64] = {0};
            for (uint32_t j = 0; j < 16 && (i + j) < dump->stack_size; j++)
                snprintf(hex + j * 3, 4, "%02X ", dump->stack[i + j]);
            dump_line("  +%03X: %s", i, hex);
        }
    }
    dump_line("CRC32: 0x%08X  Valid: %s", dump->crc32,
              eos_coredump_validate(dump) ? "YES" : "NO");
    dump_line("===================================");
}

int eos_coredump_exists(void) {
    return g_dump_valid && g_ram_dump.magic == EOS_COREDUMP_MAGIC;
}

void eos_coredump_clear(void) {
    g_dump_valid = 0;
    memset(&g_ram_dump, 0, sizeof(g_ram_dump));
}

void eos_coredump_set_flash_addr(uint32_t addr, uint32_t size) {
    g_flash_addr = addr;
    g_flash_size = size;
}

void eos_coredump_set_uart(void (*write_fn)(const char *buf, int len)) {
    g_uart_fn = write_fn;
}