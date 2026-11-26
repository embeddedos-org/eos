// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_esp32_i2c.c
 * @brief ESP32 I2C (I2C0 and I2C1) for the HAL backend: master, plus a
 *        polled slave for board-level use and self-tests
 *
 * Ports 0 and 1; pins EOS_BOARD_I2Cn_SDA/SCL_GPIO (board_esp32.h), open
 * drain with the pads' internal pull-ups (about 45 kOhm: enough for a short
 * bus at 100 kHz, add external pull-ups for anything longer or faster).
 * 7-bit addresses, 2.5 kHz to 1 MHz, polled.
 *
 * The master runs ESP32 command lists: RESTART, WRITE (address and data,
 * ACK checked), READ (ACK on all but the last byte), STOP. The FIFO holds 32
 * bytes, so a longer transfer pauses on an END command, refills or empties
 * the FIFO, and continues within the same bus transaction. A NACK, lost
 * arbitration or a timeout returns -1 and resets the controller, as ESP-IDF
 * does: the ESP32's I2C state machine can hang after one.
 *
 * Register sequences follow ESP-IDF v5.4 hal/esp32/include/hal/i2c_ll.h and
 * hal/i2c_hal.c; addresses and bit positions are from
 * soc/esp32/register/soc/i2c_reg.h. Nothing from ESP-IDF is linked.
 */

#include "eos/hal.h"
#include "board_esp32.h"
#include "hal_esp32_internal.h"

#define REG32 ESP32_REG32

#define I2C_BASE(p)          ((p) == 0U ? 0x3FF53000U : 0x3FF67000U)
#define I2C_SCL_LOW(p)       (I2C_BASE(p) + 0x00U)
#define I2C_CTR(p)           (I2C_BASE(p) + 0x04U)
#define I2C_SR(p)            (I2C_BASE(p) + 0x08U)
#define I2C_TO(p)            (I2C_BASE(p) + 0x0CU)
#define I2C_SLAVE_ADDR(p)    (I2C_BASE(p) + 0x10U)
#define I2C_FIFO_CONF(p)     (I2C_BASE(p) + 0x18U)
#define I2C_FIFO_DATA(p)     (I2C_BASE(p) + 0x1CU)
#define I2C_INT_RAW(p)       (I2C_BASE(p) + 0x20U)
#define I2C_INT_CLR(p)       (I2C_BASE(p) + 0x24U)
#define I2C_INT_ENA(p)       (I2C_BASE(p) + 0x28U)
#define I2C_SDA_HOLD(p)      (I2C_BASE(p) + 0x30U)
#define I2C_SDA_SAMPLE(p)    (I2C_BASE(p) + 0x34U)
#define I2C_SCL_HIGH(p)      (I2C_BASE(p) + 0x38U)
#define I2C_START_HOLD(p)    (I2C_BASE(p) + 0x40U)
#define I2C_RSTART_SETUP(p)  (I2C_BASE(p) + 0x44U)
#define I2C_STOP_HOLD(p)     (I2C_BASE(p) + 0x48U)
#define I2C_STOP_SETUP(p)    (I2C_BASE(p) + 0x4CU)
#define I2C_SCL_FILTER(p)    (I2C_BASE(p) + 0x50U)
#define I2C_SDA_FILTER(p)    (I2C_BASE(p) + 0x54U)
#define I2C_COMD(p, n)       (I2C_BASE(p) + 0x58U + 4U * (n))
/* TX FIFO writes through the AHB alias, as ESP-IDF's i2c_ll_write_txfifo. */
#define I2C_FIFO_AHB(p)      ((p) == 0U ? 0x6001301CU : 0x6002701CU)

