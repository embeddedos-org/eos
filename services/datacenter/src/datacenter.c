// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/datacenter.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ==== Virtualization ==== */

void eos_vm_init(EosVirtualization *vm, uint32_t total_cores, uint32_t total_mem_mb) {
    memset(vm, 0, sizeof(*vm));
    vm->total_cpu_cores = total_cores;
    vm->total_memory_mb = total_mem_mb;
    vm->isolation_enabled = 1;
}

int eos_vm_create(EosVirtualization *vm, const char *name, uint32_t cores, uint32_t mem_mb) {
    if (vm->count >= EOS_VM_MAX_INSTANCES) return -1;
    EosVmInstance *inst = &vm->instances[vm->count];
    strncpy(inst->name, name, EOS_VM_MAX_NAME - 1);
    inst->id = vm->count;
    inst->state = EOS_VM_STOPPED;
    inst->cpu_cores = cores;
    inst->memory_mb = mem_mb;
    return vm->count++;
}

int eos_vm_start(EosVirtualization *vm, int id) {
    if (id < 0 || id >= vm->count) return -1;
    vm->instances[id].state = EOS_VM_RUNNING;
    return 0;
}

int eos_vm_stop(EosVirtualization *vm, int id) {
    if (id < 0 || id >= vm->count) return -1;
    vm->instances[id].state = EOS_VM_STOPPED;
    return 0;
}

int eos_vm_pause(EosVirtualization *vm, int id) {
    if (id < 0 || id >= vm->count) return -1;
    vm->instances[id].state = EOS_VM_PAUSED;
    return 0;
}

int eos_vm_destroy(EosVirtualization *vm, int id) {
    if (id < 0 || id >= vm->count) return -1;
    memmove(&vm->instances[id], &vm->instances[id+1],
            sizeof(EosVmInstance) * (size_t)(vm->count - id - 1));
    vm->count--;
    return 0;
}

const EosVmInstance *eos_vm_get(const EosVirtualization *vm, int id) {
    if (id < 0 || id >= vm->count) return NULL;
    return &vm->instances[id];
}

void eos_vm_dump(const EosVirtualization *vm) {
    const char *states[] = {"STOPPED","RUNNING","PAUSED","ERROR"};
    printf("VMs: %d/%d (cores=%u, mem=%uMB)\n", vm->count, EOS_VM_MAX_INSTANCES,
           vm->total_cpu_cores, vm->total_memory_mb);
    for (int i = 0; i < vm->count; i++) {
        const EosVmInstance *v = &vm->instances[i];
        printf("  [%d] %-16s %s cores=%u mem=%uMB cpu=%u%% mem=%u%%\n",
               v->id, v->name, states[v->state], v->cpu_cores, v->memory_mb,
               v->cpu_usage_pct, v->mem_usage_pct);
    }
}

/* ==== BMC/IPMI ==== */

int eos_bmc_init(EosBmc *bmc, uint32_t i2c_addr) {
    memset(bmc, 0, sizeof(*bmc));
    bmc->info.i2c_addr = i2c_addr;
    return 0;
}

int eos_bmc_send_cmd(EosBmc *bmc, EosBmcCommand cmd, const void *data, size_t len) {
    (void)bmc; (void)cmd; (void)data; (void)len;
    return 0;
}

int eos_bmc_get_sensor(EosBmc *bmc, uint8_t id, EosBmcSensor *out) {
    for (int i = 0; i < bmc->sensor_count; i++) {
        if (bmc->sensors[i].sensor_id == id) { *out = bmc->sensors[i]; return 0; }
    }
    return -1;
}

int eos_bmc_read_all_sensors(EosBmc *bmc) { (void)bmc; return 0; }
int eos_bmc_power_control(EosBmc *bmc, int action) { (void)bmc; (void)action; return 0; }

void eos_bmc_dump(const EosBmc *bmc) {
    printf("BMC: id=%u fw=%u ipmi=%u addr=0x%x %s\n",
           bmc->info.device_id, bmc->info.firmware_rev, bmc->info.ipmi_version,
           bmc->info.i2c_addr, bmc->info.connected ? "connected" : "disconnected");
    for (int i = 0; i < bmc->sensor_count; i++) {
        const EosBmcSensor *s = &bmc->sensors[i];
        printf("  [%u] %-16s %d %s %s\n", s->sensor_id, s->name, s->value, s->unit,
               s->alarm ? "ALARM" : "OK");
    }
}

