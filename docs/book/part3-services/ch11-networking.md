# Chapter 11: Networking

**Author:** Srikanth Patchava & EmbeddedOS Contributors

---

## 11.1 Overview

The EoS networking layer (eos/net.h) provides a transport-agnostic API
covering sockets, HTTP, MQTT, and mDNS service discovery. It works over
WiFi, Ethernet, BLE, or cellular transports through a unified interface
gated by the EOS_ENABLE_NET configuration flag.

```
+--------------------------------------------------------+
|                   Application                          |
+----------+----------+----------+-----------------------+
|  Socket  |  HTTP    |  MQTT    |  mDNS                 |
|  API     |  Client  |  Client  |  Discovery             |
+----------+----------+----------+-----------------------+
|                  <eos/net.h>                            |
+--------------------------------------------------------+
|         Transport HAL (WiFi / ETH / BLE / Cell)        |
+--------------------------------------------------------+
```

## 11.2 Socket API

The socket API provides BSD-style TCP/UDP communication primitives.

### 11.2.1 Core Types

```c
typedef enum {
    EOS_NET_TCP = 0,
    EOS_NET_UDP = 1,
} eos_net_proto_t;

typedef struct {
    uint32_t ip;        /* Network byte order */
    uint16_t port;
} eos_net_addr_t;

typedef int eos_socket_t;
#define EOS_SOCKET_INVALID (-1)
```

### 11.2.2 Initialization

```c
#include <eos/net.h>

/* Initialize the network stack -- call once at startup */
int rc = eos_net_init();
if (rc != 0) {
    printf("Network init failed: %d\n", rc);
}

/* Shutdown at teardown */
eos_net_deinit();
```

### 11.2.3 TCP Client Example

```c
void tcp_client_example(void)
{
    /* Resolve hostname */
    uint32_t ip;
    eos_net_resolve("api.example.com", &ip);

    /* Create TCP socket */
    eos_socket_t sock = eos_net_socket(EOS_NET_TCP);
    if (sock == EOS_SOCKET_INVALID) return;

    /* Connect */
    eos_net_addr_t addr = { .ip = ip, .port = 8080 };
    eos_net_connect(sock, &addr);

    /* Send request */
    const char *msg = "GET /status HTTP/1.0\r\n\r\n";
    eos_net_send(sock, msg, strlen(msg));

    /* Receive response */
    char buf[512];
    int n = eos_net_recv(sock, buf, sizeof(buf), 5000);
    if (n > 0) {
        buf[n] = 0;
        printf("Response: %s\n", buf);
    }

    eos_net_close(sock);
}
```

### 11.2.4 TCP Server Example

```c
void tcp_server_example(void)
{
    eos_socket_t srv = eos_net_socket(EOS_NET_TCP);
    eos_net_bind(srv, 80);
    eos_net_listen(srv, 4);

    while (1) {
        eos_net_addr_t client;
        eos_socket_t conn = eos_net_accept(srv, &client);
        if (conn == EOS_SOCKET_INVALID) continue;

        char buf[256];
        int n = eos_net_recv(conn, buf, sizeof(buf), 3000);
        if (n > 0) {
            const char *resp = "HTTP/1.0 200 OK\r\n\r\nHello";
            eos_net_send(conn, resp, strlen(resp));
        }
        eos_net_close(conn);
    }
}
```

### 11.2.5 UDP Communication

```c
void udp_example(void)
{
    eos_socket_t sock = eos_net_socket(EOS_NET_UDP);
    eos_net_bind(sock, 5000);   /* Bind for receiving */

    /* Send a datagram */
    eos_net_addr_t dest = { .ip = 0xC0A80001, .port = 5001 };
    eos_net_sendto(sock, "ping", 4, &dest);

    /* Receive a datagram */
    char buf[128];
    eos_net_addr_t src;
    int n = eos_net_recvfrom(sock, buf, sizeof(buf), &src, 2000);
    if (n > 0) {
        printf("Got %d bytes from port %u\n", n, src.port);
    }

    eos_net_close(sock);
}
```

### 11.2.6 Socket API Reference

| Function            | Description                              |
|---------------------|------------------------------------------|
| eos_net_init        | Initialize network stack                 |
| eos_net_deinit      | Shutdown network stack                   |
| eos_net_socket      | Create TCP or UDP socket                 |
| eos_net_connect     | Connect to remote address                |
| eos_net_bind        | Bind socket to local port                |
| eos_net_listen      | Mark socket as passive (server)          |
| eos_net_accept      | Accept incoming connection               |
| eos_net_send        | Send data on connected socket            |
| eos_net_recv        | Receive with timeout (ms)                |
| eos_net_sendto      | Send UDP datagram                        |
| eos_net_recvfrom    | Receive UDP datagram with source addr    |
| eos_net_close       | Close socket                             |
| eos_net_resolve     | DNS hostname resolution                  |