#define CTR_SDA_FORCE_OUT    (1U << 0)   /* open drain */
#define CTR_SCL_FORCE_OUT    (1U << 1)
#define CTR_MS_MODE          (1U << 4)   /* master */
#define CTR_TRANS_START      (1U << 5)
#define CTR_CLK_EN           (1U << 8)
#define SR_SLAVE_ADDRESSED   (1U << 5)
#define SR_RXFIFO_CNT(sr)    (((sr) >> 8) & 0x3FU)
#define SR_TXFIFO_CNT(sr)    (((sr) >> 18) & 0x3FU)
#define FIFO_TX_RST          (1U << 13)
#define FIFO_RX_RST          (1U << 12)
#define INT_END_DETECT       (1U << 3)
#define INT_ARB_LOST         (1U << 5)
#define INT_TRANS_COMPLETE   (1U << 7)
#define INT_TIME_OUT         (1U << 8)
#define INT_ACK_ERR          (1U << 10)
#define INT_ALL              0x1FFFU
#define FILTER_EN_7          ((1U << 3) | 7U)   /* enable, 7 APB cycles */

#define OP_RESTART 0U
#define OP_WRITE   1U
#define OP_READ    2U
#define OP_STOP    3U
#define OP_END     4U
#define CMD(op, n, ack_en, ack_val) \
    (((uint32_t)(op) << 11) | ((uint32_t)(ack_val) << 10) | ((uint32_t)(ack_en) << 8) | (uint32_t)(n))

#define DPORT_I2C0_CLK_EN    (1U << 7)
#define DPORT_I2C1_CLK_EN    (1U << 18)

/* gpio_sig_map.h */
#define I2C0_SCL_IDX 29U
#define I2C0_SDA_IDX 30U
#define I2C1_SCL_IDX 95U
#define I2C1_SDA_IDX 96U

#define I2C_FIFO_LEN 32U
#define I2C_MIN_HZ   2500U        /* SCL low period field is 14 bits */
#define I2C_MAX_HZ   1000000U

/* GPIO_PINn_REG: pad driver (bit 2) selects open drain. */
#define GPIO_PIN_REG(n)      (0x3FF44088U + 4U * (n))
#define GPIO_PAD_DRIVER      (1U << 2)

typedef struct {
    bool     ready;
    bool     slave;
    uint32_t hz;
    uint16_t addr;           /* slave address, if a slave */
    uint16_t sda, scl;
} i2c_port_t;

static i2c_port_t s_i2c[2];

static uint32_t sig_scl(uint8_t p) { return p == 0U ? I2C0_SCL_IDX : I2C1_SCL_IDX; }
static uint32_t sig_sda(uint8_t p) { return p == 0U ? I2C0_SDA_IDX : I2C1_SDA_IDX; }

/* A pad driven open drain by @p out_sig, read by @p in_sig, pulled up. */
static int i2c_pad(uint16_t pin, uint32_t out_sig, uint32_t in_sig)
{
    if (!esp32_pin_usable(pin) || pin >= 34U) return -1;
    esp32_gpio_set(pin, true);
    eos_gpio_config_t g = { .pin = pin, .mode = EOS_GPIO_INPUT, .pull = EOS_GPIO_PULL_UP };
    if (esp32_gpio_configure(&g) != 0) return -1;
    REG32(GPIO_PIN_REG(pin)) |= GPIO_PAD_DRIVER;
    if (esp32_pin_route_output(pin, out_sig) != 0) return -1;
    /* esp32_pin_route_output() clears the pull bits; an idle bus needs them. */
    esp32_pin_pull_up(pin, true);
    return in_sig == 0xFFFFFFFFU ? 0 : esp32_pin_route_input(pin, in_sig);
}

/* ESP-IDF's i2c_ll_master_cal_bus_clk() and i2c_ll_master_set_bus_timing()
 * with the SCL filter on at threshold 7 (its default): SCL high is counted
 * from when the filtered line is seen high, hence the 7 + 6 subtracted. */
