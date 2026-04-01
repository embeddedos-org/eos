// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file motor_ctrl.h
 * @brief EoS Motor Control Framework — PID, trajectory, encoder feedback
 *
 * High-level motor control with PID loops, trajectory planning,
 * and encoder-based position feedback.
 */

#ifndef EOS_MOTOR_CTRL_H
#define EOS_MOTOR_CTRL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <eos/eos_config.h>

#if EOS_ENABLE_MOTOR_CTRL

#ifdef __cplusplus
extern "C" {
#endif

#define EOS_MOTOR_CTRL_MAX 8

/* ============================================================
 * PID Parameters
 * ============================================================ */

typedef struct {
    float kp;
    float ki;
    float kd;
    float integral_max;
    float output_min;
    float output_max;
} eos_pid_params_t;

/* ============================================================
 * Trajectory Segment
 * ============================================================ */

typedef struct {
    int32_t  target_position;
    int16_t  max_speed;         /* steps/sec or RPM */
    int16_t  acceleration;      /* steps/sec^2 */
    uint32_t duration_ms;
} eos_trajectory_t;

/* ============================================================
 * Motor Controller Config
 * ============================================================ */

typedef struct {
    uint8_t         motor_id;       /* maps to HAL eos_motor_config_t id */
    eos_pid_params_t pid_speed;
    eos_pid_params_t pid_position;
    uint16_t        encoder_cpr;    /* counts per revolution */
    uint32_t        control_rate_hz;
} eos_motor_ctrl_config_t;

/* ============================================================
 * Motor Controller Status
 * ============================================================ */

typedef struct {
    int32_t  current_position;
    int16_t  current_speed;
    int32_t  target_position;
    int16_t  target_speed;
    float    pid_output;
    bool     in_motion;
    bool     stalled;
} eos_motor_ctrl_status_t;

/* ============================================================
 * API
 * ============================================================ */

int  eos_motor_ctrl_init(void);
void eos_motor_ctrl_deinit(void);

int  eos_motor_ctrl_configure(const eos_motor_ctrl_config_t *cfg);
int  eos_motor_ctrl_remove(uint8_t motor_id);

/* Speed control (closed-loop) */
int  eos_motor_ctrl_set_speed(uint8_t motor_id, int16_t target_speed);

/* Position control (closed-loop) */
int  eos_motor_ctrl_set_position(uint8_t motor_id, int32_t target_position);
int  eos_motor_ctrl_move_relative(uint8_t motor_id, int32_t delta);

/* Trajectory */
int  eos_motor_ctrl_run_trajectory(uint8_t motor_id,
                                    const eos_trajectory_t *segments,
                                    size_t num_segments);

/* Control */
int  eos_motor_ctrl_stop(uint8_t motor_id);
int  eos_motor_ctrl_emergency_stop(uint8_t motor_id);
int  eos_motor_ctrl_reset_position(uint8_t motor_id);

/* PID tuning */
int  eos_motor_ctrl_set_pid_speed(uint8_t motor_id, const eos_pid_params_t *pid);
int  eos_motor_ctrl_set_pid_position(uint8_t motor_id, const eos_pid_params_t *pid);

/* Status */
int  eos_motor_ctrl_get_status(uint8_t motor_id, eos_motor_ctrl_status_t *status);

/* Periodic update — call from timer ISR or control loop */
void eos_motor_ctrl_update(void);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ENABLE_MOTOR_CTRL */
#endif /* EOS_MOTOR_CTRL_H */