/* ==== RAID ==== */

int eos_raid_init(EosRaidArray *arr, EosRaidLevel level, int disk_count) {
    memset(arr, 0, sizeof(*arr));
    arr->level = level;
    arr->stripe_size_kb = 64;
    (void)disk_count;
    return 0;
}

int eos_raid_add_disk(EosRaidArray *arr, uint64_t size, const char *model) {
    if (arr->disk_count >= EOS_RAID_MAX_DISKS) return -1;
    EosRaidDisk *d = &arr->disks[arr->disk_count];
    d->id = arr->disk_count;
    d->state = EOS_DISK_ONLINE;
    d->size_bytes = size;
    strncpy(d->model, model, 31);
    arr->total_capacity += size;
    arr->disk_count++;

    switch (arr->level) {
    case EOS_RAID_0: arr->usable_capacity = arr->total_capacity; break;
    case EOS_RAID_1: arr->usable_capacity = arr->total_capacity / 2; break;
    case EOS_RAID_5: arr->usable_capacity = arr->total_capacity * (uint64_t)(arr->disk_count - 1) / (uint64_t)arr->disk_count; break;
    case EOS_RAID_6: arr->usable_capacity = arr->total_capacity * (uint64_t)(arr->disk_count - 2) / (uint64_t)arr->disk_count; break;
    case EOS_RAID_10: arr->usable_capacity = arr->total_capacity / 2; break;
    }
    return d->id;
}

int eos_raid_remove_disk(EosRaidArray *arr, int disk_id) {
    if (disk_id < 0 || disk_id >= arr->disk_count) return -1;
    arr->disks[disk_id].state = EOS_DISK_FAILED;
    arr->degraded = 1;
    return 0;
}

int eos_raid_rebuild(EosRaidArray *arr, int failed_id, int spare_id) {
    if (failed_id < 0 || spare_id < 0) return -1;
    arr->disks[spare_id].state = EOS_DISK_REBUILDING;
    arr->rebuilding = 1;
    arr->rebuild_pct = 0;
    arr->disks[spare_id].state = EOS_DISK_ONLINE;
    arr->disks[failed_id].state = EOS_DISK_SPARE;
    arr->rebuilding = 0;
    arr->rebuild_pct = 100;
    arr->degraded = 0;
    return 0;
}

int eos_raid_read(const EosRaidArray *arr, uint64_t offset, void *buf, uint32_t len) {
    (void)arr; (void)offset; (void)buf; (void)len;
    return 0;
}

int eos_raid_write(EosRaidArray *arr, uint64_t offset, const void *buf, uint32_t len) {
    (void)arr; (void)offset; (void)buf; (void)len;
    return 0;
}

int eos_raid_check_health(EosRaidArray *arr) {
    arr->degraded = 0;
    for (int i = 0; i < arr->disk_count; i++) {
        if (arr->disks[i].state == EOS_DISK_FAILED || arr->disks[i].state == EOS_DISK_DEGRADED)
            arr->degraded = 1;
    }
    return arr->degraded ? -1 : 0;
}

void eos_raid_dump(const EosRaidArray *arr) {
    const char *levels[] = {"RAID-0","RAID-1","RAID-5","RAID-6","RAID-10"};
    const char *dstates[] = {"ONLINE","DEGRADED","FAILED","REBUILDING","SPARE"};
    printf("RAID: %s %d disks capacity=%lluGB usable=%lluGB %s\n",
           levels[arr->level], arr->disk_count,
           (unsigned long long)(arr->total_capacity / (1024ULL*1024*1024)),
           (unsigned long long)(arr->usable_capacity / (1024ULL*1024*1024)),
           arr->degraded ? "DEGRADED" : "HEALTHY");
    for (int i = 0; i < arr->disk_count; i++) {
        printf("  [%d] %-12s %s %lluGB errors=%u\n", arr->disks[i].id, arr->disks[i].model,
               dstates[arr->disks[i].state],
               (unsigned long long)(arr->disks[i].size_bytes / (1024ULL*1024*1024)),
               arr->disks[i].error_count);
    }
}

/* ==== Thermal + Fan ==== */

int eos_thermal_init(EosThermalMgr *tm) { memset(tm, 0, sizeof(*tm)); tm->auto_control = 1; return 0; }