static void i2c_timing(uint8_t p, uint32_t hz)
{
    uint32_t half = ESP32_APB_HZ / hz / 2U;
    REG32(I2C_SCL_FILTER(p)) = FILTER_EN_7;
    REG32(I2C_SDA_FILTER(p)) = FILTER_EN_7;
    REG32(I2C_SCL_LOW(p)) = half - 1U;
    REG32(I2C_SCL_HIGH(p)) = half - 13U;
    REG32(I2C_SDA_HOLD(p)) = half / 2U;
    REG32(I2C_SDA_SAMPLE(p)) = half / 2U;
    REG32(I2C_RSTART_SETUP(p)) = half;
    REG32(I2C_STOP_SETUP(p)) = half;
    REG32(I2C_START_HOLD(p)) = half;
    REG32(I2C_STOP_HOLD(p)) = half;
    REG32(I2C_TO(p)) = half * 20U;      /* ten SCL periods of a stuck bus */
}

static int i2c_setup(uint8_t p, uint32_t hz, bool slave, uint16_t addr)
{
    i2c_port_t *s = &s_i2c[p];
    s->ready = false;
    if (hz < I2C_MIN_HZ || hz > I2C_MAX_HZ) return -1;
    s->sda = p == 0U ? EOS_BOARD_I2C0_SDA_GPIO : EOS_BOARD_I2C1_SDA_GPIO;
    s->scl = p == 0U ? EOS_BOARD_I2C0_SCL_GPIO : EOS_BOARD_I2C1_SCL_GPIO;

    uint32_t bit = p == 0U ? DPORT_I2C0_CLK_EN : DPORT_I2C1_CLK_EN;
    ESP32_REG32(ESP32_DPORT_PERIP_RST_EN) |= bit;     /* full reset first */
    esp32_periph_enable(bit);

    REG32(I2C_CTR(p)) = CTR_SDA_FORCE_OUT | CTR_SCL_FORCE_OUT | CTR_CLK_EN |
                        (slave ? 0U : CTR_MS_MODE);
    REG32(I2C_FIFO_CONF(p)) = FIFO_TX_RST | FIFO_RX_RST;   /* FIFO mode */
    REG32(I2C_FIFO_CONF(p)) = 0;
    REG32(I2C_INT_ENA(p)) = 0;
    REG32(I2C_INT_CLR(p)) = INT_ALL;
    i2c_timing(p, hz);
    if (slave) REG32(I2C_SLAVE_ADDR(p)) = addr & 0x7FU;

    if (i2c_pad(s->sda, sig_sda(p), sig_sda(p)) != 0 ||
        i2c_pad(s->scl, sig_scl(p), sig_scl(p)) != 0) {
        return -1;
    }
    s->hz = hz;
    s->slave = slave;
    s->addr = addr;
    s->ready = true;
    return 0;
}

int esp32_i2c_init(const eos_i2c_config_t *cfg)
{
    if (cfg == NULL || cfg->port > 1U) return -1;
    /* own_addr is the address the port answers as a slave; a master ignores
     * it. Slaves are configured with eos_esp32_i2c_slave_init(). */
    return i2c_setup(cfg->port, cfg->clock_hz, false, 0);
}

void esp32_i2c_deinit(uint8_t port)
{
    if (port > 1U) return;
    s_i2c[port].ready = false;
}

/* Wait for one of @p done or an error. 0 on @p done, -1 otherwise. */
static int i2c_wait(uint8_t p, uint32_t done, uint32_t bytes)
{
    /* Each byte is 9 SCL periods; allow four times that plus 2 ms, so a
     * clock-stretching slave within the controller's own timeout is waited
     * for, and a dead bus is not. */
    uint32_t period_cycles = ESP32_CPU_HZ / s_i2c[p].hz;
    uint32_t limit = (bytes + 2U) * 9U * period_cycles * 4U + ESP32_CPU_HZ / 500U;
    uint32_t t0 = eos_esp32_ccount();
    for (;;) {
        uint32_t raw = REG32(I2C_INT_RAW(p));
        if (raw & (INT_ACK_ERR | INT_ARB_LOST | INT_TIME_OUT)) return -1;
        if (raw & done) return 0;
        if (eos_esp32_ccount() - t0 > limit) return -1;
    }
}

