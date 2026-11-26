// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_esp32_i2s.c
 * @brief ESP32 I2S0 as the extended HAL's audio device (EOS_ENABLE_AUDIO)
 *
 * Audio id 0 is I2S0: master, Philips I2S framing, 16-bit samples, two slots
 * per frame, 8 kHz to 48 kHz. Transmit and receive run together and share
 * BCK and WS (full duplex, as ESP-IDF sets it up), on the pins
 * EOS_BOARD_I2S_{BCK,WS,DOUT,DIN}_GPIO. eos_audio_play() takes interleaved
 * left/right samples for 2 channels, or one sample per frame for 1 channel
 * (sent in both slots); eos_audio_record() returns the same layout. Volume
 * and mute scale the samples on the way out.
 *
 * When its FIFO runs dry the transmitter repeats the last word, so
 * eos_audio_play() ends every buffer with a zero frame: between buffers the
 * line idles at 0, not at the last sample. The receiver runs all the time,
 * and with no DMA nothing empties its 64-word FIFO between calls: capture is
 * continuous only while eos_audio_record() runs, and its first samples can
 * be up to 64 words that arrived before the call. eos_audio_play() of more
 * than 64 words waits for the transmit FIFO to drain, and the receiver drops
 * whatever arrives meanwhile: play and record do not overlap.
 *
 * The CPU feeds the 64-word FIFOs directly (DSCR_EN clear, no DMA), polled,
 * one 16-bit sample per FIFO word. MCLK comes from PLL_D2 (160 MHz) through
 * the fractional divider at 256 x fs and BCK is MCLK / 8 = 32 x fs, as
 * ESP-IDF configures 16-bit stereo. Register layout from ESP-IDF v5.4
 * soc/esp32/register/soc/i2s_reg.h; nothing from ESP-IDF is linked.
 */

#include "eos/hal_extended.h"
#include "board_esp32.h"
#include "hal_esp32_internal.h"

#if EOS_ENABLE_AUDIO

#define REG32 ESP32_REG32

#define I2S0                 0x3FF4F000U
#define I2S_FIFO_WR          (0x6000F000U)          /* AHB alias, as the UART's */
#define I2S_FIFO_RD          (I2S0 + 0x04U)
#define I2S_CONF             (I2S0 + 0x08U)
#define I2S_INT_RAW          (I2S0 + 0x0CU)
#define I2S_INT_CLR          (I2S0 + 0x18U)
#define I2S_FIFO_CONF        (I2S0 + 0x20U)
#define I2S_CONF_CHAN        (I2S0 + 0x2CU)
#define I2S_CONF1            (I2S0 + 0xA0U)
#define I2S_PD_CONF          (I2S0 + 0xA4U)
#define I2S_CONF2            (I2S0 + 0xA8U)
#define I2S_CLKM_CONF        (I2S0 + 0xACU)
#define I2S_SAMPLE_RATE_CONF (I2S0 + 0xB0U)
#define I2S_STATE            (I2S0 + 0xBCU)

#define CONF_TX_RESET        (1U << 0)
#define CONF_RX_RESET        (1U << 1)
#define CONF_TX_FIFO_RESET   (1U << 2)
#define CONF_RX_FIFO_RESET   (1U << 3)
#define CONF_TX_START        (1U << 4)
#define CONF_RX_START        (1U << 5)
#define CONF_RX_SLAVE_MOD    (1U << 7)
#define CONF_TX_MSB_SHIFT    (1U << 10)
#define CONF_RX_MSB_SHIFT    (1U << 11)
#define CONF_SIG_LOOPBACK    (1U << 18)   /* RX uses TX's BCK and WS */
#define INT_RX_TAKE_DATA     (1U << 0)
#define INT_TX_PUT_DATA      (1U << 1)
#define FIFO_DATA_NUM        32U          /* both thresholds, in words */
#define FIFO_CONF_FORCE      ((1U << 19) | (1U << 20))
#define CONF1_PCM_BYPASS     ((1U << 3) | (1U << 7))
#define PD_FIFO_FORCE_PU     (1U << 1)
#define CLK_EN               (1U << 20)
#define DPORT_I2S0_CLK_EN    (1U << 4)

/* gpio_sig_map.h */
#define I2S0O_BCK_OUT_IDX    23U
#define I2S0O_WS_OUT_IDX     25U
#define I2S0O_DATA_OUT23_IDX 163U         /* serial data out (bit 23 of the parallel bus) */
#define I2S0I_DATA_IN15_IDX  155U         /* serial data in */

#define I2S_PLL_D2_HZ        160000000U
#define I2S_MIN_RATE         8000U
#define I2S_MAX_RATE         48000U

