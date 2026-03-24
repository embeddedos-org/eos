// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_SERVICE_H
#define EOS_SERVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EOS_SVC_MAX_SERVICES 32
#define EOS_SVC_NAME_MAX     32

typedef enum {
    EOS_SVC_STOPPED  = 0,
    EOS_SVC_STARTING = 1,
    EOS_SVC_RUNNING  = 2,
    EOS_SVC_STOPPING = 3,
    EOS_SVC_FAILED   = 4,
    EOS_SVC_RESTARTING = 5
} EosSvcState;

typedef enum {
    EOS_SVC_RESTART_NEVER  = 0,
    EOS_SVC_RESTART_ALWAYS = 1,
    EOS_SVC_RESTART_ON_FAIL = 2
} EosSvcRestartPolicy;

typedef struct {
    int  (*start)(void *ctx);
    void (*stop)(void *ctx);
    int  (*health)(void *ctx);
    void *ctx;
} EosSvcOps;

typedef struct {
    char                name[EOS_SVC_NAME_MAX];
    EosSvcState         state;
    EosSvcRestartPolicy restart_policy;
    EosSvcOps           ops;
    uint32_t            start_count;
    uint32_t            fail_count;
    uint32_t            max_restarts;
    uint32_t            restart_delay_ms;
    uint32_t            watchdog_timeout_ms;
    uint32_t            last_health_ms;
    uint32_t            uptime_ms;
    int                 pid;
    char                depends[4][EOS_SVC_NAME_MAX];
    int                 dep_count;
} EosService;

typedef struct {
    EosService services[EOS_SVC_MAX_SERVICES];
    int        count;
    int        running;
    uint32_t   health_interval_ms;
} EosSvcManager;

/* Service manager lifecycle */
int  eos_svc_init(EosSvcManager *mgr);
int  eos_svc_start_all(EosSvcManager *mgr);
void eos_svc_stop_all(EosSvcManager *mgr);
void eos_svc_tick(EosSvcManager *mgr, uint32_t now_ms);

/* Service registration */
int  eos_svc_register(EosSvcManager *mgr, const char *name, const EosSvcOps *ops);
int  eos_svc_set_restart_policy(EosSvcManager *mgr, const char *name,
                                 EosSvcRestartPolicy policy, uint32_t max, uint32_t delay_ms);
int  eos_svc_set_watchdog(EosSvcManager *mgr, const char *name, uint32_t timeout_ms);
int  eos_svc_add_dependency(EosSvcManager *mgr, const char *name, const char *depends_on);

/* Individual service control */
int  eos_svc_start(EosSvcManager *mgr, const char *name);
int  eos_svc_stop(EosSvcManager *mgr, const char *name);
int  eos_svc_restart(EosSvcManager *mgr, const char *name);

/* Query */
EosSvcState eos_svc_get_state(EosSvcManager *mgr, const char *name);
EosService *eos_svc_find(EosSvcManager *mgr, const char *name);
void        eos_svc_dump(EosSvcManager *mgr);

#ifdef __cplusplus
}
#endif

#endif /* EOS_SERVICE_H */