int eos_thermal_add_zone(EosThermalMgr *tm, const char *name, int32_t warn_c, int32_t crit_c, int32_t shut_c) {
    if (tm->zone_count >= EOS_THERMAL_MAX_ZONES) return -1;
    EosThermalZone *z = &tm->zones[tm->zone_count++];
    strncpy(z->name, name, 31);
    z->warning_mc = warn_c * 1000; z->critical_mc = crit_c * 1000; z->shutdown_mc = shut_c * 1000;
    return 0;
}

int eos_thermal_add_fan(EosThermalMgr *tm, const char *name, uint32_t min_rpm, uint32_t max_rpm) {
    if (tm->fan_count >= EOS_FAN_MAX) return -1;
    EosFan *f = &tm->fans[tm->fan_count++];
    strncpy(f->name, name, 31);
    f->min_rpm = min_rpm; f->max_rpm = max_rpm; f->duty_pct = 30;
    return 0;
}

int eos_thermal_read_all(EosThermalMgr *tm) { (void)tm; return 0; }

int eos_thermal_update_fans(EosThermalMgr *tm) {
    if (!tm->auto_control) return 0;
    int32_t max_temp = 0;
    for (int i = 0; i < tm->zone_count; i++)
        if (tm->zones[i].temp_mc > max_temp) max_temp = tm->zones[i].temp_mc;
    uint32_t duty = 30;
    if (max_temp > 60000) duty = 50;
    if (max_temp > 70000) duty = 75;
    if (max_temp > 80000) duty = 100;
    for (int i = 0; i < tm->fan_count; i++) tm->fans[i].duty_pct = duty;
    return 0;
}

int eos_fan_set_duty(EosThermalMgr *tm, int fan_id, uint32_t duty_pct) {
    if (fan_id < 0 || fan_id >= tm->fan_count) return -1;
    tm->fans[fan_id].duty_pct = duty_pct > 100 ? 100 : duty_pct;
    return 0;
}

int eos_thermal_check_alarms(EosThermalMgr *tm) {
    int alarms = 0;
    for (int i = 0; i < tm->zone_count; i++) {
        tm->zones[i].alarm = (tm->zones[i].temp_mc >= tm->zones[i].warning_mc);
        if (tm->zones[i].alarm) alarms++;
    }
    for (int i = 0; i < tm->fan_count; i++) {
        if (tm->fans[i].rpm < tm->fans[i].min_rpm && tm->fans[i].duty_pct > 0)
            tm->fans[i].failed = 1;
    }
    return alarms;
}

void eos_thermal_dump(const EosThermalMgr *tm) {
    printf("Thermal: %d zones, %d fans (auto=%s)\n", tm->zone_count, tm->fan_count,
           tm->auto_control ? "on" : "off");
    for (int i = 0; i < tm->zone_count; i++)
        printf("  Zone %-12s %d.%dC warn=%dC crit=%dC %s\n", tm->zones[i].name,
               tm->zones[i].temp_mc/1000, (tm->zones[i].temp_mc%1000)/100,
               tm->zones[i].warning_mc/1000, tm->zones[i].critical_mc/1000,
               tm->zones[i].alarm ? "ALARM" : "OK");
    for (int i = 0; i < tm->fan_count; i++)
        printf("  Fan  %-12s %urpm duty=%u%% %s\n", tm->fans[i].name, tm->fans[i].rpm,
               tm->fans[i].duty_pct, tm->fans[i].failed ? "FAILED" : "OK");
}

/* ==== Health Monitor ==== */

int eos_health_init(EosHealthMonitor *hm, uint32_t interval_sec) {
    memset(hm, 0, sizeof(*hm));
    hm->check_interval_sec = interval_sec;
    hm->overall = EOS_HEALTH_OK;
    return 0;
}

int eos_health_register(EosHealthMonitor *hm, const char *name, EosHealthStatus (*check_fn)(void)) {
    if (hm->count >= EOS_HEALTH_MAX_CHECKS) return -1;
    EosHealthCheck *c = &hm->checks[hm->count++];
    strncpy(c->name, name, 31);
    c->status = EOS_HEALTH_UNKNOWN;
    (void)check_fn;
    return 0;
}