## 11.3 HTTP Client

The built-in HTTP client handles GET and POST requests with automatic
memory management for response bodies.

### 11.3.1 Response Structure

```c
typedef struct {
    int      status_code;       /* HTTP status (200, 404, etc.) */
    uint8_t *body;              /* Heap-allocated body          */
    size_t   body_len;          /* Body length in bytes         */
    char     content_type[64];  /* Content-Type header value    */
} eos_http_response_t;
```

### 11.3.2 GET Request

```c
eos_http_response_t resp;
int rc = eos_http_get("http://192.168.1.100/api/status", &resp);

if (rc == 0 && resp.status_code == 200) {
    printf("Content-Type: %s\n", resp.content_type);
    printf("Body (%zu bytes): %.*s\n",
           resp.body_len, (int)resp.body_len, resp.body);
}
eos_http_response_free(&resp);   /* Always free! */
```

### 11.3.3 POST Request

```c
const char *json = "{\"sensor\":\"temp\",\"value\":23.5}";
eos_http_response_t resp;

int rc = eos_http_post(
    "http://192.168.1.100/api/data",
    json, strlen(json),
    "application/json",
    &resp
);

if (rc == 0) {
    printf("POST status: %d\n", resp.status_code);
}
eos_http_response_free(&resp);
```

## 11.4 MQTT Client

MQTT is the primary protocol for IoT telemetry and command channels.
EoS provides a full MQTT 3.1.1 client with QoS 0/1/2 support.

### 11.4.1 Configuration

```c
typedef struct {
    char     broker_host[128];
    uint16_t broker_port;        /* Default: 1883      */
    char     client_id[64];
    char     username[64];
    char     password[64];
    uint16_t keepalive_sec;      /* Default: 60        */
} eos_mqtt_config_t;
```

### 11.4.2 Publish / Subscribe Example

```c
/* Message callback */
void on_message(const char *topic, const uint8_t *payload,
                size_t len, void *ctx)
{
    printf("[%s] %.*s\n", topic, (int)len, payload);
}

void mqtt_example(void)
{
    eos_mqtt_config_t cfg = {
        .broker_host  = "192.168.1.50",
        .broker_port  = 1883,
        .client_id    = "eos-device-01",
        .keepalive_sec = 30,
    };

    eos_mqtt_handle_t mqtt = eos_mqtt_connect(&cfg);
    if (mqtt == EOS_MQTT_INVALID) return;

    /* Subscribe to command topic */
    eos_mqtt_subscribe(mqtt, "device/cmd", 1, on_message, NULL);

    /* Publish telemetry */
    const char *data = "{\"temp\":22.5}";
    eos_mqtt_publish(mqtt, "device/telemetry",
                     data, strlen(data), 0);

    /* Event loop -- call periodically */
    for (int i = 0; i < 100; i++) {
        eos_mqtt_loop(mqtt, 1000);
    }

    eos_mqtt_unsubscribe(mqtt, "device/cmd");
    eos_mqtt_disconnect(mqtt);
}
```

### 11.4.3 MQTT QoS Levels

| QoS | Name              | Guarantee                    | Use Case            |
|-----|-------------------|------------------------------|---------------------|
| 0   | At most once      | Fire-and-forget              | Telemetry streams   |
| 1   | At least once     | Acknowledged delivery        | Sensor readings     |
| 2   | Exactly once      | Four-step handshake          | Commands, billing   |

### 11.4.4 MQTT API Reference

| Function               | Description                        |
|------------------------|------------------------------------|
| eos_mqtt_connect       | Connect to broker                  |
| eos_mqtt_disconnect    | Graceful disconnect                |
| eos_mqtt_publish       | Publish message to topic           |
| eos_mqtt_subscribe     | Subscribe with callback            |
| eos_mqtt_unsubscribe   | Unsubscribe from topic             |
| eos_mqtt_loop          | Process network events (timeout)   |

## 11.5 mDNS Service Discovery

Multicast DNS enables zero-configuration service discovery on local
networks without a central DNS server.

### 11.5.1 Register a Service

