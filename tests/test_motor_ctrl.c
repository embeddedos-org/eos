// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define EOS_ENABLE_MOTOR_CTRL 1
#include "eos/eos_config.h"
#include "eos/motor_ctrl.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

static void test_motor_init(void) {
    assert(eos_motor_ctrl_init() == 0);
    eos_motor_ctrl_deinit();
    PASS("motor init/deinit");
}
static void test_motor_configure(void) {
    eos_motor_ctrl_init();
    eos_motor_ctrl_config_t cfg = {0};
    cfg.motor_id = 0;
    cfg.pid_speed.kp = 1.0f;
    cfg.pid_speed.ki = 0.1f;
    cfg.pid_speed.kd = 0.01f;
    cfg.pid_speed.output_min = -100.0f;
    cfg.pid_speed.output_max = 100.0f;
    cfg.pid_speed.integral_max = 50.0f;
    cfg.encoder_cpr = 4096;
    cfg.control_rate_hz = 1000;
    assert(eos_motor_ctrl_configure(&cfg) == 0);
    eos_motor_ctrl_deinit();
    PASS("motor configure");
}
static void test_motor_configure_null(void) {
    eos_motor_ctrl_init();
    assert(eos_motor_ctrl_configure(NULL) != 0);
    eos_motor_ctrl_deinit();
    PASS("motor configure null");
}
static void test_motor_set_speed(void) {
    eos_motor_ctrl_init();
    eos_motor_ctrl_config_t cfg = {0};
    cfg.motor_id = 0;
    cfg.pid_speed.kp = 1.0f;
    cfg.pid_speed.output_min = -100.0f;
    cfg.pid_speed.output_max = 100.0f;
    cfg.encoder_cpr = 1024;
    cfg.control_rate_hz = 100;
    eos_motor_ctrl_configure(&cfg);
    assert(eos_motor_ctrl_set_speed(0, 500) == 0);
    eos_motor_ctrl_deinit();
    PASS("motor set speed");
}
static void test_motor_set_position(void) {
    eos_motor_ctrl_init();
    eos_motor_ctrl_config_t cfg = {0};
    cfg.motor_id = 0;
    cfg.pid_position.kp = 2.0f;
    cfg.pid_position.output_min = -1000.0f;
    cfg.pid_position.output_max = 1000.0f;
    cfg.encoder_cpr = 1024;
    cfg.control_rate_hz = 100;
    eos_motor_ctrl_configure(&cfg);
    assert(eos_motor_ctrl_set_position(0, 10000) == 0);
    eos_motor_ctrl_deinit();
    PASS("motor set position");
}
static void test_motor_move_relative(void) {
    eos_motor_ctrl_init();
    eos_motor_ctrl_config_t cfg = {0};
    cfg.motor_id = 0;
    cfg.pid_position.kp = 1.0f;
    cfg.pid_position.output_min = -500.0f;
    cfg.pid_position.output_max = 500.0f;
    cfg.encoder_cpr = 1024;
    cfg.control_rate_hz = 100;
    eos_motor_ctrl_configure(&cfg);
    assert(eos_motor_ctrl_move_relative(0, 500) == 0);
    eos_motor_ctrl_deinit();
    PASS("motor move relative");
}
static void test_motor_stop(void) {
    eos_motor_ctrl_init();
    eos_motor_ctrl_config_t cfg = {0};
    cfg.motor_id = 0;
    cfg.pid_speed.kp = 1.0f;
    cfg.pid_speed.output_min = -100.0f;
    cfg.pid_speed.output_max = 100.0f;
    cfg.encoder_cpr = 1024;
    cfg.control_rate_hz = 100;
    eos_motor_ctrl_configure(&cfg);
    eos_motor_ctrl_set_speed(0, 100);
    assert(eos_motor_ctrl_stop(0) == 0);
    eos_motor_ctrl_deinit();
    PASS("motor stop");
}
static void test_motor_emergency_stop(void) {
    eos_motor_ctrl_init();
    eos_motor_ctrl_config_t cfg = {0};
    cfg.motor_id = 0;
    cfg.pid_speed.kp = 1.0f;
    cfg.pid_speed.output_min = -100.0f;
    cfg.pid_speed.output_max = 100.0f;
    cfg.encoder_cpr = 1024;
    cfg.control_rate_hz = 100;
    eos_motor_ctrl_configure(&cfg);
    eos_motor_ctrl_set_speed(0, 200);
    assert(eos_motor_ctrl_emergency_stop(0) == 0);
    eos_motor_ctrl_deinit();
    PASS("motor emergency stop");
}
static void test_motor_reset_position(void) {
    eos_motor_ctrl_init();
    eos_motor_ctrl_config_t cfg = {0};
    cfg.motor_id = 0;
    cfg.pid_speed.kp = 1.0f;
    cfg.pid_speed.output_min = -100.0f;
    cfg.pid_speed.output_max = 100.0f;
    cfg.encoder_cpr = 1024;
    cfg.control_rate_hz = 100;
    eos_motor_ctrl_configure(&cfg);
    assert(eos_motor_ctrl_reset_position(0) == 0);
    eos_motor_ctrl_deinit();
    PASS("motor reset position");
}
static void test_motor_get_status(void) {
    eos_motor_ctrl_init();
    eos_motor_ctrl_config_t cfg = {0};
    cfg.motor_id = 0;
    cfg.pid_speed.kp = 1.0f;
    cfg.pid_speed.output_min = -100.0f;
    cfg.pid_speed.output_max = 100.0f;
    cfg.encoder_cpr = 1024;
    cfg.control_rate_hz = 100;
    eos_motor_ctrl_configure(&cfg);
    eos_motor_ctrl_status_t status;
    assert(eos_motor_ctrl_get_status(0, &status) == 0);
    eos_motor_ctrl_deinit();
    PASS("motor get status");
}
static void test_motor_get_status_null(void) {
    eos_motor_ctrl_init();
    eos_motor_ctrl_config_t cfg = {0};
    cfg.motor_id = 0;
    cfg.pid_speed.kp = 1.0f;
    cfg.pid_speed.output_min = -100.0f;
    cfg.pid_speed.output_max = 100.0f;
    cfg.encoder_cpr = 1024;
    cfg.control_rate_hz = 100;
    eos_motor_ctrl_configure(&cfg);
    assert(eos_motor_ctrl_get_status(0, NULL) != 0);
    eos_motor_ctrl_deinit();
    PASS("motor get status null");
}
static void test_motor_remove(void) {
    eos_motor_ctrl_init();
    eos_motor_ctrl_config_t cfg = {0};
    cfg.motor_id = 0;
    cfg.pid_speed.kp = 1.0f;
    cfg.pid_speed.output_min = -100.0f;
    cfg.pid_speed.output_max = 100.0f;
    cfg.encoder_cpr = 1024;
    cfg.control_rate_hz = 100;
    eos_motor_ctrl_configure(&cfg);
    assert(eos_motor_ctrl_remove(0) == 0);
    eos_motor_ctrl_deinit();
    PASS("motor remove");
}
static void test_motor_update(void) {
    eos_motor_ctrl_init();
    eos_motor_ctrl_config_t cfg = {0};
    cfg.motor_id = 0;
    cfg.pid_speed.kp = 1.0f;
    cfg.pid_speed.output_min = -100.0f;
    cfg.pid_speed.output_max = 100.0f;
    cfg.encoder_cpr = 1024;
    cfg.control_rate_hz = 100;
    eos_motor_ctrl_configure(&cfg);
    eos_motor_ctrl_set_speed(0, 100);
    eos_motor_ctrl_update();
    eos_motor_ctrl_deinit();
    PASS("motor update loop");
}
static void test_motor_set_pid(void) {
    eos_motor_ctrl_init();
    eos_motor_ctrl_config_t cfg = {0};
    cfg.motor_id = 0;
    cfg.pid_speed.kp = 1.0f;
    cfg.pid_speed.output_min = -100.0f;
    cfg.pid_speed.output_max = 100.0f;
    cfg.encoder_cpr = 1024;
    cfg.control_rate_hz = 100;
    eos_motor_ctrl_configure(&cfg);
    eos_pid_params_t pid = { .kp = 3.0f, .ki = 0.5f, .kd = 0.1f, .integral_max = 100.0f, .output_min = -200.0f, .output_max = 200.0f };
    assert(eos_motor_ctrl_set_pid_speed(0, &pid) == 0);
    assert(eos_motor_ctrl_set_pid_position(0, &pid) == 0);
    eos_motor_ctrl_deinit();
    PASS("motor set PID");
}
static void test_motor_trajectory(void) {
    eos_motor_ctrl_init();
    eos_motor_ctrl_config_t cfg = {0};
    cfg.motor_id = 0;
    cfg.pid_position.kp = 1.0f;
    cfg.pid_position.output_min = -1000.0f;
    cfg.pid_position.output_max = 1000.0f;
    cfg.encoder_cpr = 1024;
    cfg.control_rate_hz = 100;
    eos_motor_ctrl_configure(&cfg);
    eos_trajectory_t traj[] = {
        { .target_position = 1000, .max_speed = 100, .acceleration = 50, .duration_ms = 2000 },
        { .target_position = 0,    .max_speed = 100, .acceleration = 50, .duration_ms = 2000 },
    };
    assert(eos_motor_ctrl_run_trajectory(0, traj, 2) == 0);
    eos_motor_ctrl_deinit();
    PASS("motor trajectory");
}
int main(void) {
    printf("=== EoS Motor Control Tests ===\n");
    test_motor_init();
    test_motor_configure();
    test_motor_configure_null();
    test_motor_set_speed();
    test_motor_set_position();
    test_motor_move_relative();
    test_motor_stop();
    test_motor_emergency_stop();
    test_motor_reset_position();
    test_motor_get_status();
    test_motor_get_status_null();
    test_motor_remove();
    test_motor_update();
    test_motor_set_pid();
    test_motor_trajectory();
    printf("\n=== ALL %d MOTOR TESTS PASSED ===\n", passed);
    return 0;
}
