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
#define UART_ST_UTX(st)       (((st) >> 24) & 0xFU)
#define UART0_CLKDIV          0x3FF40014U

/* Clock control (ESP-IDF v5.4: hal/esp32/include/hal/clk_tree_ll.h,
 * esp_hw_support/port/esp32/rtc_clk.c, soc/esp32/register/soc) */
#define RTC_CNTL_OPTIONS0     0x3FF48000U
#define RTC_CNTL_CLK_CONF     0x3FF48070U
#define RTC_CNTL_REG          0x3FF4807CU
#define RTC_CNTL_STORE4       0x3FF480B0U   /* XTAL frequency, if the ROM set it */
#define RTC_CNTL_STORE5       0x3FF480B4U   /* APB frequency, for the ROM */
#define DPORT_CPU_PER_CONF    0x3FF0003CU
#define SYSCON_PLL_TICK_CONF  0x3FF66008U
#define BB_I2C_FORCE_PD       (1U << 6)
#define BBPLL_I2C_FORCE_PD    (1U << 8)
#define BBPLL_FORCE_PD        (1U << 10)
#define BIAS_I2C_FORCE_PD     (1U << 18)
#define DIG_DBIAS_WAK_S       11
#define DIG_DBIAS_1V10        4U
#define DIG_DBIAS_1V25        7U
#define SOC_CLK_SEL_S         27

/* BBPLL registers on the internal analog I2C bus (soc/regi2c_bbpll.h) */
#define I2C_BBPLL             0x66U
#define I2C_BBPLL_HOSTID      4U
#define I2C_BBPLL_IR_CAL_DELAY    0U
#define I2C_BBPLL_IR_CAL_EXT_CAP  1U
#define I2C_BBPLL_OC_LREF         2U
#define I2C_BBPLL_OC_DIV_7_0      3U
#define I2C_BBPLL_OC_ENB_FCAL     4U
#define I2C_BBPLL_OC_DCUR         5U
#define I2C_BBPLL_BBADC_DSMP      9U
#define I2C_BBPLL_OC_ENB_VCON     10U
#define I2C_BBPLL_ENDIV5          11U
#define I2C_BBPLL_BBADC_CAL_7_0   12U

/* Boot ROM (esp32.rom.ld; address in esp32.ld). */
extern void rom_i2c_writeReg(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t data);

#if EOS_ESP32_CPU_MHZ != 40U && EOS_ESP32_CPU_MHZ != 80U && \
    EOS_ESP32_CPU_MHZ != 160U && EOS_ESP32_CPU_MHZ != 240U
#error "EOS_ESP32_CPU_MHZ must be 40, 80, 160 or 240"
#endif
#if defined(EOS_SYS_CLOCK_HZ) && EOS_SYS_CLOCK_HZ != EOS_ESP32_CPU_MHZ * 1000000U
#error "EOS_SYS_CLOCK_HZ must equal EOS_ESP32_CPU_MHZ MHz: the tick counts CPU cycles"
#endif

static void delay_cycles(uint32_t n)
{
    uint32_t t0 = eos_esp32_ccount();
    while (eos_esp32_ccount() - t0 < n) {
    }
}

static void field(uint32_t reg, uint32_t shift, uint32_t mask, uint32_t value)
{
    REG32(reg) = (REG32(reg) & ~(mask << shift)) | ((value & mask) << shift);
}

/*
 * XTAL frequency in MHz as the boot ROM recorded it: both halves of STORE4
 * equal, bit 0 of the high half aside (ESP-IDF clk_ll_xtal_load_freq_mhz).
 * Every ESP32-WROOM module has a 40 MHz crystal, so an unset register is
 * taken as 40 (EOS_ESP32_XTAL_MHZ overrides). Only the 40 MHz PLL settings
 * have been run on a board, so any other crystal stops the boot (see
 * eos_esp32_early_init()).
 */
#ifndef EOS_ESP32_XTAL_MHZ
#define EOS_ESP32_XTAL_MHZ 40U
#endif
static uint32_t xtal_mhz(void)
{
    uint32_t r = REG32(RTC_CNTL_STORE4);
    if ((r & 0xFFFFU) == ((r >> 16) & 0xFFFFU) && r != 0U && r != 0xFFFFFFFFU) {
        return r & 0xFFFEU & 0xFFFFU;
    }
    return EOS_ESP32_XTAL_MHZ;
}

/*
 * Move the CPU from the crystal to the PLL: ESP-IDF's rtc_clk_bbpll_enable(),
 * rtc_clk_bbpll_configure() and rtc_clk_cpu_freq_to_pll_mhz() for a 40 MHz
 * crystal. 240 MHz takes the 480 MHz PLL setting and a 1.25 V digital core
 * supply; 80 and 160 MHz the 320 MHz setting and 1.10 V. APB becomes 80 MHz,
 * so UART0 (115200 8N1 from the ROM) gets a new divider: the caller has let
 * its TX FIFO drain first.
 */
