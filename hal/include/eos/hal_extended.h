// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file hal_extended.h
 * @brief EoS Extended HAL — Additional peripheral interfaces
 *
 * Provides unified APIs for ADC, DAC, PWM, CAN, USB, Ethernet, WiFi, BLE,
 * Camera, Audio, Display, Motor, GNSS, and IMU peripherals.
 *
 * Each peripheral is conditionally compiled based on EOS_ENABLE_* flags
 * from eos_config.h.
 */

#ifndef EOS_HAL_EXTENDED_H
#define EOS_HAL_EXTENDED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <eos/eos_config.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * ADC — Analog-to-Digital Converter
 * ============================================================ */
#if EOS_ENABLE_ADC

typedef struct {
    uint8_t  channel;
    uint8_t  resolution_bits;   /* 8, 10, 12, 16 */
    uint32_t sample_rate_hz;
} eos_adc_config_t;

int      eos_adc_init(const eos_adc_config_t *cfg);
void     eos_adc_deinit(uint8_t channel);
uint32_t eos_adc_read(uint8_t channel);
uint32_t eos_adc_read_mv(uint8_t channel);

#endif /* EOS_ENABLE_ADC */

/* ============================================================
 * DAC — Digital-to-Analog Converter
 * ============================================================ */
#if EOS_ENABLE_DAC

typedef struct {
    uint8_t  channel;
    uint8_t  resolution_bits;
    uint32_t vref_mv;
} eos_dac_config_t;

int  eos_dac_init(const eos_dac_config_t *cfg);
void eos_dac_deinit(uint8_t channel);
int  eos_dac_write(uint8_t channel, uint32_t value);
int  eos_dac_write_mv(uint8_t channel, uint32_t millivolts);

#endif /* EOS_ENABLE_DAC */

/* ============================================================
 * PWM — Pulse Width Modulation
 * ============================================================ */
#if EOS_ENABLE_PWM

typedef struct {
    uint8_t  channel;
    uint32_t frequency_hz;
    uint16_t duty_pct_x10;  /* 0–1000 = 0.0%–100.0% */
} eos_pwm_config_t;

int  eos_pwm_init(const eos_pwm_config_t *cfg);
void eos_pwm_deinit(uint8_t channel);
int  eos_pwm_set_duty(uint8_t channel, uint16_t duty_pct_x10);
int  eos_pwm_set_freq(uint8_t channel, uint32_t frequency_hz);
int  eos_pwm_start(uint8_t channel);
int  eos_pwm_stop(uint8_t channel);

#endif /* EOS_ENABLE_PWM */

/* ============================================================
 * CAN — Controller Area Network
 * ============================================================ */
#if EOS_ENABLE_CAN

typedef struct {
    uint8_t  port;
    uint32_t bitrate;       /* 125000, 250000, 500000, 1000000 */
    bool     loopback;
} eos_can_config_t;

typedef struct {
    uint32_t id;
    bool     extended;      /* true = 29-bit, false = 11-bit */
    bool     rtr;
    uint8_t  dlc;
    uint8_t  data[8];
} eos_can_msg_t;

int  eos_can_init(const eos_can_config_t *cfg);
void eos_can_deinit(uint8_t port);
int  eos_can_send(uint8_t port, const eos_can_msg_t *msg);
int  eos_can_receive(uint8_t port, eos_can_msg_t *msg, uint32_t timeout_ms);
int  eos_can_set_filter(uint8_t port, uint32_t id, uint32_t mask);

#endif /* EOS_ENABLE_CAN */

/* ============================================================
 * USB — Universal Serial Bus (device mode)
 * ============================================================ */
#if EOS_ENABLE_USB

typedef enum {
    EOS_USB_SPEED_LOW  = 0,
    EOS_USB_SPEED_FULL = 1,
    EOS_USB_SPEED_HIGH = 2,
} eos_usb_speed_t;

typedef struct {
    uint8_t         port;
    eos_usb_speed_t speed;
    uint16_t        vid;
    uint16_t        pid;
} eos_usb_config_t;

int  eos_usb_init(const eos_usb_config_t *cfg);
void eos_usb_deinit(uint8_t port);
int  eos_usb_write(uint8_t port, uint8_t ep, const uint8_t *data, size_t len);
int  eos_usb_read(uint8_t port, uint8_t ep, uint8_t *data, size_t len,
                   uint32_t timeout_ms);
bool eos_usb_is_connected(uint8_t port);

#endif /* EOS_ENABLE_USB */

