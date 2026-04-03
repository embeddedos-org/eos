# Networking Protocols Tutorial

Use MQTT, HTTP, and mDNS over WiFi or Ethernet with the EoS `net/` module.
Includes broker connection, REST client, service discovery, TLS, and error
handling patterns.

> **Prerequisites:** Enable networking in your product profile
> (`EOS_ENABLE_NET 1`) and establish a WiFi or Ethernet link first. See
> [Wireless Stacks](wireless-stacks.md) for WiFi setup.

---

## MQTT

EoS provides a lightweight MQTT 3.1.1 client via the `eos_mqtt_*` API defined
in `net/include/eos/net.h`. Supports QoS 0, 1, and 2, last will, and
TLS-secured connections.

### Broker Connection

```c
#include <eos/net.h>

static eos_mqtt_handle_t mqtt = EOS_MQTT_INVALID;

int mqtt_connect(void) {
    eos_mqtt_config_t cfg = {
        .broker_host  = "192.168.1.10",
        .broker_port  = 1883,
        .client_id    = "eos-sensor-01",
        .username     = "",
        .password     = "",
        .keepalive_sec = 60,
    };

    mqtt = eos_mqtt_connect(&cfg);
    if (mqtt == EOS_MQTT_INVALID) {
        eos_log(EOS_LOG_ERROR, "MQTT", "Connect failed");
        return -1;
    }

    eos_log(EOS_LOG_INFO, "MQTT", "Connected to %s:%d as '%s'",
            cfg.broker_host, cfg.broker_port, cfg.client_id);
    return 0;
}
```

Expected output:

```
[  2.500] MQTT : Connected to 192.168.1.10:1883 as 'eos-sensor-01'
```

### Topic Subscribe & Message Callback

```c
static void on_message(const char *topic, const uint8_t *payload,
                       size_t payload_len, void *ctx) {
    eos_log(EOS_LOG_INFO, "MQTT", "RX [%s] (%zu bytes): %.*s",
            topic, payload_len, (int)payload_len, payload);
}

int mqtt_subscribe_topics(void) {
    int rc;

    rc = eos_mqtt_subscribe(mqtt, "home/sensor/+/cmd", 1, on_message, NULL);
    if (rc != 0) {
        eos_log(EOS_LOG_ERROR, "MQTT", "Subscribe failed: %d", rc);
        return rc;
    }

    rc = eos_mqtt_subscribe(mqtt, "home/broadcast", 0, on_message, NULL);
    if (rc != 0) return rc;

    eos_log(EOS_LOG_INFO, "MQTT", "Subscribed to command and broadcast topics");
    return 0;
}
```

Expected output:

```
[  2.510] MQTT : Subscribed to command and broadcast topics
[  5.200] MQTT : RX [home/sensor/01/cmd] (14 bytes): {"led":"toggle"}
```

### Publish

```c
int mqtt_publish_sensor(float temp, float humidity) {
    char payload[128];
    int len = snprintf(payload, sizeof(payload),
                       "{\"temp\":%.1f,\"humidity\":%.1f,\"ts\":%lu}",
                       temp, humidity, eos_uptime_ms());

    int rc = eos_mqtt_publish(mqtt, "home/sensor/01/data",
                               payload, len, /*qos=*/1);
    if (rc != 0) {
        eos_log(EOS_LOG_ERROR, "MQTT", "Publish failed: %d", rc);
        return rc;
    }

    eos_log(EOS_LOG_DEBUG, "MQTT", "Published %d bytes to home/sensor/01/data", len);
    return 0;
}
```

### QoS Levels

| QoS | Delivery Guarantee | Use Case |
|-----|--------------------|----------|
| **0** | At most once (fire-and-forget) | Telemetry where occasional loss is OK |
| **1** | At least once (may duplicate) | Sensor data, status updates |
| **2** | Exactly once (4-step handshake) | Commands, billing events, firmware OTA triggers |

```c
// QoS 0 — no ACK, lowest overhead
eos_mqtt_publish(mqtt, "telemetry/heartbeat", "ok", 2, 0);

// QoS 1 — broker ACKs with PUBACK
eos_mqtt_publish(mqtt, "sensor/temperature", payload, len, 1);

// QoS 2 — full handshake (PUBREC → PUBREL → PUBCOMP)
eos_mqtt_publish(mqtt, "command/firmware_update", cmd, cmd_len, 2);
```