static int i2c_fail(uint8_t p)
{
    /* Recover the controller: ESP-IDF resets it after any error. */
    uint32_t hz = s_i2c[p].hz;
    i2c_setup(p, hz, false, 0);
    return -1;
}

static void i2c_go(uint8_t p)
{
    REG32(I2C_INT_CLR(p)) = INT_ALL;
    REG32(I2C_CTR(p)) |= CTR_TRANS_START;
}

int esp32_i2c_write(uint8_t port, uint16_t addr, const uint8_t *data, size_t len)
{
    if (port > 1U || !s_i2c[port].ready || s_i2c[port].slave || addr > 0x7FU) return -1;
    if (data == NULL && len != 0U) return -1;
    if (len > INT32_MAX) return -1;
    uint8_t p = port;
    REG32(I2C_FIFO_CONF(p)) = FIFO_TX_RST | FIFO_RX_RST;
    REG32(I2C_FIFO_CONF(p)) = 0;

    /* First segment: address and up to 31 data bytes. */
    size_t done = len < I2C_FIFO_LEN - 1U ? len : I2C_FIFO_LEN - 1U;
    REG32(I2C_FIFO_AHB(p)) = (uint32_t)(addr << 1);
    for (size_t i = 0; i < done; i++) REG32(I2C_FIFO_AHB(p)) = data[i];
    /* Everything fits in this segment, so it ends the transfer. Derived
     * from len rather than written as `done == len`: the two are equal --
     * done is len when len < 31 and 31 otherwise, so done == len exactly
     * when len <= 31 -- but spelling it this way says which quantity
     * decides, and keeps cppcheck's value flow from mistaking the ternary
     * for a constant (it reported `last` as always false). */
    bool last = len <= I2C_FIFO_LEN - 1U;
    REG32(I2C_COMD(p, 0)) = CMD(OP_RESTART, 0, 0, 0);
    REG32(I2C_COMD(p, 1)) = CMD(OP_WRITE, done + 1U, 1, 0);
    REG32(I2C_COMD(p, 2)) = last ? CMD(OP_STOP, 0, 0, 0) : CMD(OP_END, 0, 0, 0);
    i2c_go(p);
    if (i2c_wait(p, last ? INT_TRANS_COMPLETE : INT_END_DETECT, done + 1U) != 0) return i2c_fail(p);

    while (!last) {
        size_t n = len - done < I2C_FIFO_LEN ? len - done : I2C_FIFO_LEN;
        for (size_t i = 0; i < n; i++) REG32(I2C_FIFO_AHB(p)) = data[done + i];
        done += n;
        last = done == len;
        REG32(I2C_COMD(p, 0)) = CMD(OP_WRITE, n, 1, 0);
        REG32(I2C_COMD(p, 1)) = last ? CMD(OP_STOP, 0, 0, 0) : CMD(OP_END, 0, 0, 0);
        i2c_go(p);
        if (i2c_wait(p, last ? INT_TRANS_COMPLETE : INT_END_DETECT, n) != 0) return i2c_fail(p);
    }
    return (int)len;
}

