# Chapter 5: Extended HAL

*Srikanth Patchava & EmbeddedOS Contributors*

---

## 5.1 Overview

The Extended HAL (`hal_extended.h`) provides 27 additional peripheral interfaces beyond
the core GPIO, UART, SPI, I2C, and Timer covered in Chapter 4. Each peripheral is
**conditionally compiled** based on `EOS_ENABLE_*` flags set by the product profile.

### Complete Peripheral List

| # | Peripheral | Enable Flag | Category |
|---|-----------|-------------|----------|
| 1 | ADC | `EOS_ENABLE_ADC` | Analog |
| 2 | DAC | `EOS_ENABLE_DAC` | Analog |
| 3 | PWM | `EOS_ENABLE_PWM` | Analog |
| 4 | CAN | `EOS_ENABLE_CAN` | Bus |
| 5 | USB | `EOS_ENABLE_USB` | Bus |
| 6 | Ethernet | `EOS_ENABLE_ETHERNET` | Networking |
| 7 | WiFi | `EOS_ENABLE_WIFI` | Networking |
| 8 | BLE | `EOS_ENABLE_BLE` | Networking |
| 9 | Camera | `EOS_ENABLE_CAMERA` | Media |
| 10 | Audio | `EOS_ENABLE_AUDIO` | Media |
| 11 | Display | `EOS_ENABLE_DISPLAY` | Media |
| 12 | Motor | `EOS_ENABLE_MOTOR` | Actuators |
| 13 | GNSS | `EOS_ENABLE_GNSS` | Sensors |
| 14 | IMU | `EOS_ENABLE_IMU` | Sensors |
| 15 | Touch | `EOS_ENABLE_TOUCH` | Input |
| 16 | RTC | `EOS_ENABLE_RTC` | Timekeeping |
| 17 | DMA | `EOS_ENABLE_DMA` | System |
| 18 | Flash | `EOS_ENABLE_FLASH` | Storage |
| 19 | WDT | `EOS_ENABLE_WDT` | System |
| 20 | NFC | `EOS_ENABLE_NFC` | Wireless |
| 21 | IR | `EOS_ENABLE_IR` | Wireless |
| 22 | Cellular | `EOS_ENABLE_CELLULAR` | Networking |
| 23 | Radar | `EOS_ENABLE_RADAR` | Sensors |
| 24 | GPU | `EOS_ENABLE_GPU` | Graphics |
| 25 | HDMI | `EOS_ENABLE_HDMI` | Graphics |
| 26 | PCIe | `EOS_ENABLE_PCIE` | Bus |
| 27 | SDIO | `EOS_ENABLE_SDIO` | Storage |
| 28 | Haptics | `EOS_ENABLE_HAPTICS` | Actuators |

## 5.2 Analog Peripherals

### ADC — Analog-to-Digital Converter

```c
typedef struct {
    uint8_t  channel;          // ADC channel number
    uint8_t  resolution_bits;  // 8, 10, 12, or 16
    uint32_t sample_rate_hz;   // Samples per second
} eos_adc_config_t;
```

| Function | Description |
|----------|-------------|
| `eos_adc_init(cfg)` | Initialize ADC channel |
| `eos_adc_deinit(ch)` | Release ADC channel |
| `eos_adc_read(ch)` | Read raw ADC value |
| `eos_adc_read_mv(ch)` | Read in millivolts |

**Example: Reading a potentiometer**

```c
eos_adc_config_t adc = {
    .channel         = 0,
    .resolution_bits = 12,
    .sample_rate_hz  = 1000,
};
eos_adc_init(&adc);

uint32_t raw = eos_adc_read(0);       // 0–4095 for 12-bit
uint32_t mv  = eos_adc_read_mv(0);    // Millivolts
printf("ADC: raw=%u  voltage=%u mV\n", raw, mv);
```

### DAC — Digital-to-Analog Converter

