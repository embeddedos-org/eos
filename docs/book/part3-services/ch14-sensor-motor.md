# Chapter 14: Sensor & Motor Frameworks

**Author:** Srikanth Patchava & EmbeddedOS Contributors

---

## 14.1 Overview

EoS provides two complementary frameworks for physical-world interaction:
the **Sensor Framework** (`eos/sensor.h`) for uniform sensor data
acquisition and filtering, and the **Motor Control Framework**
(`eos/motor_ctrl.h`) for closed-loop motor control with PID regulation
and trajectory planning.

```
┌──────────────────────────────────────────────────────────┐
│                    Application                           │
├──────────────────────────┬───────────────────────────────┤
│     Sensor Framework     │    Motor Control Framework    │
│  register / read / filter│  PID / trajectory / encoder   │
│    <eos/sensor.h>        │    <eos/motor_ctrl.h>         │
├──────────────────────────┴───────────────────────────────┤
│                     HAL Drivers                          │
│     ADC / I2C / SPI      │     PWM / Encoder / GPIO     │
└──────────────────────────┴───────────────────────────────┘
```

---

# Part I: Sensor Framework

## 14.2 Sensor Types

EoS supports 14 built-in sensor types via `eos_sensor_type_t`:

| Enum                       | ID | Description            |
|----------------------------|----|------------------------|
| `EOS_SENSOR_TEMPERATURE`   | 0  | Temperature (°C)       |
| `EOS_SENSOR_HUMIDITY`      | 1  | Relative humidity (%)  |
| `EOS_SENSOR_PRESSURE`      | 2  | Barometric pressure    |
| `EOS_SENSOR_LIGHT`         | 3  | Ambient light (lux)    |
| `EOS_SENSOR_PROXIMITY`     | 4  | Distance / proximity   |
| `EOS_SENSOR_ACCELEROMETER` | 5  | Acceleration (m/s²)    |
| `EOS_SENSOR_GYROSCOPE`     | 6  | Angular velocity       |
| `EOS_SENSOR_MAGNETOMETER`  | 7  | Magnetic field         |
| `EOS_SENSOR_HEART_RATE`    | 8  | Heart rate (BPM)       |
| `EOS_SENSOR_DUST`          | 9  | Dust / particulate     |
| `EOS_SENSOR_GAS`           | 10 | Gas concentration      |
| `EOS_SENSOR_VOLTAGE`       | 11 | Voltage (V)            |
| `EOS_SENSOR_CURRENT`       | 12 | Current (A)            |
| `EOS_SENSOR_CUSTOM`        | 13 | User-defined sensor    |

Maximum registered sensors: `EOS_SENSOR_MAX = 16`.

## 14.3 Filter Types

The framework provides five built-in digital filters:

| Enum                | ID | Algorithm                       |
|---------------------|----|---------------------------------|
| `EOS_FILTER_NONE`   | 0  | No filtering — raw value        |
| `EOS_FILTER_AVERAGE`| 1  | Moving average                  |
| `EOS_FILTER_MEDIAN` | 2  | Median filter (spike rejection) |
| `EOS_FILTER_LOWPASS`| 3  | IIR low-pass filter             |
| `EOS_FILTER_KALMAN` | 4  | 1-D Kalman filter               |

## 14.4 Sensor Registration

Each sensor is registered with a configuration struct and a read callback:

```c
typedef int (*eos_sensor_read_fn)(uint8_t id, float *raw_value);

typedef struct {
    uint8_t            id;                 /* Unique sensor ID    */
    const char        *name;               /* Human-readable name */
    eos_sensor_type_t  type;               /* Sensor category     */
    eos_filter_type_t  filter;             /* Default filter      */
    uint8_t            filter_window;      /* Window size (1–32)  */
    uint32_t           sample_interval_ms; /* Polling period      */
    eos_sensor_read_fn read_fn;            /* Hardware read func  */
} eos_sensor_config_t;
```

### 14.4.1 Registration Example

