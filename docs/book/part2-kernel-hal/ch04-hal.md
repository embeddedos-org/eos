# Chapter 4: Hardware Abstraction Layer

*Srikanth Patchava & EmbeddedOS Contributors*

---

## 4.1 Overview


![Figure: EoS HAL Peripheral Map — 9 peripheral types across 83 board ports](images/hal-peripheral-map.png)

The Hardware Abstraction Layer (HAL) is the foundation of EoS portability. It provides
a **vendor-neutral C API** for accessing hardware peripherals, so application code
remains unchanged across all supported platforms.

The core HAL (`hal.h`) covers six peripheral families plus system utilities:

| Category | Interfaces |
|----------|-----------|
| **System** | `eos_hal_init`, `eos_hal_deinit`, `eos_delay_ms`, `eos_get_tick_ms` |
| **GPIO** | Digital I/O, interrupt callbacks |
| **UART** | Serial communication |
| **SPI** | Synchronous serial (full-duplex) |
| **I2C** | Two-wire serial bus |
| **Timer** | Hardware timers with callbacks |
| **Interrupt** | Global IRQ control, handler registration |

An additional 27 peripheral interfaces are provided in `hal_extended.h` (Chapter 5).

## 4.2 HAL Initialization

Every EoS application must call `eos_hal_init()` before using any peripheral:

```c
#include <eos/hal.h>

int main(void)
{
    int rc = eos_hal_init();
    if (rc != 0) {
        // HAL initialization failed
        return rc;
    }

    // ... use HAL APIs ...

    eos_hal_deinit();
    return 0;
}
```

### System Utility Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_hal_init` | `int eos_hal_init(void)` | Initialize HAL subsystem |
| `eos_hal_deinit` | `void eos_hal_deinit(void)` | Release all HAL resources |
| `eos_delay_ms` | `void eos_delay_ms(uint32_t ms)` | Blocking delay in milliseconds |
| `eos_get_tick_ms` | `uint32_t eos_get_tick_ms(void)` | Tick count since HAL init |

## 4.3 GPIO Interface

GPIO (General-Purpose Input/Output) is the most fundamental peripheral. EoS provides
a rich GPIO API with mode configuration, pull resistors, speed settings, and interrupt
support.

### Configuration Types

```c
typedef enum {
    EOS_GPIO_INPUT  = 0,  // Digital input
    EOS_GPIO_OUTPUT = 1,  // Push-pull output
    EOS_GPIO_AF     = 2,  // Alternate function
    EOS_GPIO_ANALOG = 3,  // Analog mode (for ADC/DAC)
} eos_gpio_mode_t;

typedef enum {
    EOS_GPIO_PULL_NONE = 0,  // No pull resistor
    EOS_GPIO_PULL_UP   = 1,  // Internal pull-up
    EOS_GPIO_PULL_DOWN = 2,  // Internal pull-down
} eos_gpio_pull_t;

typedef enum {
    EOS_GPIO_SPEED_LOW    = 0,  // Low frequency
    EOS_GPIO_SPEED_MEDIUM = 1,  // Medium frequency
    EOS_GPIO_SPEED_HIGH   = 2,  // High frequency
    EOS_GPIO_SPEED_VHIGH  = 3,  // Very high frequency
} eos_gpio_speed_t;

typedef struct {
    uint16_t         pin;     // Pin number
    eos_gpio_mode_t  mode;    // Input/Output/AF/Analog
    eos_gpio_pull_t  pull;    // Pull-up/down/none
    eos_gpio_speed_t speed;   // Output speed
    uint8_t          af_num;  // Alternate function number
} eos_gpio_config_t;
```

### GPIO API Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_gpio_init` | `int eos_gpio_init(const eos_gpio_config_t *cfg)` | Configure a GPIO pin |
| `eos_gpio_deinit` | `void eos_gpio_deinit(uint16_t pin)` | Release a GPIO pin |
| `eos_gpio_write` | `void eos_gpio_write(uint16_t pin, bool value)` | Set output high/low |
| `eos_gpio_read` | `bool eos_gpio_read(uint16_t pin)` | Read input state |
| `eos_gpio_toggle` | `void eos_gpio_toggle(uint16_t pin)` | Toggle output state |
| `eos_gpio_set_irq` | `int eos_gpio_set_irq(uint16_t pin, eos_gpio_edge_t edge, eos_gpio_callback_t cb, void *ctx)` | Register interrupt |

