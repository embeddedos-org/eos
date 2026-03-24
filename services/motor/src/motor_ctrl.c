/**
 * @file motor_ctrl.c
 * @brief EoS Motor Control Framework — Implementation with PID
 */

#include <eos/motor_ctrl.h>
#include <string.h>

#if EOS_ENABLE_MOTOR_CTRL

typedef struct {
    eos_motor_ctrl_config_t cfg;
    bool                    configured;
    int32_t                 current_position;
    int16_t                 current_speed;
    int32_t                 target_position;
    int16_t                 target_speed;
    float                   pid_integral_speed;
    float                   pid_prev_error_speed;
    float                   pid_integral_pos;
    float                   pid_prev_error_pos;
    float                   pid_output;
    bool                    in_motion;
} motor_ctrl_slot_t;

static motor_ctrl_slot_t motors[EOS_MOTOR_CTRL_MAX];
static bool ctrl_initialized = false;

int eos_motor_ctrl_init(void)
{
    memset(motors, 0, sizeof(motors));
    ctrl_initialized = true;
    return 0;
}

void eos_motor_ctrl_deinit(void)
{
    ctrl_initialized = false;
}

int eos_motor_ctrl_configure(const eos_motor_ctrl_config_t *cfg)
{
    if (!ctrl_initialized || !cfg) return -1;
    if (cfg->motor_id >= EOS_MOTOR_CTRL_MAX) return -1;

    motor_ctrl_slot_t *m = &motors[cfg->motor_id];
    memcpy(&m->cfg, cfg, sizeof(*cfg));
    m->current_position = 0;
    m->current_speed = 0;
    m->target_position = 0;
    m->target_speed = 0;
    m->pid_integral_speed = 0.0f;
    m->pid_prev_error_speed = 0.0f;
    m->pid_integral_pos = 0.0f;
    m->pid_prev_error_pos = 0.0f;
    m->pid_output = 0.0f;
    m->in_motion = false;
    m->configured = true;
    return 0;
}

int eos_motor_ctrl_remove(uint8_t motor_id)
{
    if (motor_id >= EOS_MOTOR_CTRL_MAX) return -1;
    motors[motor_id].configured = false;
    return 0;
}

int eos_motor_ctrl_set_speed(uint8_t motor_id, int16_t target_speed)
{
    if (!ctrl_initialized || motor_id >= EOS_MOTOR_CTRL_MAX) return -1;
    if (!motors[motor_id].configured) return -1;
    motors[motor_id].target_speed = target_speed;
    motors[motor_id].in_motion = (target_speed != 0);
    return 0;
}

int eos_motor_ctrl_set_position(uint8_t motor_id, int32_t target_position)
{
    if (!ctrl_initialized || motor_id >= EOS_MOTOR_CTRL_MAX) return -1;
    if (!motors[motor_id].configured) return -1;
    motors[motor_id].target_position = target_position;
    motors[motor_id].in_motion = true;
    return 0;
}

int eos_motor_ctrl_move_relative(uint8_t motor_id, int32_t delta)
{
    if (!ctrl_initialized || motor_id >= EOS_MOTOR_CTRL_MAX) return -1;
    if (!motors[motor_id].configured) return -1;
    motors[motor_id].target_position =
        motors[motor_id].current_position + delta;
    motors[motor_id].in_motion = true;
    return 0;
}

int eos_motor_ctrl_run_trajectory(uint8_t motor_id,
                                   const eos_trajectory_t *segments,
                                   size_t num_segments)
{
    if (!ctrl_initialized || motor_id >= EOS_MOTOR_CTRL_MAX) return -1;
    if (!motors[motor_id].configured || !segments || num_segments == 0) return -1;

    /* Apply the final segment target for now */
    motors[motor_id].target_position =
        segments[num_segments - 1].target_position;
    motors[motor_id].in_motion = true;
    return 0;
}

int eos_motor_ctrl_stop(uint8_t motor_id)
{
    if (motor_id >= EOS_MOTOR_CTRL_MAX) return -1;
    motors[motor_id].target_speed = 0;
    motors[motor_id].in_motion = false;
    return 0;
}

int eos_motor_ctrl_emergency_stop(uint8_t motor_id)
{
    if (motor_id >= EOS_MOTOR_CTRL_MAX) return -1;
    motors[motor_id].target_speed = 0;
    motors[motor_id].current_speed = 0;
    motors[motor_id].in_motion = false;
    motors[motor_id].pid_integral_speed = 0.0f;
    motors[motor_id].pid_integral_pos = 0.0f;
    return 0;
}

int eos_motor_ctrl_reset_position(uint8_t motor_id)
{
    if (motor_id >= EOS_MOTOR_CTRL_MAX) return -1;
    motors[motor_id].current_position = 0;
    motors[motor_id].target_position = 0;
    return 0;
}

int eos_motor_ctrl_set_pid_speed(uint8_t motor_id, const eos_pid_params_t *pid)
{
    if (motor_id >= EOS_MOTOR_CTRL_MAX || !pid) return -1;
    if (!motors[motor_id].configured) return -1;
    memcpy(&motors[motor_id].cfg.pid_speed, pid, sizeof(*pid));
    return 0;
}

int eos_motor_ctrl_set_pid_position(uint8_t motor_id,
                                     const eos_pid_params_t *pid)
{
    if (motor_id >= EOS_MOTOR_CTRL_MAX || !pid) return -1;
    if (!motors[motor_id].configured) return -1;
    memcpy(&motors[motor_id].cfg.pid_position, pid, sizeof(*pid));
    return 0;
}

int eos_motor_ctrl_get_status(uint8_t motor_id,
                               eos_motor_ctrl_status_t *status)
{
    if (motor_id >= EOS_MOTOR_CTRL_MAX || !status) return -1;
    if (!motors[motor_id].configured) return -1;

    motor_ctrl_slot_t *m = &motors[motor_id];
    status->current_position = m->current_position;
    status->current_speed = m->current_speed;
    status->target_position = m->target_position;
    status->target_speed = m->target_speed;
    status->pid_output = m->pid_output;
    status->in_motion = m->in_motion;
    status->stalled = false;
    return 0;
}

static float pid_compute(const eos_pid_params_t *pid, float error,
                          float *integral, float *prev_error)
{
    *integral += error;
    if (pid->integral_max > 0.0f) {
        if (*integral > pid->integral_max) *integral = pid->integral_max;
        if (*integral < -pid->integral_max) *integral = -pid->integral_max;
    }

    float derivative = error - *prev_error;
    *prev_error = error;

    float output = pid->kp * error + pid->ki * (*integral) + pid->kd * derivative;

    if (output > pid->output_max) output = pid->output_max;
    if (output < pid->output_min) output = pid->output_min;

    return output;
}

void eos_motor_ctrl_update(void)
{
    if (!ctrl_initialized) return;

    for (int i = 0; i < EOS_MOTOR_CTRL_MAX; i++) {
        motor_ctrl_slot_t *m = &motors[i];
        if (!m->configured || !m->in_motion) continue;

        float speed_error = (float)(m->target_speed - m->current_speed);
        m->pid_output = pid_compute(&m->cfg.pid_speed, speed_error,
                                     &m->pid_integral_speed,
                                     &m->pid_prev_error_speed);
    }
}

#endif /* EOS_ENABLE_MOTOR_CTRL */