```c
#include <eos/sensor.h>

/* Hardware-specific read function */
int read_bme280_temp(uint8_t id, float *value)
{
    *value = bme280_get_temperature();   /* HAL driver call */
    return 0;
}

int read_bme280_hum(uint8_t id, float *value)
{
    *value = bme280_get_humidity();
    return 0;
}

void sensors_setup(void)
{
    eos_sensor_init();

    eos_sensor_config_t temp_cfg = {
        .id                = 0,
        .name              = "BME280-Temp",
        .type              = EOS_SENSOR_TEMPERATURE,
        .filter            = EOS_FILTER_KALMAN,
        .filter_window     = 8,
        .sample_interval_ms = 1000,
        .read_fn           = read_bme280_temp,
    };
    eos_sensor_register(&temp_cfg);

    eos_sensor_config_t hum_cfg = {
        .id                = 1,
        .name              = "BME280-Hum",
        .type              = EOS_SENSOR_HUMIDITY,
        .filter            = EOS_FILTER_AVERAGE,
        .filter_window     = 4,
        .sample_interval_ms = 2000,
        .read_fn           = read_bme280_hum,
    };
    eos_sensor_register(&hum_cfg);
}
```

## 14.5 Reading Sensor Data

```c
typedef struct {
    float    value;          /* Current reading             */
    float    min;            /* Minimum observed value      */
    float    max;            /* Maximum observed value       */
    uint32_t timestamp_ms;  /* Timestamp of this reading    */
    bool     valid;          /* true if data is usable      */
} eos_sensor_reading_t;
```

### 14.5.1 Raw vs Filtered Reads

```c
eos_sensor_reading_t reading;

/* Raw value — direct from hardware */
eos_sensor_read(0, &reading);
printf("Raw temp: %.2f°C\n", reading.value);

/* Filtered value — processed through configured filter */
eos_sensor_read_filtered(0, &reading);
printf("Filtered temp: %.2f°C (min=%.2f max=%.2f)\n",
       reading.value, reading.min, reading.max);
```

## 14.6 Calibration

```c
typedef struct {
    float offset;       /* value = raw * scale + offset */
    float scale;
    bool  calibrated;
} eos_sensor_calib_t;
```

### 14.6.1 Manual Calibration

```c
/* Apply known offset and scale */
eos_sensor_calib_t cal = {
    .offset = -0.5f,    /* Subtract 0.5°C bias */
    .scale  = 1.02f,    /* Scale correction    */
};
eos_sensor_calibrate(0, &cal);

/* Read back current calibration */
eos_sensor_calib_t current;
eos_sensor_get_calibration(0, &current);
printf("Offset=%.2f Scale=%.2f Calibrated=%d\n",
       current.offset, current.scale, current.calibrated);
```

### 14.6.2 Auto-Calibration

```c
/* Let the framework compute calibration from baseline readings */
eos_sensor_auto_calibrate(0);
```

## 14.7 Runtime Filter Changes

```c
/* Switch sensor 0 to median filter with window size 5 */
eos_sensor_set_filter(0, EOS_FILTER_MEDIAN, 5);
```

---

# Part II: Motor Control Framework

## 14.8 Motor Control Architecture

```
  Target ──▶ ┌─────────┐    ┌─────────┐    ┌─────────┐
  (setpoint) │   PID   │───▶│   PWM   │───▶│  Motor  │
             │ Control │    │  Driver  │    │         │
             └────┬────┘    └─────────┘    └────┬────┘
                  │                              │
                  │◄── ┌──────────┐ ◄────────────┘
                       │ Encoder  │   (feedback)
                       │ Reading  │
                       └──────────┘
```

## 14.9 PID Parameters

```c
typedef struct {
    float kp;              /* Proportional gain      */
    float ki;              /* Integral gain           */
    float kd;              /* Derivative gain         */
    float integral_max;    /* Anti-windup limit       */
    float output_min;      /* Minimum output clamp    */
    float output_max;      /* Maximum output clamp    */
} eos_pid_params_t;
```

## 14.10 Motor Controller Configuration

```c
typedef struct {
    uint8_t          motor_id;        /* HAL motor ID          */
    eos_pid_params_t pid_speed;       /* PID for speed loop    */
    eos_pid_params_t pid_position;    /* PID for position loop */
    uint16_t         encoder_cpr;     /* Counts per revolution */
    uint32_t         control_rate_hz; /* PID update frequency  */
} eos_motor_ctrl_config_t;
```

### 14.10.1 Configuration Example