/* ============================================================
 * Ethernet
 * ============================================================ */
#if EOS_ENABLE_ETHERNET

typedef struct {
    uint8_t  port;
    uint8_t  mac[6];
    bool     dhcp;
    uint32_t ip;        /* network byte order */
    uint32_t netmask;
    uint32_t gateway;
} eos_eth_config_t;

int  eos_eth_init(const eos_eth_config_t *cfg);
void eos_eth_deinit(uint8_t port);
int  eos_eth_send(uint8_t port, const uint8_t *data, size_t len);
int  eos_eth_receive(uint8_t port, uint8_t *data, size_t max_len,
                      uint32_t timeout_ms);
bool eos_eth_link_up(uint8_t port);

#endif /* EOS_ENABLE_ETHERNET */

/* ============================================================
 * WiFi
 * ============================================================ */
#if EOS_ENABLE_WIFI

typedef enum {
    EOS_WIFI_SEC_OPEN = 0,
    EOS_WIFI_SEC_WPA2 = 1,
    EOS_WIFI_SEC_WPA3 = 2,
} eos_wifi_security_t;

typedef struct {
    char                ssid[33];
    char                password[65];
    eos_wifi_security_t security;
} eos_wifi_config_t;

typedef struct {
    char    ssid[33];
    int8_t  rssi;
    uint8_t channel;
    eos_wifi_security_t security;
} eos_wifi_scan_result_t;

int  eos_wifi_init(void);
void eos_wifi_deinit(void);
int  eos_wifi_connect(const eos_wifi_config_t *cfg);
int  eos_wifi_disconnect(void);
bool eos_wifi_is_connected(void);
int  eos_wifi_scan(eos_wifi_scan_result_t *results, size_t max_results,
                    size_t *found);
int  eos_wifi_get_ip(uint32_t *ip);
int  eos_wifi_send(const uint8_t *data, size_t len);
int  eos_wifi_receive(uint8_t *data, size_t max_len, uint32_t timeout_ms);

#endif /* EOS_ENABLE_WIFI */

/* ============================================================
 * BLE — Bluetooth Low Energy
 * ============================================================ */
#if EOS_ENABLE_BLE

typedef struct {
    char     device_name[32];
    uint8_t  tx_power_dbm;
    uint16_t adv_interval_ms;
} eos_ble_config_t;

typedef void (*eos_ble_rx_callback_t)(const uint8_t *data, size_t len, void *ctx);

int  eos_ble_init(const eos_ble_config_t *cfg);
void eos_ble_deinit(void);
int  eos_ble_advertise_start(void);
int  eos_ble_advertise_stop(void);
int  eos_ble_connect(const uint8_t addr[6]);
int  eos_ble_disconnect(void);
bool eos_ble_is_connected(void);
int  eos_ble_send(const uint8_t *data, size_t len);
int  eos_ble_set_rx_callback(eos_ble_rx_callback_t cb, void *ctx);

#endif /* EOS_ENABLE_BLE */

/* ============================================================
 * Camera
 * ============================================================ */
#if EOS_ENABLE_CAMERA

typedef enum {
    EOS_CAMERA_FMT_RGB565  = 0,
    EOS_CAMERA_FMT_RGB888  = 1,
    EOS_CAMERA_FMT_YUV422  = 2,
    EOS_CAMERA_FMT_JPEG    = 3,
    EOS_CAMERA_FMT_GRAY8   = 4,
} eos_camera_format_t;

typedef struct {
    uint8_t             id;
    uint16_t            width;
    uint16_t            height;
    eos_camera_format_t format;
    uint8_t             fps;
} eos_camera_config_t;

typedef struct {
    uint8_t  *data;
    size_t    size;
    uint16_t  width;
    uint16_t  height;
    eos_camera_format_t format;
    uint32_t  timestamp_ms;
} eos_camera_frame_t;

int  eos_camera_init(const eos_camera_config_t *cfg);
void eos_camera_deinit(uint8_t id);
int  eos_camera_capture(uint8_t id, eos_camera_frame_t *frame);
int  eos_camera_start_stream(uint8_t id);
int  eos_camera_stop_stream(uint8_t id);

#endif /* EOS_ENABLE_CAMERA */

/* ============================================================
 * Audio
 * ============================================================ */
#if EOS_ENABLE_AUDIO

typedef enum {
    EOS_AUDIO_FMT_PCM_8  = 0,
    EOS_AUDIO_FMT_PCM_16 = 1,
    EOS_AUDIO_FMT_PCM_24 = 2,
    EOS_AUDIO_FMT_PCM_32 = 3,
} eos_audio_format_t;