static void clock_to_pll(uint32_t cpu_mhz)
{
    int pll480 = cpu_mhz == 240U;

    REG32(RTC_CNTL_OPTIONS0) &= ~BIAS_I2C_FORCE_PD;
    REG32(RTC_CNTL_OPTIONS0) &= ~(BB_I2C_FORCE_PD | BBPLL_FORCE_PD | BBPLL_I2C_FORCE_PD);
    rom_i2c_writeReg(I2C_BBPLL, I2C_BBPLL_HOSTID, I2C_BBPLL_IR_CAL_DELAY, 0x18);
    rom_i2c_writeReg(I2C_BBPLL, I2C_BBPLL_HOSTID, I2C_BBPLL_IR_CAL_EXT_CAP, 0x20);
    rom_i2c_writeReg(I2C_BBPLL, I2C_BBPLL_HOSTID, I2C_BBPLL_OC_ENB_FCAL, 0x9A);
    rom_i2c_writeReg(I2C_BBPLL, I2C_BBPLL_HOSTID, I2C_BBPLL_OC_ENB_VCON, 0x00);
    rom_i2c_writeReg(I2C_BBPLL, I2C_BBPLL_HOSTID, I2C_BBPLL_BBADC_CAL_7_0, 0x00);

    if (pll480) {
        field(RTC_CNTL_REG, DIG_DBIAS_WAK_S, 7U, DIG_DBIAS_1V25);
        delay_cycles(3U * 40U);                       /* 3 us at 40 MHz */
    } else {
        field(RTC_CNTL_REG, DIG_DBIAS_WAK_S, 7U, DIG_DBIAS_1V10);
    }
    rom_i2c_writeReg(I2C_BBPLL, I2C_BBPLL_HOSTID, I2C_BBPLL_ENDIV5, pll480 ? 0xC3 : 0x43);
    rom_i2c_writeReg(I2C_BBPLL, I2C_BBPLL_HOSTID, I2C_BBPLL_BBADC_DSMP, pll480 ? 0x74 : 0x84);
    rom_i2c_writeReg(I2C_BBPLL, I2C_BBPLL_HOSTID, I2C_BBPLL_OC_LREF, 0x00);
    rom_i2c_writeReg(I2C_BBPLL, I2C_BBPLL_HOSTID, I2C_BBPLL_OC_DIV_7_0, pll480 ? 28 : 32);
    rom_i2c_writeReg(I2C_BBPLL, I2C_BBPLL_HOSTID, I2C_BBPLL_OC_DCUR, (3U << 6) | 6U);
    /* PLL lock: ESP-IDF waits 80 us with the 150 kHz RC slow clock, 160 us
     * with a 32 kHz crystal; wait the longer. */
    delay_cycles(160U * 40U);

    REG32(DPORT_CPU_PER_CONF) = cpu_mhz == 240U ? 2U : cpu_mhz == 160U ? 1U : 0U;
    REG32(SYSCON_PLL_TICK_CONF) = 80U - 1U;           /* REF_TICK stays 1 MHz */
    field(RTC_CNTL_CLK_CONF, SOC_CLK_SEL_S, 3U, 1U);  /* CPU clock source: PLL */
    /* Record APB for the ROM's own code (clk_ll_apb_store_freq_hz). */
    uint32_t apb = 80000000U >> 12;
    REG32(RTC_CNTL_STORE5) = (apb & 0xFFFFU) | ((apb & 0xFFFFU) << 16);
    /* UART0 115200 baud at APB 80 MHz: 80e6 * 16 / 115200 = 11111 sixteenths. */
    uint32_t div16 = (80000000U * 16U + 115200U / 2U) / 115200U;
    REG32(UART0_CLKDIV) = ((div16 & 0xFU) << 20) | (div16 >> 4);
    delay_cycles(30U * cpu_mhz);                      /* 30 us, as ESP-IDF */
}

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

    if (EOS_ESP32_CPU_MHZ == 40U) return;
    if (xtal_mhz() != 40U) {
        /* The tick, the UART dividers and every timeout are compiled for
         * EOS_ESP32_CPU_MHZ; running on regardless would get all of them
         * wrong without a sign. The console is still at the ROM's rate. */
        eos_esp32_console_puts("EOS FATAL: crystal is not 40 MHz; build with -DEOS_ESP32_CPU_MHZ=40\n");
        for (;;) {
            __asm volatile ("waiti 15");
        }
    }
    /* The ROM's boot messages may still be leaving UART0: a new divider
     * mid-byte would garble them. */
    while (UART_TXFIFO_CNT(REG32(UART0_STATUS)) != 0U ||
           UART_ST_UTX(REG32(UART0_STATUS)) != 0U) {
    }
    clock_to_pll(EOS_ESP32_CPU_MHZ);
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