### Last Will and Testament (LWT)

The broker publishes the LWT message if the client disconnects unexpectedly:

```c
eos_mqtt_config_t cfg = {
    .broker_host   = "192.168.1.10",
    .broker_port   = 1883,
    .client_id     = "eos-sensor-01",
    .keepalive_sec = 60,
};

// Set LWT before connecting
eos_mqtt_set_will(mqtt, "home/sensor/01/status", "offline", 7, /*qos=*/1,
                  /*retain=*/true);
```

When the device goes offline unexpectedly, the broker publishes:

```
Topic:   home/sensor/01/status
Payload: offline
QoS:     1
Retain:  true
```

### TLS-Secured MQTT (MQTTS)

```c
eos_mqtt_config_t cfg = {
    .broker_host   = "mqtt.example.com",
    .broker_port   = 8883,          // standard MQTTS port
    .client_id     = "eos-sensor-01",
    .username      = "device01",
    .password      = "s3cr3t",
    .keepalive_sec = 60,
};

// Load CA certificate for server verification
extern const uint8_t ca_cert_pem[];
extern const size_t  ca_cert_pem_len;

eos_mqtt_tls_config_t tls = {
    .ca_cert      = ca_cert_pem,
    .ca_cert_len  = ca_cert_pem_len,
    .verify_server = true,
};

eos_mqtt_set_tls(&cfg, &tls);
mqtt = eos_mqtt_connect(&cfg);
```

For mutual TLS (client certificate authentication):

```c
extern const uint8_t client_cert_pem[];
extern const size_t  client_cert_pem_len;
extern const uint8_t client_key_pem[];
extern const size_t  client_key_pem_len;

eos_mqtt_tls_config_t tls = {
    .ca_cert         = ca_cert_pem,
    .ca_cert_len     = ca_cert_pem_len,
    .client_cert     = client_cert_pem,
    .client_cert_len = client_cert_pem_len,
    .client_key      = client_key_pem,
    .client_key_len  = client_key_pem_len,
    .verify_server   = true,
};
```

### MQTT Event Loop

The MQTT client requires periodic calls to `eos_mqtt_loop()` to process
incoming messages, send keepalives, and handle QoS retransmissions:

```c
void mqtt_task(void *arg) {
    mqtt_connect();
    mqtt_subscribe_topics();

    while (1) {
        int rc = eos_mqtt_loop(mqtt, 100);  // 100 ms timeout
        if (rc != 0) {
            eos_log(EOS_LOG_WARN, "MQTT", "Loop error: %d — reconnecting", rc);
            eos_mqtt_disconnect(mqtt);
            eos_sleep_ms(5000);
            mqtt_connect();
            mqtt_subscribe_topics();
        }

        // Publish sensor data every 10 seconds
        static uint32_t last_pub = 0;
        uint32_t now = eos_uptime_ms();
        if (now - last_pub >= 10000) {
            float t, h;
            read_bme280(&t, &h);
            mqtt_publish_sensor(t, h);
            last_pub = now;
        }
    }
}
```

---

## HTTP Client

The EoS HTTP client supports GET, POST, and PUT requests with JSON payloads.
Defined in `net/include/eos/net.h`.

### GET Request

```c
#include <eos/net.h>

int http_get_config(void) {
    eos_http_response_t resp = {0};

    int rc = eos_http_get("http://192.168.1.10:8080/api/config", &resp);
    if (rc != 0) {
        eos_log(EOS_LOG_ERROR, "HTTP", "GET failed: %d", rc);
        return rc;
    }

    eos_log(EOS_LOG_INFO, "HTTP", "GET /api/config → %d (%s)",
            resp.status_code, resp.content_type);
    eos_log(EOS_LOG_INFO, "HTTP", "Body (%zu bytes): %.*s",
            resp.body_len, (int)resp.body_len, resp.body);

    eos_http_response_free(&resp);
    return 0;
}
```

Expected output:

```
[  3.000] HTTP : GET /api/config → 200 (application/json)
[  3.001] HTTP : Body (85 bytes): {"interval_sec":10,"led_enabled":true,"threshold":25.0}
```