int eos_health_run_all(EosHealthMonitor *hm) {
    hm->overall = EOS_HEALTH_OK;
    for (int i = 0; i < hm->count; i++) {
        if (hm->checks[i].status > hm->overall)
            hm->overall = hm->checks[i].status;
    }
    return 0;
}

EosHealthStatus eos_health_overall(const EosHealthMonitor *hm) { return hm->overall; }

void eos_health_dump(const EosHealthMonitor *hm) {
    const char *sts[] = {"OK","WARNING","CRITICAL","UNKNOWN"};
    printf("Health: %s (%d checks)\n", sts[hm->overall], hm->count);
    for (int i = 0; i < hm->count; i++)
        printf("  %-20s %s errors=%u\n", hm->checks[i].name, sts[hm->checks[i].status],
               hm->checks[i].error_count);
}

/* ==== Load Balancer ==== */

int eos_lb_init(EosLoadBalancer *lb, EosLbAlgorithm algo) {
    memset(lb, 0, sizeof(*lb));
    lb->algorithm = algo;
    return 0;
}

int eos_lb_add_backend(EosLoadBalancer *lb, const char *addr, uint16_t port, uint32_t weight) {
    if (lb->count >= EOS_LB_MAX_BACKENDS) return -1;
    EosLbBackend *b = &lb->backends[lb->count];
    strncpy(b->addr, addr, 63);
    b->port = port; b->weight = weight; b->healthy = 1;
    return lb->count++;
}

int eos_lb_remove_backend(EosLoadBalancer *lb, const char *addr) {
    for (int i = 0; i < lb->count; i++) {
        if (strcmp(lb->backends[i].addr, addr) == 0) {
            memmove(&lb->backends[i], &lb->backends[i+1],
                    sizeof(EosLbBackend) * (size_t)(lb->count - i - 1));
            lb->count--;
            return 0;
        }
    }
    return -1;
}

int eos_lb_select(EosLoadBalancer *lb, const char *client_ip) {
    if (lb->count == 0) return -1;
    int selected = -1;

    switch (lb->algorithm) {
    case EOS_LB_ROUND_ROBIN:
        for (int tries = 0; tries < lb->count; tries++) {
            int idx = (int)(lb->rr_index++ % (uint32_t)lb->count);
            if (lb->backends[idx].healthy) { selected = idx; break; }
        }
        break;
    case EOS_LB_WEIGHTED_ROUND_ROBIN: {
        uint32_t max_w = 0;
        for (int i = 0; i < lb->count; i++) {
            if (lb->backends[i].healthy && lb->backends[i].weight > max_w) {
                max_w = lb->backends[i].weight; selected = i;
            }
        }
        break;
    }
    case EOS_LB_LEAST_CONNECTIONS: {
        uint32_t min_c = UINT32_MAX;
        for (int i = 0; i < lb->count; i++) {
            if (lb->backends[i].healthy && lb->backends[i].active_conns < min_c) {
                min_c = lb->backends[i].active_conns; selected = i;
            }
        }
        break;
    }
    case EOS_LB_IP_HASH: {
        uint32_t hash = 5381;
        if (client_ip) for (const char *p = client_ip; *p; p++) hash = ((hash << 5) + hash) ^ (unsigned char)*p;
        selected = (int)(hash % (uint32_t)lb->count);
        if (!lb->backends[selected].healthy) selected = 0;
        break;
    }
    case EOS_LB_RANDOM:
        selected = (int)((uint32_t)rand() % (uint32_t)lb->count);
        break;
    }

    if (selected >= 0) {
        lb->backends[selected].active_conns++;
        lb->backends[selected].total_requests++;
        lb->total_dispatched++;
    }
    return selected;
}

void eos_lb_release(EosLoadBalancer *lb, int idx) {
    if (idx >= 0 && idx < lb->count && lb->backends[idx].active_conns > 0)
        lb->backends[idx].active_conns--;
}

void eos_lb_dump(const EosLoadBalancer *lb) {
    const char *algos[] = {"ROUND_ROBIN","WEIGHTED_RR","LEAST_CONN","RANDOM","IP_HASH"};
    printf("LB: %s %d backends total=%llu\n", algos[lb->algorithm], lb->count,
           (unsigned long long)lb->total_dispatched);
    for (int i = 0; i < lb->count; i++) {
        const EosLbBackend *b = &lb->backends[i];
        printf("  [%d] %s:%u w=%u conns=%u reqs=%llu %s\n",
               i, b->addr, b->port, b->weight, b->active_conns,
               (unsigned long long)b->total_requests, b->healthy ? "UP" : "DOWN");
    }
}

