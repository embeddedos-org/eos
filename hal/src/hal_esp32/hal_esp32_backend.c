// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file hal_esp32_backend.c
 * @brief ESP32 HAL backend: GPIO and UART0-2
 *
 * Fills the GPIO and UART slots of eos_hal_backend_t; the rest stay NULL, and
 * hal_common.c reports -1 for them rather than pretending to succeed. Register
 * addresses are from ESP-IDF v5.4 (components/soc/esp32/register/soc).
 */

#include <eos/hal.h>
#include "board_esp32.h"
#include "hal_esp32_internal.h"

#define REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))

#define GPIO_BASE             0x3FF44000U
#define GPIO_OUT_REG          (GPIO_BASE + 0x0004U)
#define GPIO_OUT_W1TS         (GPIO_BASE + 0x0008U)
#define GPIO_OUT_W1TC         (GPIO_BASE + 0x000CU)
#define GPIO_OUT1_REG         (GPIO_BASE + 0x0010U)
#define GPIO_OUT1_W1TS        (GPIO_BASE + 0x0014U)
#define GPIO_OUT1_W1TC        (GPIO_BASE + 0x0018U)
#define GPIO_ENABLE_W1TS      (GPIO_BASE + 0x0024U)
#define GPIO_ENABLE_W1TC      (GPIO_BASE + 0x0028U)
#define GPIO_ENABLE1_W1TS     (GPIO_BASE + 0x0030U)
#define GPIO_ENABLE1_W1TC     (GPIO_BASE + 0x0034U)
#define GPIO_IN_REG           (GPIO_BASE + 0x003CU)
#define GPIO_IN1_REG          (GPIO_BASE + 0x0040U)
#define GPIO_FUNC_OUT_SEL(n)  (GPIO_BASE + 0x0530U + 4U * (n))
#define SIG_GPIO_OUT_IDX      256U   /* gpio_sig_map.h: plain GPIO output */

#define IO_MUX_BASE           0x3FF49000U
#define FUN_PD                (1U << 7)
#define FUN_PU                (1U << 8)
#define FUN_IE                (1U << 9)
#define MCU_SEL_S             12
#define MCU_SEL_M             (7U << MCU_SEL_S)
#define PIN_FUNC_GPIO         2U

/* RX FIFO reads go through the peripheral-bus (APB) address and TX writes
 * through the AHB alias, as ESP-IDF's uart_ll does on the ESP32 ("Read fifo,
 * we use APB address"). Reading through the AHB alias returned wrong bytes:
 * on the board, UART2 loopback in 6-bit mode received garbled data. */
#define UART0_FIFO_APB        0x3FF40000U
#define UART0_STATUS          0x3FF4001CU
#define UART_RXFIFO_CNT(st)   ((st) & 0xFFU)

#define ESP32_GPIO_COUNT      40U

/*
 * IO_MUX register offset for each GPIO (io_mux_reg.h, PERIPHS_IO_MUX_*_U, in
 * the order of ESP-IDF's GPIO_PIN_MUX_REG table). 0 means the GPIO does not
 * exist on the ESP32-D0WD in an ESP-WROOM-32 module.
 */
static const uint8_t s_iomux_offset[ESP32_GPIO_COUNT] = {
    0x44, 0x88, 0x40, 0x84, 0x48, 0x6C, 0x60, 0x64,   /*  0- 7 */
    0x68, 0x54, 0x58, 0x5C, 0x34, 0x38, 0x30, 0x3C,   /*  8-15 */
    0x4C, 0x50, 0x70, 0x74, 0x00, 0x7C, 0x80, 0x8C,   /* 16-23 (20: PICO-V3 only) */
    0x00, 0x24, 0x28, 0x2C, 0x00, 0x00, 0x00, 0x00,   /* 24-31 */
    0x1C, 0x20, 0x14, 0x18, 0x04, 0x08, 0x0C, 0x10,   /* 32-39 */
};

/*
 * Pins the backend refuses: GPIO1 and GPIO3 are UART0, the console; GPIO6-11
 * are the module's SPI flash, and reconfiguring them stops the chip fetching
 * from flash.
 */
static int gpio_reserved(uint16_t pin)
{
    return pin == 1U || pin == 3U || (pin >= 6U && pin <= 11U);
}

bool esp32_pin_usable(uint16_t pin)
{
    return pin < ESP32_GPIO_COUNT && s_iomux_offset[pin] != 0U && !gpio_reserved(pin);
}

