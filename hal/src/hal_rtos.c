// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file hal_rtos.c
 * @brief EoS HAL — RTOS platform backend with real register-level drivers
 *
 * STM32F4 implementations behind EOS_MCU_STM32F4 guards.
 * Generic stubs as fallback for other MCUs.
 */

#include <eos/hal.h>
#include <string.h>

#if !defined(__linux__)

#define REG32(addr) (*(volatile uint32_t *)(addr))

static volatile uint32_t systick_ms = 0;
static bool hal_initialized = false;

/* ================================================================
 * STM32F4 Register-Level GPIO
 * ================================================================ */
#if defined(EOS_MCU_STM32F4)

#define GPIOA_BASE  0x40020000U
#define GPIOB_BASE  0x40020400U
#define GPIOC_BASE  0x40020800U
#define GPIOD_BASE  0x40020C00U
#define GPIO_MODER(base)   REG32((base) + 0x00)
#define GPIO_OTYPER(base)  REG32((base) + 0x04)
#define GPIO_OSPEEDR(base) REG32((base) + 0x08)
#define GPIO_PUPDR(base)   REG32((base) + 0x0C)
#define GPIO_IDR(base)     REG32((base) + 0x10)
#define GPIO_ODR(base)     REG32((base) + 0x14)
#define GPIO_BSRR(base)    REG32((base) + 0x18)
#define GPIO_AFRL(base)    REG32((base) + 0x20)
#define GPIO_AFRH(base)    REG32((base) + 0x24)

/* RCC AHB1ENR for GPIO clocks */
#define RCC_AHB1ENR     REG32(0x40023830U)

static uint32_t gpio_get_base(uint16_t pin) {
    uint8_t port = (pin >> 8) & 0xF;
    switch (port) {
        case 0: return GPIOA_BASE;
        case 1: return GPIOB_BASE;
        case 2: return GPIOC_BASE;
        case 3: return GPIOD_BASE;
        default: return GPIOA_BASE;
    }
}

static int rtos_gpio_init(const eos_gpio_config_t *cfg)
{
    if (!cfg) return -1;
    uint32_t base = gpio_get_base(cfg->pin);
    uint8_t port = (cfg->pin >> 8) & 0xF;
    uint8_t pin_num = cfg->pin & 0xFF;
    if (pin_num > 15) return -1;

    /* Enable GPIO clock */
    RCC_AHB1ENR |= (1U << port);

    /* Configure MODER */
    uint32_t moder = GPIO_MODER(base);
    moder &= ~(3U << (pin_num * 2));
    moder |= ((uint32_t)cfg->mode << (pin_num * 2));
    GPIO_MODER(base) = moder;

    /* Configure speed */
    uint32_t ospeedr = GPIO_OSPEEDR(base);
    ospeedr &= ~(3U << (pin_num * 2));
    ospeedr |= ((uint32_t)cfg->speed << (pin_num * 2));
    GPIO_OSPEEDR(base) = ospeedr;

    /* Configure pull-up/pull-down */
    uint32_t pupdr = GPIO_PUPDR(base);
    pupdr &= ~(3U << (pin_num * 2));
    pupdr |= ((uint32_t)cfg->pull << (pin_num * 2));
    GPIO_PUPDR(base) = pupdr;

    /* Configure alternate function if AF mode */
    if (cfg->mode == EOS_GPIO_AF) {
        if (pin_num < 8) {
            uint32_t afrl = GPIO_AFRL(base);
            afrl &= ~(0xFU << (pin_num * 4));
            afrl |= ((uint32_t)cfg->af_num << (pin_num * 4));
            GPIO_AFRL(base) = afrl;
        } else {
            uint32_t afrh = GPIO_AFRH(base);
            afrh &= ~(0xFU << ((pin_num - 8) * 4));
            afrh |= ((uint32_t)cfg->af_num << ((pin_num - 8) * 4));
            GPIO_AFRH(base) = afrh;
        }
    }
    return 0;
}

static void rtos_gpio_deinit(uint16_t pin) {
    uint32_t base = gpio_get_base(pin);
    uint8_t pin_num = pin & 0xFF;
    GPIO_MODER(base) &= ~(3U << (pin_num * 2));
}

