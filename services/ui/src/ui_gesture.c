// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file ui_gesture.c
 * @brief EoS UI — Gesture recognition engine
 *
 * State-machine that analyses sequences of touch-down / move / touch-up
 * events to detect:
 *   tap, double-tap, long-press, swipe (4 directions), pinch in/out
 *
 * Works through the HAL touch abstraction — agnostic to the underlying
 * touch controller IC (FT5x06, GT911, CST816, TSC2046, …).
 */

#include <eos/ui.h>

#if EOS_ENABLE_UI && EOS_ENABLE_TOUCH

#include <eos/hal_extended.h>
#include <lvgl.h>
#include <stdlib.h>
#include <string.h>

/* ---- configuration defaults ------------------------------------------ */

static eos_gesture_config_t s_gesture_cfg = {
    .swipe_min_distance = 50,
    .long_press_time_ms = 400,
    .double_tap_time_ms = 300,
    .pinch_ratio_pct    = 20,
};

/* ---- callback registry ----------------------------------------------- */

#define MAX_GESTURE_CBS 8

typedef struct {
    uint32_t               mask;
    eos_gesture_callback_t cb;
    void                  *ctx;
} gesture_cb_entry_t;

static gesture_cb_entry_t s_cbs[MAX_GESTURE_CBS];
static uint8_t            s_cb_count;

int eos_ui_on_gesture(uint32_t types, eos_gesture_callback_t cb, void *ctx)
{
    if (!cb || s_cb_count >= MAX_GESTURE_CBS) return -1;
    s_cbs[s_cb_count].mask = types;
    s_cbs[s_cb_count].cb   = cb;
    s_cbs[s_cb_count].ctx  = ctx;
    s_cb_count++;
    return 0;
}

int eos_ui_configure_gestures(const eos_gesture_config_t *cfg)
{
    if (!cfg) return -1;
    s_gesture_cfg = *cfg;
    return 0;
}

/* ---- state machine --------------------------------------------------- */

typedef enum {
    GS_IDLE,
    GS_TOUCH_DOWN,
    GS_MOVING,
    GS_WAIT_DOUBLE,
} gesture_state_t;

static struct {
    gesture_state_t state;
    int16_t         start_x;
    int16_t         start_y;
    int16_t         cur_x;
    int16_t         cur_y;
    uint32_t        down_tick;
    uint32_t        last_tap_tick;
    bool            was_pressed;

    /* pinch tracking (two-finger) */
    int32_t         initial_dist2;
} s_gs;

static void emit(eos_gesture_type_t type)
{
    eos_gesture_event_t ev = {
        .type        = type,
        .start_x     = s_gs.start_x,
        .start_y     = s_gs.start_y,
        .end_x       = s_gs.cur_x,
        .end_y       = s_gs.cur_y,
        .duration_ms = lv_tick_get() - s_gs.down_tick,
    };
    for (uint8_t i = 0; i < s_cb_count; i++) {
        if (s_cbs[i].mask & (uint32_t)type)
            s_cbs[i].cb(&ev, s_cbs[i].ctx);
    }
}

static int32_t dist2(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    int32_t dx = x2 - x1;
    int32_t dy = y2 - y1;
    return dx * dx + dy * dy;
}

/* Called from ui_touch_drv.c every time touch data arrives. */
void eos_ui_gesture_feed(const eos_touch_point_t *pts, uint8_t count)
{
    if (count == 0) return;

    bool pressed = pts[0].pressed;
    int16_t x = (int16_t)pts[0].x;
    int16_t y = (int16_t)pts[0].y;
    uint32_t now = lv_tick_get();

    /* ---- two-finger pinch detection ---------------------------------- */
    if (count >= 2 && pressed && pts[1].pressed) {
        int32_t d = dist2(x, y, (int16_t)pts[1].x, (int16_t)pts[1].y);
        if (s_gs.initial_dist2 == 0) {
            s_gs.initial_dist2 = d;
        } else {
            int32_t threshold = s_gs.initial_dist2 *
                                (int32_t)s_gesture_cfg.pinch_ratio_pct / 100;
            if (d > s_gs.initial_dist2 + threshold) {
                emit(EOS_GESTURE_PINCH_OUT);
                s_gs.initial_dist2 = d;
            } else if (d < s_gs.initial_dist2 - threshold) {
                emit(EOS_GESTURE_PINCH_IN);
                s_gs.initial_dist2 = d;
            }
        }
        return;
    }
    s_gs.initial_dist2 = 0;

    /* ---- single-finger state machine --------------------------------- */
    switch (s_gs.state) {
        case GS_IDLE:
            if (pressed) {
                s_gs.state   = GS_TOUCH_DOWN;
                s_gs.start_x = x;
                s_gs.start_y = y;
                s_gs.cur_x   = x;
                s_gs.cur_y   = y;
                s_gs.down_tick = now;
            }
            break;

        case GS_TOUCH_DOWN:
            s_gs.cur_x = x;
            s_gs.cur_y = y;

            if (!pressed) {
                /* Finger lifted — could be tap or double-tap */
                if (s_gs.last_tap_tick != 0 &&
                    now - s_gs.last_tap_tick < s_gesture_cfg.double_tap_time_ms) {
                    emit(EOS_GESTURE_DOUBLE_TAP);
                    s_gs.state = GS_IDLE;
                    s_gs.last_tap_tick = 0;
                } else {
                    emit(EOS_GESTURE_TAP);
                    s_gs.last_tap_tick = now;
                    s_gs.state = GS_IDLE;
                }
            } else {
                int32_t dx = x - s_gs.start_x;
                int32_t dy = y - s_gs.start_y;
                int32_t d2 = dx * dx + dy * dy;
                int32_t threshold = (int32_t)s_gesture_cfg.swipe_min_distance *
                                    s_gesture_cfg.swipe_min_distance;
                if (d2 > threshold / 4)
                    s_gs.state = GS_MOVING;

                if (now - s_gs.down_tick >= s_gesture_cfg.long_press_time_ms) {
                    emit(EOS_GESTURE_LONG_PRESS);
                    s_gs.state = GS_IDLE;
                }
            }
            break;

        case GS_MOVING:
            s_gs.cur_x = x;
            s_gs.cur_y = y;

            if (!pressed) {
                int32_t dx = s_gs.cur_x - s_gs.start_x;
                int32_t dy = s_gs.cur_y - s_gs.start_y;
                int32_t adx = dx < 0 ? -dx : dx;
                int32_t ady = dy < 0 ? -dy : dy;
                int32_t min_d = (int32_t)s_gesture_cfg.swipe_min_distance;

                if (adx > ady && adx >= min_d) {
                    emit(dx > 0 ? EOS_GESTURE_SWIPE_RIGHT
                                : EOS_GESTURE_SWIPE_LEFT);
                } else if (ady >= adx && ady >= min_d) {
                    emit(dy > 0 ? EOS_GESTURE_SWIPE_DOWN
                                : EOS_GESTURE_SWIPE_UP);
                }
                s_gs.state = GS_IDLE;
            }
            break;

        case GS_WAIT_DOUBLE:
            /* Not actively used — handled inline in TOUCH_DOWN release */
            s_gs.state = GS_IDLE;
            break;
    }

    s_gs.was_pressed = pressed;
}

#endif /* EOS_ENABLE_UI && EOS_ENABLE_TOUCH */
