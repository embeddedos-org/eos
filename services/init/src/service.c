// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/service.h"
#include <string.h>
#include <stdio.h>

int eos_svc_init(EosSvcManager *mgr) {
    if (!mgr) return -1;
    memset(mgr, 0, sizeof(*mgr));
    mgr->health_interval_ms = 5000;
    return 0;
}

EosService *eos_svc_find(EosSvcManager *mgr, const char *name) {
    if (!mgr || !name) return NULL;
    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->services[i].name, name) == 0)
            return &mgr->services[i];
    }
    return NULL;
}

int eos_svc_register(EosSvcManager *mgr, const char *name, const EosSvcOps *ops) {
    if (!mgr || !name || !ops) return -1;
    if (mgr->count >= EOS_SVC_MAX_SERVICES) return -1;
    if (eos_svc_find(mgr, name)) return -1;

    EosService *svc = &mgr->services[mgr->count];
    memset(svc, 0, sizeof(*svc));
    strncpy(svc->name, name, EOS_SVC_NAME_MAX - 1);
    svc->ops = *ops;
    svc->state = EOS_SVC_STOPPED;
    svc->restart_policy = EOS_SVC_RESTART_ON_FAIL;
    svc->max_restarts = 3;
    svc->restart_delay_ms = 1000;
    svc->watchdog_timeout_ms = 30000;
    mgr->count++;
    return 0;
}

int eos_svc_set_restart_policy(EosSvcManager *mgr, const char *name,
                                EosSvcRestartPolicy policy, uint32_t max, uint32_t delay_ms) {
    EosService *svc = eos_svc_find(mgr, name);
    if (!svc) return -1;
    svc->restart_policy = policy;
    svc->max_restarts = max;
    svc->restart_delay_ms = delay_ms;
    return 0;
}

int eos_svc_set_watchdog(EosSvcManager *mgr, const char *name, uint32_t timeout_ms) {
    EosService *svc = eos_svc_find(mgr, name);
    if (!svc) return -1;
    svc->watchdog_timeout_ms = timeout_ms;
    return 0;
}

int eos_svc_add_dependency(EosSvcManager *mgr, const char *name, const char *depends_on) {
    EosService *svc = eos_svc_find(mgr, name);
    if (!svc || !depends_on) return -1;
    if (svc->dep_count >= 4) return -1;
    strncpy(svc->depends[svc->dep_count], depends_on, EOS_SVC_NAME_MAX - 1);
    svc->dep_count++;
    return 0;
}

static int deps_satisfied(EosSvcManager *mgr, EosService *svc) {
    for (int i = 0; i < svc->dep_count; i++) {
        EosService *dep = eos_svc_find(mgr, svc->depends[i]);
        if (!dep || dep->state != EOS_SVC_RUNNING) return 0;
    }
    return 1;
}

int eos_svc_start(EosSvcManager *mgr, const char *name) {
    EosService *svc = eos_svc_find(mgr, name);
    if (!svc) return -1;
    if (svc->state == EOS_SVC_RUNNING) return 0;
    if (!deps_satisfied(mgr, svc)) return -1;

    svc->state = EOS_SVC_STARTING;
    int ret = 0;
    if (svc->ops.start)
        ret = svc->ops.start(svc->ops.ctx);
    if (ret == 0) {
        svc->state = EOS_SVC_RUNNING;
        svc->start_count++;
        svc->uptime_ms = 0;
    } else {
        svc->state = EOS_SVC_FAILED;
        svc->fail_count++;
    }
    return ret;
}

int eos_svc_stop(EosSvcManager *mgr, const char *name) {
    EosService *svc = eos_svc_find(mgr, name);
    if (!svc) return -1;
    if (svc->state == EOS_SVC_STOPPED) return 0;

    svc->state = EOS_SVC_STOPPING;
    if (svc->ops.stop)
        svc->ops.stop(svc->ops.ctx);
    svc->state = EOS_SVC_STOPPED;
    return 0;
}

