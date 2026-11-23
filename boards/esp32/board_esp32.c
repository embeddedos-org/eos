// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file board_esp32.c
 * @brief ESP32 board bring-up: watchdogs, console, fatal report
 */

#include "board_esp32.h"
#include "xtensa_frame.h"

#define REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))

/* Timer group 0 watchdog (MWDT0) */
#define TIMG0_WDTCONFIG0      0x3FF5F048U
#define TIMG0_WDTWPROTECT     0x3FF5F064U
/* RTC watchdog (RWDT) */
#define RTC_CNTL_WDTCONFIG0   0x3FF4808CU
#define RTC_CNTL_WDTWPROTECT  0x3FF480A4U
/* Both watchdogs take the same write-protect key (rwdt_ll.h, mwdt_ll.h). */
#define WDT_WKEY              0x50D83AA1U

/* UART0. TX FIFO writes go through the AHB alias, as ESP-IDF's uart_ll does
 * on the ESP32; status is read on the peripheral bus. */
#define UART0_FIFO_AHB        0x60000000U
#define UART0_STATUS          0x3FF4001CU
#define UART_TXFIFO_CNT(st)   (((st) >> 16) & 0xFFU)
#define UART_FIFO_DEPTH       128U

void eos_esp32_early_init(void)
{
    /*
     * After a flash boot the ROM has armed the RTC watchdog and the timer
     * group 0 watchdog with "flashboot" protection (ESP-IDF,
     * bootloader_init.c). Nothing in EoS feeds them yet, so both are switched
     * off. Writing 0 to WDTCONFIG0 clears both the enable and the flashboot
     * bit.
     */
    REG32(TIMG0_WDTWPROTECT) = WDT_WKEY;
    REG32(TIMG0_WDTCONFIG0) = 0;
    REG32(TIMG0_WDTWPROTECT) = 0;

    REG32(RTC_CNTL_WDTWPROTECT) = WDT_WKEY;
    REG32(RTC_CNTL_WDTCONFIG0) = 0;
    REG32(RTC_CNTL_WDTWPROTECT) = 0;
}

uint32_t eos_esp32_ccount(void)
{
    uint32_t v;
    __asm volatile ("rsr.ccount %0" : "=a" (v));
    return v;
}

static void console_putc(char c)
{
    while (UART_TXFIFO_CNT(REG32(UART0_STATUS)) >= UART_FIFO_DEPTH - 2U) {
    }
    REG32(UART0_FIFO_AHB) = (uint8_t)c;
}

void eos_esp32_console_write(const char *s, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\n') console_putc('\r');
        console_putc(s[i]);
    }
}

void eos_esp32_console_puts(const char *s)
{
    size_t n = 0;
    while (s[n] != '\0') n++;
    eos_esp32_console_write(s, n);
}

void eos_esp32_console_put_u32(uint32_t v)
{
    char buf[10];
    size_t n = 0;
    do {
        buf[n++] = (char)('0' + (v % 10U));
        v /= 10U;
    } while (v != 0U && n < sizeof(buf));
    while (n > 0) console_putc(buf[--n]);
}

static void console_put_hex(uint32_t v)
{
    static const char digits[] = "0123456789abcdef";
    eos_esp32_console_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        console_putc(digits[(v >> shift) & 0xFU]);
    }
}

/*
 * Called by vectors.S with interrupts masked, on the interrupt stack, for any
 * exception the port cannot resume from. Reports and stops: there is no task
 * state worth returning to, and pretending otherwise would hide the fault.
 */
__attribute__((noreturn))
void eos_xtensa_fatal(uint32_t code, uint32_t exccause, uint32_t epc1,
                      uint32_t excvaddr)
{
    eos_esp32_console_puts("\nEOS FATAL code=");
    eos_esp32_console_put_u32(code);
    eos_esp32_console_puts(" exccause=");
    eos_esp32_console_put_u32(exccause);
    eos_esp32_console_puts(" epc1=");
    console_put_hex(epc1);
    eos_esp32_console_puts(" excvaddr=");
    console_put_hex(excvaddr);
    eos_esp32_console_puts("\n");
    for (;;) {
        __asm volatile ("waiti 15");
    }
}
