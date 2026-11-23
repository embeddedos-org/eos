# Wireless Protocols Tutorial

Configure and use BLE and WiFi wireless stacks on EoS, and know where
LoRa/LoRaWAN stand. Each section uses only APIs that are actually declared
in the tree, and says plainly what is not implemented yet.

> **Prerequisites:** Build EoS with a product profile that enables wireless
> (e.g., `iot`, `gateway`, `smart_home`). See
> [Configuration Examples](configuration-examples.md).

## Status at a glance

| Stack | Header | API status | Backend ships in this tree? |
|-------|--------|------------|------------------------------|
| BLE | `<eos/hal_extended.h>` | Declared: config + 9 functions, **no GATT** | **No** — the only definitions are stubs that return `-1` |
| WiFi | `<eos/hal_extended.h>` | Declared: config + 11 functions | **No** — the only definitions are stubs that return `-1` |
| LoRa / LoRaWAN | *(none — no header exists)* | **No API in EoS** | **No** |

BLE and WiFi are declared in `hal/include/eos/hal_extended.h` and gated on
`EOS_ENABLE_BLE` / `EOS_ENABLE_WIFI`, which both default to `0` in
`include/eos/eos_config.h` — set them to `1` in your product profile to
compile the declarations in. The function definitions in this tree are the
stubs in `hal/src/hal_extended_stubs.c`: every call returns `-1` (or
`false`) until a vendor backend registers through
`eos_hal_register_ext_backend()` with the `ble_*` / `wifi_*` vtable
entries. No such backend ships in this tree. So the samples below compile
against the real headers, but on a stock checkout they fail at run time
with `-1` — check every return code, as the samples do, instead of
assuming the radio is up.

There is no `eos/ble.h`, `eos/wifi.h`, `eos/lora.h`, or `eos/lorawan.h`
in the tree. Do not include them.

---

## Bluetooth Low Energy (BLE)

BLE is a thin HAL: a config struct, advertising on/off, connect/disconnect,
raw send/receive, and an RX callback. There is **no GAP/GATT abstraction**
in EoS — no advertising parameters, no GATT services or characteristics,
and no pairing/bonding API. Those identifiers do not exist in the tree.

