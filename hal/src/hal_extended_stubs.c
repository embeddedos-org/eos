// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file hal_extended_stubs.c
 * @brief EoS Extended HAL — Stub implementations for all extended peripherals
 *
 * Provides default stub implementations that return -1 (not supported).
 * Platform backends override these with real implementations.
 */

#include <eos/hal_extended.h>
#include <string.h>

/* ============================================================
 * Extended Backend Registration
 * ============================================================ */

static const eos_hal_ext_backend_t *s_ext_backend = NULL;

int eos_hal_register_ext_backend(const eos_hal_ext_backend_t *backend)
{
    if (!backend) return -1;
    s_ext_backend = backend;
    return 0;
}

const eos_hal_ext_backend_t *eos_hal_get_ext_backend(void)
{
    return s_ext_backend;
}

/* ---- ADC ---- */
#if EOS_ENABLE_ADC

int eos_adc_init(const eos_adc_config_t *cfg)
{
    (void)cfg;
    return -1;
}

void eos_adc_deinit(uint8_t channel)
{
    (void)channel;
}

uint32_t eos_adc_read(uint8_t channel)
{
    (void)channel;
    return 0;
}

uint32_t eos_adc_read_mv(uint8_t channel)
{
    (void)channel;
    return 0;
}

#endif /* EOS_ENABLE_ADC */

/* ---- DAC ---- */
#if EOS_ENABLE_DAC

int eos_dac_init(const eos_dac_config_t *cfg)
{
    (void)cfg;
    return -1;
}

void eos_dac_deinit(uint8_t channel)
{
    (void)channel;
}

int eos_dac_write(uint8_t channel, uint32_t value)
{
    (void)channel; (void)value;
    return -1;
}

int eos_dac_write_mv(uint8_t channel, uint32_t millivolts)
{
    (void)channel; (void)millivolts;
    return -1;
}

#endif /* EOS_ENABLE_DAC */

/* ---- PWM ---- */
#if EOS_ENABLE_PWM

int eos_pwm_init(const eos_pwm_config_t *cfg)
{
    (void)cfg;
    return -1;
}

void eos_pwm_deinit(uint8_t channel)
{
    (void)channel;
}

int eos_pwm_set_duty(uint8_t channel, uint16_t duty_pct_x10)
{
    (void)channel; (void)duty_pct_x10;
    return -1;
}

int eos_pwm_set_freq(uint8_t channel, uint32_t frequency_hz)
{
    (void)channel; (void)frequency_hz;
    return -1;
}

int eos_pwm_start(uint8_t channel)
{
    (void)channel;
    return -1;
}

int eos_pwm_stop(uint8_t channel)
{
    (void)channel;
    return -1;
}

#endif /* EOS_ENABLE_PWM */

/* ---- CAN ---- */
#if EOS_ENABLE_CAN

int eos_can_init(const eos_can_config_t *cfg)
{
    (void)cfg;
    return -1;
}

void eos_can_deinit(uint8_t port)
{
    (void)port;
}

int eos_can_send(uint8_t port, const eos_can_msg_t *msg)
{
    (void)port; (void)msg;
    return -1;
}

int eos_can_receive(uint8_t port, eos_can_msg_t *msg, uint32_t timeout_ms)
{
    (void)port; (void)msg; (void)timeout_ms;
    return -1;
}

int eos_can_set_filter(uint8_t port, uint32_t id, uint32_t mask)
{
    (void)port; (void)id; (void)mask;
    return -1;
}

#endif /* EOS_ENABLE_CAN */

/* ---- USB ---- */
#if EOS_ENABLE_USB

int eos_usb_init(const eos_usb_config_t *cfg)
{
    (void)cfg;
    return -1;
}

void eos_usb_deinit(uint8_t port)
{
    (void)port;
}

int eos_usb_write(uint8_t port, uint8_t ep, const uint8_t *data, size_t len)
{
    (void)port; (void)ep; (void)data; (void)len;
    return -1;
}

