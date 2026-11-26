// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_esp32_sleep.c
 * @brief ESP32 light and deep sleep with timer wake-up (board API)
 *
 * The portable power API (power/include/eos/power.h) has no platform
 * backend, so ESP32 sleep is a board API (board_esp32.h):
 *
 *  - eos_esp32_light_sleep_us(): the digital core keeps power and state and
 *    the CPU resumes after the call. Its clock is gated meanwhile, so CCOUNT
 *    and with it the kernel tick stand still for the sleep: kernel time does
 *    not include it. Call it with nothing else needing the CPU.
 *  - eos_esp32_deep_sleep_us(): the digital core powers off and the wake-up
 *    is a reset (reason 5, DEEPSLEEP_RESET); the boot ROM loads the image
 *    again. Variables marked EOS_ESP32_RTC_NOINIT, in RTC slow memory, keep
 *    their values; the RTC timer keeps counting across it.
 *
 * Register settings follow ESP-IDF v5.4 esp_hw_support/port/esp32/
 * rtc_sleep.c (rtc_sleep_init(), rtc_sleep_start()) for a timer wake-up
 * with RTC slow memory kept on. Both run on the 40 MHz crystal, as ESP-IDF's
 * do, and light sleep switches back to the PLL after. The RTC slow clock is
 * the internal ~150 kHz RC oscillator; its rate is measured against CCOUNT
 * once and the result kept in RTC memory.
 */

#include "board_esp32.h"
#include "hal_esp32_internal.h"

#define REG32 ESP32_REG32

#define RTC_CNTL_OPTIONS0        0x3FF48000U
#define RTC_CNTL_SLP_TIMER0      0x3FF48004U
#define RTC_CNTL_SLP_TIMER1      0x3FF48008U
#define RTC_CNTL_TIME_UPDATE     0x3FF4800CU
#define RTC_CNTL_TIME0           0x3FF48010U
#define RTC_CNTL_TIME1           0x3FF48014U
#define RTC_CNTL_STATE0          0x3FF48018U
#define RTC_CNTL_RESET_STATE     0x3FF48034U
#define RTC_CNTL_WAKEUP_STATE    0x3FF48038U
#define RTC_CNTL_INT_RAW         0x3FF48040U
#define RTC_CNTL_INT_CLR         0x3FF48048U
#define RTC_CNTL_SLP_REJECT_CONF 0x3FF48064U
#define RTC_CNTL_PWC             0x3FF48080U
#define RTC_CNTL_DIG_PWC         0x3FF48084U
#define RTC_CNTL_DIG_ISO         0x3FF48088U
#define RTC_CNTL_STORE6          0x3FF480B8U   /* deep-sleep wake stub entry */
#define UART0_STATUS             0x3FF4001CU

#define TIME_VALID_INT_CLR       (1U << 4)
#define MAIN_TIMER_INT_CLR       (1U << 8)
#define TIME_UPDATE              (1U << 31)
#define TIME_VALID               (1U << 30)
#define SLEEP_EN                 (1U << 31)
#define WAKEUP_ENA_S             11
#define WAKEUP_ENA_M             (0x7FFU << WAKEUP_ENA_S)
#define RTC_TIMER_TRIG_EN        (1U << 3)
#define SLP_WAKEUP_INT           (1U << 0)
#define SLP_REJECT_INT           (1U << 1)
#define PWC_PD_EN                (1U << 20)
#define PWC_SLOWMEM_PD_EN        (1U << 17)
#define PWC_SLOWMEM_FORCE_PU     (1U << 16)
#define PWC_FASTMEM_PD_EN        (1U << 14)
#define PWC_FASTMEM_FORCE_PU     (1U << 13)
#define PWC_SLOWMEM_FORCE_NOISO  (1U << 2)
#define PWC_FASTMEM_FORCE_NOISO  (1U << 0)
#define DIG_PWC_DG_WRAP_PD_EN    (1U << 31)
#define DIG_PWC_WIFI_PD_EN       (1U << 30)
#define DIG_PWC_DG_WRAP_FORCE_PU (1U << 20)
#define DIG_PWC_DG_WRAP_FORCE_PD (1U << 19)
#define DIG_ISO_DG_PAD_FORCE_ISO   (1U << 13)
#define DIG_ISO_DG_PAD_FORCE_NOISO (1U << 12)

