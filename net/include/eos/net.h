// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file net.h
 * @brief EoS Networking Abstraction
 *
 * Transport-agnostic socket API, HTTP client, MQTT client, and mDNS.
 * Works over WiFi, Ethernet, BLE, or cellular transports.
 */

#ifndef EOS_NET_H
#define EOS_NET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <eos/eos_config.h>

#if EOS_ENABLE_NET

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Socket API
 * ============================================================ */

typedef enum {
    EOS_NET_TCP = 0,
    EOS_NET_UDP = 1,
} eos_net_proto_t;

typedef struct {
    uint32_t ip;        /* network byte order */
    uint16_t port;
} eos_net_addr_t;

typedef int eos_socket_t;

#define EOS_SOCKET_INVALID (-1)

int  eos_net_init(void);
void eos_net_deinit(void);

eos_socket_t eos_net_socket(eos_net_proto_t proto);
int  eos_net_connect(eos_socket_t sock, const eos_net_addr_t *addr);
int  eos_net_bind(eos_socket_t sock, uint16_t port);
int  eos_net_listen(eos_socket_t sock, int backlog);
eos_socket_t eos_net_accept(eos_socket_t sock, eos_net_addr_t *client_addr);
int  eos_net_send(eos_socket_t sock, const void *data, size_t len);
int  eos_net_recv(eos_socket_t sock, void *buf, size_t len, uint32_t timeout_ms);
int  eos_net_sendto(eos_socket_t sock, const void *data, size_t len,
                     const eos_net_addr_t *dest);
int  eos_net_recvfrom(eos_socket_t sock, void *buf, size_t len,
                       eos_net_addr_t *src, uint32_t timeout_ms);
int  eos_net_close(eos_socket_t sock);

/* DNS resolution */
int  eos_net_resolve(const char *hostname, uint32_t *ip);

/* ============================================================
 * HTTP Client
 * ============================================================ */

typedef struct {
    int      status_code;
    uint8_t *body;
    size_t   body_len;
    char     content_type[64];
} eos_http_response_t;

int eos_http_get(const char *url, eos_http_response_t *resp);
int eos_http_post(const char *url, const void *body, size_t body_len,
                   const char *content_type, eos_http_response_t *resp);
void eos_http_response_free(eos_http_response_t *resp);

/* ============================================================
 * MQTT Client
 * ============================================================ */

typedef struct {
    char     broker_host[128];
    uint16_t broker_port;
    char     client_id[64];
    char     username[64];
    char     password[64];
    uint16_t keepalive_sec;
} eos_mqtt_config_t;

typedef void (*eos_mqtt_msg_callback_t)(const char *topic,
                                         const uint8_t *payload,
                                         size_t payload_len, void *ctx);

typedef int eos_mqtt_handle_t;

#define EOS_MQTT_INVALID (-1)

eos_mqtt_handle_t eos_mqtt_connect(const eos_mqtt_config_t *cfg);
int  eos_mqtt_disconnect(eos_mqtt_handle_t handle);
int  eos_mqtt_publish(eos_mqtt_handle_t handle, const char *topic,
                       const void *payload, size_t len, uint8_t qos);
int  eos_mqtt_subscribe(eos_mqtt_handle_t handle, const char *topic,
                          uint8_t qos, eos_mqtt_msg_callback_t cb, void *ctx);
int  eos_mqtt_unsubscribe(eos_mqtt_handle_t handle, const char *topic);
int  eos_mqtt_loop(eos_mqtt_handle_t handle, uint32_t timeout_ms);

/* ============================================================
 * mDNS
 * ============================================================ */

int eos_mdns_init(void);
void eos_mdns_deinit(void);
int eos_mdns_register(const char *service_name, const char *service_type,
                       uint16_t port);
int eos_mdns_resolve(const char *service_type, eos_net_addr_t *addr,
                      uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ENABLE_NET */
#endif /* EOS_NET_H */
