/**
 * @file sensor.c
 * @brief EoS Sensor Framework — Implementation
 */

#include <eos/sensor.h>
#include <string.h>

#if EOS_ENABLE_SENSOR

typedef struct {
    eos_sensor_config_t cfg;
    eos_sensor_calib_t  calib;
    bool                registered;
    float               filter_buf[16];
    uint8_t             filter_idx;
    uint8_t             filter_count;
} sensor_slot_t;

static sensor_slot_t sensors[EOS_SENSOR_MAX];
static bool sensor_initialized = false;

int eos_sensor_init(void)
{
    memset(sensors, 0, sizeof(sensors));
    sensor_initialized = true;
    return 0;
}

void eos_sensor_deinit(void)
{
    sensor_initialized = false;
}

int eos_sensor_register(const eos_sensor_config_t *cfg)
{
    if (!sensor_initialized || !cfg) return -1;
    if (cfg->id >= EOS_SENSOR_MAX) return -1;
    if (sensors[cfg->id].registered) return -1;

    sensor_slot_t *s = &sensors[cfg->id];
    memcpy(&s->cfg, cfg, sizeof(*cfg));
    s->calib.offset = 0.0f;
    s->calib.scale = 1.0f;
    s->calib.calibrated = false;
    s->filter_idx = 0;
    s->filter_count = 0;
    s->registered = true;
    return 0;
}

int eos_sensor_unregister(uint8_t id)
{
    if (id >= EOS_SENSOR_MAX) return -1;
    sensors[id].registered = false;
    return 0;
}

int eos_sensor_read(uint8_t id, eos_sensor_reading_t *reading)
{
    if (!sensor_initialized || id >= EOS_SENSOR_MAX) return -1;
    sensor_slot_t *s = &sensors[id];
    if (!s->registered || !s->cfg.read_fn) return -1;

    float raw;
    int rc = s->cfg.read_fn(id, &raw);
    if (rc != 0) return rc;

    float calibrated = (raw + s->calib.offset) * s->calib.scale;

    if (reading) {
        reading->value = calibrated;
        reading->min = calibrated;
        reading->max = calibrated;
        reading->timestamp_ms = 0; /* platform should fill */
        reading->valid = true;
    }
    return 0;
}

static float filter_average(sensor_slot_t *s)
{
    if (s->filter_count == 0) return 0.0f;
    float sum = 0.0f;
    uint8_t cnt = s->filter_count;
    if (cnt > s->cfg.filter_window) cnt = s->cfg.filter_window;
    for (uint8_t i = 0; i < cnt; i++) {
        sum += s->filter_buf[i];
    }
    return sum / (float)cnt;
}

int eos_sensor_read_filtered(uint8_t id, eos_sensor_reading_t *reading)
{
    if (!sensor_initialized || id >= EOS_SENSOR_MAX) return -1;
    sensor_slot_t *s = &sensors[id];
    if (!s->registered || !s->cfg.read_fn) return -1;

    float raw;
    int rc = s->cfg.read_fn(id, &raw);
    if (rc != 0) return rc;

    float calibrated = (raw + s->calib.offset) * s->calib.scale;

    uint8_t win = s->cfg.filter_window;
    if (win == 0 || win > 16) win = 1;

    s->filter_buf[s->filter_idx] = calibrated;
    s->filter_idx = (s->filter_idx + 1) % win;
    if (s->filter_count < win) s->filter_count++;

    float filtered = calibrated;
    if (s->cfg.filter == EOS_FILTER_AVERAGE || s->cfg.filter == EOS_FILTER_LOWPASS) {
        filtered = filter_average(s);
    }

    if (reading) {
        reading->value = filtered;
        reading->min = filtered;
        reading->max = filtered;
        reading->timestamp_ms = 0;
        reading->valid = true;
    }
    return 0;
}

int eos_sensor_calibrate(uint8_t id, const eos_sensor_calib_t *calib)
{
    if (id >= EOS_SENSOR_MAX || !calib) return -1;
    if (!sensors[id].registered) return -1;
    memcpy(&sensors[id].calib, calib, sizeof(*calib));
    sensors[id].calib.calibrated = true;
    return 0;
}

int eos_sensor_get_calibration(uint8_t id, eos_sensor_calib_t *calib)
{
    if (id >= EOS_SENSOR_MAX || !calib) return -1;
    if (!sensors[id].registered) return -1;
    memcpy(calib, &sensors[id].calib, sizeof(*calib));
    return 0;
}

int eos_sensor_auto_calibrate(uint8_t id)
{
    if (id >= EOS_SENSOR_MAX) return -1;
    if (!sensors[id].registered) return -1;
    sensors[id].calib.offset = 0.0f;
    sensors[id].calib.scale = 1.0f;
    sensors[id].calib.calibrated = true;
    return 0;
}

int eos_sensor_set_filter(uint8_t id, eos_filter_type_t filter,
                           uint8_t window_size)
{
    if (id >= EOS_SENSOR_MAX) return -1;
    if (!sensors[id].registered) return -1;
    sensors[id].cfg.filter = filter;
    sensors[id].cfg.filter_window = (window_size > 16) ? 16 : window_size;
    sensors[id].filter_idx = 0;
    sensors[id].filter_count = 0;
    return 0;
}

int eos_sensor_get_count(void)
{
    int count = 0;
    for (int i = 0; i < EOS_SENSOR_MAX; i++) {
        if (sensors[i].registered) count++;
    }
    return count;
}

int eos_sensor_get_info(uint8_t id, eos_sensor_config_t *cfg)
{
    if (id >= EOS_SENSOR_MAX || !cfg) return -1;
    if (!sensors[id].registered) return -1;
    memcpy(cfg, &sensors[id].cfg, sizeof(*cfg));
    return 0;
}

#endif /* EOS_ENABLE_SENSOR */