/* ==== Routing ==== */

int eos_route_init(EosRoutingTable *rt) { memset(rt, 0, sizeof(*rt)); return 0; }

int eos_route_add(EosRoutingTable *rt, uint32_t dest, uint32_t mask,
                  uint32_t gateway, uint32_t metric, const char *iface) {
    if (rt->count >= EOS_ROUTE_MAX) return -1;
    EosRoute *r = &rt->routes[rt->count++];
    r->dest_ip = dest; r->mask = mask; r->gateway = gateway;
    r->metric = metric; r->active = 1;
    strncpy(r->iface, iface, 15);
    return 0;
}

int eos_route_remove(EosRoutingTable *rt, uint32_t dest, uint32_t mask) {
    for (int i = 0; i < rt->count; i++) {
        if (rt->routes[i].dest_ip == dest && rt->routes[i].mask == mask) {
            memmove(&rt->routes[i], &rt->routes[i+1],
                    sizeof(EosRoute) * (size_t)(rt->count - i - 1));
            rt->count--;
            return 0;
        }
    }
    return -1;
}

int eos_route_lookup(const EosRoutingTable *rt, uint32_t dest_ip) {
    int best = -1; uint32_t best_mask = 0; uint32_t best_metric = UINT32_MAX;
    for (int i = 0; i < rt->count; i++) {
        if (!rt->routes[i].active) continue;
        if ((dest_ip & rt->routes[i].mask) == (rt->routes[i].dest_ip & rt->routes[i].mask)) {
            if (rt->routes[i].mask > best_mask ||
                (rt->routes[i].mask == best_mask && rt->routes[i].metric < best_metric)) {
                best = i; best_mask = rt->routes[i].mask; best_metric = rt->routes[i].metric;
            }
        }
    }
    return best;
}

void eos_route_dump(const EosRoutingTable *rt) {
    printf("Routes: %d entries\n", rt->count);
    for (int i = 0; i < rt->count; i++) {
        const EosRoute *r = &rt->routes[i];
        printf("  %u.%u.%u.%u/%u via %u.%u.%u.%u dev %s metric %u\n",
               (r->dest_ip>>24)&0xFF,(r->dest_ip>>16)&0xFF,(r->dest_ip>>8)&0xFF,r->dest_ip&0xFF,
               eos_popcount32(r->mask),
               (r->gateway>>24)&0xFF,(r->gateway>>16)&0xFF,(r->gateway>>8)&0xFF,r->gateway&0xFF,
               r->iface, r->metric);
    }
}

/* ==== QoS ==== */

int eos_qos_init(EosQosManager *qm, uint32_t bandwidth_kbps) {
    memset(qm, 0, sizeof(*qm));
    qm->total_bandwidth_kbps = bandwidth_kbps;
    return 0;
}

int eos_qos_add_queue(EosQosManager *qm, const char *name, EosQosType type,
                      uint32_t priority, uint32_t rate_kbps, uint32_t burst_kb) {
    if (qm->count >= EOS_QOS_MAX_QUEUES) return -1;
    EosQosQueue *q = &qm->queues[qm->count++];
    strncpy(q->name, name, 31);
    q->type = type; q->priority = priority;
    q->rate_kbps = rate_kbps; q->burst_kb = burst_kb;
    q->tokens = burst_kb;
    return 0;
}

int eos_qos_enqueue(EosQosManager *qm, int queue_id, uint32_t pkt_size) {
    if (queue_id < 0 || queue_id >= qm->count) return -1;
    EosQosQueue *q = &qm->queues[queue_id];
    uint32_t pkt_kb = (pkt_size + 1023) / 1024;
    if (q->type == EOS_QOS_TOKEN_BUCKET && q->tokens < pkt_kb) {
        q->packets_dropped++;
        return -1;
    }
    q->tokens -= pkt_kb;
    q->packets_passed++;
    return 0;
}

int eos_qos_dequeue(EosQosManager *qm) {
    int best = -1; uint32_t best_prio = UINT32_MAX;
    for (int i = 0; i < qm->count; i++) {
        if (qm->queues[i].packets_passed > 0 && qm->queues[i].priority < best_prio) {
            best_prio = qm->queues[i].priority; best = i;
        }
    }
    return best;
}