### POST Request with JSON Payload

```c
int http_post_sensor_data(float temp, float humidity) {
    char json[128];
    int len = snprintf(json, sizeof(json),
                       "{\"device\":\"eos-sensor-01\","
                       "\"temp\":%.1f,\"humidity\":%.1f}",
                       temp, humidity);

    eos_http_response_t resp = {0};
    int rc = eos_http_post("http://192.168.1.10:8080/api/data",
                            json, len, "application/json", &resp);
    if (rc != 0) {
        eos_log(EOS_LOG_ERROR, "HTTP", "POST failed: %d", rc);
        return rc;
    }

    eos_log(EOS_LOG_INFO, "HTTP", "POST /api/data → %d", resp.status_code);

    if (resp.status_code == 201) {
        eos_log(EOS_LOG_INFO, "HTTP", "Data accepted by server");
    } else {
        eos_log(EOS_LOG_WARN, "HTTP", "Unexpected status: %d", resp.status_code);
    }

    eos_http_response_free(&resp);
    return 0;
}
```

Expected output:

```
[  3.500] HTTP : POST /api/data → 201
[  3.501] HTTP : Data accepted by server
```

### PUT Request

```c
int http_update_device_name(const char *new_name) {
    char json[128];
    int len = snprintf(json, sizeof(json), "{\"name\":\"%s\"}", new_name);

    eos_http_response_t resp = {0};

    // Build a PUT request using the socket API
    eos_net_addr_t addr;
    eos_net_resolve("192.168.1.10", &addr.ip);
    addr.port = 8080;

    eos_socket_t sock = eos_net_socket(EOS_NET_TCP);
    if (sock == EOS_SOCKET_INVALID) return -1;

    int rc = eos_net_connect(sock, &addr);
    if (rc != 0) { eos_net_close(sock); return rc; }

    char request[512];
    int req_len = snprintf(request, sizeof(request),
        "PUT /api/device HTTP/1.1\r\n"
        "Host: 192.168.1.10:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n%s", len, json);

    eos_net_send(sock, request, req_len);

    char response[512];
    int recv_len = eos_net_recv(sock, response, sizeof(response) - 1, 5000);
    if (recv_len > 0) {
        response[recv_len] = '\0';
        eos_log(EOS_LOG_INFO, "HTTP", "PUT response: %.32s...", response);
    }

    eos_net_close(sock);
    return 0;
}
```

### Response Parsing — Lightweight JSON

For resource-constrained devices, parse JSON manually without a full JSON
library:

```c
#include <string.h>
#include <stdlib.h>

// Extract a numeric value from a JSON key
float json_get_float(const char *json, const char *key) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *pos = strstr(json, search);
    if (!pos) return 0.0f;
    pos += strlen(search);
    while (*pos == ' ') pos++;
    return strtof(pos, NULL);
}

// Extract a boolean value
bool json_get_bool(const char *json, const char *key) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *pos = strstr(json, search);
    if (!pos) return false;
    pos += strlen(search);
    while (*pos == ' ') pos++;
    return strncmp(pos, "true", 4) == 0;
}

// Usage:
void apply_config(const char *json) {
    float threshold = json_get_float(json, "threshold");
    bool led_on     = json_get_bool(json, "led_enabled");

    eos_log(EOS_LOG_INFO, "APP", "Config: threshold=%.1f, led=%s",
            threshold, led_on ? "on" : "off");
}
```

### Error Handling Patterns

```c
typedef enum {
    HTTP_OK          = 0,
    HTTP_ERR_DNS     = -1,
    HTTP_ERR_CONNECT = -2,
    HTTP_ERR_SEND    = -3,
    HTTP_ERR_RECV    = -4,
    HTTP_ERR_TIMEOUT = -5,
    HTTP_ERR_STATUS  = -6,
} http_error_t;

int http_get_with_retry(const char *url, eos_http_response_t *resp,
                        int max_retries) {
    for (int attempt = 0; attempt < max_retries; attempt++) {
        int rc = eos_http_get(url, resp);

        if (rc == 0 && resp->status_code >= 200 && resp->status_code < 300) {
            return HTTP_OK;
        }

        if (rc == 0) {
            eos_log(EOS_LOG_WARN, "HTTP", "Status %d on attempt %d/%d",
                    resp->status_code, attempt + 1, max_retries);
            eos_http_response_free(resp);

            // Don't retry client errors (4xx)
            if (resp->status_code >= 400 && resp->status_code < 500) {
                return HTTP_ERR_STATUS;
            }
        } else {
            eos_log(EOS_LOG_WARN, "HTTP", "Request error %d on attempt %d/%d",
                    rc, attempt + 1, max_retries);
        }

        // Exponential backoff: 1s, 2s, 4s, ...
        uint32_t backoff = (1 << attempt) * 1000;
        if (backoff > 30000) backoff = 30000;
        eos_sleep_ms(backoff);
    }

    eos_log(EOS_LOG_ERROR, "HTTP", "All %d attempts failed for %s",
            max_retries, url);
    return HTTP_ERR_TIMEOUT;
}
```

