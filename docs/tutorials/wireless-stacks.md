# Wireless Protocols Tutorial

Configure and use BLE, WiFi, and LoRa wireless stacks on EoS. Each section
includes EoS API code snippets and expected UART output.

> **Prerequisites:** Build EoS with a product profile that enables wireless
> (e.g., `iot`, `gateway`, `smart_home`). See
> [Configuration Examples](configuration-examples.md).

---

## Bluetooth Low Energy (BLE)

EoS provides a GAP/GATT abstraction over the BLE radio. Enable with
`EOS_ENABLE_BLE 1` in your product profile.

### GAP — Advertising

GAP (Generic Access Profile) controls device discovery and connection
establishment.

```c
#include <eos/ble.h>

static const eos_ble_adv_params_t adv_params = {
    .interval_min_ms = 100,
    .interval_max_ms = 200,
    .type            = EOS_BLE_ADV_CONNECTABLE_UNDIRECTED,
    .channel_map     = EOS_BLE_ADV_CH_ALL,  // channels 37, 38, 39
    .filter_policy   = EOS_BLE_ADV_FILTER_NONE,
};

static const uint8_t adv_data[] = {
    0x02, 0x01, 0x06,                   // flags: LE General Discoverable
    0x0B, 0x09, 'E','o','S','-','S','e','n','s','o','r',  // complete local name
    0x03, 0x03, 0x1A, 0x18,             // 16-bit UUID: Environmental Sensing
};

int ble_start_advertising(void) {
    int rc = eos_ble_init();
    if (rc != 0) {
        eos_log(EOS_LOG_ERROR, "BLE", "Init failed: %d", rc);
        return rc;
    }

    rc = eos_ble_gap_set_adv_data(adv_data, sizeof(adv_data));
    if (rc != 0) return rc;

    rc = eos_ble_gap_start_adv(&adv_params);
    eos_log(EOS_LOG_INFO, "BLE", "Advertising started (100–200 ms interval)");
    return rc;
}
```

Expected UART output:

```
[  0.050] BLE  : BLE stack initialized (SoftDevice / HCI)
[  0.051] BLE  : Advertising started (100–200 ms interval)
[  5.320] BLE  : GAP event: CONNECTED (peer=AA:BB:CC:DD:EE:FF, conn_handle=0)
```

### GATT — Service & Characteristics

GATT (Generic Attribute Profile) defines the data model exposed over BLE.

```c
// Define a custom Environmental Sensing service
static eos_ble_gatt_service_t env_service;
static eos_ble_gatt_char_t temp_char;
static eos_ble_gatt_char_t humidity_char;

static uint8_t temp_value[2]     = {0};  // int16, 0.01 °C resolution
static uint8_t humidity_value[2] = {0};  // uint16, 0.01 % resolution

void gatt_setup(void) {
    // Register service (UUID 0x181A = Environmental Sensing)
    eos_ble_gatt_service_init(&env_service, EOS_BLE_UUID16(0x181A),
                               EOS_BLE_GATT_PRIMARY);

    // Temperature characteristic (UUID 0x2A6E)
    eos_ble_gatt_char_init(&temp_char, EOS_BLE_UUID16(0x2A6E),
                            EOS_BLE_GATT_PROP_READ | EOS_BLE_GATT_PROP_NOTIFY,
                            temp_value, sizeof(temp_value));

    // Humidity characteristic (UUID 0x2A6F)
    eos_ble_gatt_char_init(&humidity_char, EOS_BLE_UUID16(0x2A6F),
                            EOS_BLE_GATT_PROP_READ | EOS_BLE_GATT_PROP_NOTIFY,
                            humidity_value, sizeof(humidity_value));

    eos_ble_gatt_service_add_char(&env_service, &temp_char);
    eos_ble_gatt_service_add_char(&env_service, &humidity_char);
    eos_ble_gatt_register(&env_service);

    eos_log(EOS_LOG_INFO, "BLE", "GATT Environmental Sensing service registered");
}
```

### Characteristic Read & Write

```c
// Update and notify connected client
void update_ble_sensor_data(float temp_c, float humidity_pct) {
    int16_t temp_raw = (int16_t)(temp_c * 100);
    uint16_t hum_raw = (uint16_t)(humidity_pct * 100);

    temp_value[0] = temp_raw & 0xFF;
    temp_value[1] = (temp_raw >> 8) & 0xFF;
    humidity_value[0] = hum_raw & 0xFF;
    humidity_value[1] = (hum_raw >> 8) & 0xFF;

    eos_ble_gatt_notify(&temp_char);
    eos_ble_gatt_notify(&humidity_char);
}

// Handle write from client (e.g., configuration characteristic)
static void on_write(eos_ble_gatt_char_t *ch, const uint8_t *data,
                     uint16_t len, void *ctx) {
    eos_log(EOS_LOG_INFO, "BLE", "Write to char 0x%04X: %d bytes",
            ch->uuid.uuid16, len);
    if (ch == &config_char && len == 1) {
        uint8_t interval = data[0];
        set_sensor_interval_sec(interval);
        eos_log(EOS_LOG_INFO, "BLE", "Sensor interval set to %d s", interval);
    }
}
```