int eos_usb_read(uint8_t port, uint8_t ep, uint8_t *data, size_t len,
                  uint32_t timeout_ms)
{
    (void)port; (void)ep; (void)data; (void)len; (void)timeout_ms;
    return -1;
}

bool eos_usb_is_connected(uint8_t port)
{
    (void)port;
    return false;
}

#endif /* EOS_ENABLE_USB */

/* ---- Ethernet ---- */
#if EOS_ENABLE_ETHERNET

int eos_eth_init(const eos_eth_config_t *cfg)
{
    (void)cfg;
    return -1;
}

void eos_eth_deinit(uint8_t port)
{
    (void)port;
}

int eos_eth_send(uint8_t port, const uint8_t *data, size_t len)
{
    (void)port; (void)data; (void)len;
    return -1;
}

int eos_eth_receive(uint8_t port, uint8_t *data, size_t max_len,
                     uint32_t timeout_ms)
{
    (void)port; (void)data; (void)max_len; (void)timeout_ms;
    return -1;
}

bool eos_eth_link_up(uint8_t port)
{
    (void)port;
    return false;
}

#endif /* EOS_ENABLE_ETHERNET */

/* ---- WiFi ---- */
#if EOS_ENABLE_WIFI

int eos_wifi_init(void) { return -1; }
void eos_wifi_deinit(void) {}
int eos_wifi_connect(const eos_wifi_config_t *cfg) { (void)cfg; return -1; }
int eos_wifi_disconnect(void) { return -1; }
bool eos_wifi_is_connected(void) { return false; }

int eos_wifi_scan(eos_wifi_scan_result_t *results, size_t max_results,
                   size_t *found)
{
    (void)results; (void)max_results;
    if (found) *found = 0;
    return -1;
}

int eos_wifi_get_ip(uint32_t *ip) { (void)ip; return -1; }

int eos_wifi_send(const uint8_t *data, size_t len)
{
    (void)data; (void)len;
    return -1;
}

int eos_wifi_receive(uint8_t *data, size_t max_len, uint32_t timeout_ms)
{
    (void)data; (void)max_len; (void)timeout_ms;
    return -1;
}

#endif /* EOS_ENABLE_WIFI */

/* ---- BLE ---- */
#if EOS_ENABLE_BLE

int eos_ble_init(const eos_ble_config_t *cfg) { (void)cfg; return -1; }
void eos_ble_deinit(void) {}
int eos_ble_advertise_start(void) { return -1; }
int eos_ble_advertise_stop(void) { return -1; }
int eos_ble_connect(const uint8_t addr[6]) { (void)addr; return -1; }
int eos_ble_disconnect(void) { return -1; }
bool eos_ble_is_connected(void) { return false; }
int eos_ble_send(const uint8_t *data, size_t len) { (void)data; (void)len; return -1; }

int eos_ble_set_rx_callback(eos_ble_rx_callback_t cb, void *ctx)
{
    (void)cb; (void)ctx;
    return -1;
}

#endif /* EOS_ENABLE_BLE */

/* ---- Camera ---- */
#if EOS_ENABLE_CAMERA

int eos_camera_init(const eos_camera_config_t *cfg) { (void)cfg; return -1; }
void eos_camera_deinit(uint8_t id) { (void)id; }

int eos_camera_capture(uint8_t id, eos_camera_frame_t *frame)
{
    (void)id; (void)frame;
    return -1;
}

int eos_camera_start_stream(uint8_t id) { (void)id; return -1; }
int eos_camera_stop_stream(uint8_t id) { (void)id; return -1; }

#endif /* EOS_ENABLE_CAMERA */

/* ---- Audio ---- */
#if EOS_ENABLE_AUDIO

int eos_audio_init(const eos_audio_config_t *cfg) { (void)cfg; return -1; }
void eos_audio_deinit(uint8_t id) { (void)id; }