#define GPIO_FUNC_IN_SEL(sig) (GPIO_BASE + 0x0130U + 4U * (sig))
#define GPIO_SIG_IN_SEL       (1U << 7)   /* route through the matrix, not IO_MUX */

int esp32_pin_route_output(uint16_t pin, uint32_t signal)
{
    if (!esp32_pin_usable(pin) || pin >= 34U || signal > SIG_GPIO_OUT_IDX) return -1;
    esp32_rtc_pad_release(pin);
    uint32_t mux = REG32(IO_MUX_BASE + s_iomux_offset[pin]);
    mux &= ~(MCU_SEL_M | FUN_PU | FUN_PD);
    mux |= (PIN_FUNC_GPIO << MCU_SEL_S) | FUN_IE;   /* IE: the pad can be read back */
    REG32(IO_MUX_BASE + s_iomux_offset[pin]) = mux;
    REG32(GPIO_FUNC_OUT_SEL(pin)) = signal;
    if (pin < 32U) REG32(GPIO_ENABLE_W1TS) = 1U << pin;
    else           REG32(GPIO_ENABLE1_W1TS) = 1U << (pin - 32U);
    return 0;
}

int esp32_pin_route_input(uint16_t pin, uint32_t signal)
{
    if (!esp32_pin_usable(pin) || signal >= SIG_GPIO_OUT_IDX) return -1;
    esp32_rtc_pad_release(pin);
    uint32_t mux = REG32(IO_MUX_BASE + s_iomux_offset[pin]);
    mux &= ~MCU_SEL_M;
    mux |= (PIN_FUNC_GPIO << MCU_SEL_S) | FUN_IE;
    REG32(IO_MUX_BASE + s_iomux_offset[pin]) = mux;
    REG32(GPIO_FUNC_IN_SEL(signal)) = GPIO_SIG_IN_SEL | pin;
    return 0;
}

static int esp32_gpio_init(const eos_gpio_config_t *cfg)
{
    if (cfg == NULL || !esp32_pin_usable(cfg->pin)) return -1;
    uint16_t pin = cfg->pin;

    if (cfg->mode != EOS_GPIO_INPUT && cfg->mode != EOS_GPIO_OUTPUT) return -1;
    /* GPIO34-39 are input-only pads with no internal pull resistors: an
     * output, or a pull that would silently do nothing, is refused. */
    if (pin >= 34U && (cfg->mode == EOS_GPIO_OUTPUT || cfg->pull != EOS_GPIO_PULL_NONE)) {
        return -1;
    }
    /* A pin an ADC or DAC used is still on the RTC mux; take it back. */
    esp32_rtc_pad_release(pin);

    uint32_t mux = REG32(IO_MUX_BASE + s_iomux_offset[pin]);
    mux &= ~(MCU_SEL_M | FUN_PU | FUN_PD);
    mux |= (PIN_FUNC_GPIO << MCU_SEL_S) | FUN_IE;   /* IE: GPIO_IN reads the pad */
    if (cfg->pull == EOS_GPIO_PULL_UP) mux |= FUN_PU;
    else if (cfg->pull == EOS_GPIO_PULL_DOWN) mux |= FUN_PD;
    REG32(IO_MUX_BASE + s_iomux_offset[pin]) = mux;

    if (cfg->mode == EOS_GPIO_OUTPUT) {
        REG32(GPIO_FUNC_OUT_SEL(pin)) = SIG_GPIO_OUT_IDX;
        if (pin < 32U) REG32(GPIO_ENABLE_W1TS) = 1U << pin;
        else           REG32(GPIO_ENABLE1_W1TS) = 1U << (pin - 32U);
    } else {
        if (pin < 32U) REG32(GPIO_ENABLE_W1TC) = 1U << pin;
        else           REG32(GPIO_ENABLE1_W1TC) = 1U << (pin - 32U);
    }
    return 0;
}

static void esp32_gpio_write(uint16_t pin, bool value)
{
    if (!esp32_pin_usable(pin) || pin >= 34U) return;
    if (pin < 32U) REG32(value ? GPIO_OUT_W1TS : GPIO_OUT_W1TC) = 1U << pin;
    else           REG32(value ? GPIO_OUT1_W1TS : GPIO_OUT1_W1TC) = 1U << (pin - 32U);
}