### Notifications

```c
// Enable notifications by writing 0x0001 to the CCCD
// (Client does this; firmware handles it automatically via GATT stack)

// Periodic notification from a task:
void sensor_ble_task(void *arg) {
    while (1) {
        float t, h;
        read_bme280(&t, &h);
        update_ble_sensor_data(t, h);
        eos_log(EOS_LOG_DEBUG, "BLE", "Notified: temp=%.1f°C, hum=%.1f%%", t, h);
        eos_sleep_ms(1000);
    }
}
```

Expected output:

```
[  1.000] BLE  : Notified: temp=23.4°C, hum=48.2%
[  2.000] BLE  : Notified: temp=23.5°C, hum=48.0%
[  3.000] BLE  : Notified: temp=23.4°C, hum=48.3%
```

### Pairing & Bonding

```c
static const eos_ble_security_params_t sec_params = {
    .bond         = true,
    .mitm         = true,       // man-in-the-middle protection
    .io_caps      = EOS_BLE_IO_DISPLAY_YESNO,
    .oob          = false,
    .min_key_size = 16,
    .max_key_size = 16,
};

void ble_enable_security(void) {
    eos_ble_gap_set_security(&sec_params);
    eos_log(EOS_LOG_INFO, "BLE", "Security: bonding + MITM (display yes/no)");
}

// Pairing event callback
static void on_passkey_display(uint32_t passkey, void *ctx) {
    eos_log(EOS_LOG_INFO, "BLE", "Passkey: %06lu", passkey);
    // Display passkey on OLED/LCD or UART for user confirmation
}
```

Expected output during pairing:

```
[  6.100] BLE  : Security request from AA:BB:CC:DD:EE:FF
[  6.101] BLE  : Passkey: 482193
[  8.500] BLE  : Pairing complete — bonded, encrypted (AES-128-CCM)
```

---

## WiFi

EoS supports WiFi station mode over ESP32 or similar radios. Enable with
`EOS_ENABLE_WIFI 1`.

### Station Mode — Connect to AP

```c
#include <eos/wifi.h>

static const eos_wifi_config_t wifi_cfg = {
    .ssid     = "MyNetwork",
    .password = "s3cur3p@ss",
    .security = EOS_WIFI_WPA2_PSK,
    .channel  = 0,              // auto-select
};

static void wifi_event_handler(eos_wifi_event_t event, void *ctx) {
    switch (event) {
    case EOS_WIFI_EVENT_CONNECTED:
        eos_log(EOS_LOG_INFO, "WIFI", "Connected to '%s'", wifi_cfg.ssid);
        break;
    case EOS_WIFI_EVENT_GOT_IP: {
        eos_wifi_ip_info_t ip;
        eos_wifi_get_ip(&ip);
        eos_log(EOS_LOG_INFO, "WIFI", "IP: %d.%d.%d.%d",
                (ip.addr >> 0) & 0xFF, (ip.addr >> 8) & 0xFF,
                (ip.addr >> 16) & 0xFF, (ip.addr >> 24) & 0xFF);
        break;
    }
    case EOS_WIFI_EVENT_DISCONNECTED:
        eos_log(EOS_LOG_WARN, "WIFI", "Disconnected — reconnecting...");
        eos_wifi_connect(&wifi_cfg);
        break;
    default:
        break;
    }
}

int wifi_init_station(void) {
    int rc = eos_wifi_init();
    if (rc != 0) return rc;

    eos_wifi_set_event_handler(wifi_event_handler, NULL);
    eos_wifi_set_mode(EOS_WIFI_MODE_STA);
    return eos_wifi_connect(&wifi_cfg);
}
```

Expected output:

```
[  0.100] WIFI : WiFi stack initialized
[  0.101] WIFI : Scanning for 'MyNetwork'...
[  1.450] WIFI : Connected to 'MyNetwork' (ch 6, RSSI -52 dBm)
[  2.100] WIFI : DHCP lease obtained
[  2.101] WIFI : IP: 192.168.1.42
```

### AP Scanning