| Function | Description |
|----------|-------------|
| `eos_dac_init(cfg)` | Initialize DAC channel |
| `eos_dac_deinit(ch)` | Release DAC channel |
| `eos_dac_write(ch, val)` | Write raw DAC value |
| `eos_dac_write_mv(ch, mv)` | Write in millivolts |

### PWM — Pulse Width Modulation

```c
typedef struct {
    uint8_t  channel;        // PWM channel
    uint32_t frequency_hz;   // PWM frequency
    uint16_t duty_pct_x10;   // 0–1000 (0.0%–100.0%)
} eos_pwm_config_t;
```

| Function | Description |
|----------|-------------|
| `eos_pwm_init(cfg)` | Initialize PWM channel |
| `eos_pwm_set_duty(ch, pct)` | Set duty cycle (0–1000) |
| `eos_pwm_set_freq(ch, hz)` | Change frequency |
| `eos_pwm_start(ch)` | Start PWM output |
| `eos_pwm_stop(ch)` | Stop PWM output |

**Example: LED dimming**

```c
eos_pwm_config_t pwm = {
    .channel      = 0,
    .frequency_hz = 1000,
    .duty_pct_x10 = 500,  // 50.0%
};
eos_pwm_init(&pwm);
eos_pwm_start(0);

// Fade from 0% to 100%
for (uint16_t d = 0; d <= 1000; d += 10) {
    eos_pwm_set_duty(0, d);
    eos_delay_ms(20);
}
```

## 5.3 Bus Interfaces

### CAN — Controller Area Network

```c
typedef struct {
    uint32_t id;          // CAN identifier
    bool     extended;    // true = 29-bit, false = 11-bit
    bool     rtr;         // Remote Transmission Request
    uint8_t  dlc;         // Data Length Code (0–8)
    uint8_t  data[8];     // Payload
} eos_can_msg_t;
```

| Function | Description |
|----------|-------------|
| `eos_can_init(cfg)` | Initialize CAN port |
| `eos_can_send(port, msg)` | Transmit CAN frame |
| `eos_can_receive(port, msg, timeout)` | Receive CAN frame |
| `eos_can_set_filter(port, id, mask)` | Set acceptance filter |

### USB — Universal Serial Bus (Device Mode)

| Function | Description |
|----------|-------------|
| `eos_usb_init(cfg)` | Initialize USB device |
| `eos_usb_write(port, ep, data, len)` | Write to endpoint |
| `eos_usb_read(port, ep, buf, len, timeout)` | Read from endpoint |
| `eos_usb_is_connected(port)` | Check connection status |

## 5.4 Networking Interfaces

### WiFi

```c
typedef struct {
    char                ssid[33];
    char                password[65];
    eos_wifi_security_t security;  // OPEN, WPA2, WPA3
} eos_wifi_config_t;
```

**Complete WiFi workflow:**

```c
eos_wifi_init();

eos_wifi_config_t wifi = {
    .ssid     = "MyNetwork",
    .password = "secret123",
    .security = EOS_WIFI_SEC_WPA2,
};
eos_wifi_connect(&wifi);

// Wait for connection
while (!eos_wifi_is_connected()) {
    eos_delay_ms(100);
}

uint32_t ip;
eos_wifi_get_ip(&ip);
printf("Connected! IP: %d.%d.%d.%d\n",
    ip & 0xFF, (ip >> 8) & 0xFF,
    (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
```

### BLE — Bluetooth Low Energy

| Function | Description |
|----------|-------------|
| `eos_ble_init(cfg)` | Initialize BLE stack |
| `eos_ble_advertise_start()` | Start advertising |
| `eos_ble_advertise_stop()` | Stop advertising |
| `eos_ble_connect(addr)` | Connect to peripheral |
| `eos_ble_disconnect()` | Disconnect |
| `eos_ble_is_connected()` | Check connection |
| `eos_ble_send(data, len)` | Transmit data |
| `eos_ble_set_rx_callback(cb, ctx)` | Register RX handler |

### Cellular / Modem

Supports 2G through 5G, NB-IoT, and LTE Cat-M:

