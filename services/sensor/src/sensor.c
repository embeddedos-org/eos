// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/sensor.h"
#include <string.h>

#if EOS_ENABLE_SENSOR

#define FILTER_WIN 8

typedef struct {
    eos_sensor_config_t cfg;
    eos_sensor_calib_t  calib;
    uint8_t  in_use;
    float    filter_buf[FILTER_WIN];
    int      filter_idx;
    int      filter_count;
    float    last_filtered;
    uint32_t read_count;
} sensor_ctx_t;

static sensor_ctx_t g_s[EOS_SENSOR_MAX];
static int g_count = 0;
static int g_init = 0;

int eos_sensor_init(void) { memset(g_s, 0, sizeof(g_s)); g_count = 0; g_init = 1; return 0; }
void eos_sensor_deinit(void) { g_init = 0; g_count = 0; }

int eos_sensor_register(const eos_sensor_config_t *cfg) {
    if (!g_init || !cfg || cfg->id >= EOS_SENSOR_MAX || g_s[cfg->id].in_use) return -1;
    memset(&g_s[cfg->id], 0, sizeof(sensor_ctx_t));
    g_s[cfg->id].cfg = *cfg;
    g_s[cfg->id].calib.scale = 1.0f;
    g_s[cfg->id].in_use = 1;
    g_count++;
    return 0;
}

int eos_sensor_unregister(uint8_t id) {
    if (!g_init || id >= EOS_SENSOR_MAX || !g_s[id].in_use) return -1;
    g_s[id].in_use = 0; g_count--;
    return 0;
}

int eos_sensor_read(uint8_t id, eos_sensor_reading_t *r) {
    if (!g_init || id >= EOS_SENSOR_MAX || !g_s[id].in_use || !r) return -1;
    float raw = 0;
    if (g_s[id].cfg.read_fn) g_s[id].cfg.read_fn(id, &raw);
    else raw = (float)g_s[id].read_count;
    r->value = (raw + g_s[id].calib.offset) * g_s[id].calib.scale;
    r->timestamp_ms = g_s[id].read_count++;
    r->valid = true;
    return 0;
}

static float do_avg(sensor_ctx_t *s, float v) {
    s->filter_buf[s->filter_idx] = v;
    s->filter_idx = (s->filter_idx + 1) % FILTER_WIN;
    if (s->filter_count < FILTER_WIN) s->filter_count++;
    float sum = 0; for (int i = 0; i < s->filter_count; i++) sum += s->filter_buf[i];
    return sum / (float)s->filter_count;
}

static float do_median(sensor_ctx_t *s, float v) {
    s->filter_buf[s->filter_idx] = v;
    s->filter_idx = (s->filter_idx + 1) % FILTER_WIN;
    if (s->filter_count < FILTER_WIN) s->filter_count++;
    float sorted[FILTER_WIN]; memcpy(sorted, s->filter_buf, sizeof(float) * (size_t)s->filter_count);
    for (int i = 0; i < s->filter_count - 1; i++)
        for (int j = i + 1; j < s->filter_count; j++)
            if (sorted[j] < sorted[i]) { float t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t; }
    return sorted[s->filter_count / 2];
}

static float do_lowpass(sensor_ctx_t *s, float v) {
    if (s->filter_count == 0) { s->last_filtered = v; s->filter_count = 1; return v; }
    float alpha = 0.2f;
    s->last_filtered = alpha * v + (1.0f - alpha) * s->last_filtered;
    return s->last_filtered;
}

int eos_sensor_read_filtered(uint8_t id, eos_sensor_reading_t *r) {
    int ret = eos_sensor_read(id, r);
    if (ret != 0) return ret;
    sensor_ctx_t *s = &g_s[id];
    switch (s->cfg.filter) {
    case EOS_FILTER_AVERAGE: r->value = do_avg(s, r->value); break;
    case EOS_FILTER_MEDIAN:  r->value = do_median(s, r->value); break;
    case EOS_FILTER_LOWPASS: r->value = do_lowpass(s, r->value); break;
    default: break;
    }
    return 0;
}

int eos_sensor_calibrate(uint8_t id, const eos_sensor_calib_t *c) {
    if (!g_init || id >= EOS_SENSOR_MAX || !g_s[id].in_use || !c) return -1;
    g_s[id].calib = *c; return 0;
}

int eos_sensor_get_calibration(uint8_t id, eos_sensor_calib_t *c) {
    if (!g_init || id >= EOS_SENSOR_MAX || !g_s[id].in_use || !c) return -1;
    *c = g_s[id].calib; return 0;
}

int eos_sensor_auto_calibrate(uint8_t id) {
    if (!g_init || id >= EOS_SENSOR_MAX || !g_s[id].in_use) return -1;
    float sum = 0;
    for (int i = 0; i < 16; i++) { eos_sensor_reading_t r; eos_sensor_read(id, &r); sum += r.value; }
    g_s[id].calib.offset = -(sum / 16.0f);
    g_s[id].calib.calibrated = true;
    return 0;
}

int eos_sensor_set_filter(uint8_t id, eos_filter_type_t f, uint8_t window_size) {
    if (!g_init || id >= EOS_SENSOR_MAX || !g_s[id].in_use) return -1;
    g_s[id].cfg.filter = f;
    g_s[id].cfg.filter_window = window_size;
    g_s[id].filter_idx = 0; g_s[id].filter_count = 0;
    (void)window_size;
    return 0;
}

int eos_sensor_get_count(void) { return g_count; }
int eos_sensor_get_info(uint8_t id, eos_sensor_config_t *cfg) {
    if (!g_init || id >= EOS_SENSOR_MAX || !g_s[id].in_use || !cfg) return -1;
    *cfg = g_s[id].cfg; return 0;
}

#endif /* EOS_ENABLE_SENSOR */