int eos_audio_play(uint8_t id, const uint8_t *data, size_t len)
{
    (void)id; (void)data; (void)len;
    return -1;
}

int eos_audio_record(uint8_t id, uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    (void)id; (void)buf; (void)len; (void)timeout_ms;
    return -1;
}

int eos_audio_set_volume(uint8_t id, uint8_t volume_pct)
{
    (void)id; (void)volume_pct;
    return -1;
}

int eos_audio_mute(uint8_t id, bool mute)
{
    (void)id; (void)mute;
    return -1;
}

#endif /* EOS_ENABLE_AUDIO */

/* ---- Display ---- */
#if EOS_ENABLE_DISPLAY

int eos_display_init(const eos_display_config_t *cfg) { (void)cfg; return -1; }
void eos_display_deinit(uint8_t id) { (void)id; }

int eos_display_draw_pixel(uint8_t id, uint16_t x, uint16_t y, uint32_t color)
{
    (void)id; (void)x; (void)y; (void)color;
    return -1;
}

int eos_display_draw_rect(uint8_t id, uint16_t x, uint16_t y,
                           uint16_t w, uint16_t h, uint32_t color)
{
    (void)id; (void)x; (void)y; (void)w; (void)h; (void)color;
    return -1;
}

int eos_display_draw_bitmap(uint8_t id, uint16_t x, uint16_t y,
                             uint16_t w, uint16_t h, const uint8_t *data)
{
    (void)id; (void)x; (void)y; (void)w; (void)h; (void)data;
    return -1;
}

int eos_display_flush(uint8_t id) { (void)id; return -1; }
int eos_display_clear(uint8_t id, uint32_t color) { (void)id; (void)color; return -1; }

int eos_display_set_brightness(uint8_t id, uint8_t brightness_pct)
{
    (void)id; (void)brightness_pct;
    return -1;
}

#endif /* EOS_ENABLE_DISPLAY */

/* ---- Motor ---- */
#if EOS_ENABLE_MOTOR

int eos_motor_init(const eos_motor_config_t *cfg) { (void)cfg; return -1; }
void eos_motor_deinit(uint8_t id) { (void)id; }

int eos_motor_set_speed(uint8_t id, int16_t speed_pct)
{
    (void)id; (void)speed_pct;
    return -1;
}

int eos_motor_set_position(uint8_t id, int32_t position)
{
    (void)id; (void)position;
    return -1;
}

int eos_motor_brake(uint8_t id) { (void)id; return -1; }
int eos_motor_coast(uint8_t id) { (void)id; return -1; }

#endif /* EOS_ENABLE_MOTOR */

/* ---- GNSS ---- */
#if EOS_ENABLE_GNSS

int eos_gnss_init(const eos_gnss_config_t *cfg) { (void)cfg; return -1; }
void eos_gnss_deinit(void) {}

int eos_gnss_get_position(eos_gnss_position_t *pos)
{
    (void)pos;
    return -1;
}

bool eos_gnss_has_fix(void) { return false; }

#endif /* EOS_ENABLE_GNSS */

/* ---- IMU ---- */
#if EOS_ENABLE_IMU

int eos_imu_init(const eos_imu_config_t *cfg) { (void)cfg; return -1; }
void eos_imu_deinit(uint8_t id) { (void)id; }

int eos_imu_read_accel(uint8_t id, eos_imu_vec3_t *accel)
{
    (void)id; (void)accel;
    return -1;
}

int eos_imu_read_gyro(uint8_t id, eos_imu_vec3_t *gyro)
{
    (void)id; (void)gyro;
    return -1;
}

int eos_imu_read_mag(uint8_t id, eos_imu_vec3_t *mag)
{
    (void)id; (void)mag;
    return -1;
}

int eos_imu_read_temp(uint8_t id, float *temp_c)
{
    (void)id; (void)temp_c;
    return -1;
}