static bool s_ready;
static uint32_t s_rate, s_channels;
static uint8_t s_volume = 100;
static bool s_mute;

/*
 * MCLK = 160 MHz / (N + b/a), N 2..255, 0 <= b < a <= 63. Best b/a for the
 * fraction left over, by exhaustive search (at most 63*63/2 steps).
 */
static void clkm(uint32_t mclk, uint32_t *n, uint32_t *b, uint32_t *a)
{
    *n = I2S_PLL_D2_HZ / mclk;
    uint32_t rem = I2S_PLL_D2_HZ % mclk;     /* fraction = rem / mclk */
    *b = 0;
    *a = 1;
    uint64_t best = UINT64_MAX;
    for (uint32_t aa = 1; aa <= 63U && rem != 0U; aa++) {
        uint32_t bb = (uint32_t)(((uint64_t)rem * aa + mclk / 2U) / mclk);
        if (bb >= aa) continue;
        /* |rem/mclk - bb/aa|, scaled by mclk*aa */
        int64_t d = (int64_t)rem * aa - (int64_t)bb * mclk;
        uint64_t err = (uint64_t)(d < 0 ? -d : d) * 64U / aa;
        if (err < best) {
            best = err;
            *b = bb;
            *a = aa;
        }
    }
}

uint32_t eos_esp32_i2s_get_rate(void)
{
    if (!s_ready) return 0;
    uint32_t v = REG32(I2S_CLKM_CONF);
    uint32_t n = v & 0xFFU, b = (v >> 8) & 0x3FU, a = (v >> 14) & 0x3FU;
    if (a == 0U) a = 1U;
    /* fs = 160 MHz / ((n + b/a) * 256) */
    return (uint32_t)(((uint64_t)I2S_PLL_D2_HZ * a) / (((uint64_t)n * a + b) * 256U));
}

int eos_audio_init(const eos_audio_config_t *cfg)
{
    if (cfg == NULL || cfg->id != 0U) return -1;
    if (cfg->format != EOS_AUDIO_FMT_PCM_16) return -1;
    if (cfg->channels != 1U && cfg->channels != 2U) return -1;
    if (cfg->sample_rate < I2S_MIN_RATE || cfg->sample_rate > I2S_MAX_RATE) return -1;
    s_ready = false;

    esp32_periph_enable(DPORT_I2S0_CLK_EN);
    REG32(I2S_CONF) = CONF_TX_RESET | CONF_RX_RESET | CONF_TX_FIFO_RESET | CONF_RX_FIFO_RESET;
    REG32(I2S_CONF) = 0;
    REG32(I2S_CONF2) = 0;                                  /* not LCD or camera */
    REG32(I2S_CONF1) = CONF1_PCM_BYPASS;
    REG32(I2S_PD_CONF) = PD_FIFO_FORCE_PU;
    /* CPU FIFO access, 16-bit dual-channel words, thresholds of 32. */
    REG32(I2S_FIFO_CONF) = FIFO_CONF_FORCE | (FIFO_DATA_NUM << 6) | FIFO_DATA_NUM;
    REG32(I2S_CONF_CHAN) = 0;                              /* dual channel both ways */

    uint32_t n, b, a;
    clkm(cfg->sample_rate * 256U, &n, &b, &a);
    REG32(I2S_CLKM_CONF) = CLK_EN | (a << 14) | (b << 8) | n;
    /* BCK = MCLK / 8; 16 bits per slot each way. */
    REG32(I2S_SAMPLE_RATE_CONF) = (16U << 18) | (16U << 12) | (8U << 6) | 8U;

    /* Philips framing (MSB one BCK after WS), TX master, RX slave on TX's
     * BCK and WS. */
    REG32(I2S_CONF) = CONF_TX_MSB_SHIFT | CONF_RX_MSB_SHIFT | CONF_RX_SLAVE_MOD | CONF_SIG_LOOPBACK;

    if (esp32_pin_route_output(EOS_BOARD_I2S_BCK_GPIO, I2S0O_BCK_OUT_IDX) != 0 ||
        esp32_pin_route_output(EOS_BOARD_I2S_WS_GPIO, I2S0O_WS_OUT_IDX) != 0 ||
        esp32_pin_route_output(EOS_BOARD_I2S_DOUT_GPIO, I2S0O_DATA_OUT23_IDX) != 0 ||
        esp32_pin_route_input(EOS_BOARD_I2S_DIN_GPIO, I2S0I_DATA_IN15_IDX) != 0) {
        return -1;
    }
    s_rate = cfg->sample_rate;
    s_channels = cfg->channels;
    s_ready = true;
    REG32(I2S_CONF) |= CONF_TX_START | CONF_RX_START;
    return 0;
}