That gap matters beyond this tutorial: Matter commissioning happens over
BLE (GATT), so the missing GATT API is the unmet prerequisite for Thread
commissioning. It is recorded as such in eNet's
[BLE commissioning audit](https://github.com/embeddedos-org/eNet/blob/master/docs/ble-commissioning-audit.md)
(decision record SH1, 2026-11-08). A GATT API is a separate, larger piece
of work, not something this tutorial can paper over.

### What exists

Declared in `hal/include/eos/hal_extended.h` (under `#if EOS_ENABLE_BLE`):

```c
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
```

### Init, advertise, send, receive

This follows the in-tree example `examples/ble-sensor/main.c`, which uses
the same calls. With no backend registered, `eos_ble_init()` returns
`-1` — the sample reports that instead of continuing as if the radio
were up.

```c
#include <eos/hal_extended.h>
#include <stdio.h>

static void ble_rx_handler(const uint8_t *data, size_t len, void *ctx)
{
    (void)ctx;
    printf("[ble] RX %zu bytes: ", len);
    for (size_t i = 0; i < len && i < 16; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
}

int ble_start_advertising(void)
{
    eos_ble_config_t ble_cfg = {
        .device_name     = "EoS-Sensor",
        .tx_power_dbm    = 0,
        .adv_interval_ms = 100,
    };

    int rc = eos_ble_init(&ble_cfg);
    if (rc != 0) {
        /* -1 with no backend registered (see "Status at a glance"). */
        printf("[ble] init failed: %d\n", rc);
        return rc;
    }

    rc = eos_ble_set_rx_callback(ble_rx_handler, NULL);
    if (rc != 0) {
        printf("[ble] RX callback failed: %d\n", rc);
        return rc;
    }

    rc = eos_ble_advertise_start();
    if (rc != 0) {
        printf("[ble] advertise failed: %d\n", rc);
        return rc;
    }

    printf("[ble] advertising as '%s'\n", ble_cfg.device_name);
    return 0;
}

int ble_send_packet(const uint8_t *data, size_t len)
{
    if (!eos_ble_is_connected()) {
        printf("[ble] not connected, dropping %zu bytes\n", len);
        return -1;
    }
    return eos_ble_send(data, len);
}

void ble_stop(void)
{
    eos_ble_advertise_stop();
    eos_ble_disconnect();
    eos_ble_deinit();
}
```

Notes on the sample:

- Advertising is on/off only (`eos_ble_advertise_start()` /
  `eos_ble_advertise_stop()`). There is no API to set advertising
  parameters, data, intervals, or channels — the only interval that
  exists is `adv_interval_ms` in `eos_ble_config_t`.
- `eos_ble_send()` sends raw bytes. There is no GATT service,
  characteristic, notification, or write-callback API, so there is no
  "Environmental Sensing service" to register and no characteristic to
  notify. Sensor data goes out as a raw payload; the peer must know
  its layout.
- `eos_ble_connect()` takes a 6-byte peer address for a central-role
  connection. There is no security/pairing API: no bonding, no MITM
  flag, no passkey callback.

---

## WiFi

WiFi is station connect/disconnect, scanning, and raw send/receive over
the link. There is **no event-callback API, no station/AP mode
selection, and no static-IP / DHCP / DNS configuration API** in the
declared surface — poll `eos_wifi_is_connected()` for link state
instead of waiting for events.

### What exists

Declared in `hal/include/eos/hal_extended.h` (under `#if EOS_ENABLE_WIFI`):

```c
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
```

### Station Mode — Connect to AP

```c
#include <eos/hal_extended.h>
#include <stdio.h>

static const eos_wifi_config_t wifi_cfg = {
    .ssid     = "MyNetwork",
    .password = "s3cur3p@ss",
    .security = EOS_WIFI_SEC_WPA2,
};

int wifi_init_station(void)
{
    int rc = eos_wifi_init();
    if (rc != 0) {
        /* -1 with no backend registered (see "Status at a glance"). */
        printf("[wifi] init failed: %d\n", rc);
        return rc;
    }

    rc = eos_wifi_connect(&wifi_cfg);
    if (rc != 0) {
        printf("[wifi] connect failed: %d\n", rc);
        return rc;
    }

    if (eos_wifi_is_connected()) {
        uint32_t ip = 0;
        if (eos_wifi_get_ip(&ip) == 0) {
            printf("[wifi] IP: %u.%u.%u.%u\n",
                   (ip >> 0) & 0xFF, (ip >> 8) & 0xFF,
                   (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
        }
    }
    return 0;
}
```

### AP Scanning

```c
void wifi_scan_networks(void)
{
    eos_wifi_scan_result_t results[16];
    size_t found = 0;

    int rc = eos_wifi_scan(results, 16, &found);
    if (rc != 0) {
        printf("[wifi] scan failed: %d\n", rc);
        return;
    }

    printf("[wifi] Found %zu networks:\n", found);
    for (size_t i = 0; i < found; i++) {
        printf("[wifi]   [%zu] %-32s  ch=%2d  rssi=%d dBm  %s\n",
               i, results[i].ssid, results[i].channel, results[i].rssi,
               results[i].security == EOS_WIFI_SEC_OPEN ? "OPEN" :
               results[i].security == EOS_WIFI_SEC_WPA2 ? "WPA2" : "WPA3");
    }
}
```

### Reconnection

There is no disconnect event, so reconnection is a poll: if
`eos_wifi_is_connected()` is false, call `eos_wifi_connect(&wifi_cfg)`
again. Apply your own retry limit and backoff between attempts — the
HAL declares no timer for it.

### After the link is up

MQTT, HTTP, and mDNS run over the link through `<eos/net.h>` — see
[Networking Protocols](networking-protocols.md). That tutorial needs
`EOS_ENABLE_NET 1` plus an established WiFi or Ethernet link.

---

## LoRa / LoRaWAN — unimplemented in EoS

LoRa and LoRaWAN have **no API in EoS**: no `eos/lora.h`, no
`eos/lorawan.h`, no `EOS_ENABLE_LORA` flag, and no LoRa declarations in
`hal/include/eos/hal_extended.h` or `net/include/eos/net.h`. The code
samples that used to be in this section — raw LoRa transmit/receive,
LoRaWAN ABP/OTAA join, duty-cycle checks — called functions that do not
exist, so they are removed, not rewritten.

Background only (generic LoRa concepts, not EoS parameters):

| Parameter | Description | Typical Values |
|-----------|-------------|----------------|
| **Spreading Factor (SF)** | Trade range for data rate | SF7 (fast) – SF12 (long range) |
| **Bandwidth (BW)** | Channel bandwidth | 125 kHz, 250 kHz, 500 kHz |
| **Coding Rate (CR)** | Forward error correction | 4/5, 4/6, 4/7, 4/8 |
| **TX Power** | Transmit power | 2–20 dBm |

Higher SF = longer range, lower data rate, higher airtime.

The owning track is eNet: its
[LoRaWAN study](https://github.com/embeddedos-org/eNet/blob/master/docs/lorawan-study.md)
is a research note (no code yet) on what a LoRaWAN story for eNet could
look like. Until that lands, do not write code against a LoRa API in eos
— there is nothing to link against.

---

## Wireless Troubleshooting

| Issue | Protocol | Fix |
|-------|----------|-----|
| Including `eos/ble.h` / `eos/wifi.h` fails to compile | BLE / WiFi | Those headers do not exist. Include `<eos/hal_extended.h>` instead |
| `eos_ble_init()` / `eos_wifi_init()` returns `-1` | BLE / WiFi | No backend is registered — the in-tree definitions are stubs. A vendor backend must register via `eos_hal_register_ext_backend()` |
| BLE / WiFi functions are not declared at all | BLE / WiFi | `EOS_ENABLE_BLE` / `EOS_ENABLE_WIFI` default to `0` in `include/eos/eos_config.h`. Set the flag to `1` in your product profile |
| No scan results (`eos_wifi_scan()` returns `-1`) | WiFi | Same as above: no backend, so the stub reports `*found = 0` and `-1` |
| Need a GATT service, pairing, or Matter commissioning over BLE | BLE | No GATT API exists in EoS. See eNet's [BLE commissioning audit](https://github.com/embeddedos-org/eNet/blob/master/docs/ble-commissioning-audit.md) — that work is a separate, larger piece |
| LoRa / LoRaWAN code does not compile | LoRa | No LoRa API exists in EoS. See eNet's [LoRaWAN study](https://github.com/embeddedos-org/eNet/blob/master/docs/lorawan-study.md) |
| MQTT / HTTP over WiFi | WiFi | Use `<eos/net.h>` once the link is up — see [Networking Protocols](networking-protocols.md) |

---

## Next Steps

- [STM32 Deployment](stm32-deployment.md) — Board bring-up
- [Debugging Guide](debugging-guide.md) — GDB, hard faults, memory leaks
- [Networking Protocols](networking-protocols.md) — MQTT, HTTP, mDNS over WiFi/Ethernet
- [Configuration Examples](configuration-examples.md) — Product profiles and build variants