### GPIO Interrupt Example

```c
void button_handler(uint16_t pin, void *ctx)
{
    bool *pressed = (bool *)ctx;
    *pressed = true;
    printf("Button pressed on pin %d\n", pin);
}

int main(void)
{
    eos_hal_init();

    // Configure button as input with pull-up
    eos_gpio_config_t btn_cfg = {
        .pin  = 11,
        .mode = EOS_GPIO_INPUT,
        .pull = EOS_GPIO_PULL_UP,
    };
    eos_gpio_init(&btn_cfg);

    // Register falling-edge interrupt
    static bool pressed = false;
    eos_gpio_set_irq(11, EOS_GPIO_EDGE_FALLING, button_handler, &pressed);

    while (!pressed) {
        eos_delay_ms(10);
    }
    return 0;
}
```

## 4.4 UART Interface

UART (Universal Asynchronous Receiver-Transmitter) provides serial communication.

### Configuration

```c
typedef struct {
    uint8_t           port;       // UART port number (0, 1, 2, ...)
    uint32_t          baudrate;   // Baud rate (9600, 115200, etc.)
    uint8_t           data_bits;  // 7, 8, or 9
    eos_uart_parity_t parity;     // NONE, EVEN, ODD
    eos_uart_stop_t   stop_bits;  // 1 or 2 stop bits
} eos_uart_config_t;
```

### UART API Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_uart_init` | `int eos_uart_init(const eos_uart_config_t *cfg)` | Initialize UART port |
| `eos_uart_deinit` | `void eos_uart_deinit(uint8_t port)` | Release UART port |
| `eos_uart_write` | `int eos_uart_write(uint8_t port, const uint8_t *data, size_t len)` | Transmit data |
| `eos_uart_read` | `int eos_uart_read(uint8_t port, uint8_t *data, size_t len, uint32_t timeout_ms)` | Receive with timeout |
| `eos_uart_set_rx_callback` | `int eos_uart_set_rx_callback(uint8_t port, eos_uart_rx_callback_t cb, void *ctx)` | Register RX callback |

### UART Echo Example

```c
#include <eos/hal.h>

int main(void)
{
    eos_hal_init();

    eos_uart_config_t uart_cfg = {
        .port      = 0,
        .baudrate  = 115200,
        .data_bits = 8,
        .parity    = EOS_UART_PARITY_NONE,
        .stop_bits = EOS_UART_STOP_1,
    };
    eos_uart_init(&uart_cfg);

    uint8_t buf[64];
    while (1) {
        int n = eos_uart_read(0, buf, sizeof(buf), 1000);
        if (n > 0) {
            eos_uart_write(0, buf, n);  // Echo back
        }
    }
}
```

## 4.5 SPI Interface

SPI (Serial Peripheral Interface) is a full-duplex synchronous serial bus.

### Configuration

```c
typedef struct {
    uint8_t        port;           // SPI port number
    uint32_t       clock_hz;       // Clock frequency
    eos_spi_mode_t mode;           // Mode 0–3 (CPOL/CPHA)
    uint8_t        bits_per_word;  // Usually 8
    uint16_t       cs_pin;         // Chip-select GPIO pin
} eos_spi_config_t;
```

### SPI Mode Reference

| Mode | CPOL | CPHA | Clock Idle | Data Sampled |
|------|------|------|-----------|-------------|
| 0 | 0 | 0 | Low | Rising edge |
| 1 | 0 | 1 | Low | Falling edge |
| 2 | 1 | 0 | High | Falling edge |
| 3 | 1 | 1 | High | Rising edge |

### SPI API Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_spi_init` | `int eos_spi_init(const eos_spi_config_t *cfg)` | Initialize SPI port |
| `eos_spi_deinit` | `void eos_spi_deinit(uint8_t port)` | Release SPI port |
| `eos_spi_transfer` | `int eos_spi_transfer(uint8_t port, const uint8_t *tx, uint8_t *rx, size_t len)` | Full-duplex transfer |
| `eos_spi_write` | `int eos_spi_write(uint8_t port, const uint8_t *data, size_t len)` | Write only |
| `eos_spi_read` | `int eos_spi_read(uint8_t port, uint8_t *data, size_t len)` | Read only |

