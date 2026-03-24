// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/motor_ctrl.h"
#include <string.h>

#if EOS_ENABLE_MOTOR_CTRL

typedef struct {
    eos_motor_ctrl_config_t cfg;
    eos_motor_ctrl_status_t status;
    uint8_t in_use;
    float pid_integral_s, pid_prev_err_s;
    float pid_integral_p, pid_prev_err_p;
} motor_t;

static motor_t g_m[EOS_MOTOR_CTRL_MAX];
static int g_init = 0;

static float pid_compute(const eos_pid_params_t *p, float error, float *integral, float *prev_err, float dt) {
    *integral += error * dt;
    if (*integral > p->integral_max) *integral = p->integral_max;
    if (*integral < -p->integral_max) *integral = -p->integral_max;
    float d = (dt > 0) ? (error - *prev_err) / dt : 0;
    *prev_err = error;
    float out = p->kp * error + p->ki * (*integral) + p->kd * d;
    if (out > p->output_max) out = p->output_max;
    if (out < p->output_min) out = p->output_min;
    return out;
}

int eos_motor_ctrl_init(void) { memset(g_m, 0, sizeof(g_m)); g_init = 1; return 0; }
void eos_motor_ctrl_deinit(void) { for (int i = 0; i < EOS_MOTOR_CTRL_MAX; i++) if (g_m[i].in_use) eos_motor_ctrl_stop(i); g_init = 0; }

int eos_motor_ctrl_configure(const eos_motor_ctrl_config_t *cfg) {
    if (!g_init || !cfg || cfg->motor_id >= EOS_MOTOR_CTRL_MAX) return -1;
    uint8_t id = cfg->motor_id;
    memset(&g_m[id], 0, sizeof(motor_t));
    g_m[id].cfg = *cfg; g_m[id].in_use = 1;
    return 0;
}

int eos_motor_ctrl_remove(uint8_t id) {
    if (!g_init || id >= EOS_MOTOR_CTRL_MAX || !g_m[id].in_use) return -1;
    g_m[id].in_use = 0; return 0;
}

int eos_motor_ctrl_set_speed(uint8_t id, int16_t speed) {
    if (!g_init || id >= EOS_MOTOR_CTRL_MAX || !g_m[id].in_use) return -1;
    g_m[id].status.target_speed = speed; g_m[id].status.in_motion = (speed != 0);
    return 0;
}

int eos_motor_ctrl_set_position(uint8_t id, int32_t pos) {
    if (!g_init || id >= EOS_MOTOR_CTRL_MAX || !g_m[id].in_use) return -1;
    g_m[id].status.target_position = pos; g_m[id].status.in_motion = true;
    return 0;
}

int eos_motor_ctrl_move_relative(uint8_t id, int32_t delta) {
    if (!g_init || id >= EOS_MOTOR_CTRL_MAX || !g_m[id].in_use) return -1;
    g_m[id].status.target_position += delta; g_m[id].status.in_motion = true;
    return 0;
}

int eos_motor_ctrl_run_trajectory(uint8_t id, const eos_trajectory_t *seg, size_t n) {
    if (!g_init || id >= EOS_MOTOR_CTRL_MAX || !g_m[id].in_use || !seg || n == 0) return -1;
    g_m[id].status.target_position = seg[n-1].target_position;
    g_m[id].status.target_speed = seg[0].max_speed;
    g_m[id].status.in_motion = true;
    return 0;
}

int eos_motor_ctrl_stop(uint8_t id) {
    if (!g_init || id >= EOS_MOTOR_CTRL_MAX || !g_m[id].in_use) return -1;
    g_m[id].status.target_speed = 0; g_m[id].status.in_motion = false;
    return 0;
}

int eos_motor_ctrl_emergency_stop(uint8_t id) {
    if (!g_init || id >= EOS_MOTOR_CTRL_MAX || !g_m[id].in_use) return -1;
    g_m[id].status.target_speed = 0; g_m[id].status.current_speed = 0;
    g_m[id].status.in_motion = false; g_m[id].status.pid_output = 0;
    g_m[id].pid_integral_s = 0; g_m[id].pid_integral_p = 0;
    return 0;
}

int eos_motor_ctrl_reset_position(uint8_t id) {
    if (!g_init || id >= EOS_MOTOR_CTRL_MAX || !g_m[id].in_use) return -1;
    g_m[id].status.current_position = 0; g_m[id].status.target_position = 0;
    return 0;
}

int eos_motor_ctrl_set_pid_speed(uint8_t id, const eos_pid_params_t *pid) {
    if (!g_init || id >= EOS_MOTOR_CTRL_MAX || !g_m[id].in_use || !pid) return -1;
    g_m[id].cfg.pid_speed = *pid; return 0;
}

int eos_motor_ctrl_set_pid_position(uint8_t id, const eos_pid_params_t *pid) {
    if (!g_init || id >= EOS_MOTOR_CTRL_MAX || !g_m[id].in_use || !pid) return -1;
    g_m[id].cfg.pid_position = *pid; return 0;
}

int eos_motor_ctrl_get_status(uint8_t id, eos_motor_ctrl_status_t *st) {
    if (!g_init || id >= EOS_MOTOR_CTRL_MAX || !g_m[id].in_use || !st) return -1;
    *st = g_m[id].status; return 0;
}

void eos_motor_ctrl_update(void) {
    float dt = 1.0f / 1000.0f;
    for (int i = 0; i < EOS_MOTOR_CTRL_MAX; i++) {
        if (!g_m[i].in_use || !g_m[i].status.in_motion) continue;
        motor_t *m = &g_m[i];
        float speed_err = (float)(m->status.target_speed - m->status.current_speed);
        float out = pid_compute(&m->cfg.pid_speed, speed_err, &m->pid_integral_s, &m->pid_prev_err_s, dt);
        m->status.pid_output = out;
        m->status.current_speed += (int16_t)(out * 0.1f);
        m->status.current_position += (int32_t)(m->status.current_speed * dt);
    }
}

#endif /* EOS_ENABLE_MOTOR_CTRL */