---

## mDNS (Multicast DNS)

mDNS enables zero-configuration service discovery on the local network. Devices
can advertise services and discover peers without a central DNS server. API is
in `net/include/eos/net.h`.

### Service Advertisement

Register your device as a discoverable service on the LAN:

```c
#include <eos/net.h>

int mdns_advertise_sensor(void) {
    int rc = eos_mdns_init();
    if (rc != 0) {
        eos_log(EOS_LOG_ERROR, "MDNS", "Init failed: %d", rc);
        return rc;
    }

    // Advertise as "_eos-sensor._tcp.local" on port 8080
    rc = eos_mdns_register("EoS-Sensor-01", "_eos-sensor._tcp", 8080);
    if (rc != 0) {
        eos_log(EOS_LOG_ERROR, "MDNS", "Register failed: %d", rc);
        return rc;
    }

    eos_log(EOS_LOG_INFO, "MDNS",
            "Advertising 'EoS-Sensor-01' as _eos-sensor._tcp on port 8080");
    return 0;
}
```

Expected output:

```
[  2.200] MDNS : mDNS responder initialized
[  2.201] MDNS : Advertising 'EoS-Sensor-01' as _eos-sensor._tcp on port 8080
```

### Service Discovery

Find other EoS devices on the network:

```c
int mdns_find_gateway(void) {
    eos_net_addr_t addr;

    eos_log(EOS_LOG_INFO, "MDNS", "Searching for _eos-gateway._tcp ...");
    int rc = eos_mdns_resolve("_eos-gateway._tcp", &addr, 10000);

    if (rc == 0) {
        eos_log(EOS_LOG_INFO, "MDNS",
                "Found gateway at %d.%d.%d.%d:%d",
                (addr.ip >> 0) & 0xFF, (addr.ip >> 8) & 0xFF,
                (addr.ip >> 16) & 0xFF, (addr.ip >> 24) & 0xFF,
                addr.port);
    } else {
        eos_log(EOS_LOG_WARN, "MDNS", "No gateway found (timeout)");
    }

    return rc;
}
```

Expected output:

```
[  3.000] MDNS : Searching for _eos-gateway._tcp ...
[  3.150] MDNS : Found gateway at 192.168.1.1:8080
```

### Zero-Conf Device Pairing

Use mDNS discovery + HTTP handshake for automatic device pairing:

```c
int auto_pair_with_gateway(void) {
    // Step 1: Discover gateway via mDNS
    eos_net_addr_t gw;
    int rc = eos_mdns_resolve("_eos-gateway._tcp", &gw, 10000);
    if (rc != 0) {
        eos_log(EOS_LOG_ERROR, "PAIR", "Gateway not found");
        return rc;
    }

    // Step 2: Send pairing request via HTTP POST
    char url[128];
    snprintf(url, sizeof(url), "http://%d.%d.%d.%d:%d/api/pair",
             (gw.ip >> 0) & 0xFF, (gw.ip >> 8) & 0xFF,
             (gw.ip >> 16) & 0xFF, (gw.ip >> 24) & 0xFF, gw.port);

    char body[128];
    int len = snprintf(body, sizeof(body),
                       "{\"device_id\":\"eos-sensor-01\","
                       "\"type\":\"environmental\","
                       "\"capabilities\":[\"temp\",\"humidity\"]}");

    eos_http_response_t resp = {0};
    rc = eos_http_post(url, body, len, "application/json", &resp);

    if (rc == 0 && resp.status_code == 200) {
        eos_log(EOS_LOG_INFO, "PAIR", "Paired with gateway at %s", url);
        // Parse response for MQTT broker details, etc.
    } else {
        eos_log(EOS_LOG_ERROR, "PAIR", "Pairing failed: HTTP %d", resp.status_code);
    }

    eos_http_response_free(&resp);
    return rc;
}
```