### SPI Sensor Read Example

```c
uint8_t spi_read_register(uint8_t reg)
{
    uint8_t tx[2] = { reg | 0x80, 0x00 };  // Read bit + register
    uint8_t rx[2] = { 0 };

    eos_spi_transfer(0, tx, rx, 2);
    return rx[1];
}
```

## 4.6 I2C Interface

I2C (Inter-Integrated Circuit) is a two-wire bus for communicating with sensors,
EEPROMs, and other peripherals.

### I2C API Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_i2c_init` | `int eos_i2c_init(const eos_i2c_config_t *cfg)` | Initialize I2C port |
| `eos_i2c_deinit` | `void eos_i2c_deinit(uint8_t port)` | Release I2C port |
| `eos_i2c_write` | `int eos_i2c_write(uint8_t port, uint16_t addr, const uint8_t *data, size_t len)` | Write to device |
| `eos_i2c_read` | `int eos_i2c_read(uint8_t port, uint16_t addr, uint8_t *data, size_t len)` | Read from device |
| `eos_i2c_write_reg` | `int eos_i2c_write_reg(uint8_t port, uint16_t addr, uint8_t reg, const uint8_t *data, size_t len)` | Write to register |
| `eos_i2c_read_reg` | `int eos_i2c_read_reg(uint8_t port, uint16_t addr, uint8_t reg, uint8_t *data, size_t len)` | Read from register |

### I2C Temperature Sensor Example

```c
#include <eos/hal.h>
#include <stdio.h>

#define TMP102_ADDR  0x48
#define TMP102_TEMP  0x00

float read_temperature(void)
{
    uint8_t raw[2];
    eos_i2c_read_reg(0, TMP102_ADDR, TMP102_TEMP, raw, 2);

    int16_t temp = (raw[0] << 4) | (raw[1] >> 4);
    if (temp & 0x800) temp |= 0xF000;  // Sign extend
    return temp * 0.0625f;
}

int main(void)
{
    eos_hal_init();

    eos_i2c_config_t i2c_cfg = {
        .port     = 0,
        .clock_hz = 400000,  // 400 kHz (Fast mode)
        .own_addr = 0,
    };
    eos_i2c_init(&i2c_cfg);

    while (1) {
        float temp = read_temperature();
        printf("Temperature: %.2f C\n", temp);
        eos_delay_ms(1000);
    }
}
```

## 4.7 Timer Interface

Hardware timers provide precise periodic or one-shot callbacks.

### Timer API Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_timer_init` | `int eos_timer_init(const eos_timer_config_t *cfg)` | Configure timer |
| `eos_timer_deinit` | `void eos_timer_deinit(uint8_t timer_id)` | Release timer |
| `eos_timer_start` | `int eos_timer_start(uint8_t timer_id)` | Start counting |
| `eos_timer_stop` | `int eos_timer_stop(uint8_t timer_id)` | Stop counting |
| `eos_timer_get_count` | `uint32_t eos_timer_get_count(uint8_t timer_id)` | Read counter value |

### Periodic Timer Example

```c
void heartbeat(uint8_t timer_id, void *ctx)
{
    uint32_t *count = (uint32_t *)ctx;
    (*count)++;
    printf("Heartbeat #%u\n", *count);
}

int main(void)
{
    eos_hal_init();

    static uint32_t beat_count = 0;
    eos_timer_config_t tmr = {
        .timer_id    = 0,
        .period_us   = 1000000,  // 1 second
        .auto_reload = true,
        .callback    = heartbeat,
        .ctx         = &beat_count,
    };
    eos_timer_init(&tmr);
    eos_timer_start(0);

    while (beat_count < 10) {
        eos_delay_ms(100);
    }
    eos_timer_stop(0);
}
```

## 4.8 Interrupt Control Interface

