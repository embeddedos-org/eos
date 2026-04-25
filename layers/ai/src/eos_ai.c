// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file eos_ai.c
 * @brief EoS AI integration — lifecycle management and intent bridge.
 *
 * Wires ENI (input), EIPC (transport), and EAI (agent) together
 * under a single EoS-managed lifecycle.
 */

#include "eos_ai.h"
#include <string.h>
#include <stdio.h>

/* ── Internal state ── */

static struct {
    eos_ai_config_t  config;
    eos_ai_state_t   state;
    bool             initialized;
} s_ai;

/* ── Ring buffer for AI message passing ── */

#define AI_RINGBUF_SIZE 4096

typedef struct {
    uint8_t  buf[AI_RINGBUF_SIZE];
    volatile uint32_t head;  /* write position */
    volatile uint32_t tail;  /* read position */
} ai_ringbuf_t;

static ai_ringbuf_t s_ringbuf;

/* Message header written into the ring buffer before each payload:
 *   [1 byte type] [2 bytes payload_len (LE)] [payload ...] */
enum {
    AI_MSG_INTENT       = 0x01,
    AI_MSG_TOOL_REQUEST = 0x02,
    AI_MSG_RESULT       = 0x03,
};

static void ringbuf_init(ai_ringbuf_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

static uint32_t ringbuf_used(const ai_ringbuf_t *rb)
{
    return (rb->head - rb->tail) % AI_RINGBUF_SIZE;
}

static uint32_t ringbuf_free(const ai_ringbuf_t *rb)
{
    /* Reserve 1 byte to distinguish full from empty */
    return AI_RINGBUF_SIZE - 1 - ringbuf_used(rb);
}

static int ringbuf_write(ai_ringbuf_t *rb, const uint8_t *data, uint32_t len)
{
    if (len > ringbuf_free(rb)) return -1;
    for (uint32_t i = 0; i < len; i++) {
        rb->buf[rb->head % AI_RINGBUF_SIZE] = data[i];
        rb->head = (rb->head + 1) % AI_RINGBUF_SIZE;
    }
    return 0;
}

static int ringbuf_read(ai_ringbuf_t *rb, uint8_t *data, uint32_t len)
{
    if (len > ringbuf_used(rb)) return -1;
    for (uint32_t i = 0; i < len; i++) {
        data[i] = rb->buf[rb->tail % AI_RINGBUF_SIZE];
        rb->tail = (rb->tail + 1) % AI_RINGBUF_SIZE;
    }
    return 0;
}

static int ringbuf_write_msg(ai_ringbuf_t *rb, uint8_t type,
                              const uint8_t *payload, uint16_t payload_len)
{
    uint32_t total = 3 + (uint32_t)payload_len; /* type(1) + len(2) + data */
    if (total > ringbuf_free(rb)) return -1;

    uint8_t hdr[3];
    hdr[0] = type;
    hdr[1] = (uint8_t)(payload_len & 0xFF);
    hdr[2] = (uint8_t)((payload_len >> 8) & 0xFF);
    ringbuf_write(rb, hdr, 3);
    ringbuf_write(rb, payload, payload_len);
    return 0;
}

static int ringbuf_read_msg(ai_ringbuf_t *rb, uint8_t *type,
                             uint8_t *payload, uint16_t max_len,
                             uint16_t *out_len)
{
    if (ringbuf_used(rb) < 3) return -1;

    uint8_t hdr[3];
    /* Peek header without consuming */
    uint32_t saved_tail = rb->tail;
    ringbuf_read(rb, hdr, 3);
    uint16_t plen = (uint16_t)hdr[1] | ((uint16_t)hdr[2] << 8);

    if (plen > max_len || ringbuf_used(rb) < plen) {
        rb->tail = saved_tail; /* restore */
        return -1;
    }

    *type = hdr[0];
    *out_len = plen;
    ringbuf_read(rb, payload, plen);
    return 0;
}

/* ── Simple yield / busy-wait helper ── */

static void ai_yield(void)
{
#if defined(__GNUC__)
    __asm__ volatile("" ::: "memory");
#endif
}

/* ── Lifecycle ── */

eos_ai_status_t eos_ai_init(const eos_ai_config_t *config)
{
    if (!config) return EOS_AI_ERR_CONFIG;

    memset(&s_ai, 0, sizeof(s_ai));
    memcpy(&s_ai.config, config, sizeof(*config));

    if (!config->eipc_address || !config->eipc_hmac_key) {
        return EOS_AI_ERR_CONFIG;
    }

    s_ai.state = EOS_AI_STATE_INITIALIZING;
    ringbuf_init(&s_ringbuf);

    s_ai.initialized = true;
    s_ai.state = EOS_AI_STATE_READY;
    return EOS_AI_OK;
}

eos_ai_status_t eos_ai_start(void)
{
    if (!s_ai.initialized) return EOS_AI_ERR_NOT_READY;
    if (s_ai.state == EOS_AI_STATE_RUNNING) return EOS_AI_OK;

    s_ai.state = EOS_AI_STATE_RUNNING;
    return EOS_AI_OK;
}

eos_ai_status_t eos_ai_stop(void)
{
    if (s_ai.state != EOS_AI_STATE_RUNNING) return EOS_AI_OK;

    s_ai.state = EOS_AI_STATE_STOPPED;
    return EOS_AI_OK;
}

eos_ai_state_t eos_ai_state(void)
{
    return s_ai.state;
}

/* ── Intent bridge ── */

eos_ai_status_t eos_ai_send_intent(const char *intent, float confidence)
{
    if (!intent) return EOS_AI_ERR_CONFIG;
    if (s_ai.state != EOS_AI_STATE_RUNNING) return EOS_AI_ERR_NOT_READY;

    /* Serialize: intent_string \0 confidence_float (4 bytes) */
    size_t intent_len = strlen(intent);
    uint16_t payload_len = (uint16_t)(intent_len + 1 + sizeof(float));

    /* Guard against overflow */
    if (payload_len > AI_RINGBUF_SIZE - 4) return EOS_AI_ERR_RUNTIME;

    uint8_t payload[AI_RINGBUF_SIZE];
    memcpy(payload, intent, intent_len + 1);   /* include NUL */
    memcpy(payload + intent_len + 1, &confidence, sizeof(float));

    if (ringbuf_write_msg(&s_ringbuf, AI_MSG_INTENT, payload, payload_len) != 0) {
        return EOS_AI_ERR_RUNTIME;
    }

    return EOS_AI_OK;
}

eos_ai_status_t eos_ai_send_tool_request(const char *tool,
                                          const char *const *keys,
                                          const char *const *values,
                                          int arg_count)
{
    if (!tool) return EOS_AI_ERR_CONFIG;
    if (s_ai.state != EOS_AI_STATE_RUNNING) return EOS_AI_ERR_NOT_READY;

    /* Serialize to JSON-like format:
     * {"tool":"<name>","args":{"<k1>":"<v1>","<k2>":"<v2>",...}} */
    uint8_t payload[AI_RINGBUF_SIZE];
    int pos = 0;
    int max = (int)(AI_RINGBUF_SIZE - 4);

    pos += snprintf((char *)payload + pos, (size_t)(max - pos),
                    "{\"tool\":\"%s\",\"args\":{", tool);
    if (pos >= max) return EOS_AI_ERR_RUNTIME;

    for (int i = 0; i < arg_count && keys && values; i++) {
        if (!keys[i] || !values[i]) continue;
        if (i > 0 && pos < max) {
            payload[pos++] = ',';
        }
        pos += snprintf((char *)payload + pos, (size_t)(max - pos),
                        "\"%s\":\"%s\"", keys[i], values[i]);
        if (pos >= max) return EOS_AI_ERR_RUNTIME;
    }

    pos += snprintf((char *)payload + pos, (size_t)(max - pos), "}}");
    if (pos >= max) return EOS_AI_ERR_RUNTIME;

    uint16_t payload_len = (uint16_t)pos;
    if (ringbuf_write_msg(&s_ringbuf, AI_MSG_TOOL_REQUEST, payload, payload_len) != 0) {
        return EOS_AI_ERR_RUNTIME;
    }

    return EOS_AI_OK;
}

/* ── Query ── */

eos_ai_status_t eos_ai_receive_result(char *buf, size_t buf_size,
                                       uint32_t timeout_ms)
{
    if (!buf || buf_size == 0) return EOS_AI_ERR_CONFIG;
    if (s_ai.state != EOS_AI_STATE_RUNNING) return EOS_AI_ERR_NOT_READY;

    /* Poll the ring buffer with a simple busy-wait loop.
     * In a real RTOS this would use a semaphore or event flag. */
    volatile uint32_t elapsed = 0;
    const uint32_t poll_step = 1; /* ~1ms per iteration (approximation) */

    while (elapsed < timeout_ms) {
        if (ringbuf_used(&s_ringbuf) >= 3) {
            uint8_t type = 0;
            uint16_t out_len = 0;
            uint8_t tmp[AI_RINGBUF_SIZE];

            if (ringbuf_read_msg(&s_ringbuf, &type, tmp,
                                 (uint16_t)(AI_RINGBUF_SIZE - 1), &out_len) == 0) {
                /* Copy result to caller buffer */
                size_t copy_len = (out_len < buf_size - 1) ? out_len : buf_size - 1;
                memcpy(buf, tmp, copy_len);
                buf[copy_len] = '\0';
                return EOS_AI_OK;
            }
        }
        ai_yield();
        elapsed += poll_step;
    }

    buf[0] = '\0';
    return EOS_AI_ERR_RUNTIME; /* timeout */
}

/* ── Diagnostics ── */

void eos_ai_print_status(void)
{
    const char *state_str;
    switch (s_ai.state) {
    case EOS_AI_STATE_STOPPED:      state_str = "STOPPED"; break;
    case EOS_AI_STATE_INITIALIZING: state_str = "INITIALIZING"; break;
    case EOS_AI_STATE_READY:        state_str = "READY"; break;
    case EOS_AI_STATE_RUNNING:      state_str = "RUNNING"; break;
    case EOS_AI_STATE_ERROR:        state_str = "ERROR"; break;
    default:                        state_str = "UNKNOWN"; break;
    }

    printf("EoS AI Subsystem:\n");
    printf("  state:       %s\n", state_str);
    printf("  variant:     %s\n",
           s_ai.config.variant == EOS_AI_VARIANT_MIN ? "min" : "framework");
    printf("  eipc_addr:   %s\n", s_ai.config.eipc_address ? s_ai.config.eipc_address : "(none)");
    printf("  eni_service: %s\n", s_ai.config.eni_service_id ? s_ai.config.eni_service_id : "(none)");
    printf("  eai_service: %s\n", s_ai.config.eai_service_id ? s_ai.config.eai_service_id : "(none)");
    printf("  audit:       %s\n", s_ai.config.enable_audit ? "on" : "off");
    printf("  policy:      %s\n", s_ai.config.enable_policy ? "on" : "off");
    printf("  ringbuf:     %u/%d bytes used\n", ringbuf_used(&s_ringbuf), AI_RINGBUF_SIZE);
}
