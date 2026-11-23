// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_esp32_analog.c
 * @brief ESP32 ADC, DAC and PWM (LEDC) for the extended HAL
 *
 * Channel numbering on the ESP32, which the portable API leaves to the port:
 *
 *   ADC  channel 0-7   = ADC1 channel 0-7  (GPIO36, 37, 38, 39, 32, 33, 34, 35)
 *        channel 10-19 = ADC2 channel 0-9  (GPIO4, 0, 2, 15, 13, 12, 14, 27, 25, 26)
 *   DAC  channel 0, 1  = GPIO25, GPIO26 (8-bit)
 *   PWM  channel 0-3   = LEDC high-speed channel n on high-speed timer n, on
 *                        the pin EOS_BOARD_PWM_GPIOS[n] names (board_esp32.h)
 *
 * eos_adc_read() and eos_adc_read_mv() return 0xFFFFFFFF (no 9-12 bit
 * result can) when a conversion does not complete within 1 ms, and 0 for a
 * channel that was never initialised, as the other ports do.
 *
 * The register sequences follow ESP-IDF v5.4 (hal/esp32/include/hal/adc_ll.h,
 * dac_ll.h, ledc_ll.h, sar_ctrl_ll.h, hal/adc_oneshot_hal.c); addresses and
 * bit positions are from components/soc/esp32/register/soc. Nothing from
 * ESP-IDF is linked.
 */

#include <eos/hal_extended.h>
#include "board_esp32.h"
#include "hal_esp32_internal.h"

#define REG32 ESP32_REG32

/* ---- RTC pads -------------------------------------------------------------
 * The ADC and DAC reach a pin through its RTC pad: the pad's MUX_SEL bit
 * hands it from the digital IO_MUX to the RTC block. Values are those of
 * ESP-IDF's rtc_io_desc[] (soc/esp32/rtc_io_periph.c), resolved from the
 * register headers.
 */
typedef struct {
    uint8_t  gpio;
    uint32_t reg, mux, fun_s, ie, rue, rde;
} rtc_pad_t;

static const rtc_pad_t s_rtc_pads[] = {
    { 36, 0x3FF4847CU, 0x08000000U, 22, 0x00080000U, 0x00000000U, 0x00000000U },
    { 37, 0x3FF4847CU, 0x04000000U, 17, 0x00004000U, 0x00000000U, 0x00000000U },
    { 38, 0x3FF4847CU, 0x02000000U, 12, 0x00000200U, 0x00000000U, 0x00000000U },
    { 39, 0x3FF4847CU, 0x01000000U,  7, 0x00000010U, 0x00000000U, 0x00000000U },
    { 34, 0x3FF48480U, 0x20000000U, 26, 0x00800000U, 0x00000000U, 0x00000000U },
    { 35, 0x3FF48480U, 0x10000000U, 21, 0x00040000U, 0x00000000U, 0x00000000U },
    { 25, 0x3FF48484U, 0x00020000U, 15, 0x00000800U, 0x08000000U, 0x10000000U },
    { 26, 0x3FF48488U, 0x00020000U, 15, 0x00000800U, 0x08000000U, 0x10000000U },
    { 33, 0x3FF4848CU, 0x00040000U, 15, 0x00000800U, 0x08000000U, 0x10000000U },
    { 32, 0x3FF4848CU, 0x00020000U,  9, 0x00000020U, 0x00400000U, 0x00800000U },
    {  4, 0x3FF48494U, 0x00080000U, 17, 0x00002000U, 0x08000000U, 0x10000000U },
    {  0, 0x3FF48498U, 0x00080000U, 17, 0x00002000U, 0x08000000U, 0x10000000U },
    {  2, 0x3FF4849CU, 0x00080000U, 17, 0x00002000U, 0x08000000U, 0x10000000U },
    { 15, 0x3FF484A0U, 0x00080000U, 17, 0x00002000U, 0x08000000U, 0x10000000U },
    { 13, 0x3FF484A4U, 0x00080000U, 17, 0x00002000U, 0x08000000U, 0x10000000U },
    { 12, 0x3FF484A8U, 0x00080000U, 17, 0x00002000U, 0x08000000U, 0x10000000U },
    { 14, 0x3FF484ACU, 0x00080000U, 17, 0x00002000U, 0x08000000U, 0x10000000U },
    { 27, 0x3FF484B0U, 0x00080000U, 17, 0x00002000U, 0x08000000U, 0x10000000U },
};

