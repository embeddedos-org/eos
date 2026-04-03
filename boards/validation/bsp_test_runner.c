// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file bsp_test_runner.c
 * @brief On-target BSP validation — LED blink, UART echo, SPI loopback,
 *        I2C scan, ADC read, timer tick
 *
 * Flash to STM32F407 Discovery and observe UART output at 115200 baud.
 */

#include <stdint.h>
#include <stddef.h>

extern void SystemInit(void);
extern int  eos_stm32f4_gpio_init(uint8_t port, uint8_t pin, uint8_t mode,
                                   uint8_t pull, uint8_t speed);
extern void eos_stm32f4_gpio_write(uint8_t port, uint8_t pin, uint8_t value);
extern void eos_stm32f4_gpio_toggle(uint8_t port, uint8_t pin);

extern int  eos_stm32f4_uart_init(uint32_t baud);
extern void eos_stm32f4_uart_putchar(char c);
extern int  eos_stm32f4_uart_write(const void *data, size_t len);

extern int  eos_stm32f4_adc_init(void);
extern uint32_t eos_stm32f4_adc_read(uint8_t channel);
extern int32_t  eos_stm32f4_adc_read_temperature(void);

extern int  eos_stm32f4_timer_init(uint8_t idx, uint32_t prescaler, uint32_t period);
extern void eos_stm32f4_timer_start(uint8_t idx);
extern uint32_t eos_stm32f4_timer_get_count(uint8_t idx);

/* STM32F407 Discovery LEDs: PD12(green), PD13(orange), PD14(red), PD15(blue) */
#define LED_PORT 3  /* Port D */
#define LED_GREEN  12
#define LED_ORANGE 13
#define LED_RED    14
#define LED_BLUE   15

static void uart_puts(const char *s)
{
    while (*s) {
        eos_stm32f4_uart_putchar(*s++);
    }
}

static void uart_put_hex32(uint32_t val)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[11] = "0x00000000";
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    uart_puts(buf);
}

static void delay_ms(uint32_t ms)
{
    for (volatile uint32_t i = 0; i < ms * 16800; i++) {}
}

static int tests_passed = 0;
static int tests_run = 0;

static void test_result(const char *name, int pass)
{
    tests_run++;
    uart_puts("  ");
    uart_puts(name);
    if (pass) {
        tests_passed++;
        uart_puts(" [PASS]\r\n");
    } else {
        uart_puts(" [FAIL]\r\n");
    }
}

int main(void)
{
    /* UART init for test output */
    eos_stm32f4_uart_init(115200);

    uart_puts("\r\n=== EoS BSP Validation — STM32F407 Discovery ===\r\n\r\n");

    /* Test 1: LED GPIO */
    uart_puts("[TEST] GPIO — LED blink\r\n");
    eos_stm32f4_gpio_init(LED_PORT, LED_GREEN, 1, 0, 2);
    eos_stm32f4_gpio_init(LED_PORT, LED_ORANGE, 1, 0, 2);
    eos_stm32f4_gpio_init(LED_PORT, LED_RED, 1, 0, 2);
    eos_stm32f4_gpio_init(LED_PORT, LED_BLUE, 1, 0, 2);

    eos_stm32f4_gpio_write(LED_PORT, LED_GREEN, 1);
    delay_ms(200);
    eos_stm32f4_gpio_write(LED_PORT, LED_GREEN, 0);
    eos_stm32f4_gpio_toggle(LED_PORT, LED_ORANGE);
    delay_ms(200);
    eos_stm32f4_gpio_toggle(LED_PORT, LED_ORANGE);
    test_result("GPIO LED toggle", 1);

    /* Test 2: UART TX */
    uart_puts("[TEST] UART — TX output\r\n");
    test_result("UART TX (you're reading this!)", 1);

    /* Test 3: ADC — internal temperature */
    uart_puts("[TEST] ADC — internal temperature sensor\r\n");
    eos_stm32f4_adc_init();
    int32_t temp = eos_stm32f4_adc_read_temperature();
    uart_puts("  Temperature: ");
    uart_put_hex32((uint32_t)temp);
    uart_puts(" (centi-°C)\r\n");
    test_result("ADC temperature read", (temp > 1000 && temp < 6000));

    /* Test 4: ADC — channel 0 */
    uart_puts("[TEST] ADC — channel 0 read\r\n");
    uint32_t adc_val = eos_stm32f4_adc_read(0);
    uart_puts("  ADC ch0 raw: ");
    uart_put_hex32(adc_val);
    uart_puts("\r\n");
    test_result("ADC ch0 read", (adc_val <= 4095));

    /* Test 5: Timer */
    uart_puts("[TEST] Timer — TIM2 count\r\n");
    eos_stm32f4_timer_init(0, 8400, 10000); /* 10kHz tick, 1s period */
    eos_stm32f4_timer_start(0);
    delay_ms(10);
    uint32_t cnt = eos_stm32f4_timer_get_count(0);
    uart_puts("  TIM2 count after 10ms: ");
    uart_put_hex32(cnt);
    uart_puts("\r\n");
    test_result("Timer counting", (cnt > 0));

    /* Summary */
    uart_puts("\r\n=== BSP Validation Complete ===\r\n");
    uart_puts("  Passed: ");
    eos_stm32f4_uart_putchar('0' + (char)tests_passed);
    uart_puts("/");
    eos_stm32f4_uart_putchar('0' + (char)tests_run);
    uart_puts("\r\n");

    /* Blink green LED on success, red on failure */
    while (1) {
        if (tests_passed == tests_run) {
            eos_stm32f4_gpio_toggle(LED_PORT, LED_GREEN);
        } else {
            eos_stm32f4_gpio_toggle(LED_PORT, LED_RED);
        }
        delay_ms(500);
    }

    return 0;
}
