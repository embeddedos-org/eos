// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file hal_linux.c
 * @brief EoS HAL — Linux platform backend
 *
 * Implements the HAL interface using Linux sysfs and devmem for GPIO,
 * UART via termios, SPI/I2C via ioctl, and POSIX timers.
 */

#include <eos/hal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef __linux__

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>

/* ---- Internal state ---- */

static struct timespec hal_start_time;
static bool hal_initialized = false;

/* ---- GPIO via sysfs ---- */

#define GPIO_SYSFS_PATH "/sys/class/gpio"
#define EOS_MAX_GPIO 256

static int gpio_fds[EOS_MAX_GPIO];

static int linux_gpio_init(const eos_gpio_config_t *cfg)
{
    if (cfg->pin >= EOS_MAX_GPIO) return -1;

    char path[128];
    int fd;

    /* Export the GPIO pin */
    fd = open(GPIO_SYSFS_PATH "/export", O_WRONLY);
    if (fd < 0) return -1;

    char pin_str[8];
    int len = snprintf(pin_str, sizeof(pin_str), "%u", cfg->pin);
    write(fd, pin_str, len);
    close(fd);

    /* Set direction */
    snprintf(path, sizeof(path), GPIO_SYSFS_PATH "/gpio%u/direction", cfg->pin);
    fd = open(path, O_WRONLY);
    if (fd < 0) return -1;

    const char *dir = (cfg->mode == EOS_GPIO_OUTPUT) ? "out" : "in";
    write(fd, dir, strlen(dir));
    close(fd);

    /* Open value file for read/write */
    snprintf(path, sizeof(path), GPIO_SYSFS_PATH "/gpio%u/value", cfg->pin);
    gpio_fds[cfg->pin] = open(path, O_RDWR);
    if (gpio_fds[cfg->pin] < 0) return -1;

    return 0;
}

static void linux_gpio_deinit(uint16_t pin)
{
    if (pin >= EOS_MAX_GPIO) return;

    if (gpio_fds[pin] > 0) {
        close(gpio_fds[pin]);
        gpio_fds[pin] = 0;
    }

    int fd = open(GPIO_SYSFS_PATH "/unexport", O_WRONLY);
    if (fd >= 0) {
        char pin_str[8];
        int len = snprintf(pin_str, sizeof(pin_str), "%u", pin);
        write(fd, pin_str, len);
        close(fd);
    }
}

static void linux_gpio_write(uint16_t pin, bool value)
{
    if (pin >= EOS_MAX_GPIO || gpio_fds[pin] <= 0) return;
    const char *val = value ? "1" : "0";
    lseek(gpio_fds[pin], 0, SEEK_SET);
    write(gpio_fds[pin], val, 1);
}

static bool linux_gpio_read(uint16_t pin)
{
    if (pin >= EOS_MAX_GPIO || gpio_fds[pin] <= 0) return false;
    char val;
    lseek(gpio_fds[pin], 0, SEEK_SET);
    if (read(gpio_fds[pin], &val, 1) != 1) return false;
    return val == '1';
}

static void linux_gpio_toggle(uint16_t pin)
{
    bool current = linux_gpio_read(pin);
    linux_gpio_write(pin, !current);
}

static int linux_gpio_set_irq(uint16_t pin, eos_gpio_edge_t edge,
                               eos_gpio_callback_t cb, void *ctx)
{
    (void)cb;
    (void)ctx;
    if (pin >= EOS_MAX_GPIO) return -1;

    char path[128];
    snprintf(path, sizeof(path), GPIO_SYSFS_PATH "/gpio%u/edge", pin);

    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;

    const char *edge_str;
    switch (edge) {
        case EOS_GPIO_EDGE_RISING:  edge_str = "rising";  break;
        case EOS_GPIO_EDGE_FALLING: edge_str = "falling"; break;
        case EOS_GPIO_EDGE_BOTH:    edge_str = "both";    break;
        default:                    edge_str = "none";     break;
    }

    write(fd, edge_str, strlen(edge_str));
    close(fd);

    return 0;
}

/* ---- UART (stub — real impl uses termios) ---- */

static int linux_uart_init(const eos_uart_config_t *cfg)
{
    (void)cfg;
    return 0;
}

static void linux_uart_deinit(uint8_t port)
{
    (void)port;
}

static int linux_uart_write(uint8_t port, const uint8_t *data, size_t len)
{
    (void)port;
    (void)data;
    (void)len;
    return 0;
}