static const rtc_pad_t *rtc_pad(uint16_t gpio)
{
    for (size_t i = 0; i < sizeof(s_rtc_pads) / sizeof(s_rtc_pads[0]); i++) {
        if (s_rtc_pads[i].gpio == gpio) return &s_rtc_pads[i];
    }
    return NULL;
}

/* Give the pad to the RTC block for analog use: RTC function 0, no digital
 * input, no pulls. */
static int rtc_pad_analog(uint16_t gpio)
{
    const rtc_pad_t *p = rtc_pad(gpio);
    if (p == NULL) return -1;
    uint32_t v = REG32(p->reg);
    v &= ~((3U << p->fun_s) | p->ie | p->rue | p->rde);
    v |= p->mux;
    REG32(p->reg) = v;
    return 0;
}

void esp32_rtc_pad_release(uint16_t gpio)
{
    const rtc_pad_t *p = rtc_pad(gpio);
    if (p != NULL) REG32(p->reg) &= ~p->mux;
}

/* ---- ADC ------------------------------------------------------------------ */
#if EOS_ENABLE_ADC

#define SENS_SAR_READ_CTRL    0x3FF48800U
#define SENS_SAR_MEAS_WAIT2   0x3FF4880CU
#define SENS_SAR_MEAS_CTRL    0x3FF48810U
#define SENS_SAR_START_FORCE  0x3FF4882CU
#define SENS_SAR_ATTEN1       0x3FF48834U
#define SENS_SAR_ATTEN2       0x3FF48838U
#define SENS_SAR_SLAVE_ADDR1  0x3FF4883CU
#define SENS_SAR_MEAS_START1  0x3FF48854U
#define SENS_SAR_TOUCH_CTRL1  0x3FF48858U
#define SENS_SAR_READ_CTRL2   0x3FF48890U
#define SENS_SAR_MEAS_START2  0x3FF48894U
#define SENS_SAR_MEAS_CTRL2   0x3FF488A0U
#define SYSCON_SARADC_CTRL    0x3FF66010U
#define RTC_IO_HALL_SENS      0x3FF48478U

#define MEAS_DATA_M     0xFFFFU
#define MEAS_DONE       (1U << 16)
#define MEAS_START      (1U << 17)
#define MEAS_START_FORCE (1U << 18)
#define EN_PAD_S        19
#define EN_PAD_M        (0xFFFU << EN_PAD_S)
#define EN_PAD_FORCE    (1U << 31)

/*
 * 12 dB attenuation: the widest input range (about 150-2450 mV is linear;
 * readings above that follow ESP-IDF's lookup table). eos_adc_read_mv()
 * applies the chip's eFuse calibration (hal_esp32_adc_cal.c).
 */
#define ADC_ATTEN_12DB  3U

static const uint8_t s_adc1_gpio[8]  = { 36, 37, 38, 39, 32, 33, 34, 35 };
static const uint8_t s_adc2_gpio[10] = { 4, 0, 2, 15, 13, 12, 14, 27, 25, 26 };
static uint8_t s_adc_bits[20];   /* 0: channel not initialised */

static int adc_split(uint8_t channel, int *unit, int *ch, uint16_t *gpio)
{
    if (channel < 8U) { *unit = 1; *ch = channel; *gpio = s_adc1_gpio[channel]; return 0; }
    if (channel >= 10U && channel < 20U) {
        *unit = 2; *ch = channel - 10; *gpio = s_adc2_gpio[*ch]; return 0;
    }
    return -1;
}