typedef struct {
    uint8_t             id;
    uint32_t            sample_rate;
    uint8_t             channels;
    eos_audio_format_t  format;
} eos_audio_config_t;

int  eos_audio_init(const eos_audio_config_t *cfg);
void eos_audio_deinit(uint8_t id);
int  eos_audio_play(uint8_t id, const uint8_t *data, size_t len);
int  eos_audio_record(uint8_t id, uint8_t *buf, size_t len,
                       uint32_t timeout_ms);
int  eos_audio_set_volume(uint8_t id, uint8_t volume_pct);
int  eos_audio_mute(uint8_t id, bool mute);

#endif /* EOS_ENABLE_AUDIO */

/* ============================================================
 * Display
 * ============================================================ */
#if EOS_ENABLE_DISPLAY

typedef enum {
    EOS_DISPLAY_COLOR_1BPP  = 0,
    EOS_DISPLAY_COLOR_RGB565 = 1,
    EOS_DISPLAY_COLOR_RGB888 = 2,
} eos_display_color_t;

typedef struct {
    uint8_t             id;
    uint16_t            width;
    uint16_t            height;
    eos_display_color_t color_mode;
} eos_display_config_t;

int  eos_display_init(const eos_display_config_t *cfg);
void eos_display_deinit(uint8_t id);
int  eos_display_draw_pixel(uint8_t id, uint16_t x, uint16_t y,
                             uint32_t color);
int  eos_display_draw_rect(uint8_t id, uint16_t x, uint16_t y,
                            uint16_t w, uint16_t h, uint32_t color);
int  eos_display_draw_bitmap(uint8_t id, uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h,
                              const uint8_t *data);
int  eos_display_flush(uint8_t id);
int  eos_display_clear(uint8_t id, uint32_t color);
int  eos_display_set_brightness(uint8_t id, uint8_t brightness_pct);

#endif /* EOS_ENABLE_DISPLAY */

/* ============================================================
 * Motor Control (HAL-level)
 * ============================================================ */
#if EOS_ENABLE_MOTOR

typedef enum {
    EOS_MOTOR_DC      = 0,
    EOS_MOTOR_STEPPER = 1,
    EOS_MOTOR_SERVO   = 2,
    EOS_MOTOR_BLDC    = 3,
} eos_motor_type_t;

typedef struct {
    uint8_t          id;
    eos_motor_type_t type;
    uint16_t         pwm_pin;
    uint16_t         dir_pin;
    uint16_t         enable_pin;
    uint16_t         encoder_pin_a;
    uint16_t         encoder_pin_b;
} eos_motor_config_t;

int  eos_motor_init(const eos_motor_config_t *cfg);
void eos_motor_deinit(uint8_t id);
int  eos_motor_set_speed(uint8_t id, int16_t speed_pct);  /* -100 to +100 */
int  eos_motor_set_position(uint8_t id, int32_t position);
int  eos_motor_brake(uint8_t id);
int  eos_motor_coast(uint8_t id);

#endif /* EOS_ENABLE_MOTOR */

/* ============================================================
 * GPS / GNSS
 * ============================================================ */
#if EOS_ENABLE_GNSS

typedef struct {
    uint8_t  port;
    uint32_t baudrate;
} eos_gnss_config_t;

typedef struct {
    double   latitude;
    double   longitude;
    float    altitude_m;
    float    speed_mps;
    float    heading_deg;
    uint8_t  satellites;
    bool     fix_valid;
    uint32_t utc_time;   /* seconds since midnight */
} eos_gnss_position_t;

int  eos_gnss_init(const eos_gnss_config_t *cfg);
void eos_gnss_deinit(void);
int  eos_gnss_get_position(eos_gnss_position_t *pos);
bool eos_gnss_has_fix(void);

#endif /* EOS_ENABLE_GNSS */

/* ============================================================
 * IMU — Inertial Measurement Unit
 * ============================================================ */
#if EOS_ENABLE_IMU

typedef struct {
    uint8_t  id;
    uint8_t  i2c_port;
    uint16_t i2c_addr;
    uint16_t sample_rate_hz;
} eos_imu_config_t;

typedef struct {
    float x, y, z;
} eos_imu_vec3_t;