int eos_svc_restart(EosSvcManager *mgr, const char *name) {
    eos_svc_stop(mgr, name);
    return eos_svc_start(mgr, name);
}

int eos_svc_start_all(EosSvcManager *mgr) {
    if (!mgr) return -1;
    mgr->running = 1;

    /* Start services in dependency order — multiple passes */
    int started;
    do {
        started = 0;
        for (int i = 0; i < mgr->count; i++) {
            EosService *svc = &mgr->services[i];
            if (svc->state == EOS_SVC_STOPPED && deps_satisfied(mgr, svc)) {
                if (eos_svc_start(mgr, svc->name) == 0)
                    started++;
            }
        }
    } while (started > 0);
    return 0;
}

void eos_svc_stop_all(EosSvcManager *mgr) {
    if (!mgr) return;
    /* Stop in reverse order */
    for (int i = mgr->count - 1; i >= 0; i--)
        eos_svc_stop(mgr, mgr->services[i].name);
    mgr->running = 0;
}

void eos_svc_tick(EosSvcManager *mgr, uint32_t now_ms) {
    if (!mgr || !mgr->running) return;

    for (int i = 0; i < mgr->count; i++) {
        EosService *svc = &mgr->services[i];
        if (svc->state == EOS_SVC_RUNNING) {
            svc->uptime_ms = now_ms;

            /* Health check */
            if (svc->ops.health && (now_ms - svc->last_health_ms) >= mgr->health_interval_ms) {
                svc->last_health_ms = now_ms;
                if (svc->ops.health(svc->ops.ctx) != 0) {
                    svc->state = EOS_SVC_FAILED;
                    svc->fail_count++;
                }
            }

            /* Watchdog — if health check hasn't run in timeout period */
            if (svc->watchdog_timeout_ms > 0 &&
                svc->last_health_ms > 0 &&
                (now_ms - svc->last_health_ms) > svc->watchdog_timeout_ms) {
                svc->state = EOS_SVC_FAILED;
                svc->fail_count++;
            }
        }

        /* Auto-restart failed services */
        if (svc->state == EOS_SVC_FAILED) {
            int should_restart = 0;
            switch (svc->restart_policy) {
            case EOS_SVC_RESTART_ALWAYS:
                should_restart = 1;
                break;
            case EOS_SVC_RESTART_ON_FAIL:
                should_restart = (svc->fail_count <= svc->max_restarts);
                break;
            case EOS_SVC_RESTART_NEVER:
            default:
                break;
            }
            if (should_restart) {
                svc->state = EOS_SVC_RESTARTING;
                eos_svc_start(mgr, svc->name);
            }
        }
    }
}

EosSvcState eos_svc_get_state(EosSvcManager *mgr, const char *name) {
    EosService *svc = eos_svc_find(mgr, name);
    return svc ? svc->state : EOS_SVC_STOPPED;
}

static const char *state_str(EosSvcState s) {
    switch (s) {
    case EOS_SVC_STOPPED:    return "stopped";
    case EOS_SVC_STARTING:   return "starting";
    case EOS_SVC_RUNNING:    return "running";
    case EOS_SVC_STOPPING:   return "stopping";
    case EOS_SVC_FAILED:     return "failed";
    case EOS_SVC_RESTARTING: return "restarting";
    default:                 return "unknown";
    }
}

void eos_svc_dump(EosSvcManager *mgr) {
    if (!mgr) return;
    fprintf(stderr, "=== EoS Service Manager (%d services) ===\n", mgr->count);
    for (int i = 0; i < mgr->count; i++) {
        EosService *svc = &mgr->services[i];
        fprintf(stderr, "  %-20s  %-10s  starts:%-3u fails:%-3u",
                svc->name, state_str(svc->state), svc->start_count, svc->fail_count);
        if (svc->dep_count > 0) {
            fprintf(stderr, "  deps:");
            for (int j = 0; j < svc->dep_count; j++)
                fprintf(stderr, " %s", svc->depends[j]);
        }
        fprintf(stderr, "\n");
    }
}