```c
typedef enum {
    EOS_CELL_2G    = 0,
    EOS_CELL_3G    = 1,
    EOS_CELL_4G    = 2,
    EOS_CELL_5G    = 3,
    EOS_CELL_NB    = 4,  // NB-IoT
    EOS_CELL_CAT_M = 5,  // LTE Cat-M
} eos_cellular_tech_t;
```

| Function | Description |
|----------|-------------|
| `eos_cellular_init(cfg)` | Initialize modem |
| `eos_cellular_connect()` | Establish data connection |
| `eos_cellular_get_status(status)` | Get signal/registration info |
| `eos_cellular_send(data, len)` | Send data |
| `eos_cellular_sms_send(number, msg)` | Send SMS |

## 5.5 Media Interfaces

### Camera

```c
typedef struct {
    uint8_t             id;
    uint16_t            width;
    uint16_t            height;
    eos_camera_format_t format;   // RGB565, RGB888, YUV422, JPEG, GRAY8
    uint8_t             fps;
} eos_camera_config_t;

typedef struct {
    uint8_t  *data;
    size_t    size;
    uint16_t  width, height;
    eos_camera_format_t format;
    uint32_t  timestamp_ms;
} eos_camera_frame_t;
```

### Audio

| Function | Description |
|----------|-------------|
| `eos_audio_init(cfg)` | Initialize audio codec |
| `eos_audio_play(id, data, len)` | Play audio buffer |
| `eos_audio_record(id, buf, len, timeout)` | Record audio |
| `eos_audio_set_volume(id, pct)` | Set volume (0–100%) |
| `eos_audio_mute(id, mute)` | Mute/unmute |

### Display

```c
int eos_display_init(const eos_display_config_t *cfg);
int eos_display_draw_pixel(uint8_t id, uint16_t x, uint16_t y, uint32_t color);
int eos_display_draw_rect(uint8_t id, uint16_t x, uint16_t y,
                           uint16_t w, uint16_t h, uint32_t color);
int eos_display_draw_bitmap(uint8_t id, uint16_t x, uint16_t y,
                             uint16_t w, uint16_t h, const uint8_t *data);
int eos_display_flush(uint8_t id);
int eos_display_clear(uint8_t id, uint32_t color);
int eos_display_set_brightness(uint8_t id, uint8_t brightness_pct);
```

## 5.6 Sensor Interfaces

### GNSS (GPS)

```c
typedef struct {
    double   latitude;
    double   longitude;
    float    altitude_m;
    float    speed_mps;
    float    heading_deg;
    uint8_t  satellites;
    bool     fix_valid;
    uint32_t utc_time;
} eos_gnss_position_t;
```

### IMU — Inertial Measurement Unit

```c
typedef struct { float x, y, z; } eos_imu_vec3_t;

int eos_imu_read_accel(uint8_t id, eos_imu_vec3_t *accel);
int eos_imu_read_gyro(uint8_t id, eos_imu_vec3_t *gyro);
int eos_imu_read_mag(uint8_t id, eos_imu_vec3_t *mag);
int eos_imu_read_temp(uint8_t id, float *temp_c);
```

### Radar / Lidar

Supports ultrasonic, LiDAR, mmWave, and Time-of-Flight sensors:

```c
typedef struct {
    float    distance_mm;
    float    velocity_mps;
    float    angle_deg;
    uint8_t  signal_strength;
    bool     valid;
} eos_radar_measurement_t;
```

## 5.7 System & Storage Peripherals

### DMA — Direct Memory Access

```c
typedef struct {
    uint8_t              channel;
    eos_dma_direction_t  direction;   // MEM_TO_MEM, MEM_TO_PERIPH, PERIPH_TO_MEM
    eos_dma_data_width_t data_width;  // 8, 16, or 32 bit
    bool                 circular;
    uint8_t              priority;    // 0=low, 3=very high
} eos_dma_config_t;
```

### Flash / EEPROM