#endif /* EOS_ENABLE_IMU */

/* ---- Touch ---- */
#if EOS_ENABLE_TOUCH

int eos_touch_init(const eos_touch_config_t *cfg) { (void)cfg; return -1; }
void eos_touch_deinit(uint8_t id) { (void)id; }

int eos_touch_read(uint8_t id, eos_touch_point_t *points, uint8_t max_points,
                    uint8_t *count)
{
    (void)id; (void)points; (void)max_points;
    if (count) *count = 0;
    return -1;
}

int eos_touch_set_callback(uint8_t id, eos_touch_callback_t cb, void *ctx)
{
    (void)id; (void)cb; (void)ctx;
    return -1;
}

int eos_touch_calibrate(uint8_t id) { (void)id; return -1; }

#endif /* EOS_ENABLE_TOUCH */

/* ---- RTC ---- */
#if EOS_ENABLE_RTC

int eos_rtc_init(void) { return -1; }
void eos_rtc_deinit(void) {}

int eos_rtc_set_time(const eos_rtc_time_t *time) { (void)time; return -1; }

int eos_rtc_get_time(eos_rtc_time_t *time)
{
    (void)time;
    return -1;
}

int eos_rtc_set_alarm(const eos_rtc_time_t *alarm,
                       eos_rtc_alarm_callback_t cb, void *ctx)
{
    (void)alarm; (void)cb; (void)ctx;
    return -1;
}

int eos_rtc_cancel_alarm(void) { return -1; }
uint32_t eos_rtc_get_unix_timestamp(void) { return 0; }

#endif /* EOS_ENABLE_RTC */

/* ---- DMA ---- */
#if EOS_ENABLE_DMA

int eos_dma_init(const eos_dma_config_t *cfg) { (void)cfg; return -1; }
void eos_dma_deinit(uint8_t channel) { (void)channel; }

int eos_dma_start(uint8_t channel, const void *src, void *dst, size_t len)
{
    (void)channel; (void)src; (void)dst; (void)len;
    return -1;
}

int eos_dma_stop(uint8_t channel) { (void)channel; return -1; }
bool eos_dma_busy(uint8_t channel) { (void)channel; return false; }
size_t eos_dma_remaining(uint8_t channel) { (void)channel; return 0; }

int eos_dma_set_callback(uint8_t channel, eos_dma_callback_t cb, void *ctx)
{
    (void)channel; (void)cb; (void)ctx;
    return -1;
}

#endif /* EOS_ENABLE_DMA */

/* ---- Flash ---- */
#if EOS_ENABLE_FLASH

int eos_flash_init(const eos_flash_config_t *cfg) { (void)cfg; return -1; }
void eos_flash_deinit(uint8_t id) { (void)id; }

int eos_flash_read(uint8_t id, uint32_t addr, void *buf, size_t len)
{
    (void)id; (void)addr; (void)buf; (void)len;
    return -1;
}

int eos_flash_write(uint8_t id, uint32_t addr, const void *data, size_t len)
{
    (void)id; (void)addr; (void)data; (void)len;
    return -1;
}

int eos_flash_erase_sector(uint8_t id, uint32_t sector_addr)
{
    (void)id; (void)sector_addr;
    return -1;
}

int eos_flash_erase_chip(uint8_t id) { (void)id; return -1; }

int eos_flash_get_info(uint8_t id, eos_flash_info_t *info)
{
    (void)id; (void)info;
    return -1;
}

#endif /* EOS_ENABLE_FLASH */

/* ---- Watchdog ---- */
#if EOS_ENABLE_WDT

int eos_wdt_init(const eos_wdt_config_t *cfg) { (void)cfg; return -1; }
void eos_wdt_deinit(void) {}
int eos_wdt_start(void) { return -1; }
int eos_wdt_stop(void) { return -1; }
void eos_wdt_feed(void) {}

int eos_wdt_set_callback(eos_wdt_callback_t cb, void *ctx)
{
    (void)cb; (void)ctx;
    return -1;
}