```c
#include <eos/motor_ctrl.h>

void motor_setup(void)
{
    eos_motor_ctrl_init();

    eos_motor_ctrl_config_t cfg = {
        .motor_id       = 0,
        .pid_speed      = { .kp=1.0, .ki=0.1, .kd=0.01,
                            .integral_max=100, .output_min=-100,
                            .output_max=100 },
        .pid_position   = { .kp=2.0, .ki=0.05, .kd=0.1,
                            .integral_max=500, .output_min=-200,
                            .output_max=200 },
        .encoder_cpr    = 1024,    /* Quadrature encoder */
        .control_rate_hz = 1000,   /* 1 kHz PID loop     */
    };
    eos_motor_ctrl_configure(&cfg);
}
```

## 14.11 Speed Control

```c
/* Set target speed: positive = forward, negative = reverse */
eos_motor_ctrl_set_speed(0, 500);    /* 500 RPM forward  */
eos_motor_ctrl_set_speed(0, -300);   /* 300 RPM reverse  */
eos_motor_ctrl_set_speed(0, 0);      /* Stop              */
```

## 14.12 Position Control

```c
/* Move to absolute position (encoder counts) */
eos_motor_ctrl_set_position(0, 10240);  /* 10 revolutions */

/* Move relative to current position */
eos_motor_ctrl_move_relative(0, 1024);  /* +1 revolution  */
eos_motor_ctrl_move_relative(0, -512);  /* -0.5 revolutions */
```

## 14.13 Trajectory Planning

Multi-segment trajectories enable smooth motion profiles:

```c
typedef struct {
    int32_t  target_position;   /* End position (counts)  */
    int16_t  max_speed;         /* Maximum speed (RPM)    */
    int16_t  acceleration;      /* Accel (steps/sec²)     */
    uint32_t duration_ms;       /* Segment duration       */
} eos_trajectory_t;
```

### 14.13.1 Trajectory Example

```c
eos_trajectory_t path[] = {
    { .target_position = 2048,  .max_speed = 200,
      .acceleration = 100,      .duration_ms = 2000 },
    { .target_position = 4096,  .max_speed = 500,
      .acceleration = 200,      .duration_ms = 1500 },
    { .target_position = 0,     .max_speed = 300,
      .acceleration = 150,      .duration_ms = 3000 },
};

eos_motor_ctrl_run_trajectory(0, path, 3);
```

```
 Speed
  ▲
  │      ╱‾‾‾‾╲
  │     ╱      ╲         ╱‾‾╲
  │    ╱        ╲       ╱    ╲
  │   ╱          ╲     ╱      ╲
  │──╱            ╲───╱        ╲──────▶ Time
  └──────────────────────────────────
     Seg 1       Seg 2      Seg 3
```

## 14.14 Motor Status

```c
typedef struct {
    int32_t  current_position;  /* Encoder position      */
    int16_t  current_speed;     /* Measured speed (RPM)  */
    int32_t  target_position;   /* Commanded position    */
    int16_t  target_speed;      /* Commanded speed       */
    float    pid_output;        /* Current PID output    */
    bool     in_motion;         /* Motor is moving       */
    bool     stalled;           /* Stall detected        */
} eos_motor_ctrl_status_t;
```

```c
eos_motor_ctrl_status_t st;
eos_motor_ctrl_get_status(0, &st);

printf("Pos: %d / %d  Speed: %d / %d  PID: %.2f\n",
       st.current_position, st.target_position,
       st.current_speed, st.target_speed,
       st.pid_output);

if (st.stalled) {
    printf("WARNING: Motor stall detected!\n");
    eos_motor_ctrl_emergency_stop(0);
}
```

## 14.15 Emergency Stop and Safety

```c
/* Graceful stop — decelerates to zero */
eos_motor_ctrl_stop(0);

/* Emergency stop — immediate halt, disables PWM */
eos_motor_ctrl_emergency_stop(0);

/* Reset encoder position to zero (homing) */
eos_motor_ctrl_reset_position(0);
```

## 14.16 PID Tuning at Runtime

```c
/* Adjust speed PID gains without re-initialization */
eos_pid_params_t new_pid = {
    .kp = 1.5, .ki = 0.2, .kd = 0.02,
    .integral_max = 150,
    .output_min = -100, .output_max = 100,
};
eos_motor_ctrl_set_pid_speed(0, &new_pid);

/* Adjust position PID separately */
eos_motor_ctrl_set_pid_position(0, &new_pid);
```