static bool esp32_gpio_read(uint16_t pin)
{
    if (!esp32_pin_usable(pin)) return false;
    if (pin < 32U) return (REG32(GPIO_IN_REG) >> pin) & 1U;
    return (REG32(GPIO_IN1_REG) >> (pin - 32U)) & 1U;
}

static void esp32_gpio_toggle(uint16_t pin)
{
    if (!esp32_pin_usable(pin) || pin >= 34U) return;
    /* Toggle the output latch, not the pad: an external load cannot make
     * this flip the wrong way. */
    bool on = (pin < 32U) ? ((REG32(GPIO_OUT_REG) >> pin) & 1U)
                          : ((REG32(GPIO_OUT1_REG) >> (pin - 32U)) & 1U);
    esp32_gpio_write(pin, !on);
}

/*
 * UART0 is the console: the ROM set it to 115200 8N1 and the backend leaves it
 * alone. UART1 and UART2 are configured here, on the pins board_esp32.h names
 * (UART1's own IO_MUX pins are the module's flash lines, so it always goes
 * through the GPIO matrix).
 */
#define UART_BASE(p)        ((p) == 1U ? 0x3FF50000U : 0x3FF6E000U)
#define UART_FIFO_AHB(p)    ((p) == 1U ? 0x60010000U : 0x6002E000U)
#define UART_STATUS(p)      (UART_BASE(p) + 0x1CU)
#define UART_CLKDIV(p)      (UART_BASE(p) + 0x14U)
#define UART_CONF0(p)       (UART_BASE(p) + 0x20U)
#define UART_RS485_CONF(p)  (UART_BASE(p) + 0x44U)
#define UART_TXFIFO_CNT(st) (((st) >> 16) & 0xFFU)
#define UART_CONF0_PARITY      (1U << 0)    /* 1: odd */
#define UART_CONF0_PARITY_EN   (1U << 1)
#define UART_CONF0_BIT_NUM_S   2
#define UART_CONF0_STOP_NUM_S  4
#define UART_CONF0_LOOPBACK    (1U << 14)
#define UART_RS485_DL1_EN      (1U << 2)
#define UART_FIFO_DEPTH        128U

#define DPORT_UART1_CLK_EN  (1U << 5)
#define DPORT_UART2_CLK_EN  (1U << 23)
#define DPORT_UART_MEM_EN   (1U << 24)
#define SIG_U1TXD_OUT 17U
#define SIG_U1RXD_IN  17U
#define SIG_U2TXD_OUT 198U
#define SIG_U2RXD_IN  198U

#define UART_FIFO_APB(p)       (UART_BASE(p) + 0x00U)
#define UART_MEM_RX_STATUS(p)  (UART_BASE(p) + 0x60U)

static bool s_uart_ready[3];
static uint32_t s_uart_baud[3];
static uint32_t s_uart_frame_bits[3];

#define UART_ST_UTX(st)  (((st) >> 24) & 0xFU)   /* transmitter state, 0 = idle */
#define UART_ST_URX(st)  (((st) >> 8) & 0xFU)    /* receiver state, 0 = idle */

static void uart_wait_tx_idle(uint8_t p)
{
    while (UART_TXFIFO_CNT(REG32(UART_STATUS(p))) != 0U || UART_ST_UTX(REG32(UART_STATUS(p))) != 0U) {
    }
}

/*
 * Empty the RX FIFO. RXFIFO_RST does not do it on the ESP32 (ESP-IDF,
 * hal/esp32/include/hal/uart_ll.h: "Hardware issue: we can not use
 * `rxfifo_rst` to reset the hw rxfifo"), so read until the count is zero
 * and the FIFO memory's read and write pointers meet, as ESP-IDF does.
 * Bytes left there would otherwise be returned ahead of the next transfer.
 */
static void uart_rx_drain(uint8_t p)
{
    for (;;) {
        uint32_t mem = REG32(UART_MEM_RX_STATUS(p));
        uint32_t rd = (mem >> 2) & 0x7FFU, wr = (mem >> 13) & 0x7FFU;
        if (UART_RXFIFO_CNT(REG32(UART_STATUS(p))) == 0U && rd == wr) break;
        (void)REG32(UART_FIFO_APB(p));
    }
}

