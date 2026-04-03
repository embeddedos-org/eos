// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_AI_H
#define EOS_AI_H

/**
 * @file eos_ai.h
 * @brief EoS AI Integration Layer
 *
 * Provides the bridge between EoS OS services and the EAI/ENI/EIPC
 * AI subsystem. This header unifies the AI layer APIs under the
 * EoS namespace and provides lifecycle management.
 *
 * Architecture:
 *   EoS Application
 *       ↓
 *   eos_ai_* API (this header)
 *       ↓
 *   ┌─────────┐    ┌──────┐    ┌─────────┐
 *   │   ENI   │ ←→ │ EIPC │ ←→ │   EAI   │
 *   │ (input) │    │ (IPC)│    │ (agent) │
 *   └─────────┘    └──────┘    └─────────┘
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── AI subsystem status ── */

typedef enum {
    EOS_AI_OK = 0,
    EOS_AI_ERR_INIT,
    EOS_AI_ERR_CONFIG,
    EOS_AI_ERR_RUNTIME,
    EOS_AI_ERR_CONNECT,
    EOS_AI_ERR_NOT_READY,
} eos_ai_status_t;

typedef enum {
    EOS_AI_VARIANT_MIN,
    EOS_AI_VARIANT_FRAMEWORK,
} eos_ai_variant_t;

typedef enum {
    EOS_AI_STATE_STOPPED,
    EOS_AI_STATE_INITIALIZING,
    EOS_AI_STATE_READY,
    EOS_AI_STATE_RUNNING,
    EOS_AI_STATE_ERROR,
} eos_ai_state_t;

/* ── Configuration ── */

typedef struct {
    eos_ai_variant_t variant;
    const char      *eipc_address;
    const char      *eipc_hmac_key;
    const char      *eni_service_id;
    const char      *eai_service_id;
    bool             enable_audit;
    bool             enable_policy;
    uint32_t         heartbeat_interval_ms;
} eos_ai_config_t;

/* ── Lifecycle ── */

eos_ai_status_t eos_ai_init(const eos_ai_config_t *config);
eos_ai_status_t eos_ai_start(void);
eos_ai_status_t eos_ai_stop(void);
eos_ai_state_t  eos_ai_state(void);

/* ── Intent bridge (ENI → EIPC → EAI) ── */

eos_ai_status_t eos_ai_send_intent(const char *intent, float confidence);
eos_ai_status_t eos_ai_send_tool_request(const char *tool,
                                          const char *const *keys,
                                          const char *const *values,
                                          int arg_count);

/* ── Query ── */

eos_ai_status_t eos_ai_receive_result(char *buf, size_t buf_size,
                                       uint32_t timeout_ms);

/* ── Diagnostics ── */

void eos_ai_print_status(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_AI_H */