/* RTC slow clock rate in Hz, measured once; kept across deep sleep. */
static uint32_t s_slow_hz EOS_ESP32_RTC_NOINIT;
static uint32_t s_slow_hz_check EOS_ESP32_RTC_NOINIT;
#define SLOW_HZ_TAG 0x534C4F57U   /* "SLOW" */

/* ESP-IDF rtc_cntl_ll_get_rtc_time(): request an update, wait for VALID,
 * and clear VALID so the next request waits for fresh values. */
uint64_t eos_esp32_rtc_time(void)
{
    REG32(RTC_CNTL_TIME_UPDATE) = TIME_UPDATE;
    uint32_t t0 = eos_esp32_ccount();
    while ((REG32(RTC_CNTL_TIME_UPDATE) & TIME_VALID) == 0U) {
        if (eos_esp32_ccount() - t0 > ESP32_CPU_HZ / 1000U) break;   /* 1 ms */
    }
    REG32(RTC_CNTL_INT_CLR) = TIME_VALID_INT_CLR;
    return ((uint64_t)(REG32(RTC_CNTL_TIME1) & 0xFFFFU) << 32) | REG32(RTC_CNTL_TIME0);
}

uint32_t eos_esp32_rtc_slow_hz(void)
{
    if (s_slow_hz_check == (s_slow_hz ^ SLOW_HZ_TAG) && s_slow_hz != 0U) return s_slow_hz;
    /* 20 ms of CCOUNT against the RTC timer: about 3000 slow cycles, so a
     * reading to 0.03%; the RC oscillator itself drifts more than that. */
    uint64_t r0 = eos_esp32_rtc_time();
    uint32_t c0 = eos_esp32_ccount();
    while (eos_esp32_ccount() - c0 < ESP32_CPU_HZ / 50U) {
    }
    uint64_t r1 = eos_esp32_rtc_time();
    s_slow_hz = (uint32_t)((r1 - r0) * 50U);
    s_slow_hz_check = s_slow_hz ^ SLOW_HZ_TAG;
    return s_slow_hz;
}

uint32_t eos_esp32_reset_reason(void)
{
    return REG32(RTC_CNTL_RESET_STATE) & 0x3FU;
}

uint32_t eos_esp32_wakeup_cause(void)
{
    return REG32(RTC_CNTL_WAKEUP_STATE) & 0x7FFU;
}

static void uart0_flush(void)
{
    while ((((REG32(UART0_STATUS) >> 16) & 0xFFU) != 0U) ||
           (((REG32(UART0_STATUS) >> 24) & 0xFU) != 0U)) {
    }
}

/* Arm the RTC timer @p us from now and select it as the only wake-up. */
static int arm_timer(uint64_t us)
{
    uint32_t hz = eos_esp32_rtc_slow_hz();
    if (hz == 0U || us == 0U) return -1;
    uint64_t ticks = us * hz / 1000000U;
    if (ticks < 2U) ticks = 2U;
    uint64_t target = eos_esp32_rtc_time() + ticks;
    if (target >> 48) return -1;
    /* ESP-IDF rtc_cntl_ll_set_wakeup_timer(): the compare value only. The
     * alarm-enable bit raises the main-timer interrupt instead, and a stale
     * one wakes the chip at once. */
    REG32(RTC_CNTL_SLP_TIMER0) = (uint32_t)target;
    REG32(RTC_CNTL_SLP_TIMER1) = (uint32_t)(target >> 32);
    REG32(RTC_CNTL_INT_CLR) = MAIN_TIMER_INT_CLR;
    REG32(RTC_CNTL_WAKEUP_STATE) = (REG32(RTC_CNTL_WAKEUP_STATE) & ~WAKEUP_ENA_M) |
                                   (RTC_TIMER_TRIG_EN << WAKEUP_ENA_S);
    REG32(RTC_CNTL_SLP_REJECT_CONF) = 0;          /* nothing rejects the sleep */
    REG32(RTC_CNTL_INT_CLR) = SLP_WAKEUP_INT | SLP_REJECT_INT;
    return 0;
}

/* rtc_sleep_init(): power-up and wait times of the blocks the sleep state
 * machine switches, 1 slow-clock cycle each
 * (RTC_CNTL_OTHER_BLOCKS_POWERUP/WAIT_CYCLES), and the minimum sleep of 128
 * cycles. */