int  eos_imu_init(const eos_imu_config_t *cfg);
void eos_imu_deinit(uint8_t id);
int  eos_imu_read_accel(uint8_t id, eos_imu_vec3_t *accel);
int  eos_imu_read_gyro(uint8_t id, eos_imu_vec3_t *gyro);
int  eos_imu_read_mag(uint8_t id, eos_imu_vec3_t *mag);
int  eos_imu_read_temp(uint8_t id, float *temp_c);

#endif /* EOS_ENABLE_IMU */

/* ============================================================
 * Touch / Input Panel
 * ============================================================ */
#if EOS_ENABLE_TOUCH

typedef enum {
    EOS_TOUCH_RESISTIVE   = 0,
    EOS_TOUCH_CAPACITIVE  = 1,
} eos_touch_type_t;

typedef struct {
    uint8_t          id;
    eos_touch_type_t type;
    uint16_t         width;
    uint16_t         height;
    uint8_t          max_points;
    uint8_t          i2c_port;
    uint16_t         i2c_addr;
} eos_touch_config_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t pressure;
    uint8_t  id;
    bool     pressed;
} eos_touch_point_t;

typedef void (*eos_touch_callback_t)(const eos_touch_point_t *points,
                                      uint8_t count, void *ctx);

int  eos_touch_init(const eos_touch_config_t *cfg);
void eos_touch_deinit(uint8_t id);
int  eos_touch_read(uint8_t id, eos_touch_point_t *points, uint8_t max_points,
                     uint8_t *count);
int  eos_touch_set_callback(uint8_t id, eos_touch_callback_t cb, void *ctx);
int  eos_touch_calibrate(uint8_t id);

#endif /* EOS_ENABLE_TOUCH */

/* ============================================================
 * RTC — Real-Time Clock
 * ============================================================ */
#if EOS_ENABLE_RTC

typedef struct {
    uint16_t year;
    uint8_t  month;   /* 1–12 */
    uint8_t  day;     /* 1–31 */
    uint8_t  hour;    /* 0–23 */
    uint8_t  minute;  /* 0–59 */
    uint8_t  second;  /* 0–59 */
    uint8_t  weekday; /* 0=Sun, 6=Sat */
} eos_rtc_time_t;

typedef void (*eos_rtc_alarm_callback_t)(void *ctx);

int  eos_rtc_init(void);
void eos_rtc_deinit(void);
int  eos_rtc_set_time(const eos_rtc_time_t *time);
int  eos_rtc_get_time(eos_rtc_time_t *time);
int  eos_rtc_set_alarm(const eos_rtc_time_t *alarm,
                        eos_rtc_alarm_callback_t cb, void *ctx);
int  eos_rtc_cancel_alarm(void);
uint32_t eos_rtc_get_unix_timestamp(void);

#endif /* EOS_ENABLE_RTC */

/* ============================================================
 * DMA — Direct Memory Access
 * ============================================================ */
#if EOS_ENABLE_DMA

typedef enum {
    EOS_DMA_MEM_TO_MEM    = 0,
    EOS_DMA_MEM_TO_PERIPH = 1,
    EOS_DMA_PERIPH_TO_MEM = 2,
} eos_dma_direction_t;

typedef enum {
    EOS_DMA_WIDTH_8  = 0,
    EOS_DMA_WIDTH_16 = 1,
    EOS_DMA_WIDTH_32 = 2,
} eos_dma_data_width_t;

typedef struct {
    uint8_t              channel;
    eos_dma_direction_t  direction;
    eos_dma_data_width_t data_width;
    bool                 circular;
    uint8_t              priority;  /* 0=low, 3=very high */
} eos_dma_config_t;

typedef void (*eos_dma_callback_t)(uint8_t channel, bool error, void *ctx);

int  eos_dma_init(const eos_dma_config_t *cfg);
void eos_dma_deinit(uint8_t channel);
int  eos_dma_start(uint8_t channel, const void *src, void *dst, size_t len);
int  eos_dma_stop(uint8_t channel);
bool eos_dma_busy(uint8_t channel);
size_t eos_dma_remaining(uint8_t channel);
int  eos_dma_set_callback(uint8_t channel, eos_dma_callback_t cb, void *ctx);

#endif /* EOS_ENABLE_DMA */

/* ============================================================
 * Flash / EEPROM — Non-volatile Storage
 * ============================================================ */
#if EOS_ENABLE_FLASH

typedef struct {
    uint8_t  id;
    uint32_t base_addr;
    uint32_t total_size;
    uint32_t sector_size;
    uint32_t page_size;
} eos_flash_config_t;

typedef struct {
    uint32_t total_size;
    uint32_t sector_size;
    uint32_t page_size;
    uint32_t sector_count;
} eos_flash_info_t;