static void rtos_gpio_write(uint16_t pin, bool value) {
    uint32_t base = gpio_get_base(pin);
    uint8_t pin_num = pin & 0xFF;
    GPIO_BSRR(base) = value ? (1U << pin_num) : (1U << (pin_num + 16));
}

static bool rtos_gpio_read(uint16_t pin) {
    uint32_t base = gpio_get_base(pin);
    uint8_t pin_num = pin & 0xFF;
    return (GPIO_IDR(base) & (1U << pin_num)) != 0;
}

static void rtos_gpio_toggle(uint16_t pin) {
    uint32_t base = gpio_get_base(pin);
    uint8_t pin_num = pin & 0xFF;
    GPIO_ODR(base) ^= (1U << pin_num);
}

static int rtos_gpio_set_irq(uint16_t pin, eos_gpio_edge_t edge,
                              eos_gpio_callback_t cb, void *ctx) {
    (void)pin; (void)edge; (void)cb; (void)ctx;
    /* EXTI configuration would go here */
    return 0;
}

/* ================================================================
 * STM32F4 UART (USART1/USART2)
 * ================================================================ */

#define USART1_BASE 0x40011000U
#define USART2_BASE 0x40004400U
#define USART_SR(base)   REG32((base) + 0x00)
#define USART_DR(base)   REG32((base) + 0x04)
#define USART_BRR(base)  REG32((base) + 0x08)
#define USART_CR1(base)  REG32((base) + 0x0C)
#define RCC_APB2ENR      REG32(0x40023844U)
#define RCC_APB1ENR      REG32(0x40023840U)

#define USART_CR1_UE    (1U << 13)
#define USART_CR1_TE    (1U << 3)
#define USART_CR1_RE    (1U << 2)
#define USART_SR_TXE    (1U << 7)
#define USART_SR_RXNE   (1U << 5)
#define USART_SR_TC     (1U << 6)

static uint32_t uart_get_base(uint8_t port) {
    return (port == 0) ? USART1_BASE : USART2_BASE;
}

static int rtos_uart_init(const eos_uart_config_t *cfg) {
    if (!cfg) return -1;
    uint32_t base = uart_get_base(cfg->port);

    /* Enable clock */
    if (cfg->port == 0) RCC_APB2ENR |= (1U << 4);   /* USART1 */
    else                RCC_APB1ENR |= (1U << 17);   /* USART2 */

    /* Configure baud rate (assuming 16MHz APB clock) */
    uint32_t apb_clk = (cfg->port == 0) ? 84000000U : 42000000U;
    USART_BRR(base) = apb_clk / cfg->baudrate;

    /* Enable USART, TX, RX */
    USART_CR1(base) = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    return 0;
}

static void rtos_uart_deinit(uint8_t port) {
    uint32_t base = uart_get_base(port);
    USART_CR1(base) = 0;
}

static int rtos_uart_write(uint8_t port, const uint8_t *data, size_t len) {
    uint32_t base = uart_get_base(port);
    for (size_t i = 0; i < len; i++) {
        while (!(USART_SR(base) & USART_SR_TXE)) {}
        USART_DR(base) = data[i];
    }
    while (!(USART_SR(base) & USART_SR_TC)) {}
    return (int)len;
}

static int rtos_uart_read(uint8_t port, uint8_t *data, size_t len, uint32_t timeout_ms) {
    uint32_t base = uart_get_base(port);
    uint32_t start = systick_ms;
    for (size_t i = 0; i < len; i++) {
        while (!(USART_SR(base) & USART_SR_RXNE)) {
            if (timeout_ms != 0xFFFFFFFF && (systick_ms - start) >= timeout_ms)
                return (int)i;
        }
        data[i] = (uint8_t)(USART_DR(base) & 0xFF);
    }
    return (int)len;
}

/* ================================================================
 * STM32F4 SPI (SPI1)
 * ================================================================ */