void eos_audio_deinit(uint8_t id)
{
    if (id != 0U || !s_ready) return;
    REG32(I2S_CONF) &= ~(CONF_TX_START | CONF_RX_START);
    s_ready = false;
}

int eos_esp32_i2s_set_loopback(bool enable)
{
    if (!s_ready) return -1;
    return esp32_pin_route_input(enable ? EOS_BOARD_I2S_DOUT_GPIO : EOS_BOARD_I2S_DIN_GPIO,
                                 I2S0I_DATA_IN15_IDX);
}

static int16_t scale(int16_t v)
{
    if (s_mute) return 0;
    return (int16_t)((int32_t)v * s_volume / 100);
}

/* Time for @p words FIFO words at the current rate, doubled, plus 10 ms. */
static uint32_t i2s_timeout_cycles(size_t words)
{
    uint64_t us = (uint64_t)words * 1000000U / (2U * s_rate) * 2U + 10000U;
    return us * (ESP32_CPU_HZ / 1000000U) > UINT32_MAX ? UINT32_MAX
                                                       : (uint32_t)(us * (ESP32_CPU_HZ / 1000000U));
}

int eos_audio_play(uint8_t id, const uint8_t *data, size_t len)
{
    if (id != 0U || !s_ready || data == NULL || (len & 1U) != 0U || len > INT32_MAX) return -1;
    size_t samples = len / 2U;
    size_t words = samples * (s_channels == 1U ? 2U : 1U);
    size_t total = words + 2U;                 /* + the closing zero frame */
    uint32_t limit = i2s_timeout_cycles(total);
    uint32_t t0 = eos_esp32_ccount();
    size_t w = 0;
    while (w < total) {
        /* TX_PUT_DATA: fewer than FIFO_DATA_NUM words queued, so at least
         * 64 - 32 = 32 fit. */
        REG32(I2S_INT_CLR) = INT_TX_PUT_DATA;
        if ((REG32(I2S_INT_RAW) & INT_TX_PUT_DATA) == 0U) {
            if (eos_esp32_ccount() - t0 > limit) return -1;
            continue;
        }
        for (uint32_t k = 0; k < FIFO_DATA_NUM && w < total; k++, w++) {
            if (w >= words) {
                REG32(I2S_FIFO_WR) = 0;
                continue;
            }
            size_t i = s_channels == 1U ? w / 2U : w;
            int16_t v = (int16_t)(data[2U * i] | (data[2U * i + 1U] << 8));
            REG32(I2S_FIFO_WR) = (uint16_t)scale(v);
        }
    }
    return (int)len;
}

int eos_audio_record(uint8_t id, uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    if (id != 0U || !s_ready || buf == NULL || (len & 1U) != 0U || len > INT32_MAX) return -1;
    size_t samples = len / 2U;
    size_t words = samples * (s_channels == 1U ? 2U : 1U);
    uint32_t per_ms = ESP32_CPU_HZ / 1000U, elapsed_ms = 0, acc = 0, last = eos_esp32_ccount();
    size_t w = 0;
    while (w < words) {
        REG32(I2S_INT_CLR) = INT_RX_TAKE_DATA;
        if ((REG32(I2S_INT_RAW) & INT_RX_TAKE_DATA) == 0U) {
            uint32_t now = eos_esp32_ccount();
            acc += now - last;
            last = now;
            while (acc >= per_ms) { acc -= per_ms; elapsed_ms++; }
            if (elapsed_ms >= timeout_ms) break;
            continue;
        }
        for (uint32_t k = 0; k < FIFO_DATA_NUM && w < words; k++, w++) {
            /* Erratum CPU-3.21: I2S0's FIFO read register (0x3FF4F004) is
             * one of the five, so this read must not be interrupted. The
             * surrounding drain loop stays outside the mask. */
            uint16_t v = (uint16_t)eos_esp32_fifo_read(I2S_FIFO_RD);
            /* Mono keeps the left slot of each frame. */
            if (s_channels == 1U && (w & 1U) != 0U) continue;
            size_t i = s_channels == 1U ? w / 2U : w;
            buf[2U * i] = (uint8_t)v;
            buf[2U * i + 1U] = (uint8_t)(v >> 8);
        }
    }
    size_t got = s_channels == 1U ? w / 2U : w;
    return (int)(got * 2U);
}

int eos_audio_set_volume(uint8_t id, uint8_t volume_pct)
{
    if (id != 0U || volume_pct > 100U) return -1;
    s_volume = volume_pct;
    return 0;
}

int eos_audio_mute(uint8_t id, bool mute)
{
    if (id != 0U) return -1;
    s_mute = mute;
    return 0;
}

#endif /* EOS_ENABLE_AUDIO */