Global interrupt management for critical sections:

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_irq_disable` | `void eos_irq_disable(void)` | Disable all interrupts |
| `eos_irq_enable` | `void eos_irq_enable(void)` | Enable all interrupts |
| `eos_irq_register` | `int eos_irq_register(uint16_t irq, void (*handler)(void), uint8_t priority)` | Register ISR |
| `eos_irq_unregister` | `void eos_irq_unregister(uint16_t irq)` | Remove ISR |
| `eos_irq_set_priority` | `void eos_irq_set_priority(uint16_t irq, uint8_t priority)` | Change priority |

## 4.9 Backend Dispatch Pattern

The HAL achieves portability through a **backend vtable pattern**. Each platform
implements an `eos_hal_backend_t` struct with function pointers for every HAL operation.

### The Backend Structure

```c
typedef struct {
    const char *name;

    // System
    int  (*init)(void);
    void (*deinit)(void);
    void (*delay_ms)(uint32_t ms);
    uint32_t (*get_tick_ms)(void);

    // GPIO
    int  (*gpio_init)(const eos_gpio_config_t *cfg);
    void (*gpio_deinit)(uint16_t pin);
    void (*gpio_write)(uint16_t pin, bool value);
    bool (*gpio_read)(uint16_t pin);
    void (*gpio_toggle)(uint16_t pin);
    int  (*gpio_set_irq)(uint16_t pin, eos_gpio_edge_t edge,
                          eos_gpio_callback_t cb, void *ctx);

    // UART, SPI, I2C, Timer, Interrupt ...
    // (function pointers for each peripheral)
} eos_hal_backend_t;
```

### Registering a Backend

At startup, the platform-specific code registers its backend:

```c
// backends/stm32/stm32_hal.c
static const eos_hal_backend_t stm32_backend = {
    .name       = "stm32",
    .init       = stm32_hal_init,
    .deinit     = stm32_hal_deinit,
    .delay_ms   = stm32_delay_ms,
    .get_tick_ms = stm32_get_tick_ms,
    .gpio_init  = stm32_gpio_init,
    .gpio_write = stm32_gpio_write,
    .gpio_read  = stm32_gpio_read,
    .gpio_toggle = stm32_gpio_toggle,
    // ... all other operations
};

void stm32_register(void)
{
    eos_hal_register_backend(&stm32_backend);
}
```

### How Dispatch Works

When application code calls `eos_gpio_write(pin, true)`, the HAL dispatches:

```
eos_gpio_write(pin, true)
    └─→ backend->gpio_write(pin, true)
         └─→ stm32_gpio_write(pin, true)
              └─→ HAL_GPIO_WritePin(GPIOx, pin_mask, GPIO_PIN_SET)
```

## 4.10 Writing a Custom Backend

To port EoS to a new platform:

1. Create `backends/my_platform/my_hal.c`
2. Implement all function pointers in `eos_hal_backend_t`
3. Call `eos_hal_register_backend(&my_backend)` during platform init
4. Add the backend source to CMakeLists.txt

Minimal skeleton:

```c
#include <eos/hal.h>

static int my_init(void) { /* init clocks, peripherals */ return 0; }
static void my_deinit(void) { /* cleanup */ }
static void my_delay_ms(uint32_t ms) { /* platform delay */ }
static uint32_t my_get_tick_ms(void) { /* return ms counter */ return 0; }

static int  my_gpio_init(const eos_gpio_config_t *cfg) { return 0; }
static void my_gpio_write(uint16_t pin, bool val) { /* write register */ }
static bool my_gpio_read(uint16_t pin) { return false; }
static void my_gpio_toggle(uint16_t pin) { /* toggle register */ }
// ... implement remaining peripherals ...

static const eos_hal_backend_t my_backend = {
    .name       = "my_platform",
    .init       = my_init,
    .deinit     = my_deinit,
    .delay_ms   = my_delay_ms,
    .get_tick_ms = my_get_tick_ms,
    .gpio_init  = my_gpio_init,
    .gpio_write = my_gpio_write,
    .gpio_read  = my_gpio_read,
    .gpio_toggle = my_gpio_toggle,
};

void my_platform_register(void)
{
    eos_hal_register_backend(&my_backend);
}
```

---

*Next: [Chapter 5 — Extended HAL](ch05-hal-extended.md)*