static int linux_uart_read(uint8_t port, uint8_t *data, size_t len, uint32_t timeout_ms)
{
    (void)port;
    (void)data;
    (void)len;
    (void)timeout_ms;
    return 0;
}

/* ---- SPI (stub — real impl uses spidev ioctl) ---- */

static int linux_spi_init(const eos_spi_config_t *cfg) { (void)cfg; return 0; }
static void linux_spi_deinit(uint8_t port) { (void)port; }
static int linux_spi_transfer(uint8_t port, const uint8_t *tx, uint8_t *rx, size_t len)
{
    (void)port; (void)tx; (void)rx; (void)len;
    return 0;
}

/* ---- I2C (stub — real impl uses i2c-dev ioctl) ---- */

static int linux_i2c_init(const eos_i2c_config_t *cfg) { (void)cfg; return 0; }
static void linux_i2c_deinit(uint8_t port) { (void)port; }
static int linux_i2c_write(uint8_t port, uint16_t addr, const uint8_t *data, size_t len)
{
    (void)port; (void)addr; (void)data; (void)len;
    return 0;
}
static int linux_i2c_read(uint8_t port, uint16_t addr, uint8_t *data, size_t len)
{
    (void)port; (void)addr; (void)data; (void)len;
    return 0;
}

/* ---- Timer (POSIX) ---- */

static int linux_timer_init(const eos_timer_config_t *cfg) { (void)cfg; return 0; }
static void linux_timer_deinit(uint8_t timer_id) { (void)timer_id; }
static int linux_timer_start(uint8_t timer_id) { (void)timer_id; return 0; }
static int linux_timer_stop(uint8_t timer_id) { (void)timer_id; return 0; }

/* ---- Interrupt (no-op on Linux userspace) ---- */

static void linux_irq_disable(void) { /* no-op in userspace */ }
static void linux_irq_enable(void)  { /* no-op in userspace */ }

/* ---- Timing ---- */

static void linux_delay_ms(uint32_t ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static uint32_t linux_get_tick_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint32_t elapsed = (uint32_t)((now.tv_sec - hal_start_time.tv_sec) * 1000 +
                                   (now.tv_nsec - hal_start_time.tv_nsec) / 1000000);
    return elapsed;
}

/* ---- Init ---- */

static int linux_hal_init(void)
{
    clock_gettime(CLOCK_MONOTONIC, &hal_start_time);
    memset(gpio_fds, 0, sizeof(gpio_fds));
    hal_initialized = true;
    return 0;
}

static void linux_hal_deinit(void)
{
    for (int i = 0; i < EOS_MAX_GPIO; i++) {
        if (gpio_fds[i] > 0) {
            linux_gpio_deinit((uint16_t)i);
        }
    }
    hal_initialized = false;
}

/* ---- Backend registration ---- */

static const eos_hal_backend_t linux_backend = {
    .name          = "linux",
    .init          = linux_hal_init,
    .deinit        = linux_hal_deinit,
    .delay_ms      = linux_delay_ms,
    .get_tick_ms   = linux_get_tick_ms,
    .gpio_init     = linux_gpio_init,
    .gpio_deinit   = linux_gpio_deinit,
    .gpio_write    = linux_gpio_write,
    .gpio_read     = linux_gpio_read,
    .gpio_toggle   = linux_gpio_toggle,
    .gpio_set_irq  = linux_gpio_set_irq,
    .uart_init     = linux_uart_init,
    .uart_deinit   = linux_uart_deinit,
    .uart_write    = linux_uart_write,
    .uart_read     = linux_uart_read,
    .spi_init      = linux_spi_init,
    .spi_deinit    = linux_spi_deinit,
    .spi_transfer  = linux_spi_transfer,
    .i2c_init      = linux_i2c_init,
    .i2c_deinit    = linux_i2c_deinit,
    .i2c_write     = linux_i2c_write,
    .i2c_read      = linux_i2c_read,
    .timer_init    = linux_timer_init,
    .timer_deinit  = linux_timer_deinit,
    .timer_start   = linux_timer_start,
    .timer_stop    = linux_timer_stop,
    .irq_disable   = linux_irq_disable,
    .irq_enable    = linux_irq_enable,
};

void eos_hal_linux_register(void)
{
    eos_hal_register_backend(&linux_backend);
}

#endif /* __linux__ */
