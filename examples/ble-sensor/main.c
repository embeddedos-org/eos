// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file main.c
 * @brief EoS Example: BLE Sensor — Real IoT application
 *
 * Reads temperature from an I2C sensor, computes a CRC checksum,
 * and advertises the data over BLE. Demonstrates a complete IoT
 * sensor node using HAL (BLE, I2C, GPIO), kernel tasks, and crypto.
 */

#include <eos/hal.h>
#include <eos/hal_extended.h>
#include <eos/kernel.h>
#include <eos/crypto.h>
#include <stdio.h>
#include <string.h>

#define LED_PIN       13
#define I2C_PORT      0
#define SENSOR_ADDR   0x48    /* TMP102 / LM75 temperature sensor */
#define TEMP_REG      0x00
#define SAMPLE_MS     2000

typedef struct {
    int16_t  temperature_x100;  /* temperature in 0.01°C units */
    uint32_t sequence;
    uint32_t crc;
} sensor_packet_t;

static volatile uint32_t g_sequence = 0;

static int16_t read_temperature(void)
{
    uint8_t raw[2] = {0};
    int ret = eos_i2c_read_reg(I2C_PORT, SENSOR_ADDR, TEMP_REG, raw, 2);
    if (ret < 0) {
        printf("[ble-sensor] I2C read failed: %d\n", ret);
        return 0;
    }

    /* Convert raw bytes to temperature (0.01°C units) */
    int16_t raw_temp = (int16_t)((raw[0] << 8) | raw[1]) >> 4;
    return (int16_t)(raw_temp * 625 / 100);  /* 0.0625°C per LSB → 0.01°C */
}

static void sensor_task(void *arg)
{
    (void)arg;

    printf("[ble-sensor] Sensor task started\n");

    while (1) {
        /* Read temperature */
        int16_t temp = read_temperature();

        /* Build BLE advertisement packet */
        sensor_packet_t pkt = {
            .temperature_x100 = temp,
            .sequence = g_sequence++,
            .crc = 0,
        };

        /* Compute CRC over the data fields */
        pkt.crc = eos_crc32(0, &pkt, offsetof(sensor_packet_t, crc));

        /* Send over BLE */
        int ret = eos_ble_send((const uint8_t *)&pkt, sizeof(pkt));
        if (ret == 0) {
            printf("[ble-sensor] TX seq=%lu temp=%d.%02d°C crc=0x%08lx\n",
                   (unsigned long)pkt.sequence,
                   pkt.temperature_x100 / 100,
                   pkt.temperature_x100 % 100,
                   (unsigned long)pkt.crc);
        }

        /* Blink LED to indicate activity */
        eos_gpio_toggle(LED_PIN);

        eos_task_delay_ms(SAMPLE_MS);
    }
}

static void ble_rx_handler(const uint8_t *data, size_t len, void *ctx)
{
    (void)ctx;
    printf("[ble-sensor] RX %zu bytes: ", len);
    for (size_t i = 0; i < len && i < 16; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
}

int main(void)
{
    eos_hal_init();
    eos_kernel_init();

    /* Configure LED */
    eos_gpio_config_t led_cfg = {
        .pin   = LED_PIN,
        .mode  = EOS_GPIO_OUTPUT,
        .pull  = EOS_GPIO_PULL_NONE,
        .speed = EOS_GPIO_SPEED_LOW,
    };
    eos_gpio_init(&led_cfg);

    /* Configure I2C for temperature sensor */
    eos_i2c_config_t i2c_cfg = {
        .port     = I2C_PORT,
        .clock_hz = 400000,  /* 400 kHz fast mode */
        .own_addr = 0,
    };
    eos_i2c_init(&i2c_cfg);

    /* Configure BLE */
    eos_ble_config_t ble_cfg = {
        .device_name    = "EoS-Sensor",
        .tx_power_dbm   = 0,
        .adv_interval_ms = 1000,
    };
    eos_ble_init(&ble_cfg);
    eos_ble_set_rx_callback(ble_rx_handler, NULL);
    eos_ble_advertise_start();

    printf("[ble-sensor] BLE advertising as '%s'\n", ble_cfg.device_name);

    /* Create sensor task */
    eos_task_create("sensor", sensor_task, NULL, 2, 1024);

    printf("[ble-sensor] Starting kernel...\n");
    eos_kernel_start();

    return 0;
}