/*
 * Reconfiguring a UART, or switching it into or out of loopback, can glitch
 * the receiver's input. The receiver takes the glitch as a start bit, and if
 * data follows with no idle gap it can stay misaligned with the real frames.
 * ESP-IDF's driver shows the same thing on the ESP32: after one loopback test,
 * each reconfiguration's data arrived a byte late or garbled. So after a
 * change wait two frame times, until the receiver is idle, then drain.
 */
static void uart_settle(uint8_t p)
{
    uint32_t wait = 2U * s_uart_frame_bits[p] * (ESP32_APB_HZ / s_uart_baud[p] + 1U);
    uint32_t t0 = eos_esp32_ccount();
    while (eos_esp32_ccount() - t0 < wait || UART_ST_URX(REG32(UART_STATUS(p))) != 0U) {
    }
    uart_rx_drain(p);
}

static int esp32_uart_init(const eos_uart_config_t *cfg)
{
    if (cfg == NULL) return -1;
    if (cfg->port == EOS_BOARD_CONSOLE_UART) {
        /* The console keeps the ROM's 115200 8N1; anything else is refused
         * rather than silently left at 115200. */
        if (cfg->baudrate != 115200U || cfg->data_bits != 8U) return -1;
        if (cfg->parity != EOS_UART_PARITY_NONE || cfg->stop_bits != EOS_UART_STOP_1) return -1;
        return 0;
    }
    uint8_t p = cfg->port;
    if (p != 1U && p != 2U) return -1;
    if (cfg->baudrate < 300U || cfg->baudrate > 5000000U) return -1;
    if (cfg->data_bits < 5U || cfg->data_bits > 8U) return -1;
    if (cfg->parity > EOS_UART_PARITY_ODD || cfg->stop_bits > EOS_UART_STOP_2) return -1;

    uint16_t tx = (p == 1U) ? EOS_BOARD_UART1_TX_GPIO : EOS_BOARD_UART2_TX_GPIO;
    uint16_t rx = (p == 1U) ? EOS_BOARD_UART1_RX_GPIO : EOS_BOARD_UART2_RX_GPIO;
    if (!esp32_pin_usable(tx) || !esp32_pin_usable(rx) || tx == rx) return -1;

    esp32_periph_enable(((p == 1U) ? DPORT_UART1_CLK_EN : DPORT_UART2_CLK_EN) | DPORT_UART_MEM_EN);
    /* Reconfiguring mid-transmission would corrupt the frame on the wire. */
    if (s_uart_ready[p]) uart_wait_tx_idle(p);

    /* Baud: APB / (CLKDIV + FRAG/16), so the divider in 1/16ths is
     * APB * 16 / baud, rounded to nearest. */
    uint32_t div16 = (ESP32_APB_HZ * 16U + cfg->baudrate / 2U) / cfg->baudrate;
    REG32(UART_CLKDIV(p)) = ((div16 & 0xFU) << 20) | (div16 >> 4);

    uint32_t conf0 = REG32(UART_CONF0(p));
    conf0 &= ~(UART_CONF0_PARITY | UART_CONF0_PARITY_EN | (3U << UART_CONF0_BIT_NUM_S) |
               (3U << UART_CONF0_STOP_NUM_S) | UART_CONF0_LOOPBACK);
    conf0 |= (uint32_t)(cfg->data_bits - 5U) << UART_CONF0_BIT_NUM_S;
    if (cfg->parity != EOS_UART_PARITY_NONE) {
        conf0 |= UART_CONF0_PARITY_EN;
        if (cfg->parity == EOS_UART_PARITY_ODD) conf0 |= UART_CONF0_PARITY;
    }
    /* Two stop bits are stop_bit_num = 1 plus RS485 DL1_EN on the ESP32,
     * as ESP-IDF's uart_ll_set_stop_bits() does. */
    conf0 |= 1U << UART_CONF0_STOP_NUM_S;
    if (cfg->stop_bits == EOS_UART_STOP_2) REG32(UART_RS485_CONF(p)) |= UART_RS485_DL1_EN;
    else                                   REG32(UART_RS485_CONF(p)) &= ~UART_RS485_DL1_EN;
    /* No FIFO reset bit is used. RXFIFO_RST is broken on the ESP32 (see
     * uart_rx_drain()), and ESP-IDF toggles TXFIFO_RST only on UART0
     * (hal/esp32/include/hal/uart_ll.h, uart_ll_txfifo_rst()). Toggling
     * both here made every second reconfiguration of UART2 receive garbage
     * on the board. The transmitter is idle before this write (see above)
     * and uart_settle() empties the receiver. */
    REG32(UART_CONF0(p)) = conf0;

    if (esp32_pin_route_output(tx, (p == 1U) ? SIG_U1TXD_OUT : SIG_U2TXD_OUT) != 0) return -1;
    if (esp32_pin_route_input(rx, (p == 1U) ? SIG_U1RXD_IN : SIG_U2RXD_IN) != 0) return -1;
    /* Pull RX to the line's idle level, so an unconnected RX does not float
     * into noise frames; a connected transmitter overrides it. */
    REG32(IO_MUX_BASE + s_iomux_offset[rx]) |= FUN_PU;
    s_uart_baud[p] = cfg->baudrate;
    s_uart_frame_bits[p] = 1U + cfg->data_bits + (cfg->parity != EOS_UART_PARITY_NONE ? 1U : 0U) +
                           (cfg->stop_bits == EOS_UART_STOP_2 ? 2U : 1U);
    uart_settle(p);
    s_uart_ready[p] = true;
    return 0;
}