#endif /* EOS_ENABLE_WDT */

/* ---- NFC ---- */
#if EOS_ENABLE_NFC

int eos_nfc_init(const eos_nfc_config_t *cfg) { (void)cfg; return -1; }
void eos_nfc_deinit(uint8_t id) { (void)id; }

int eos_nfc_detect_tag(uint8_t id, eos_nfc_tag_t *tag, uint32_t timeout_ms)
{
    (void)id; (void)tag; (void)timeout_ms;
    return -1;
}

int eos_nfc_read_tag(uint8_t id, uint8_t block, uint8_t *data, size_t len)
{
    (void)id; (void)block; (void)data; (void)len;
    return -1;
}

int eos_nfc_write_tag(uint8_t id, uint8_t block, const uint8_t *data, size_t len)
{
    (void)id; (void)block; (void)data; (void)len;
    return -1;
}

int eos_nfc_send_ndef(uint8_t id, const uint8_t *payload, size_t len)
{
    (void)id; (void)payload; (void)len;
    return -1;
}

#endif /* EOS_ENABLE_NFC */

/* ---- IR ---- */
#if EOS_ENABLE_IR

int eos_ir_init(const eos_ir_config_t *cfg) { (void)cfg; return -1; }
void eos_ir_deinit(uint8_t id) { (void)id; }

int eos_ir_transmit(uint8_t id, const eos_ir_code_t *code)
{
    (void)id; (void)code;
    return -1;
}

int eos_ir_transmit_raw(uint8_t id, const uint16_t *timings, size_t count)
{
    (void)id; (void)timings; (void)count;
    return -1;
}

int eos_ir_set_rx_callback(uint8_t id, eos_ir_rx_callback_t cb, void *ctx)
{
    (void)id; (void)cb; (void)ctx;
    return -1;
}

#endif /* EOS_ENABLE_IR */

/* ---- Cellular ---- */
#if EOS_ENABLE_CELLULAR

int eos_cellular_init(const eos_cellular_config_t *cfg) { (void)cfg; return -1; }
void eos_cellular_deinit(void) {}
int eos_cellular_connect(void) { return -1; }
int eos_cellular_disconnect(void) { return -1; }
bool eos_cellular_is_connected(void) { return false; }

int eos_cellular_get_status(eos_cellular_status_t *status)
{
    (void)status;
    return -1;
}

int eos_cellular_send(const uint8_t *data, size_t len)
{
    (void)data; (void)len;
    return -1;
}

int eos_cellular_receive(uint8_t *data, size_t max_len, uint32_t timeout_ms)
{
    (void)data; (void)max_len; (void)timeout_ms;
    return -1;
}

int eos_cellular_sms_send(const char *number, const char *message)
{
    (void)number; (void)message;
    return -1;
}

#endif /* EOS_ENABLE_CELLULAR */

/* ---- Radar / Lidar ---- */
#if EOS_ENABLE_RADAR

int eos_radar_init(const eos_radar_config_t *cfg) { (void)cfg; return -1; }
void eos_radar_deinit(uint8_t id) { (void)id; }

int eos_radar_measure(uint8_t id, eos_radar_measurement_t *result)
{
    (void)id; (void)result;
    return -1;
}

int eos_radar_start_continuous(uint8_t id, uint32_t interval_ms)
{
    (void)id; (void)interval_ms;
    return -1;
}

int eos_radar_stop_continuous(uint8_t id) { (void)id; return -1; }

#endif /* EOS_ENABLE_RADAR */

/* ---- GPU ---- */
#if EOS_ENABLE_GPU