```c
eos_mdns_init();

/* Advertise an HTTP server */
eos_mdns_register("EoS-Device", "_http._tcp", 80);

/* Advertise an MQTT broker */
eos_mdns_register("EoS-Broker", "_mqtt._tcp", 1883);
```

### 11.5.2 Discover a Service

```c
eos_net_addr_t addr;
int rc = eos_mdns_resolve("_http._tcp", &addr, 5000);

if (rc == 0) {
    printf("Found service at %u.%u.%u.%u:%u\n",
           (addr.ip >> 24) & 0xFF, (addr.ip >> 16) & 0xFF,
           (addr.ip >> 8)  & 0xFF,  addr.ip & 0xFF,
           addr.port);
}

eos_mdns_deinit();
```

## 11.6 TLS Integration

EoS networking supports TLS through compile-time integration with
mbedTLS or wolfSSL. When EOS_ENABLE_TLS is defined, the HTTP and
MQTT clients automatically negotiate TLS connections for https URLs
and MQTT port 8883.

```
+----------------------------------+
|         Application              |
+----------+-----------------------+
|  HTTP    |  MQTT                 |
|  (TLS)   |  (TLS)               |
+----------+-----------------------+
|      mbedTLS / wolfSSL           |
+----------------------------------+
|      EoS Socket Layer            |
+----------------------------------+
```

### 11.6.1 Enabling TLS

In eos_config.h:

```c
#define EOS_ENABLE_NET      1
#define EOS_ENABLE_TLS      1
#define EOS_TLS_BACKEND     EOS_TLS_MBEDTLS  /* or EOS_TLS_WOLFSSL */
```

## 11.7 Network Architecture Patterns

### 11.7.1 Sensor Gateway

```c
void sensor_gateway(void)
{
    eos_net_init();

    /* Connect to cloud MQTT broker */
    eos_mqtt_config_t cfg = {
        .broker_host = "cloud.example.com",
        .broker_port = 8883,   /* TLS */
        .client_id   = "gateway-01",
    };
    eos_mqtt_handle_t mqtt = eos_mqtt_connect(&cfg);

    /* Register for local mDNS discovery */
    eos_mdns_init();
    eos_mdns_register("Gateway", "_eos._tcp", 9000);

    /* Publish sensor data periodically */
    while (1) {
        char payload[128];
        snprintf(payload, sizeof(payload),
                 "{\"temp\":%.1f,\"hum\":%.1f}",
                 read_temp(), read_hum());

        eos_mqtt_publish(mqtt, "sensors/env",
                         payload, strlen(payload), 1);
        eos_mqtt_loop(mqtt, 1000);
    }
}
```

## 11.8 Error Handling

All networking functions return negative error codes on failure:

| Return Value | Meaning                    |
|--------------|----------------------------|
| 0            | Success                    |
| -1           | General error              |
| -2           | Timeout                    |
| -3           | Connection refused         |
| -4           | DNS resolution failed      |
| -5           | TLS handshake failed       |

## 11.9 Complete API Reference

| Function                  | Description                          |
|---------------------------|--------------------------------------|
| eos_net_init              | Initialize network stack             |
| eos_net_deinit            | Shutdown network stack               |
| eos_net_socket            | Create socket (TCP/UDP)              |
| eos_net_connect           | Connect to remote host               |
| eos_net_bind              | Bind to local port                   |
| eos_net_listen            | Listen for connections               |
| eos_net_accept            | Accept incoming connection           |
| eos_net_send              | Send data                            |
| eos_net_recv              | Receive data with timeout            |
| eos_net_sendto            | Send UDP datagram                    |
| eos_net_recvfrom          | Receive UDP datagram                 |
| eos_net_close             | Close socket                         |
| eos_net_resolve           | DNS resolution                       |
| eos_http_get              | HTTP GET request                     |
| eos_http_post             | HTTP POST request                    |
| eos_http_response_free    | Free HTTP response memory            |
| eos_mqtt_connect          | Connect to MQTT broker               |
| eos_mqtt_disconnect       | Disconnect from broker               |
| eos_mqtt_publish          | Publish to topic                     |
| eos_mqtt_subscribe        | Subscribe to topic                   |
| eos_mqtt_unsubscribe      | Unsubscribe from topic               |
| eos_mqtt_loop             | Process MQTT events                  |
| eos_mdns_init             | Initialize mDNS                      |
| eos_mdns_deinit           | Shutdown mDNS                        |
| eos_mdns_register         | Register mDNS service                |
| eos_mdns_resolve          | Discover mDNS service                |

---

*Next: [Chapter 12 -- Filesystem](ch12-filesystem.md)*