int  eos_flash_init(const eos_flash_config_t *cfg);
void eos_flash_deinit(uint8_t id);
int  eos_flash_read(uint8_t id, uint32_t addr, void *buf, size_t len);
int  eos_flash_write(uint8_t id, uint32_t addr, const void *data, size_t len);
int  eos_flash_erase_sector(uint8_t id, uint32_t sector_addr);
int  eos_flash_erase_chip(uint8_t id);
int  eos_flash_get_info(uint8_t id, eos_flash_info_t *info);

#endif /* EOS_ENABLE_FLASH */

/* ============================================================
 * Hardware Watchdog Timer
 * ============================================================ */
#if EOS_ENABLE_WDT

typedef struct {
    uint32_t timeout_ms;
    bool     reset_on_expire; /* true = system reset, false = interrupt */
} eos_wdt_config_t;

typedef void (*eos_wdt_callback_t)(void *ctx);

int  eos_wdt_init(const eos_wdt_config_t *cfg);
void eos_wdt_deinit(void);
int  eos_wdt_start(void);
int  eos_wdt_stop(void);
void eos_wdt_feed(void);
int  eos_wdt_set_callback(eos_wdt_callback_t cb, void *ctx);

#endif /* EOS_ENABLE_WDT */

/* ============================================================
 * NFC — Near Field Communication
 * ============================================================ */
#if EOS_ENABLE_NFC

typedef enum {
    EOS_NFC_MODE_READER    = 0,
    EOS_NFC_MODE_EMULATOR  = 1,
    EOS_NFC_MODE_P2P       = 2,
} eos_nfc_mode_t;

typedef struct {
    uint8_t        id;
    eos_nfc_mode_t mode;
    uint8_t        i2c_port;
    uint16_t       i2c_addr;
} eos_nfc_config_t;

typedef struct {
    uint8_t  uid[10];
    uint8_t  uid_len;
    uint8_t  type;
} eos_nfc_tag_t;

int  eos_nfc_init(const eos_nfc_config_t *cfg);
void eos_nfc_deinit(uint8_t id);
int  eos_nfc_detect_tag(uint8_t id, eos_nfc_tag_t *tag, uint32_t timeout_ms);
int  eos_nfc_read_tag(uint8_t id, uint8_t block, uint8_t *data, size_t len);
int  eos_nfc_write_tag(uint8_t id, uint8_t block, const uint8_t *data,
                        size_t len);
int  eos_nfc_send_ndef(uint8_t id, const uint8_t *payload, size_t len);

#endif /* EOS_ENABLE_NFC */

/* ============================================================
 * IR — Infrared Transmitter/Receiver
 * ============================================================ */
#if EOS_ENABLE_IR

typedef enum {
    EOS_IR_PROTO_NEC    = 0,
    EOS_IR_PROTO_RC5    = 1,
    EOS_IR_PROTO_RC6    = 2,
    EOS_IR_PROTO_SONY   = 3,
    EOS_IR_PROTO_RAW    = 4,
} eos_ir_protocol_t;

typedef struct {
    uint8_t          id;
    uint16_t         tx_pin;
    uint16_t         rx_pin;
    uint32_t         carrier_hz; /* typically 38000 */
} eos_ir_config_t;

typedef struct {
    eos_ir_protocol_t protocol;
    uint32_t          address;
    uint32_t          command;
    bool              repeat;
} eos_ir_code_t;

typedef void (*eos_ir_rx_callback_t)(const eos_ir_code_t *code, void *ctx);

int  eos_ir_init(const eos_ir_config_t *cfg);
void eos_ir_deinit(uint8_t id);
int  eos_ir_transmit(uint8_t id, const eos_ir_code_t *code);
int  eos_ir_transmit_raw(uint8_t id, const uint16_t *timings, size_t count);
int  eos_ir_set_rx_callback(uint8_t id, eos_ir_rx_callback_t cb, void *ctx);

#endif /* EOS_ENABLE_IR */

/* ============================================================
 * Cellular / Modem
 * ============================================================ */
#if EOS_ENABLE_CELLULAR

typedef enum {
    EOS_CELL_2G  = 0,
    EOS_CELL_3G  = 1,
    EOS_CELL_4G  = 2,
    EOS_CELL_5G  = 3,
    EOS_CELL_NB  = 4,  /* NB-IoT */
    EOS_CELL_CAT_M = 5, /* LTE Cat-M */
} eos_cellular_tech_t;