void eos_qos_dump(const EosQosManager *qm) {
    const char *types[] = {"PRIORITY","TOKEN_BUCKET","WEIGHTED_FAIR"};
    printf("QoS: %d queues, bandwidth=%ukbps\n", qm->count, qm->total_bandwidth_kbps);
    for (int i = 0; i < qm->count; i++) {
        const EosQosQueue *q = &qm->queues[i];
        printf("  %-12s %s prio=%u rate=%ukbps passed=%llu dropped=%llu\n",
               q->name, types[q->type], q->priority, q->rate_kbps,
               (unsigned long long)q->packets_passed, (unsigned long long)q->packets_dropped);
    }
}

/* ==== Failover ==== */

int eos_failover_init(EosFailoverCluster *fc, EosFailoverMode mode, uint32_t timeout_ms) {
    memset(fc, 0, sizeof(*fc));
    fc->mode = mode;
    fc->timeout_ms = timeout_ms;
    fc->primary_idx = -1;
    fc->auto_failback = 1;
    return 0;
}

int eos_failover_add_node(EosFailoverCluster *fc, const char *name,
                          const char *addr, int is_primary) {
    if (fc->count >= EOS_FAILOVER_MAX_NODES) return -1;
    EosFailoverNode *n = &fc->nodes[fc->count];
    strncpy(n->name, name, 31);
    strncpy(n->addr, addr, 63);
    n->state = is_primary ? EOS_NODE_ACTIVE : EOS_NODE_STANDBY;
    n->is_primary = is_primary;
    if (is_primary) fc->primary_idx = fc->count;
    return fc->count++;
}

int eos_failover_heartbeat(EosFailoverCluster *fc, int node_idx) {
    if (node_idx < 0 || node_idx >= fc->count) return -1;
    fc->nodes[node_idx].last_heartbeat = fc->nodes[node_idx].heartbeat_ms;
    return 0;
}

int eos_failover_check(EosFailoverCluster *fc) {
    for (int i = 0; i < fc->count; i++) {
        if (fc->nodes[i].state == EOS_NODE_ACTIVE &&
            fc->nodes[i].heartbeat_ms - fc->nodes[i].last_heartbeat > fc->timeout_ms) {
            return eos_failover_trigger(fc, i);
        }
    }
    return 0;
}

int eos_failover_trigger(EosFailoverCluster *fc, int failed_idx) {
    if (failed_idx < 0 || failed_idx >= fc->count) return -1;
    fc->nodes[failed_idx].state = EOS_NODE_FAILED;
    fc->nodes[failed_idx].failover_count++;

    for (int i = 0; i < fc->count; i++) {
        if (i != failed_idx && fc->nodes[i].state == EOS_NODE_STANDBY) {
            fc->nodes[i].state = EOS_NODE_ACTIVE;
            fc->primary_idx = i;
            return 0;
        }
    }
    return -1;
}

int eos_failover_failback(EosFailoverCluster *fc, int recovered_idx) {
    if (recovered_idx < 0 || recovered_idx >= fc->count) return -1;
    if (!fc->auto_failback) return 0;

    fc->nodes[recovered_idx].state = EOS_NODE_SYNCING;
    fc->nodes[recovered_idx].state = EOS_NODE_ACTIVE;

    if (fc->nodes[recovered_idx].is_primary) {
        fc->nodes[fc->primary_idx].state = EOS_NODE_STANDBY;
        fc->primary_idx = recovered_idx;
    }
    return 0;
}

void eos_failover_dump(const EosFailoverCluster *fc) {
    const char *modes[] = {"ACTIVE_STANDBY","ACTIVE_ACTIVE","N+1"};
    const char *states[] = {"ACTIVE","STANDBY","FAILED","SYNCING"};
    printf("Failover: %s %d nodes timeout=%ums primary=%d\n",
           modes[fc->mode], fc->count, fc->timeout_ms, fc->primary_idx);
    for (int i = 0; i < fc->count; i++) {
        const EosFailoverNode *n = &fc->nodes[i];
        printf("  [%d] %-12s %s %s failovers=%u %s\n",
               i, n->name, n->addr, states[n->state], n->failover_count,
               n->is_primary ? "(primary)" : "");
    }
}