int eos_adc_init(const eos_adc_config_t *cfg)
{
    int unit, ch;
    uint16_t gpio;
    if (cfg == NULL || adc_split(cfg->channel, &unit, &ch, &gpio) != 0) return -1;
    /* The SAR ADC resolves 9 to 12 bits. sample_rate_hz is not used: every
     * eos_adc_read() is one conversion started by software. */
    if (cfg->resolution_bits < 9U || cfg->resolution_bits > 12U) return -1;
    if (rtc_pad_analog(gpio) != 0) return -1;
    s_adc_bits[cfg->channel] = cfg->resolution_bits;
    return 0;
}

void eos_adc_deinit(uint8_t channel)
{
    int unit, ch;
    uint16_t gpio;
    if (adc_split(channel, &unit, &ch, &gpio) != 0) return;
    s_adc_bits[channel] = 0;
    esp32_rtc_pad_release(gpio);
}

/*
 * A conversion takes tens of microseconds; one that has not finished after
 * 1 ms will not finish. Every wait is bounded so a failed ADC (or an emulator
 * that does not model it) returns EOS_ADC_ESP32_ERROR instead of hanging.
 * eos_adc_read() returns uint32_t with no error channel, and no 9-12 bit
 * reading can equal UINT32_MAX.
 */
#define EOS_ADC_ESP32_ERROR 0xFFFFFFFFU
#define ADC_TIMEOUT_CYCLES  (ESP32_APB_HZ / 1000U)

static bool wait_set(uint32_t reg, uint32_t mask)
{
    uint32_t t0 = eos_esp32_ccount();
    while ((REG32(reg) & mask) == 0U) {
        if (eos_esp32_ccount() - t0 > ADC_TIMEOUT_CYCLES) return false;
    }
    return true;
}

static void field(uint32_t reg, uint32_t shift, uint32_t mask, uint32_t value)
{
    REG32(reg) = (REG32(reg) & ~(mask << shift)) | ((value & mask) << shift);
}

uint32_t eos_adc_read(uint8_t channel)
{
    int unit, ch;
    uint16_t gpio;
    if (adc_split(channel, &unit, &ch, &gpio) != 0 || s_adc_bits[channel] == 0U) return 0;
    uint32_t bits = s_adc_bits[channel] - 9U;   /* 0..3 = 9..12 bits */

    field(SENS_SAR_MEAS_WAIT2, 18, 3, 3);        /* SAR powered on (sar_ctrl_ll) */
    REG32(RTC_IO_HALL_SENS) &= ~(1U << 31);      /* hall sensor off */
    field(SENS_SAR_MEAS_WAIT2, 16, 3, 2);        /* LNA amplifier powered down */
    REG32(SENS_SAR_MEAS_CTRL) &= ~((0xFU << 4) | (0xFU << 8) | (0xFU << 12));

    if (unit == 1) {
        /* Stop ADC1 sampling from toggling the DAC (dac_ll_rtc_sync_by_adc). */
        field(SENS_SAR_MEAS_CTRL2, 0, 0xF, 0);
        field(SENS_SAR_READ_CTRL, 0, 0xFF, 1);   /* clock divider */
        REG32(SENS_SAR_READ_CTRL) |= (1U << 28); /* data invert, as the RTC path needs */
        field(SENS_SAR_ATTEN1, (uint32_t)ch * 2U, 3, ADC_ATTEN_12DB);
        field(SENS_SAR_START_FORCE, 0, 3, bits);
        field(SENS_SAR_READ_CTRL, 16, 3, bits);
        /* RTC controller, software start and channel select */
        REG32(SENS_SAR_READ_CTRL) &= ~(1U << 27);
        REG32(SENS_SAR_MEAS_START1) |= MEAS_START_FORCE | EN_PAD_FORCE;
        REG32(SENS_SAR_TOUCH_CTRL1) |= (1U << 26) | (1U << 27);
        REG32(SENS_SAR_MEAS_START1) = (REG32(SENS_SAR_MEAS_START1) & ~EN_PAD_M) |
                                      ((1U << ch) << EN_PAD_S);
        uint32_t t0 = eos_esp32_ccount();
        while (((REG32(SENS_SAR_SLAVE_ADDR1) >> 22) & 0xFFU) != 0U) {
            if (eos_esp32_ccount() - t0 > ADC_TIMEOUT_CYCLES) return EOS_ADC_ESP32_ERROR;
        }
        REG32(SENS_SAR_MEAS_START1) &= ~MEAS_START;
        REG32(SENS_SAR_MEAS_START1) |= MEAS_START;
        if (!wait_set(SENS_SAR_MEAS_START1, MEAS_DONE)) return EOS_ADC_ESP32_ERROR;
        return REG32(SENS_SAR_MEAS_START1) & MEAS_DATA_M;
    }

    field(SENS_SAR_READ_CTRL2, 0, 0xFF, 1);
    field(SENS_SAR_START_FORCE, 5, 7, 4);        /* PA power monitor cap, as the PHY sets */
    REG32(SENS_SAR_READ_CTRL2) |= (1U << 29);
    field(SENS_SAR_ATTEN2, (uint32_t)ch * 2U, 3, ADC_ATTEN_12DB);
    field(SENS_SAR_START_FORCE, 2, 3, bits);
    field(SENS_SAR_READ_CTRL2, 16, 3, bits);
    REG32(SENS_SAR_MEAS_START2) |= MEAS_START_FORCE | EN_PAD_FORCE;
    REG32(SENS_SAR_READ_CTRL2) &= ~((1U << 28) | (1U << 27));   /* RTC, not digital/PWDET */
    REG32(SYSCON_SARADC_CTRL) |= (1U << 2);
    REG32(SENS_SAR_MEAS_START2) = (REG32(SENS_SAR_MEAS_START2) & ~EN_PAD_M) |
                                  ((1U << ch) << EN_PAD_S);
    REG32(SENS_SAR_MEAS_START2) &= ~MEAS_START;
    REG32(SENS_SAR_MEAS_START2) |= MEAS_START;
    if (!wait_set(SENS_SAR_MEAS_START2, MEAS_DONE)) return EOS_ADC_ESP32_ERROR;
    return REG32(SENS_SAR_MEAS_START2) & MEAS_DATA_M;
}