#define SPI1_BASE   0x40013000U
#define SPI_CR1(base)  REG32((base) + 0x00)
#define SPI_SR(base)   REG32((base) + 0x08)
#define SPI_DR(base)   REG32((base) + 0x0C)
#define SPI_SR_TXE     (1U << 1)
#define SPI_SR_RXNE    (1U << 0)
#define SPI_SR_BSY     (1U << 7)
#define SPI_CR1_SPE    (1U << 6)
#define SPI_CR1_MSTR   (1U << 2)

static int rtos_spi_init(const eos_spi_config_t *cfg) {
    if (!cfg) return -1;
    RCC_APB2ENR |= (1U << 12); /* SPI1 clock */
    uint32_t cr1 = SPI_CR1_MSTR | SPI_CR1_SPE;
    cr1 |= ((uint32_t)cfg->mode << 0); /* CPOL/CPHA */
    /* Baud rate prescaler: approximate from clock_hz */
    uint32_t div = 84000000U / cfg->clock_hz;
    uint32_t br = 0;
    while (div > 2 && br < 7) { div >>= 1; br++; }
    cr1 |= (br << 3);
    SPI_CR1(SPI1_BASE) = cr1;
    return 0;
}

static void rtos_spi_deinit(uint8_t port) {
    (void)port;
    SPI_CR1(SPI1_BASE) = 0;
}

static int rtos_spi_transfer(uint8_t port, const uint8_t *tx, uint8_t *rx, size_t len) {
    (void)port;
    uint32_t base = SPI1_BASE;
    for (size_t i = 0; i < len; i++) {
        while (!(SPI_SR(base) & SPI_SR_TXE)) {}
        SPI_DR(base) = tx ? tx[i] : 0xFF;
        while (!(SPI_SR(base) & SPI_SR_RXNE)) {}
        uint8_t rd = (uint8_t)(SPI_DR(base) & 0xFF);
        if (rx) rx[i] = rd;
    }
    while (SPI_SR(base) & SPI_SR_BSY) {}
    return (int)len;
}

/* ================================================================
 * STM32F4 I2C (I2C1)
 * ================================================================ */

#define I2C1_BASE   0x40005400U
#define I2C_CR1(base)    REG32((base) + 0x00)
#define I2C_CR2(base)    REG32((base) + 0x04)
#define I2C_OAR1(base)   REG32((base) + 0x08)
#define I2C_DR_REG(base) REG32((base) + 0x10)
#define I2C_SR1(base)    REG32((base) + 0x14)
#define I2C_SR2(base)    REG32((base) + 0x18)
#define I2C_CCR(base)    REG32((base) + 0x1C)
#define I2C_CR1_PE       (1U << 0)
#define I2C_CR1_START    (1U << 8)
#define I2C_CR1_STOP     (1U << 9)
#define I2C_CR1_ACK      (1U << 10)
#define I2C_SR1_SB       (1U << 0)
#define I2C_SR1_ADDR     (1U << 1)
#define I2C_SR1_TXE      (1U << 7)
#define I2C_SR1_RXNE     (1U << 6)

static int rtos_i2c_init(const eos_i2c_config_t *cfg) {
    if (!cfg) return -1;
    RCC_APB1ENR |= (1U << 21); /* I2C1 clock */
    I2C_CR1(I2C1_BASE) = 0; /* Reset */
    I2C_CR2(I2C1_BASE) = 42; /* 42 MHz APB1 */
    /* Standard mode 100kHz: CCR = APB1_CLK / (2 * 100000) */
    I2C_CCR(I2C1_BASE) = 42000000U / (2 * (cfg->clock_hz ? cfg->clock_hz : 100000));
    I2C_CR1(I2C1_BASE) = I2C_CR1_PE | I2C_CR1_ACK;
    return 0;
}

static void rtos_i2c_deinit(uint8_t port) {
    (void)port;
    I2C_CR1(I2C1_BASE) = 0;
}