typedef struct {
    uint8_t             port;       /* UART port to modem */
    uint32_t            baudrate;
    eos_cellular_tech_t technology;
    char                apn[64];
    char                pin[8];
} eos_cellular_config_t;

typedef struct {
    eos_cellular_tech_t tech;
    int8_t              rssi;
    uint8_t             signal_bars; /* 0–5 */
    bool                registered;
    char                operator_name[32];
} eos_cellular_status_t;

int  eos_cellular_init(const eos_cellular_config_t *cfg);
void eos_cellular_deinit(void);
int  eos_cellular_connect(void);
int  eos_cellular_disconnect(void);
bool eos_cellular_is_connected(void);
int  eos_cellular_get_status(eos_cellular_status_t *status);
int  eos_cellular_send(const uint8_t *data, size_t len);
int  eos_cellular_receive(uint8_t *data, size_t max_len, uint32_t timeout_ms);
int  eos_cellular_sms_send(const char *number, const char *message);

#endif /* EOS_ENABLE_CELLULAR */

/* ============================================================
 * Radar / Lidar — Distance & Object Sensing
 * ============================================================ */
#if EOS_ENABLE_RADAR

typedef enum {
    EOS_RADAR_ULTRASONIC = 0,
    EOS_RADAR_LIDAR      = 1,
    EOS_RADAR_MMWAVE     = 2,
    EOS_RADAR_TOF        = 3,  /* Time-of-Flight */
} eos_radar_type_t;

typedef struct {
    uint8_t          id;
    eos_radar_type_t type;
    uint16_t         trigger_pin;
    uint16_t         echo_pin;
    uint8_t          i2c_port;
    uint16_t         i2c_addr;
} eos_radar_config_t;

typedef struct {
    float    distance_mm;
    float    velocity_mps;
    float    angle_deg;
    uint8_t  signal_strength;
    bool     valid;
} eos_radar_measurement_t;

int  eos_radar_init(const eos_radar_config_t *cfg);
void eos_radar_deinit(uint8_t id);
int  eos_radar_measure(uint8_t id, eos_radar_measurement_t *result);
int  eos_radar_start_continuous(uint8_t id, uint32_t interval_ms);
int  eos_radar_stop_continuous(uint8_t id);

#endif /* EOS_ENABLE_RADAR */

/* ============================================================
 * GPU / Graphics Accelerator
 * ============================================================ */
#if EOS_ENABLE_GPU

typedef enum {
    EOS_GPU_2D       = 0,
    EOS_GPU_3D       = 1,
    EOS_GPU_COMPUTE  = 2,
} eos_gpu_type_t;

typedef struct {
    uint8_t       id;
    eos_gpu_type_t type;
    uint32_t      vram_size;
    uint32_t      clock_mhz;
} eos_gpu_config_t;

int  eos_gpu_init(const eos_gpu_config_t *cfg);
void eos_gpu_deinit(uint8_t id);
int  eos_gpu_submit(uint8_t id, const void *cmd_buf, size_t len);
int  eos_gpu_wait_idle(uint8_t id);
int  eos_gpu_alloc(uint8_t id, size_t size, void **ptr);
int  eos_gpu_free(uint8_t id, void *ptr);

#endif /* EOS_ENABLE_GPU */

/* ============================================================
 * HDMI / Video Output
 * ============================================================ */
#if EOS_ENABLE_HDMI

typedef enum {
    EOS_HDMI_RES_720P  = 0,
    EOS_HDMI_RES_1080P = 1,
    EOS_HDMI_RES_4K    = 2,
    EOS_HDMI_RES_8K    = 3,
} eos_hdmi_resolution_t;

typedef struct {
    uint8_t              id;
    eos_hdmi_resolution_t resolution;
    uint8_t              refresh_hz;
    bool                 audio_enabled;
} eos_hdmi_config_t;

int  eos_hdmi_init(const eos_hdmi_config_t *cfg);
void eos_hdmi_deinit(uint8_t id);
bool eos_hdmi_is_connected(uint8_t id);
int  eos_hdmi_set_resolution(uint8_t id, eos_hdmi_resolution_t res, uint8_t hz);
int  eos_hdmi_output_frame(uint8_t id, const void *framebuffer, size_t size);

#endif /* EOS_ENABLE_HDMI */

/* ============================================================
 * PCIe — PCI Express
 * ============================================================ */
#if EOS_ENABLE_PCIE

typedef struct {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  max_lanes;
} eos_pcie_config_t;

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  revision;
    uint8_t  link_speed;
    uint8_t  link_width;
} eos_pcie_device_info_t;