uint32_t eos_adc_read_mv(uint8_t channel)
{
    if (channel >= sizeof(s_adc_bits) || s_adc_bits[channel] == 0U) return 0;
    uint32_t raw = eos_adc_read(channel);
    if (raw == EOS_ADC_ESP32_ERROR) return EOS_ADC_ESP32_ERROR;
    return esp32_adc_raw_to_mv(channel < 8U ? 1 : 2, raw, s_adc_bits[channel]);
}

#endif /* EOS_ENABLE_ADC */

/* ---- DAC ------------------------------------------------------------------ */
#if EOS_ENABLE_DAC

#define SENS_SAR_DAC_CTRL2  0x3FF4889CU
#define DAC_CW_EN(ch)       ((ch) == 0U ? (1U << 24) : (1U << 25))
#define PDAC_REG(ch)        ((ch) == 0U ? 0x3FF48484U : 0x3FF48488U)
#define PDAC_VALUE_S        19
#define PDAC_XPD            (1U << 18)
#define PDAC_XPD_FORCE      (1U << 10)

static uint32_t s_dac_vref_mv[2];

int eos_dac_init(const eos_dac_config_t *cfg)
{
    /* The ESP32 DAC is 8 bits; 0 asks for the default. */
    if (cfg == NULL || cfg->channel > 1U) return -1;
    if (cfg->resolution_bits != 0U && cfg->resolution_bits != 8U) return -1;
    if (rtc_pad_analog(cfg->channel == 0U ? 25U : 26U) != 0) return -1;
    s_dac_vref_mv[cfg->channel] = cfg->vref_mv != 0U ? cfg->vref_mv : 3300U;
    REG32(SENS_SAR_DAC_CTRL2) &= ~DAC_CW_EN(cfg->channel);   /* no cosine generator */
    uint32_t reg = PDAC_REG(cfg->channel);
    REG32(reg) = (REG32(reg) & ~(0xFFU << PDAC_VALUE_S)) | PDAC_XPD | PDAC_XPD_FORCE;
    return 0;
}

