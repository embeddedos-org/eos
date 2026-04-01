// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file datacenter.h
 * @brief EoS Data Center Services
 *
 * Provides virtualization, IPMI/BMC, RAID, thermal/fan management,
 * health monitoring, load balancing, routing, QoS, and failover
 * for server and data center deployments.
 */

#ifndef EOS_DATACENTER_H
#define EOS_DATACENTER_H

#include <stdint.h>
#include <stddef.h>

/* Portable population count (number of set bits) */
static inline uint32_t eos_popcount32(uint32_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return (uint32_t)__builtin_popcount(x);
#elif defined(_MSC_VER)
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    return (((x + (x >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24;
#else
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    return (((x + (x >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24;
#endif
}

/* ---- Virtualization / Container Isolation ---- */

#define EOS_VM_MAX_INSTANCES   16
#define EOS_VM_MAX_NAME        64

typedef enum {
    EOS_VM_STOPPED,
    EOS_VM_RUNNING,
    EOS_VM_PAUSED,
    EOS_VM_ERROR
} EosVmState;

typedef struct {
    char name[EOS_VM_MAX_NAME];
    int id;
    EosVmState state;
    uint32_t cpu_cores;
    uint32_t memory_mb;
    uint32_t disk_mb;
    uint32_t cpu_usage_pct;
    uint32_t mem_usage_pct;
} EosVmInstance;

typedef struct {
    EosVmInstance instances[EOS_VM_MAX_INSTANCES];
    int count;
    uint32_t total_cpu_cores;
    uint32_t total_memory_mb;
    int isolation_enabled;
} EosVirtualization;

void eos_vm_init(EosVirtualization *vm, uint32_t total_cores, uint32_t total_mem_mb);
int  eos_vm_create(EosVirtualization *vm, const char *name, uint32_t cores, uint32_t mem_mb);
int  eos_vm_start(EosVirtualization *vm, int id);
int  eos_vm_stop(EosVirtualization *vm, int id);
int  eos_vm_pause(EosVirtualization *vm, int id);
int  eos_vm_destroy(EosVirtualization *vm, int id);
const EosVmInstance *eos_vm_get(const EosVirtualization *vm, int id);
void eos_vm_dump(const EosVirtualization *vm);

/* ---- IPMI / BMC Interface ---- */

typedef enum {
    EOS_BMC_CMD_GET_DEVICE_ID     = 0x01,
    EOS_BMC_CMD_COLD_RESET        = 0x02,
    EOS_BMC_CMD_WARM_RESET        = 0x03,
    EOS_BMC_CMD_GET_SELFTEST      = 0x04,
    EOS_BMC_CMD_GET_SENSOR        = 0x2D,
    EOS_BMC_CMD_SET_POWER         = 0x08,
    EOS_BMC_CMD_GET_SOL           = 0x09,
    EOS_BMC_CMD_CHASSIS_CTRL      = 0x02,
    EOS_BMC_CMD_GET_FRU           = 0x11
} EosBmcCommand;

typedef struct {
    uint8_t device_id;
    uint8_t firmware_rev;
    uint8_t ipmi_version;
    char manufacturer[32];
    char product[32];
    int connected;
    uint32_t i2c_addr;
} EosBmcInfo;

typedef struct {
    uint8_t sensor_id;
    char name[32];
    int32_t value;
    char unit[8];
    int32_t threshold_low;
    int32_t threshold_high;
    int alarm;
} EosBmcSensor;

#define EOS_BMC_MAX_SENSORS 32

typedef struct {
    EosBmcInfo info;
    EosBmcSensor sensors[EOS_BMC_MAX_SENSORS];
    int sensor_count;
} EosBmc;

int  eos_bmc_init(EosBmc *bmc, uint32_t i2c_addr);
int  eos_bmc_send_cmd(EosBmc *bmc, EosBmcCommand cmd, const void *data, size_t len);
int  eos_bmc_get_sensor(EosBmc *bmc, uint8_t id, EosBmcSensor *out);
int  eos_bmc_read_all_sensors(EosBmc *bmc);
int  eos_bmc_power_control(EosBmc *bmc, int action);
void eos_bmc_dump(const EosBmc *bmc);

/* ---- RAID / Storage Redundancy ---- */

typedef enum {
    EOS_RAID_0,
    EOS_RAID_1,
    EOS_RAID_5,
    EOS_RAID_6,
    EOS_RAID_10
} EosRaidLevel;

typedef enum {
    EOS_DISK_ONLINE,
    EOS_DISK_DEGRADED,
    EOS_DISK_FAILED,
    EOS_DISK_REBUILDING,
    EOS_DISK_SPARE
} EosDiskState;

#define EOS_RAID_MAX_DISKS 16

typedef struct {
    int id;
    EosDiskState state;
    uint64_t size_bytes;
    uint64_t bytes_written;
    uint32_t error_count;
    char model[32];
} EosRaidDisk;

typedef struct {
    EosRaidLevel level;
    EosRaidDisk disks[EOS_RAID_MAX_DISKS];
    int disk_count;
    uint64_t total_capacity;
    uint64_t usable_capacity;
    int degraded;
    int rebuilding;
    uint32_t rebuild_pct;
    uint32_t stripe_size_kb;
} EosRaidArray;

int  eos_raid_init(EosRaidArray *arr, EosRaidLevel level, int disk_count);
int  eos_raid_add_disk(EosRaidArray *arr, uint64_t size, const char *model);
int  eos_raid_remove_disk(EosRaidArray *arr, int disk_id);
int  eos_raid_rebuild(EosRaidArray *arr, int failed_id, int spare_id);
int  eos_raid_read(const EosRaidArray *arr, uint64_t offset, void *buf, uint32_t len);
int  eos_raid_write(EosRaidArray *arr, uint64_t offset, const void *buf, uint32_t len);
int  eos_raid_check_health(EosRaidArray *arr);
void eos_raid_dump(const EosRaidArray *arr);

/* ---- Thermal Management + Fan Control ---- */

#define EOS_THERMAL_MAX_ZONES    8
#define EOS_FAN_MAX              8

typedef struct {
    char name[32];
    int32_t temp_mc;
    int32_t warning_mc;
    int32_t critical_mc;
    int32_t shutdown_mc;
    int alarm;
} EosThermalZone;

typedef struct {
    char name[32];
    uint32_t rpm;
    uint32_t duty_pct;
    uint32_t min_rpm;
    uint32_t max_rpm;
    int failed;
} EosFan;

typedef struct {
    EosThermalZone zones[EOS_THERMAL_MAX_ZONES];
    int zone_count;
    EosFan fans[EOS_FAN_MAX];
    int fan_count;
    int auto_control;
} EosThermalMgr;

int  eos_thermal_init(EosThermalMgr *tm);
int  eos_thermal_add_zone(EosThermalMgr *tm, const char *name,
                          int32_t warn_c, int32_t crit_c, int32_t shut_c);
int  eos_thermal_add_fan(EosThermalMgr *tm, const char *name,
                         uint32_t min_rpm, uint32_t max_rpm);
int  eos_thermal_read_all(EosThermalMgr *tm);
int  eos_thermal_update_fans(EosThermalMgr *tm);
int  eos_fan_set_duty(EosThermalMgr *tm, int fan_id, uint32_t duty_pct);
int  eos_thermal_check_alarms(EosThermalMgr *tm);
void eos_thermal_dump(const EosThermalMgr *tm);

/* ---- Health Monitoring / Diagnostics ---- */

typedef enum {
    EOS_HEALTH_OK,
    EOS_HEALTH_WARNING,
    EOS_HEALTH_CRITICAL,
    EOS_HEALTH_UNKNOWN
} EosHealthStatus;

typedef struct {
    char name[32];
    EosHealthStatus status;
    char message[128];
    uint32_t uptime_sec;
    uint32_t error_count;
} EosHealthCheck;

#define EOS_HEALTH_MAX_CHECKS 32

typedef struct {
    EosHealthCheck checks[EOS_HEALTH_MAX_CHECKS];
    int count;
    EosHealthStatus overall;
    uint32_t last_check_time;
    uint32_t check_interval_sec;
} EosHealthMonitor;

int  eos_health_init(EosHealthMonitor *hm, uint32_t interval_sec);
int  eos_health_register(EosHealthMonitor *hm, const char *name,
                         EosHealthStatus (*check_fn)(void));
int  eos_health_run_all(EosHealthMonitor *hm);
EosHealthStatus eos_health_overall(const EosHealthMonitor *hm);
void eos_health_dump(const EosHealthMonitor *hm);

/* ---- Load Balancing Algorithms ---- */

typedef enum {
    EOS_LB_ROUND_ROBIN,
    EOS_LB_WEIGHTED_ROUND_ROBIN,
    EOS_LB_LEAST_CONNECTIONS,
    EOS_LB_RANDOM,
    EOS_LB_IP_HASH
} EosLbAlgorithm;

typedef struct {
    char addr[64];
    uint16_t port;
    uint32_t weight;
    uint32_t active_conns;
    uint64_t total_requests;
    int healthy;
} EosLbBackend;

#define EOS_LB_MAX_BACKENDS 32

typedef struct {
    EosLbAlgorithm algorithm;
    EosLbBackend backends[EOS_LB_MAX_BACKENDS];
    int count;
    uint32_t rr_index;
    uint64_t total_dispatched;
} EosLoadBalancer;

int  eos_lb_init(EosLoadBalancer *lb, EosLbAlgorithm algo);
int  eos_lb_add_backend(EosLoadBalancer *lb, const char *addr, uint16_t port, uint32_t weight);
int  eos_lb_remove_backend(EosLoadBalancer *lb, const char *addr);
int  eos_lb_select(EosLoadBalancer *lb, const char *client_ip);
void eos_lb_release(EosLoadBalancer *lb, int backend_idx);
void eos_lb_dump(const EosLoadBalancer *lb);

/* ---- Network Routing ---- */

typedef struct {
    uint32_t dest_ip;
    uint32_t mask;
    uint32_t gateway;
    uint32_t metric;
    char iface[16];
    int active;
} EosRoute;

#define EOS_ROUTE_MAX 128

typedef struct {
    EosRoute routes[EOS_ROUTE_MAX];
    int count;
} EosRoutingTable;

int  eos_route_init(EosRoutingTable *rt);
int  eos_route_add(EosRoutingTable *rt, uint32_t dest, uint32_t mask,
                   uint32_t gateway, uint32_t metric, const char *iface);
int  eos_route_remove(EosRoutingTable *rt, uint32_t dest, uint32_t mask);
int  eos_route_lookup(const EosRoutingTable *rt, uint32_t dest_ip);
void eos_route_dump(const EosRoutingTable *rt);

/* ---- QoS / Traffic Shaping ---- */

typedef enum {
    EOS_QOS_PRIORITY,
    EOS_QOS_TOKEN_BUCKET,
    EOS_QOS_WEIGHTED_FAIR
} EosQosType;

typedef struct {
    char name[32];
    EosQosType type;
    uint32_t priority;
    uint32_t rate_kbps;
    uint32_t burst_kb;
    uint32_t tokens;
    uint64_t packets_passed;
    uint64_t packets_dropped;
} EosQosQueue;

#define EOS_QOS_MAX_QUEUES 16

typedef struct {
    EosQosQueue queues[EOS_QOS_MAX_QUEUES];
    int count;
    uint32_t total_bandwidth_kbps;
} EosQosManager;

int  eos_qos_init(EosQosManager *qm, uint32_t bandwidth_kbps);
int  eos_qos_add_queue(EosQosManager *qm, const char *name, EosQosType type,
                       uint32_t priority, uint32_t rate_kbps, uint32_t burst_kb);
int  eos_qos_enqueue(EosQosManager *qm, int queue_id, uint32_t pkt_size);
int  eos_qos_dequeue(EosQosManager *qm);
void eos_qos_dump(const EosQosManager *qm);

/* ---- Redundancy / Failover ---- */

typedef enum {
    EOS_FAILOVER_ACTIVE_STANDBY,
    EOS_FAILOVER_ACTIVE_ACTIVE,
    EOS_FAILOVER_N_PLUS_ONE
} EosFailoverMode;

typedef enum {
    EOS_NODE_ACTIVE,
    EOS_NODE_STANDBY,
    EOS_NODE_FAILED,
    EOS_NODE_SYNCING
} EosNodeState;

typedef struct {
    char name[32];
    char addr[64];
    EosNodeState state;
    uint32_t heartbeat_ms;
    uint32_t last_heartbeat;
    uint32_t failover_count;
    int is_primary;
} EosFailoverNode;

#define EOS_FAILOVER_MAX_NODES 8

typedef struct {
    EosFailoverMode mode;
    EosFailoverNode nodes[EOS_FAILOVER_MAX_NODES];
    int count;
    int primary_idx;
    uint32_t timeout_ms;
    int auto_failback;
} EosFailoverCluster;

int  eos_failover_init(EosFailoverCluster *fc, EosFailoverMode mode, uint32_t timeout_ms);
int  eos_failover_add_node(EosFailoverCluster *fc, const char *name,
                           const char *addr, int is_primary);
int  eos_failover_heartbeat(EosFailoverCluster *fc, int node_idx);
int  eos_failover_check(EosFailoverCluster *fc);
int  eos_failover_trigger(EosFailoverCluster *fc, int failed_idx);
int  eos_failover_failback(EosFailoverCluster *fc, int recovered_idx);
void eos_failover_dump(const EosFailoverCluster *fc);

#endif /* EOS_DATACENTER_H */