Expected output:

```
[  3.000] MDNS : Searching for _eos-gateway._tcp ...
[  3.150] MDNS : Found gateway at 192.168.1.1:8080
[  3.300] PAIR : Paired with gateway at http://192.168.1.1:8080/api/pair
```

---

## Error Handling Patterns

### Common Error Codes

| Code | Constant | Meaning |
|------|----------|---------|
| 0 | `EOS_OK` | Success |
| -1 | `EOS_ERR_NET_INIT` | Network stack not initialized |
| -2 | `EOS_ERR_DNS` | DNS resolution failed |
| -3 | `EOS_ERR_CONNECT` | TCP connection refused or timed out |
| -4 | `EOS_ERR_SEND` | Send buffer full or connection reset |
| -5 | `EOS_ERR_RECV` | Receive timed out |
| -6 | `EOS_ERR_TLS` | TLS handshake failed |
| -7 | `EOS_ERR_MQTT_PROTO` | MQTT protocol error (bad CONNACK) |
| -8 | `EOS_ERR_MQTT_AUTH` | MQTT authentication rejected |

### Defensive Networking Pattern

```c
void networking_main(void) {
    // 1. Initialize network stack
    int rc = eos_net_init();
    if (rc != 0) {
        eos_log(EOS_LOG_FATAL, "NET", "Network init failed: %d", rc);
        return;
    }

    // 2. Wait for IP address (WiFi/DHCP)
    uint32_t wait_start = eos_uptime_ms();
    while (!eos_net_has_ip()) {
        if (eos_uptime_ms() - wait_start > 30000) {
            eos_log(EOS_LOG_FATAL, "NET", "No IP after 30 s — aborting");
            return;
        }
        eos_sleep_ms(100);
    }

    // 3. Advertise via mDNS
    mdns_advertise_sensor();

    // 4. Connect MQTT with retry
    int mqtt_retries = 0;
    while (mqtt_retries < 5) {
        if (mqtt_connect() == 0) break;
        mqtt_retries++;
        eos_sleep_ms(2000 * mqtt_retries);
    }

    if (mqtt == EOS_MQTT_INVALID) {
        eos_log(EOS_LOG_FATAL, "NET", "MQTT connect failed after 5 attempts");
        return;
    }

    // 5. Subscribe and run event loop
    mqtt_subscribe_topics();

    while (1) {
        rc = eos_mqtt_loop(mqtt, 100);
        if (rc != 0) {
            eos_log(EOS_LOG_WARN, "MQTT", "Loop error %d — reconnecting", rc);
            eos_mqtt_disconnect(mqtt);
            eos_sleep_ms(5000);
            mqtt_connect();
            mqtt_subscribe_topics();
        }
    }
}
```

---

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| `eos_net_init()` returns -1 | WiFi/Ethernet not configured | Call `eos_wifi_connect()` first |
| DNS resolution fails | No DNS server | Set DNS via DHCP or `eos_wifi_set_dns()` |
| MQTT CONNACK error 5 | Auth rejected | Check username/password in config |
| MQTT keepalive timeout | Loop not called often enough | Reduce loop timeout or increase keepalive |
| HTTP 408 Request Timeout | Server too slow | Increase `eos_net_recv()` timeout |
| mDNS no response | Multicast blocked | Ensure router allows 224.0.0.251:5353 |
| TLS handshake fails | Certificate mismatch | Verify CA cert matches broker's cert |
| `EOS_SOCKET_INVALID` | Max sockets reached | Close unused sockets, increase limit |

---

## Next Steps

- [STM32 Deployment](stm32-deployment.md) — Board bring-up
- [Debugging Guide](debugging-guide.md) — GDB, hard faults, memory leaks
- [Wireless Stacks](wireless-stacks.md) — BLE, WiFi, LoRa configuration
- [Configuration Examples](configuration-examples.md) — Product profiles and build variants