int eos_esp32_uart_set_loopback(uint8_t port, bool enable)
{
    if ((port != 1U && port != 2U) || !s_uart_ready[port]) return -1;
    uart_wait_tx_idle(port);
    if (enable) REG32(UART_CONF0(port)) |= UART_CONF0_LOOPBACK;
    else        REG32(UART_CONF0(port)) &= ~UART_CONF0_LOOPBACK;
    uart_settle(port);
    return 0;
}

static int esp32_uart_write(uint8_t port, const uint8_t *data, size_t len)
{
    if (data == NULL && len != 0U) return -1;
    if (port == EOS_BOARD_CONSOLE_UART) {
        eos_esp32_console_write((const char *)data, len);
        return (int)len;
    }
    if ((port != 1U && port != 2U) || !s_uart_ready[port]) return -1;
    for (size_t i = 0; i < len; i++) {
        while (UART_TXFIFO_CNT(REG32(UART_STATUS(port))) >= UART_FIFO_DEPTH - 2U) {
        }
        REG32(UART_FIFO_AHB(port)) = data[i];
    }
    return (int)len;
}

static int esp32_uart_read(uint8_t port, uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (data == NULL && len != 0U) return -1;
    uint32_t status_reg, fifo_reg;
    if (port == EOS_BOARD_CONSOLE_UART) {
        status_reg = UART0_STATUS;
        fifo_reg = UART0_FIFO_APB;
    } else if ((port == 1U || port == 2U) && s_uart_ready[port]) {
        status_reg = UART_STATUS(port);
        fifo_reg = UART_FIFO_APB(port);
    } else {
        return -1;
    }
    /* CCOUNT runs at the 40 MHz the ROM leaves the CPU on and wraps every
     * 107 s, so elapsed time is accumulated from wrap-safe deltas rather than
     * measured from the start: any timeout_ms up to UINT32_MAX expires. */
    const uint32_t cycles_per_ms = 40000U;
    uint32_t last = eos_esp32_ccount();
    uint32_t cycles = 0;
    uint32_t elapsed_ms = 0;
    size_t got = 0;
    while (got < len) {
        if (UART_RXFIFO_CNT(REG32(status_reg)) != 0U) {
            data[got++] = (uint8_t)REG32(fifo_reg);
            continue;
        }
        uint32_t now = eos_esp32_ccount();
        cycles += now - last;
        last = now;
        while (cycles >= cycles_per_ms) {
            cycles -= cycles_per_ms;
            elapsed_ms++;
        }
        if (elapsed_ms >= timeout_ms) break;
    }
    return (int)got;
}

static int esp32_hal_backend_init(void)
{
    return 0;
}

static const eos_hal_backend_t s_esp32_backend = {
    .name        = "esp32",
    .init        = esp32_hal_backend_init,
    .gpio_init   = esp32_gpio_init,
    .gpio_write  = esp32_gpio_write,
    .gpio_read   = esp32_gpio_read,
    .gpio_toggle = esp32_gpio_toggle,
    .uart_init   = esp32_uart_init,
    .uart_write  = esp32_uart_write,
    .uart_read   = esp32_uart_read,
};

/* Selected by eos_hal_init() when built with -DEOS_HAL_BACKEND_ESP32. */
void eos_hal_esp32_register(void)
{
    eos_hal_register_backend(&s_esp32_backend);
}