```c
void wifi_scan_networks(void) {
    eos_wifi_scan_result_t results[16];
    int count = eos_wifi_scan(results, 16, 5000);

    eos_log(EOS_LOG_INFO, "WIFI", "Found %d networks:", count);
    for (int i = 0; i < count; i++) {
        eos_log(EOS_LOG_INFO, "WIFI", "  [%d] %-32s  ch=%2d  rssi=%d dBm  %s",
                i, results[i].ssid, results[i].channel, results[i].rssi,
                results[i].security == EOS_WIFI_OPEN ? "OPEN" :
                results[i].security == EOS_WIFI_WPA2_PSK ? "WPA2" : "WPA3");
    }
}
```

Expected output:

```
[  3.000] WIFI : Found 4 networks:
[  3.001] WIFI :   [0] MyNetwork                         ch= 6  rssi=-52 dBm  WPA2
[  3.001] WIFI :   [1] NeighborNet                       ch= 1  rssi=-78 dBm  WPA2
[  3.001] WIFI :   [2] CoffeeShop_Free                   ch=11  rssi=-85 dBm  OPEN
[  3.001] WIFI :   [3] 5G_Home                           ch=36  rssi=-70 dBm  WPA3
```

### DHCP Configuration

DHCP is enabled by default in station mode. For static IP:

```c
eos_wifi_ip_info_t static_ip = {
    .addr    = EOS_IP4(192, 168, 1, 100),
    .netmask = EOS_IP4(255, 255, 255, 0),
    .gateway = EOS_IP4(192, 168, 1, 1),
};

eos_wifi_set_ip(&static_ip);
eos_wifi_set_dns(EOS_IP4(8, 8, 8, 8));
```

### Reconnection Strategy

```c
static int reconnect_attempts = 0;
#define MAX_RECONNECT_ATTEMPTS 10

static void wifi_event_handler(eos_wifi_event_t event, void *ctx) {
    if (event == EOS_WIFI_EVENT_DISCONNECTED) {
        if (reconnect_attempts < MAX_RECONNECT_ATTEMPTS) {
            uint32_t backoff = (1 << reconnect_attempts) * 1000;
            if (backoff > 30000) backoff = 30000;
            eos_log(EOS_LOG_WARN, "WIFI",
                    "Reconnect attempt %d/%d in %lu ms",
                    reconnect_attempts + 1, MAX_RECONNECT_ATTEMPTS, backoff);
            eos_sleep_ms(backoff);
            eos_wifi_connect(&wifi_cfg);
            reconnect_attempts++;
        } else {
            eos_log(EOS_LOG_ERROR, "WIFI", "Max reconnect attempts exceeded");
        }
    } else if (event == EOS_WIFI_EVENT_GOT_IP) {
        reconnect_attempts = 0;
    }
}
```

---

## LoRa / LoRaWAN

EoS supports LoRa PHY and LoRaWAN MAC via SPI-connected radios (SX1276/SX1262).
Enable with `EOS_ENABLE_LORA 1`.

### LoRa PHY Parameters

| Parameter | Description | Typical Values |
|-----------|-------------|----------------|
| **Spreading Factor (SF)** | Trade range for data rate | SF7 (fast) – SF12 (long range) |
| **Bandwidth (BW)** | Channel bandwidth | 125 kHz, 250 kHz, 500 kHz |
| **Coding Rate (CR)** | Forward error correction | 4/5, 4/6, 4/7, 4/8 |
| **TX Power** | Transmit power | 2–20 dBm |

Higher SF = longer range, lower data rate, higher airtime.

### Raw LoRa (Point-to-Point)

```c
#include <eos/lora.h>

static const eos_lora_config_t lora_cfg = {
    .frequency_hz  = 915000000,   // 915 MHz (US ISM band)
    .spreading_factor = 7,
    .bandwidth     = EOS_LORA_BW_125K,
    .coding_rate   = EOS_LORA_CR_4_5,
    .tx_power_dbm  = 14,
    .preamble_len  = 8,
    .sync_word     = 0x12,        // private network
    .crc_on        = true,
};

int lora_send_sensor_data(void) {
    int rc = eos_lora_init(&lora_cfg);
    if (rc != 0) return rc;

    uint8_t payload[16];
    int len = encode_sensor_payload(payload, sizeof(payload));

    rc = eos_lora_transmit(payload, len, 5000);
    eos_log(EOS_LOG_INFO, "LORA", "TX %d bytes @ SF%d BW125 (airtime=%lu ms)",
            len, lora_cfg.spreading_factor, eos_lora_airtime_ms(len));
    return rc;
}

static void lora_rx_callback(const uint8_t *data, uint8_t len,
                              int16_t rssi, int8_t snr, void *ctx) {
    eos_log(EOS_LOG_INFO, "LORA", "RX %d bytes, RSSI=%d dBm, SNR=%d dB",
            len, rssi, snr);
}

void lora_start_receive(void) {
    eos_lora_set_rx_callback(lora_rx_callback, NULL);
    eos_lora_receive_continuous();
}
```