int esp32_i2c_read(uint8_t port, uint16_t addr, uint8_t *data, size_t len)
{
    if (port > 1U || !s_i2c[port].ready || s_i2c[port].slave || addr > 0x7FU) return -1;
    if (data == NULL || len == 0U || len > INT32_MAX) return -1;
    uint8_t p = port;
    REG32(I2C_FIFO_CONF(p)) = FIFO_TX_RST | FIFO_RX_RST;
    REG32(I2C_FIFO_CONF(p)) = 0;

    REG32(I2C_FIFO_AHB(p)) = (uint32_t)(addr << 1) | 1U;
    REG32(I2C_COMD(p, 0)) = CMD(OP_RESTART, 0, 0, 0);
    REG32(I2C_COMD(p, 1)) = CMD(OP_WRITE, 1, 1, 0);
    uint32_t c = 2;
    size_t done = 0;
    for (;;) {
        size_t n = len - done < I2C_FIFO_LEN ? len - done : I2C_FIFO_LEN;
        bool last = done + n == len;
        /* ACK every byte but the transfer's last, which is NACKed so the
         * slave lets go of SDA before the STOP. */
        if (last) {
            if (n > 1U) REG32(I2C_COMD(p, c++)) = CMD(OP_READ, n - 1U, 0, 0);
            REG32(I2C_COMD(p, c++)) = CMD(OP_READ, 1, 0, 1);
            REG32(I2C_COMD(p, c++)) = CMD(OP_STOP, 0, 0, 0);
        } else {
            REG32(I2C_COMD(p, c++)) = CMD(OP_READ, n, 0, 0);
            REG32(I2C_COMD(p, c++)) = CMD(OP_END, 0, 0, 0);
        }
        i2c_go(p);
        if (i2c_wait(p, last ? INT_TRANS_COMPLETE : INT_END_DETECT, n + 1U) != 0) return i2c_fail(p);
        /* RX FIFO reads use the peripheral-bus address (ESP-IDF reads it
         * with DPORT_REG_READ: the AHB path can skip a FIFO entry). */
        for (size_t i = 0; i < n; i++) data[done + i] = (uint8_t)REG32(I2C_FIFO_DATA(p));
        done += n;
        if (last) return (int)len;
        c = 0;
    }
}

/* ---- slave (board API) --------------------------------------------------- */

int eos_esp32_i2c_slave_init(uint8_t port, uint16_t addr, uint32_t clock_hz)
{
    if (port > 1U || addr == 0U || addr > 0x7FU) return -1;
    return i2c_setup(port, clock_hz, true, addr);
}

int eos_esp32_i2c_slave_queue(uint8_t port, const uint8_t *data, size_t len)
{
    if (port > 1U || !s_i2c[port].ready || !s_i2c[port].slave || data == NULL) return -1;
    size_t room = I2C_FIFO_LEN - SR_TXFIFO_CNT(REG32(I2C_SR(port)));
    size_t n = len < room ? len : room;
    for (size_t i = 0; i < n; i++) REG32(I2C_FIFO_AHB(port)) = data[i];
    return (int)n;
}

int eos_esp32_i2c_slave_take(uint8_t port, uint8_t *data, size_t max)
{
    if (port > 1U || !s_i2c[port].ready || !s_i2c[port].slave || data == NULL) return -1;
    size_t n = SR_RXFIFO_CNT(REG32(I2C_SR(port)));
    if (n > max) n = max;
    for (size_t i = 0; i < n; i++) data[i] = (uint8_t)REG32(I2C_FIFO_DATA(port));
    return (int)n;
}

bool eos_esp32_i2c_slave_addressed(uint8_t port)
{
    if (port > 1U || !s_i2c[port].ready || !s_i2c[port].slave) return false;
    return (REG32(I2C_SR(port)) & SR_SLAVE_ADDRESSED) != 0U;
}

/*
 * Self-test without wires: the slave's SDA and SCL inputs follow the
 * master's pads through the GPIO matrix, so the slave hears everything the
 * master sends. Its own replies (ACK, read data) still go to its own pads,
 * which the master does not read: a bus is a wired AND that two pads cannot
 * form inside the chip, and the ESP32 master stalls if its SDA input does
 * not follow what it drives. So the master sees every byte NACKed. Joining
 * the pads with two wires (EOS_BOARD_I2C0_SDA to I2C1_SDA, SCL to SCL) makes
 * a real bus instead.
 */
int eos_esp32_i2c_slave_listen(uint8_t slave, uint8_t master)
{
    if (master > 1U || slave > 1U || master == slave) return -1;
    const i2c_port_t *m = &s_i2c[master], *s = &s_i2c[slave];
    if (!m->ready || m->slave || !s->ready || !s->slave) return -1;
    if (esp32_pin_route_input(m->sda, sig_sda(slave)) != 0 ||
        esp32_pin_route_input(m->scl, sig_scl(slave)) != 0) {
        return -1;
    }
    return 0;
}
