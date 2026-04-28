// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file net.c
 * @brief EoS Networking ΓÇö Stub implementation
 */

#include <eos/net.h>
#include <string.h>

#if EOS_ENABLE_NET

static bool net_initialized = false;

int eos_net_init(void)
{
    net_initialized = true;
    return 0;
}

void eos_net_deinit(void)
{
    net_initialized = false;
}

eos_socket_t eos_net_socket(eos_net_proto_t proto)
{
    (void)proto;
    if (!net_initialized) return EOS_SOCKET_INVALID;
    static int next_fd = 1;
    return (eos_socket_t)next_fd++;
}

int eos_net_connect(eos_socket_t sock, const eos_net_addr_t *addr)
{
    (void)sock; (void)addr;
    return -1;
}

int eos_net_bind(eos_socket_t sock, uint16_t port)
{
    (void)sock; (void)port;
    if (!net_initialized) return -1;
    return 0;
}

int eos_net_listen(eos_socket_t sock, int backlog)
{
    (void)sock; (void)backlog;
    if (!net_initialized) return -1;
    return 0;
}

eos_socket_t eos_net_accept(eos_socket_t sock, eos_net_addr_t *client_addr)
{
    (void)sock; (void)client_addr;
    return EOS_SOCKET_INVALID;
}

int eos_net_send(eos_socket_t sock, const void *data, size_t len)
{
    (void)sock; (void)data; (void)len;
    return -1;
}

int eos_net_recv(eos_socket_t sock, void *buf, size_t len, uint32_t timeout_ms)
{
    (void)sock; (void)buf; (void)len; (void)timeout_ms;
    return -1;
}

int eos_net_sendto(eos_socket_t sock, const void *data, size_t len,
                    const eos_net_addr_t *dest)
{
    (void)sock; (void)data; (void)len; (void)dest;
    return -1;
}

int eos_net_recvfrom(eos_socket_t sock, void *buf, size_t len,
                      eos_net_addr_t *src, uint32_t timeout_ms)
{
    (void)sock; (void)buf; (void)len; (void)src; (void)timeout_ms;
    return -1;
}

int eos_net_close(eos_socket_t sock)
{
    if (sock == EOS_SOCKET_INVALID) return -1;
    (void)sock;
    return 0;
}

int eos_net_resolve(const char *hostname, uint32_t *ip)
{
    (void)hostname; (void)ip;
    return -1;
}

/* ---- HTTP ---- */

int eos_http_get(const char *url, eos_http_response_t *resp)
{
    (void)url;
    if (resp) memset(resp, 0, sizeof(*resp));
    return -1;
}

int eos_http_post(const char *url, const void *body, size_t body_len,
                   const char *content_type, eos_http_response_t *resp)
{
    (void)url; (void)body; (void)body_len; (void)content_type;
    if (resp) memset(resp, 0, sizeof(*resp));
    return -1;
}

void eos_http_response_free(eos_http_response_t *resp)
{
    if (resp) {
        resp->body = NULL;
        resp->body_len = 0;
    }
}

/* ---- MQTT ---- */

eos_mqtt_handle_t eos_mqtt_connect(const eos_mqtt_config_t *cfg)
{
    (void)cfg;
    return EOS_MQTT_INVALID;
}

int eos_mqtt_disconnect(eos_mqtt_handle_t handle)
{
    (void)handle;
    return -1;
}

int eos_mqtt_publish(eos_mqtt_handle_t handle, const char *topic,
                      const void *payload, size_t len, uint8_t qos)
{
    (void)handle; (void)topic; (void)payload; (void)len; (void)qos;
    return -1;
}

int eos_mqtt_subscribe(eos_mqtt_handle_t handle, const char *topic,
                         uint8_t qos, eos_mqtt_msg_callback_t cb, void *ctx)
{
    (void)handle; (void)topic; (void)qos; (void)cb; (void)ctx;
    return -1;
}

int eos_mqtt_unsubscribe(eos_mqtt_handle_t handle, const char *topic)
{
    (void)handle; (void)topic;
    return -1;
}

int eos_mqtt_loop(eos_mqtt_handle_t handle, uint32_t timeout_ms)
{
    (void)handle; (void)timeout_ms;
    return -1;
}

/* ---- mDNS ---- */

int eos_mdns_init(void) { return -1; }
void eos_mdns_deinit(void) {}

int eos_mdns_register(const char *service_name, const char *service_type,
                       uint16_t port)
{
    (void)service_name; (void)service_type; (void)port;
    return -1;
}

int eos_mdns_resolve(const char *service_type, eos_net_addr_t *addr,
                      uint32_t timeout_ms)
{
    (void)service_type; (void)addr; (void)timeout_ms;
    return -1;
}

#endif /* EOS_ENABLE_NET */