int  eos_pcie_init(const eos_pcie_config_t *cfg);
void eos_pcie_deinit(uint8_t bus, uint8_t device);
int  eos_pcie_enumerate(eos_pcie_device_info_t *devices, size_t max,
                         size_t *found);
int  eos_pcie_read_config(uint8_t bus, uint8_t dev, uint8_t func,
                           uint16_t offset, uint32_t *value);
int  eos_pcie_write_config(uint8_t bus, uint8_t dev, uint8_t func,
                            uint16_t offset, uint32_t value);
int  eos_pcie_map_bar(uint8_t bus, uint8_t dev, uint8_t bar,
                       void **vaddr, size_t *size);

#endif /* EOS_ENABLE_PCIE */

/* ============================================================
 * SDIO — SD Card / eMMC Interface
 * ============================================================ */
#if EOS_ENABLE_SDIO

typedef enum {
    EOS_SDIO_SD    = 0,
    EOS_SDIO_SDHC  = 1,
    EOS_SDIO_SDXC  = 2,
    EOS_SDIO_EMMC  = 3,
} eos_sdio_type_t;

typedef struct {
    uint8_t        id;
    uint8_t        bus_width;   /* 1, 4, or 8 */
    uint32_t       clock_hz;
} eos_sdio_config_t;

typedef struct {
    eos_sdio_type_t type;
    uint64_t        capacity_bytes;
    uint32_t        block_size;
    char            product_name[8];
} eos_sdio_info_t;

int  eos_sdio_init(const eos_sdio_config_t *cfg);
void eos_sdio_deinit(uint8_t id);
bool eos_sdio_is_inserted(uint8_t id);
int  eos_sdio_get_info(uint8_t id, eos_sdio_info_t *info);
int  eos_sdio_read_blocks(uint8_t id, uint32_t block, void *buf,
                           uint32_t count);
int  eos_sdio_write_blocks(uint8_t id, uint32_t block, const void *data,
                            uint32_t count);

#endif /* EOS_ENABLE_SDIO */

/* ============================================================
 * Haptics — Vibration / Tactile Feedback
 * ============================================================ */
#if EOS_ENABLE_HAPTICS

typedef enum {
    EOS_HAPTIC_ERM  = 0,   /* Eccentric Rotating Mass */
    EOS_HAPTIC_LRA  = 1,   /* Linear Resonant Actuator */
    EOS_HAPTIC_PIEZO = 2,  /* Piezoelectric */
} eos_haptic_type_t;

typedef struct {
    uint8_t          id;
    eos_haptic_type_t type;
    uint16_t         pwm_pin;
    uint16_t         enable_pin;
} eos_haptic_config_t;

int  eos_haptic_init(const eos_haptic_config_t *cfg);
void eos_haptic_deinit(uint8_t id);
int  eos_haptic_play(uint8_t id, uint8_t intensity, uint32_t duration_ms);
int  eos_haptic_play_pattern(uint8_t id, const uint8_t *pattern,
                              size_t len, uint32_t step_ms);
int  eos_haptic_stop(uint8_t id);

#endif /* EOS_ENABLE_HAPTICS */

/* ============================================================
 * Extended HAL Backend Registration
 *
 * Hardware vendors implement this vtable to provide their own
 * driver for any extended peripheral. This avoids modifying
 * the EoS stub files — just register your backend at init time.
 *
 * Usage:
 *   static const eos_hal_ext_backend_t my_nrf_ble = {
 *       .name = "nrf52_ble",
 *       .ble_init           = nrf_ble_init,
 *       .ble_deinit         = nrf_ble_deinit,
 *       .ble_advertise_start = nrf_ble_adv_start,
 *       .ble_advertise_stop  = nrf_ble_adv_stop,
 *       .ble_send           = nrf_ble_send,
 *       .ble_set_rx_callback = nrf_ble_set_rx_cb,
 *   };
 *   eos_hal_register_ext_backend(&my_nrf_ble);
 * ============================================================ */

