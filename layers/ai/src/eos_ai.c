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

    /*
     * In a full build with ENI_EIPC_ENABLED and EAI_EIPC_ENABLED:
     *
     *   ENI side:
     *     eni_eipc_bridge_init(&bridge, ENI_EIPC_MODE_DUAL);
     *     eni_eipc_bridge_connect(&bridge, address, hmac_key, eni_service_id);
     *
     *   EAI side:
     *     eai_eipc_listener_init(&listener);
     *     eai_eipc_listener_start(&listener, address, hmac_key);
     *     eai_eipc_listener_accept(&listener);
     *
     * For now, this is a configuration-only init that prepares
     * the subsystem for start().
     */

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

    /*
     * In full build: eni_eipc_bridge_send_intent(&bridge, intent, confidence);
     * This stub logs the intent for development/testing.
     */
    (void)confidence;
    return EOS_AI_OK;
}

eos_ai_status_t eos_ai_send_tool_request(const char *tool,
                                          const char *const *keys,
                                          const char *const *values,
                                          int arg_count)
{
    if (!tool) return EOS_AI_ERR_CONFIG;
    if (s_ai.state != EOS_AI_STATE_RUNNING) return EOS_AI_ERR_NOT_READY;

    (void)keys;
    (void)values;
    (void)arg_count;
    return EOS_AI_OK;
}

/* ── Query ── */

eos_ai_status_t eos_ai_receive_result(char *buf, size_t buf_size,
                                       uint32_t timeout_ms)
{
    if (!buf || buf_size == 0) return EOS_AI_ERR_CONFIG;
    if (s_ai.state != EOS_AI_STATE_RUNNING) return EOS_AI_ERR_NOT_READY;

    (void)timeout_ms;
    buf[0] = '\0';
    return EOS_AI_OK;
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
}