static int rtos_i2c_write(uint8_t port, uint16_t addr, const uint8_t *data, size_t len) {
    (void)port;
    uint32_t base = I2C1_BASE;
    I2C_CR1(base) |= I2C_CR1_START;
    while (!(I2C_SR1(base) & I2C_SR1_SB)) {}
    I2C_DR_REG(base) = (uint32_t)(addr << 1); /* Write */
    while (!(I2C_SR1(base) & I2C_SR1_ADDR)) {}
    (void)I2C_SR2(base); /* Clear ADDR */
    for (size_t i = 0; i < len; i++) {
        while (!(I2C_SR1(base) & I2C_SR1_TXE)) {}
        I2C_DR_REG(base) = data[i];
    }
    while (!(I2C_SR1(base) & I2C_SR1_TXE)) {}
    I2C_CR1(base) |= I2C_CR1_STOP;
    return (int)len;
}

static int rtos_i2c_read(uint8_t port, uint16_t addr, uint8_t *data, size_t len) {
    (void)port;
    uint32_t base = I2C1_BASE;
    I2C_CR1(base) |= I2C_CR1_START | I2C_CR1_ACK;
    while (!(I2C_SR1(base) & I2C_SR1_SB)) {}
    I2C_DR_REG(base) = (uint32_t)(addr << 1) | 1; /* Read */
    while (!(I2C_SR1(base) & I2C_SR1_ADDR)) {}
    (void)I2C_SR2(base);
    for (size_t i = 0; i < len; i++) {
        if (i == len - 1) I2C_CR1(base) &= ~I2C_CR1_ACK; /* NACK last byte */
        while (!(I2C_SR1(base) & I2C_SR1_RXNE)) {}
        data[i] = (uint8_t)(I2C_DR_REG(base) & 0xFF);
    }
    I2C_CR1(base) |= I2C_CR1_STOP;
    return (int)len;
}

/* ================================================================
 * STM32F4 Timer (TIM2)
 * ================================================================ */

#define TIM2_BASE   0x40000000U
#define TIM_CR1(base)  REG32((base) + 0x00)
#define TIM_DIER(base) REG32((base) + 0x0C)
#define TIM_SR_REG(base) REG32((base) + 0x10)
#define TIM_CNT(base)  REG32((base) + 0x24)
#define TIM_PSC(base)  REG32((base) + 0x28)
#define TIM_ARR(base)  REG32((base) + 0x2C)
#define TIM_CR1_CEN    (1U << 0)
#define TIM_CR1_ARPE   (1U << 7)

static int rtos_timer_init(const eos_timer_config_t *cfg) {
    if (!cfg) return -1;
    RCC_APB1ENR |= (1U << 0); /* TIM2 clock */
    TIM_CR1(TIM2_BASE) = 0;
    /* 1 MHz timer clock: PSC = APB1_CLK / 1MHz - 1 */
    TIM_PSC(TIM2_BASE) = 42 - 1;
    TIM_ARR(TIM2_BASE) = cfg->period_us;
    if (cfg->auto_reload) TIM_CR1(TIM2_BASE) |= TIM_CR1_ARPE;
    TIM_DIER(TIM2_BASE) = 1; /* Update interrupt enable */
    return 0;
}

static void rtos_timer_deinit(uint8_t timer_id) {
    (void)timer_id;
    TIM_CR1(TIM2_BASE) = 0;
}

static int rtos_timer_start(uint8_t timer_id) {
    (void)timer_id;
    TIM_CR1(TIM2_BASE) |= TIM_CR1_CEN;
    return 0;
}

static int rtos_timer_stop(uint8_t timer_id) {
    (void)timer_id;
    TIM_CR1(TIM2_BASE) &= ~TIM_CR1_CEN;
    return 0;
}

#else /* Generic stubs for non-STM32F4 targets */