void eos_dac_deinit(uint8_t channel)
{
    if (channel > 1U || s_dac_vref_mv[channel] == 0U) return;
    REG32(PDAC_REG(channel)) &= ~(PDAC_XPD | PDAC_XPD_FORCE);
    esp32_rtc_pad_release(channel == 0U ? 25U : 26U);
    s_dac_vref_mv[channel] = 0;
}

int eos_dac_write(uint8_t channel, uint32_t value)
{
    if (channel > 1U || s_dac_vref_mv[channel] == 0U || value > 255U) return -1;
    uint32_t reg = PDAC_REG(channel);
    REG32(reg) = (REG32(reg) & ~(0xFFU << PDAC_VALUE_S)) | (value << PDAC_VALUE_S);
    return 0;
}

int eos_dac_write_mv(uint8_t channel, uint32_t millivolts)
{
    if (channel > 1U || s_dac_vref_mv[channel] == 0U) return -1;
    if (millivolts > s_dac_vref_mv[channel]) return -1;
    uint32_t vref = s_dac_vref_mv[channel];
    return eos_dac_write(channel, (millivolts * 255U + vref / 2U) / vref);
}

#endif /* EOS_ENABLE_DAC */

/* ---- PWM (LEDC high-speed) ------------------------------------------------- */
#if EOS_ENABLE_PWM

#define LEDC_BASE             0x3FF59000U
#define LEDC_CH_CONF0(n)      (LEDC_BASE + 0x00U + 0x14U * (n))
#define LEDC_CH_HPOINT(n)     (LEDC_BASE + 0x04U + 0x14U * (n))
#define LEDC_CH_DUTY(n)       (LEDC_BASE + 0x08U + 0x14U * (n))
#define LEDC_CH_CONF1(n)      (LEDC_BASE + 0x0CU + 0x14U * (n))
#define LEDC_TIMER_CONF(n)    (LEDC_BASE + 0x140U + 0x08U * (n))
#define LEDC_SIG_OUT_EN       (1U << 2)
#define LEDC_DUTY_START       (1U << 31)
#define LEDC_DUTY_INC         (1U << 30)
#define LEDC_DUTY_NUM_S       20
#define LEDC_DUTY_CYCLE_S     10
#define LEDC_TICK_APB         (1U << 25)
#define LEDC_TIMER_RST        (1U << 24)
#define LEDC_DIV_S            5
#define LEDC_DIV_MAX          0x3FFFFU
#define DPORT_LEDC_CLK_EN     (1U << 11)
#define SIG_LEDC_HS_OUT0      71U
#define PWM_CHANNELS          4U

static const uint8_t s_pwm_gpio[8] = EOS_BOARD_PWM_GPIOS;

typedef struct {
    bool     ready;
    uint32_t freq_hz;
    uint16_t duty_x10;
    uint32_t res_bits;
} pwm_state_t;

static pwm_state_t s_pwm[PWM_CHANNELS];

/*
 * Pick the duty resolution and divider for @p freq. The LEDC counts a tick
 * of APB / div (div in 10.8 fixed point, at least 1.0) for 2^res ticks per
 * period. The finest resolution up to 20 bits whose tick fits is chosen,
 * with freq << res held to 2^24 so the divider is computed in 32 bits (no
 * libgcc in this image).
 */
static int pwm_timing(uint32_t freq, uint32_t *res_out, uint32_t *div_out)
{
    if (freq == 0U || freq > ESP32_APB_HZ / 4U) return -1;
    uint32_t res = 0;
    while (res < 20U && (freq << (res + 1U)) <= (1U << 24) &&
           (freq << (res + 1U)) <= ESP32_APB_HZ) {
        res++;
    }
    if (res == 0U) return -1;
    uint32_t f_r = freq << res;
    uint32_t q = ESP32_APB_HZ / f_r, rem = ESP32_APB_HZ % f_r;
    uint32_t div = (q << 8) | ((rem << 8) / f_r);   /* rem < f_r <= 2^24 */
    if (div < 256U || div > LEDC_DIV_MAX) return -1;
    *res_out = res;
    *div_out = div;
    return 0;
}

