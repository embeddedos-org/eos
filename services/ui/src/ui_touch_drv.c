// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file ui_touch_drv.c
 * @brief EoS UI — LVGL input-device driver bridge (touch)
 *
 * Implements the LVGL indev read callback by polling the EoS HAL touch
 * API (eos_touch_read).  The first touch point is routed to LVGL; all
 * points are forwarded to the gesture recogniser for multi-touch analysis.
 *
 * Touch coordinate rotation is handled here so the gesture layer and
 * LVGL always receive screen-space coordinates.
 */

#include <eos/ui.h>

#if EOS_ENABLE_UI && EOS_ENABLE_TOUCH

#include <eos/hal_extended.h>
#include <lvgl.h>

/* ---- module state ----------------------------------------------------- */

static uint8_t          s_touch_id;
static eos_ui_rotation_t s_rotation;
static uint16_t         s_width;
static uint16_t         s_height;

void eos_ui_touch_set_params(uint8_t id, eos_ui_rotation_t rot,
                             uint16_t w, uint16_t h)
{
    s_touch_id = id;
    s_rotation = rot;
    s_width    = w;
    s_height   = h;
}

/* ---- coordinate rotation ---------------------------------------------- */

static void rotate_point(uint16_t *x, uint16_t *y)
{
    uint16_t tmp;
    switch (s_rotation) {
        case EOS_UI_ROTATE_90:
            tmp = *x;
            *x  = *y;
            *y  = (s_width - 1) - tmp;
            break;
        case EOS_UI_ROTATE_180:
            *x = (s_width  - 1) - *x;
            *y = (s_height - 1) - *y;
            break;
        case EOS_UI_ROTATE_270:
            tmp = *x;
            *x  = (s_height - 1) - *y;
            *y  = tmp;
            break;
        default:
            break;
    }
}

/* ---- LVGL read callback ----------------------------------------------- */

/* Forward declaration — implemented in ui_gesture.c */
void eos_ui_gesture_feed(const eos_touch_point_t *points, uint8_t count);

void eos_ui_touch_read_cb(lv_indev_t *indev, void *data)
{
    lv_indev_data_t *d = (lv_indev_data_t *)data;
    eos_touch_point_t points[5];
    uint8_t count = 0;

    int rc = eos_touch_read(s_touch_id, points, 5, &count);
    if (rc != 0 || count == 0) {
        d->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    /* Rotate first point into screen coordinates */
    uint16_t px = points[0].x;
    uint16_t py = points[0].y;
    rotate_point(&px, &py);

    d->point.x = (int16_t)px;
    d->point.y = (int16_t)py;
    d->state   = points[0].pressed ? LV_INDEV_STATE_PRESSED
                                   : LV_INDEV_STATE_RELEASED;

    /* Feed all points to gesture recogniser */
    eos_ui_gesture_feed(points, count);
}

#endif /* EOS_ENABLE_UI && EOS_ENABLE_TOUCH */