static int rtos_gpio_init(const eos_gpio_config_t *cfg) { (void)cfg; return 0; }
static void rtos_gpio_deinit(uint16_t pin) { (void)pin; }
static void rtos_gpio_write(uint16_t pin, bool value) { (void)pin; (void)value; }
static bool rtos_gpio_read(uint16_t pin) { (void)pin; return false; }
static void rtos_gpio_toggle(uint16_t pin) { (void)pin; }
static int rtos_gpio_set_irq(uint16_t pin, eos_gpio_edge_t edge,
                              eos_gpio_callback_t cb, void *ctx) {
    (void)pin; (void)edge; (void)cb; (void)ctx; return 0;
}
static int rtos_uart_init(const eos_uart_config_t *cfg) { (void)cfg; return 0; }
static void rtos_uart_deinit(uint8_t port) { (void)port; }
static int rtos_uart_write(uint8_t port, const uint8_t *data, size_t len) {
    (void)port; (void)data; (void)len; return 0;
}
static int rtos_uart_read(uint8_t port, uint8_t *data, size_t len, uint32_t timeout_ms) {
    (void)port; (void)data; (void)len; (void)timeout_ms; return 0;
}
static int rtos_spi_init(const eos_spi_config_t *cfg) { (void)cfg; return 0; }
static void rtos_spi_deinit(uint8_t port) { (void)port; }
static int rtos_spi_transfer(uint8_t port, const uint8_t *tx, uint8_t *rx, size_t len) {
    (void)port; (void)tx; (void)rx; (void)len; return 0;
}
static int rtos_i2c_init(const eos_i2c_config_t *cfg) { (void)cfg; return 0; }
static void rtos_i2c_deinit(uint8_t port) { (void)port; }
static int rtos_i2c_write(uint8_t port, uint16_t addr, const uint8_t *data, size_t len) {
    (void)port; (void)addr; (void)data; (void)len; return 0;
}
static int rtos_i2c_read(uint8_t port, uint16_t addr, uint8_t *data, size_t len) {
    (void)port; (void)addr; (void)data; (void)len; return 0;
}
static int rtos_timer_init(const eos_timer_config_t *cfg) { (void)cfg; return 0; }
static void rtos_timer_deinit(uint8_t timer_id) { (void)timer_id; }
static int rtos_timer_start(uint8_t timer_id) { (void)timer_id; return 0; }
static int rtos_timer_stop(uint8_t timer_id) { (void)timer_id; return 0; }

#endif /* EOS_MCU_STM32F4 */

/* ================================================================
 * Common (all RTOS targets)
 * ================================================================ */

static void rtos_irq_disable(void) {
#if defined(__ARM_ARCH)
    __asm volatile ("cpsid i" ::: "memory");
#endif
}

static void rtos_irq_enable(void) {
#if defined(__ARM_ARCH)
    __asm volatile ("cpsie i" ::: "memory");
#endif
}

static void rtos_delay_ms(uint32_t ms) {
    uint32_t start = systick_ms;
    while ((systick_ms - start) < ms) {}
}

static uint32_t rtos_get_tick_ms(void) { return systick_ms; }

void eos_hal_systick_handler(void) { systick_ms++; }

static int rtos_hal_init(void) {
    systick_ms = 0;
    hal_initialized = true;
    return 0;
}

static void rtos_hal_deinit(void) { hal_initialized = false; }

static const eos_hal_backend_t rtos_backend = {
    .name = "rtos",
    .init = rtos_hal_init, .deinit = rtos_hal_deinit,
    .delay_ms = rtos_delay_ms, .get_tick_ms = rtos_get_tick_ms,
    .gpio_init = rtos_gpio_init, .gpio_deinit = rtos_gpio_deinit,
    .gpio_write = rtos_gpio_write, .gpio_read = rtos_gpio_read,
    .gpio_toggle = rtos_gpio_toggle, .gpio_set_irq = rtos_gpio_set_irq,
    .uart_init = rtos_uart_init, .uart_deinit = rtos_uart_deinit,
    .uart_write = rtos_uart_write, .uart_read = rtos_uart_read,
    .spi_init = rtos_spi_init, .spi_deinit = rtos_spi_deinit,
    .spi_transfer = rtos_spi_transfer,
    .i2c_init = rtos_i2c_init, .i2c_deinit = rtos_i2c_deinit,
    .i2c_write = rtos_i2c_write, .i2c_read = rtos_i2c_read,
    .timer_init = rtos_timer_init, .timer_deinit = rtos_timer_deinit,
    .timer_start = rtos_timer_start, .timer_stop = rtos_timer_stop,
    .irq_disable = rtos_irq_disable, .irq_enable = rtos_irq_enable,
};

void eos_hal_rtos_register(void) { eos_hal_register_backend(&rtos_backend); }

#endif /* !__linux__ */