Expected output:

```
[  0.100] LORA : SX1276 initialized @ 915.000 MHz
[  0.101] LORA : TX 12 bytes @ SF7 BW125 (airtime=36 ms)
[  1.500] LORA : RX 12 bytes, RSSI=-87 dBm, SNR=9 dB
```

### LoRaWAN — ABP (Activation by Personalization)

ABP uses pre-provisioned session keys — no join procedure required.

```c
#include <eos/lorawan.h>

static const eos_lorawan_abp_config_t abp_cfg = {
    .dev_addr  = 0x26011234,
    .nwk_s_key = {0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,
                  0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C},
    .app_s_key = {0x3C,0x4F,0xCF,0x09,0x88,0x15,0xF7,0xAB,
                  0xA6,0xD2,0xAE,0x28,0x16,0x15,0x7E,0x2B},
};

int lorawan_abp_init(void) {
    int rc = eos_lorawan_init();
    if (rc != 0) return rc;

    rc = eos_lorawan_join_abp(&abp_cfg);
    eos_log(EOS_LOG_INFO, "LWAN", "ABP session activated (DevAddr=0x%08X)",
            abp_cfg.dev_addr);
    return rc;
}

int lorawan_send_uplink(uint8_t port, const uint8_t *data, uint8_t len) {
    int rc = eos_lorawan_send(port, data, len, EOS_LORAWAN_UNCONFIRMED);
    eos_log(EOS_LOG_INFO, "LWAN", "Uplink port=%d, %d bytes, unconfirmed", port, len);
    return rc;
}
```

### LoRaWAN — OTAA (Over-the-Air Activation)

OTAA is preferred for production — derives session keys via a join procedure.

```c
static const eos_lorawan_otaa_config_t otaa_cfg = {
    .dev_eui  = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77},
    .app_eui  = {0x70,0xB3,0xD5,0x7E,0xD0,0x00,0x00,0x01},
    .app_key  = {0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,
                 0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C},
};

int lorawan_otaa_join(void) {
    int rc = eos_lorawan_init();
    if (rc != 0) return rc;

    eos_log(EOS_LOG_INFO, "LWAN", "Sending Join Request (OTAA)...");
    rc = eos_lorawan_join_otaa(&otaa_cfg, 30000);  // 30 s timeout

    if (rc == 0) {
        eos_log(EOS_LOG_INFO, "LWAN", "Join Accept received — session active");
    } else {
        eos_log(EOS_LOG_ERROR, "LWAN", "Join failed: %d (timeout or rejected)", rc);
    }
    return rc;
}
```

Expected OTAA output:

```
[  0.100] LWAN : LoRaWAN MAC initialized (EU868 / US915)
[  0.200] LWAN : Sending Join Request (OTAA)...
[  5.300] LWAN : Join Accept received — session active
[  5.301] LWAN : DevAddr=0x260B1A2F (assigned by network server)
```

### Duty Cycle Compliance

LoRaWAN enforces duty cycle limits (e.g., 1% in EU868):

```c
// Check remaining airtime budget before sending
uint32_t available_ms = eos_lorawan_duty_cycle_remaining_ms();
uint32_t needed_ms = eos_lora_airtime_ms(payload_len);

if (needed_ms > available_ms) {
    eos_log(EOS_LOG_WARN, "LWAN",
            "Duty cycle limit — need %lu ms, available %lu ms. "
            "Next TX in %lu s",
            needed_ms, available_ms,
            (needed_ms - available_ms) / 1000);
    return EOS_ERR_DUTY_CYCLE;
}
```

---

## Wireless Troubleshooting

| Issue | Protocol | Fix |
|-------|----------|-----|
| BLE not advertising | BLE | Check `EOS_ENABLE_BLE 1` in product profile |
| No scan results | WiFi | Verify antenna connected, check frequency band |
| DHCP timeout | WiFi | Confirm router is on same channel, check password |
| Join rejected | LoRaWAN | Verify DevEUI/AppKey match gateway server config |
| Duty cycle limit | LoRaWAN | Increase SF interval or reduce payload size |
| Pairing fails | BLE | Ensure both devices support the same IO capability |
| WiFi reconnect loop | WiFi | Add exponential backoff (see Reconnection Strategy) |
| LoRa CRC error | LoRa | Match sync word and modulation params on both ends |

---

## Next Steps

- [STM32 Deployment](stm32-deployment.md) — Board bring-up
- [Debugging Guide](debugging-guide.md) — GDB, hard faults, memory leaks
- [Networking Protocols](networking-protocols.md) — MQTT, HTTP, mDNS over WiFi/Ethernet
- [Configuration Examples](configuration-examples.md) — Product profiles and build variants