int eos_gpu_init(const eos_gpu_config_t *cfg) { (void)cfg; return -1; }
void eos_gpu_deinit(uint8_t id) { (void)id; }
int eos_gpu_submit(uint8_t id, const void *cmd_buf, size_t len) { (void)id; (void)cmd_buf; (void)len; return -1; }
int eos_gpu_wait_idle(uint8_t id) { (void)id; return -1; }
int eos_gpu_alloc(uint8_t id, size_t size, void **ptr) { (void)id; (void)size; (void)ptr; return -1; }
int eos_gpu_free(uint8_t id, void *ptr) { (void)id; (void)ptr; return -1; }

#endif /* EOS_ENABLE_GPU */

/* ---- HDMI ---- */
#if EOS_ENABLE_HDMI

int eos_hdmi_init(const eos_hdmi_config_t *cfg) { (void)cfg; return -1; }
void eos_hdmi_deinit(uint8_t id) { (void)id; }
bool eos_hdmi_is_connected(uint8_t id) { (void)id; return false; }
int eos_hdmi_set_resolution(uint8_t id, eos_hdmi_resolution_t res, uint8_t hz) { (void)id; (void)res; (void)hz; return -1; }
int eos_hdmi_output_frame(uint8_t id, const void *framebuffer, size_t size) { (void)id; (void)framebuffer; (void)size; return -1; }

#endif /* EOS_ENABLE_HDMI */

/* ---- PCIe ---- */
#if EOS_ENABLE_PCIE

int eos_pcie_init(const eos_pcie_config_t *cfg) { (void)cfg; return -1; }
void eos_pcie_deinit(uint8_t bus, uint8_t device) { (void)bus; (void)device; }
int eos_pcie_enumerate(eos_pcie_device_info_t *devices, size_t max, size_t *found) { (void)devices; (void)max; if (found) *found = 0; return -1; }
int eos_pcie_read_config(uint8_t bus, uint8_t dev, uint8_t func, uint16_t offset, uint32_t *value) { (void)bus; (void)dev; (void)func; (void)offset; (void)value; return -1; }
int eos_pcie_write_config(uint8_t bus, uint8_t dev, uint8_t func, uint16_t offset, uint32_t value) { (void)bus; (void)dev; (void)func; (void)offset; (void)value; return -1; }
int eos_pcie_map_bar(uint8_t bus, uint8_t dev, uint8_t bar, void **vaddr, size_t *size) { (void)bus; (void)dev; (void)bar; (void)vaddr; (void)size; return -1; }

#endif /* EOS_ENABLE_PCIE */

/* ---- SDIO ---- */
#if EOS_ENABLE_SDIO

int eos_sdio_init(const eos_sdio_config_t *cfg) { (void)cfg; return -1; }
void eos_sdio_deinit(uint8_t id) { (void)id; }
bool eos_sdio_is_inserted(uint8_t id) { (void)id; return false; }
int eos_sdio_get_info(uint8_t id, eos_sdio_info_t *info) { (void)id; (void)info; return -1; }
int eos_sdio_read_blocks(uint8_t id, uint32_t block, void *buf, uint32_t count) { (void)id; (void)block; (void)buf; (void)count; return -1; }
int eos_sdio_write_blocks(uint8_t id, uint32_t block, const void *data, uint32_t count) { (void)id; (void)block; (void)data; (void)count; return -1; }

#endif /* EOS_ENABLE_SDIO */

/* ---- Haptics ---- */
#if EOS_ENABLE_HAPTICS

int eos_haptic_init(const eos_haptic_config_t *cfg) { (void)cfg; return -1; }
void eos_haptic_deinit(uint8_t id) { (void)id; }
int eos_haptic_play(uint8_t id, uint8_t intensity, uint32_t duration_ms) { (void)id; (void)intensity; (void)duration_ms; return -1; }
int eos_haptic_play_pattern(uint8_t id, const uint8_t *pattern, size_t len, uint32_t step_ms) { (void)id; (void)pattern; (void)len; (void)step_ms; return -1; }
int eos_haptic_stop(uint8_t id) { (void)id; return -1; }

#endif /* EOS_ENABLE_HAPTICS */