#define RTC_CNTL_TIMER3          0x3FF48024U
#define RTC_CNTL_TIMER4          0x3FF48028U
#define RTC_CNTL_TIMER5          0x3FF4802CU
static void sleep_timers(void)
{
    const uint32_t blocks = (1U << 25) | (1U << 16) | (1U << 9) | 1U;
    REG32(RTC_CNTL_TIMER3) = blocks;   /* ROM/RAM and Wi-Fi power-up, wait */
    REG32(RTC_CNTL_TIMER4) = blocks;   /* RTC and digital wrapper */
    REG32(RTC_CNTL_TIMER5) = (REG32(RTC_CNTL_TIMER5) & 0xFFU) | (1U << 25) | (1U << 16) | (128U << 8);
}

/* RTC slow memory stays powered and unisolated; RTC fast memory too (it
 * holds nothing of ours, but powering it down needs the wake-stub CRC). */
static void keep_rtc_memory(void)
{
    uint32_t pwc = REG32(RTC_CNTL_PWC);
    pwc &= ~(PWC_SLOWMEM_PD_EN | PWC_FASTMEM_PD_EN);
    pwc |= PWC_SLOWMEM_FORCE_PU | PWC_SLOWMEM_FORCE_NOISO |
           PWC_FASTMEM_FORCE_PU | PWC_FASTMEM_FORCE_NOISO;
    REG32(RTC_CNTL_PWC) = pwc;
}

static int sleep_start(void)
{
    sleep_timers();
    REG32(RTC_CNTL_STATE0) |= SLEEP_EN;
    while ((REG32(RTC_CNTL_INT_RAW) & (SLP_WAKEUP_INT | SLP_REJECT_INT)) == 0U) {
    }
    uint32_t raw = REG32(RTC_CNTL_INT_RAW);
    REG32(RTC_CNTL_INT_CLR) = SLP_WAKEUP_INT | SLP_REJECT_INT;
    return (raw & SLP_REJECT_INT) ? -1 : 0;
}

int eos_esp32_light_sleep_us(uint64_t us)
{
    uint32_t crit = eos_esp32_irq_save();
    uart0_flush();
    if (arm_timer(us) != 0) {
        eos_esp32_irq_restore(crit);
        return -1;
    }
    keep_rtc_memory();
    /* Light sleep: the digital core and the modem keep power. */
    REG32(RTC_CNTL_DIG_PWC) &= ~(DIG_PWC_DG_WRAP_PD_EN | DIG_PWC_WIFI_PD_EN);
    REG32(RTC_CNTL_PWC) &= ~PWC_PD_EN;
    if (EOS_ESP32_CPU_MHZ != 40U) esp32_clock_to_xtal();
    int rc = sleep_start();
    if (EOS_ESP32_CPU_MHZ != 40U) esp32_clock_to_pll(EOS_ESP32_CPU_MHZ);
    eos_esp32_irq_restore(crit);
    return rc;
}

int eos_esp32_deep_sleep_us(uint64_t us)
{
    (void)eos_esp32_irq_save();
    uart0_flush();
    eos_esp32_rtc_slow_hz();                       /* measured before the timer is armed */
    if (arm_timer(us) != 0) return -1;
    keep_rtc_memory();
    REG32(RTC_CNTL_STORE6) = 0;                    /* no wake stub: a normal boot */
    REG32(RTC_CNTL_DIG_ISO) &= ~(DIG_ISO_DG_PAD_FORCE_ISO | DIG_ISO_DG_PAD_FORCE_NOISO);
    uint32_t dig = REG32(RTC_CNTL_DIG_PWC);
    dig |= DIG_PWC_DG_WRAP_PD_EN | DIG_PWC_WIFI_PD_EN;
    dig &= ~(DIG_PWC_DG_WRAP_FORCE_PU | DIG_PWC_DG_WRAP_FORCE_PD);
    REG32(RTC_CNTL_DIG_PWC) = dig;
    REG32(RTC_CNTL_PWC) |= PWC_PD_EN;              /* RTC peripherals off */
    if (EOS_ESP32_CPU_MHZ != 40U) esp32_clock_to_xtal();
    /* Deep sleep ends in a reset; getting here means it was rejected. */
    (void)sleep_start();
    return -1;
}