static void pwm_apply_duty(uint8_t ch)
{
    uint32_t duty = (uint32_t)s_pwm[ch].duty_x10 * (1U << s_pwm[ch].res_bits) / 1000U;
    REG32(LEDC_CH_HPOINT(ch)) = 0;
    REG32(LEDC_CH_DUTY(ch)) = duty << 4;          /* 4 fractional bits */
    REG32(LEDC_CH_CONF1(ch)) = LEDC_DUTY_START | LEDC_DUTY_INC |
                               (1U << LEDC_DUTY_NUM_S) | (1U << LEDC_DUTY_CYCLE_S);
}

static int pwm_apply_timer(uint8_t ch)
{
    uint32_t res, div;
    if (pwm_timing(s_pwm[ch].freq_hz, &res, &div) != 0) return -1;
    s_pwm[ch].res_bits = res;
    uint32_t conf = res | (div << LEDC_DIV_S) | LEDC_TICK_APB;
    REG32(LEDC_TIMER_CONF(ch)) = conf | LEDC_TIMER_RST;
    REG32(LEDC_TIMER_CONF(ch)) = conf;
    return 0;
}

int eos_pwm_init(const eos_pwm_config_t *cfg)
{
    if (cfg == NULL || cfg->channel >= PWM_CHANNELS || cfg->duty_pct_x10 > 1000U) return -1;
    uint8_t ch = cfg->channel;
    uint32_t res, div;
    if (pwm_timing(cfg->frequency_hz, &res, &div) != 0) return -1;
    if (!esp32_pin_usable(s_pwm_gpio[ch])) return -1;

    esp32_periph_enable(DPORT_LEDC_CLK_EN);
    s_pwm[ch].freq_hz = cfg->frequency_hz;
    s_pwm[ch].duty_x10 = cfg->duty_pct_x10;
    if (pwm_apply_timer(ch) != 0) return -1;
    pwm_apply_duty(ch);
    REG32(LEDC_CH_CONF0(ch)) = ch | LEDC_SIG_OUT_EN;   /* timer n, output on */
    if (esp32_pin_route_output(s_pwm_gpio[ch], SIG_LEDC_HS_OUT0 + ch) != 0) return -1;
    s_pwm[ch].ready = true;
    return 0;
}

void eos_pwm_deinit(uint8_t channel)
{
    if (channel >= PWM_CHANNELS || !s_pwm[channel].ready) return;
    REG32(LEDC_CH_CONF0(channel)) &= ~LEDC_SIG_OUT_EN;   /* idle level 0 */
    s_pwm[channel].ready = false;
}

int eos_pwm_set_duty(uint8_t channel, uint16_t duty_pct_x10)
{
    if (channel >= PWM_CHANNELS || !s_pwm[channel].ready || duty_pct_x10 > 1000U) return -1;
    s_pwm[channel].duty_x10 = duty_pct_x10;
    pwm_apply_duty(channel);
    return 0;
}

int eos_pwm_set_freq(uint8_t channel, uint32_t frequency_hz)
{
    if (channel >= PWM_CHANNELS || !s_pwm[channel].ready) return -1;
    uint32_t res, div;
    if (pwm_timing(frequency_hz, &res, &div) != 0) return -1;
    s_pwm[channel].freq_hz = frequency_hz;
    if (pwm_apply_timer(channel) != 0) return -1;
    pwm_apply_duty(channel);   /* the duty in ticks depends on the resolution */
    return 0;
}

int eos_pwm_start(uint8_t channel)
{
    if (channel >= PWM_CHANNELS || !s_pwm[channel].ready) return -1;
    REG32(LEDC_CH_CONF0(channel)) |= LEDC_SIG_OUT_EN;
    return 0;
}

int eos_pwm_stop(uint8_t channel)
{
    if (channel >= PWM_CHANNELS || !s_pwm[channel].ready) return -1;
    REG32(LEDC_CH_CONF0(channel)) &= ~LEDC_SIG_OUT_EN;
    return 0;
}

#endif /* EOS_ENABLE_PWM */