## 14.17 Control Loop Integration

The `eos_motor_ctrl_update()` function must be called periodically at the
configured `control_rate_hz`. Typical integration points:

```c
/* Option 1: Timer ISR */
void timer_isr(void)
{
    eos_motor_ctrl_update();
}

/* Option 2: RTOS task */
void motor_task(void *arg)
{
    while (1) {
        eos_motor_ctrl_update();
        eos_task_delay_ms(1);   /* 1 kHz */
    }
}
```

## 14.18 Combined Sensor + Motor Example

```c
void temperature_fan_controller(void)
{
    eos_sensor_init();
    eos_motor_ctrl_init();

    /* Register temperature sensor */
    eos_sensor_config_t tcfg = {
        .id = 0, .name = "Heatsink",
        .type = EOS_SENSOR_TEMPERATURE,
        .filter = EOS_FILTER_LOWPASS,
        .filter_window = 4,
        .sample_interval_ms = 500,
        .read_fn = read_ntc_temp,
    };
    eos_sensor_register(&tcfg);

    /* Configure fan motor */
    eos_motor_ctrl_config_t mcfg = {
        .motor_id = 0,
        .pid_speed = { .kp=0.5, .ki=0.1, .kd=0.0,
                       .integral_max=100,
                       .output_min=0, .output_max=100 },
        .encoder_cpr = 2,
        .control_rate_hz = 100,
    };
    eos_motor_ctrl_configure(&mcfg);

    /* Control loop */
    while (1) {
        eos_sensor_reading_t temp;
        eos_sensor_read_filtered(0, &temp);

        int16_t fan_speed = 0;
        if (temp.value > 50.0f) fan_speed = (int16_t)(temp.value * 10);
        if (fan_speed > 1000)   fan_speed = 1000;

        eos_motor_ctrl_set_speed(0, fan_speed);
        eos_task_delay_ms(500);
    }
}
```

## 14.19 API Reference Summary

### Sensor API

| Function                      | Description                      |
|-------------------------------|----------------------------------|
| `eos_sensor_init`             | Initialize sensor framework      |
| `eos_sensor_deinit`           | Shutdown sensor framework        |
| `eos_sensor_register`         | Register a sensor                |
| `eos_sensor_unregister`       | Unregister a sensor              |
| `eos_sensor_read`             | Read raw sensor value            |
| `eos_sensor_read_filtered`    | Read filtered sensor value       |
| `eos_sensor_calibrate`        | Apply calibration                |
| `eos_sensor_get_calibration`  | Read current calibration         |
| `eos_sensor_auto_calibrate`   | Auto-calibrate sensor            |
| `eos_sensor_set_filter`       | Change filter type/window        |
| `eos_sensor_get_count`        | Get registered sensor count      |
| `eos_sensor_get_info`         | Get sensor configuration         |

### Motor Control API

| Function                          | Description                    |
|-----------------------------------|--------------------------------|
| `eos_motor_ctrl_init`             | Initialize motor framework     |
| `eos_motor_ctrl_deinit`           | Shutdown motor framework       |
| `eos_motor_ctrl_configure`        | Configure a motor controller   |
| `eos_motor_ctrl_remove`           | Remove motor configuration     |
| `eos_motor_ctrl_set_speed`        | Set target speed (closed-loop) |
| `eos_motor_ctrl_set_position`     | Set target position            |
| `eos_motor_ctrl_move_relative`    | Move by delta                  |
| `eos_motor_ctrl_run_trajectory`   | Execute trajectory segments    |
| `eos_motor_ctrl_stop`             | Graceful stop                  |
| `eos_motor_ctrl_emergency_stop`   | Immediate halt                 |
| `eos_motor_ctrl_reset_position`   | Zero encoder position          |
| `eos_motor_ctrl_set_pid_speed`    | Tune speed PID                 |
| `eos_motor_ctrl_set_pid_position` | Tune position PID              |
| `eos_motor_ctrl_get_status`       | Get motor status               |
| `eos_motor_ctrl_update`           | Run one PID iteration          |

---

*Next: [Chapter 15 — Compatibility Layers](ch15-compat.md)*