typedef struct {
    const char *name;

#if EOS_ENABLE_BLE
    int  (*ble_init)(const eos_ble_config_t *cfg);
    void (*ble_deinit)(void);
    int  (*ble_advertise_start)(void);
    int  (*ble_advertise_stop)(void);
    int  (*ble_connect)(const uint8_t addr[6]);
    int  (*ble_disconnect)(void);
    bool (*ble_is_connected)(void);
    int  (*ble_send)(const uint8_t *data, size_t len);
    int  (*ble_set_rx_callback)(eos_ble_rx_callback_t cb, void *ctx);
#endif

#if EOS_ENABLE_WIFI
    int  (*wifi_init)(const eos_wifi_config_t *cfg);
    void (*wifi_deinit)(void);
    int  (*wifi_connect)(const char *ssid, const char *password);
    int  (*wifi_disconnect)(void);
    int  (*wifi_send)(const uint8_t *data, size_t len);
    int  (*wifi_receive)(uint8_t *data, size_t max_len, uint32_t timeout_ms);
#endif

#if EOS_ENABLE_CAMERA
    int  (*camera_init)(const eos_camera_config_t *cfg);
    void (*camera_deinit)(uint8_t id);
    int  (*camera_capture)(uint8_t id, eos_camera_frame_t *frame);
#endif

#if EOS_ENABLE_DISPLAY
    int  (*display_init)(const eos_display_config_t *cfg);
    void (*display_deinit)(uint8_t id);
    int  (*display_draw_pixel)(uint8_t id, uint16_t x, uint16_t y, uint32_t color);
    int  (*display_draw_rect)(uint8_t id, uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h, uint32_t color);
    int  (*display_draw_bitmap)(uint8_t id, uint16_t x, uint16_t y,
                                uint16_t w, uint16_t h, const uint8_t *data);
    int  (*display_flush)(uint8_t id);
    int  (*display_clear)(uint8_t id, uint32_t color);
    int  (*display_set_brightness)(uint8_t id, uint8_t brightness_pct);
#endif

#if EOS_ENABLE_TOUCH
    int  (*touch_init)(const eos_touch_config_t *cfg);
    void (*touch_deinit)(uint8_t id);
    int  (*touch_read)(uint8_t id, eos_touch_point_t *points,
                       uint8_t max_points, uint8_t *count);
    int  (*touch_set_callback)(uint8_t id, eos_touch_callback_t cb, void *ctx);
    int  (*touch_calibrate)(uint8_t id);
#endif

#if EOS_ENABLE_MOTOR
    int  (*motor_init)(const eos_motor_config_t *cfg);
    void (*motor_deinit)(uint8_t id);
    int  (*motor_set_speed)(uint8_t id, int16_t speed_pct);
    int  (*motor_brake)(uint8_t id);
#endif

#if EOS_ENABLE_GNSS
    int  (*gnss_init)(const eos_gnss_config_t *cfg);
    void (*gnss_deinit)(void);
    int  (*gnss_get_position)(eos_gnss_position_t *pos);
#endif

#if EOS_ENABLE_IMU
    int  (*imu_init)(const eos_imu_config_t *cfg);
    void (*imu_deinit)(uint8_t id);
    int  (*imu_read_accel)(uint8_t id, eos_imu_vec3_t *data);
    int  (*imu_read_gyro)(uint8_t id, eos_imu_vec3_t *data);
#endif

#if EOS_ENABLE_AUDIO
    int  (*audio_init)(const eos_audio_config_t *cfg);
    void (*audio_deinit)(uint8_t id);
    int  (*audio_play)(uint8_t id, const uint8_t *data, size_t len);
    int  (*audio_record)(uint8_t id, uint8_t *buf, size_t len, uint32_t timeout_ms);
#endif

#if EOS_ENABLE_GPU
    int  (*gpu_init)(const eos_gpu_config_t *cfg);
    void (*gpu_deinit)(uint8_t id);
    int  (*gpu_submit)(uint8_t id, const void *cmd_buf, size_t len);
    int  (*gpu_wait_idle)(uint8_t id);
    int  (*gpu_alloc)(uint8_t id, size_t size, void **ptr);
    int  (*gpu_free)(uint8_t id, void *ptr);
#endif
} eos_hal_ext_backend_t;

/**
 * Register an extended HAL backend. Vendors call this to plug in their
 * hardware-specific drivers for BLE, WiFi, Camera, Display, etc.
 * Multiple backends can be registered — later registrations override earlier ones
 * for the same peripheral.
 *
 * @param backend  Pointer to backend vtable (must remain valid for program lifetime).
 * @return 0 on success, negative error code on failure.
 */
int eos_hal_register_ext_backend(const eos_hal_ext_backend_t *backend);

/**
 * Get the currently active extended HAL backend.
 *
 * @return Pointer to the active extended backend, or NULL if none registered.
 */
const eos_hal_ext_backend_t *eos_hal_get_ext_backend(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_HAL_EXTENDED_H */