| Function | Description |
|----------|-------------|
| `eos_flash_init(cfg)` | Initialize flash device |
| `eos_flash_read(id, addr, buf, len)` | Read from flash |
| `eos_flash_write(id, addr, data, len)` | Write to flash |
| `eos_flash_erase_sector(id, addr)` | Erase a sector |
| `eos_flash_erase_chip(id)` | Full chip erase |
| `eos_flash_get_info(id, info)` | Get flash geometry |

### RTC — Real-Time Clock

```c
typedef struct {
    uint16_t year;
    uint8_t  month, day;
    uint8_t  hour, minute, second;
    uint8_t  weekday;  // 0=Sun, 6=Sat
} eos_rtc_time_t;
```

### Watchdog Timer

| Function | Description |
|----------|-------------|
| `eos_wdt_init(cfg)` | Configure watchdog |
| `eos_wdt_start()` | Start watchdog |
| `eos_wdt_feed()` | Reset/kick watchdog counter |
| `eos_wdt_stop()` | Stop watchdog |

## 5.8 The Extended Backend vtable

Hardware vendors implement the `eos_hal_ext_backend_t` vtable to provide their own
drivers for extended peripherals.

```c
typedef struct {
    const char *name;

    // BLE operations (conditionally compiled)
    int  (*ble_init)(const eos_ble_config_t *cfg);
    void (*ble_deinit)(void);
    int  (*ble_advertise_start)(void);
    int  (*ble_send)(const uint8_t *data, size_t len);

    // WiFi operations
    int  (*wifi_init)(const eos_wifi_config_t *cfg);
    int  (*wifi_connect)(const char *ssid, const char *password);

    // Camera, Display, Touch, Motor, GNSS, IMU, Audio, GPU, HDMI...
    // (each peripheral has its own set of function pointers)
} eos_hal_ext_backend_t;
```

### Registering an Extended Backend

```c
static const eos_hal_ext_backend_t my_nrf_ble = {
    .name               = "nrf52_ble",
    .ble_init           = nrf_ble_init,
    .ble_deinit         = nrf_ble_deinit,
    .ble_advertise_start = nrf_ble_adv_start,
    .ble_advertise_stop  = nrf_ble_adv_stop,
    .ble_send           = nrf_ble_send,
    .ble_set_rx_callback = nrf_ble_set_rx_cb,
};

void nrf_platform_init(void)
{
    eos_hal_register_ext_backend(&my_nrf_ble);
}
```

Multiple backends can be registered — later registrations override earlier ones for
the same peripheral. This allows vendor-specific optimizations while maintaining the
standard API.

### Retrieving the Active Backend

```c
const eos_hal_ext_backend_t *ext = eos_hal_get_ext_backend();
if (ext && ext->ble_init) {
    ext->ble_init(&ble_cfg);
}
```

## 5.9 Conditional Compilation

Each extended peripheral is guarded by an `EOS_ENABLE_*` preprocessor flag. If the
flag is not defined (or is 0), the peripheral code is completely excluded from the
binary.

This is critical for resource-constrained targets where every kilobyte matters.

**Example: A product that only needs BLE and Display**

```c
// products/my_wearable.h
#define EOS_ENABLE_BLE     1
#define EOS_ENABLE_DISPLAY 1
// All other EOS_ENABLE_* are implicitly 0
```

Only BLE and Display code is compiled. CAN, USB, Camera, GPU, etc. add zero bytes.

## 5.10 Summary

The Extended HAL gives EoS coverage across the full spectrum of embedded hardware:

| Category | Peripherals |
|----------|------------|
| Analog | ADC, DAC, PWM |
| Bus | CAN, USB, PCIe, SDIO |
| Networking | WiFi, BLE, Ethernet, Cellular |
| Media | Camera, Audio, Display, HDMI |
| Sensors | IMU, GNSS, Radar |
| Input | Touch, NFC, IR |
| System | DMA, Flash, RTC, WDT |
| Actuators | Motor, Haptics |
| Graphics | GPU, HDMI |

---

*Next: [Chapter 6 — RTOS Kernel](ch06-kernel.md)*
