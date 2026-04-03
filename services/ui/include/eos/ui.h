// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file ui.h
 * @brief EoS UI Service — LVGL Integration Layer
 *
 * Bridges LVGL to the EoS HAL display and touch drivers, providing:
 *   - One-call init/deinit for display and input device drivers
 *   - Tick source and task-handler wrapper
 *   - Gesture recognition (tap, swipe, long-press, pinch)
 *   - Theme switching (dark / light)
 *
 * Users link LVGL separately; this layer provides the driver bridge.
 */

#ifndef EOS_UI_H
#define EOS_UI_H

#include <eos/eos_config.h>

#if EOS_ENABLE_UI

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward-declare LVGL opaque types so this header compiles without lvgl.h */
typedef struct _lv_display_t lv_display_t;
typedef struct _lv_indev_t   lv_indev_t;

/* ============================================================
 * Configuration
 * ============================================================ */

typedef enum {
    EOS_UI_FB_FULL    = 0,  /**< Full-screen framebuffer (one or two) */
    EOS_UI_FB_PARTIAL = 1,  /**< Partial (band) framebuffer            */
    EOS_UI_FB_DIRECT  = 2,  /**< Direct mode — no intermediate buffer  */
} eos_ui_fb_mode_t;

typedef enum {
    EOS_UI_THEME_LIGHT = 0,
    EOS_UI_THEME_DARK  = 1,
} eos_ui_theme_t;

typedef enum {
    EOS_UI_ROTATE_0   = 0,
    EOS_UI_ROTATE_90  = 1,
    EOS_UI_ROTATE_180 = 2,
    EOS_UI_ROTATE_270 = 3,
} eos_ui_rotation_t;

typedef struct {
    uint16_t           screen_width;
    uint16_t           screen_height;
    uint8_t            color_depth;       /**< 1, 16, or 24 */
    uint8_t            display_id;        /**< HAL display id   */
    uint8_t            touch_id;          /**< HAL touch id     */
    eos_ui_fb_mode_t   fb_mode;
    eos_ui_rotation_t  rotation;
    uint16_t           dpi;               /**< Dots-per-inch (0 = LVGL default) */
    bool               double_buffer;     /**< Use two framebuffers? */
} eos_ui_config_t;

/* ============================================================
 * Lifecycle
 * ============================================================ */

/**
 * Initialize the LVGL subsystem and create display / input drivers.
 *
 * @param cfg  UI configuration (screen size, color depth, etc.)
 * @return 0 on success, negative error code on failure.
 */
int  eos_ui_init(const eos_ui_config_t *cfg);

/**
 * Tear down the UI layer, free framebuffers, and call lv_deinit().
 */
void eos_ui_deinit(void);

/**
 * Run one iteration of the LVGL timer/task handler.
 * Call this periodically from the main loop or an RTOS task.
 *
 * @return Recommended sleep time in ms until the next call.
 */
uint32_t eos_ui_task_handler(void);

/* ============================================================
 * Accessors
 * ============================================================ */

lv_display_t *eos_ui_get_display(void);
lv_indev_t   *eos_ui_get_indev(void);

/* ============================================================
 * Theme
 * ============================================================ */

int eos_ui_set_theme(eos_ui_theme_t theme);

/* ============================================================
 * Gesture Recognition
 * ============================================================ */

typedef enum {
    EOS_GESTURE_NONE        = 0,
    EOS_GESTURE_TAP         = (1 << 0),
    EOS_GESTURE_DOUBLE_TAP  = (1 << 1),
    EOS_GESTURE_LONG_PRESS  = (1 << 2),
    EOS_GESTURE_SWIPE_LEFT  = (1 << 3),
    EOS_GESTURE_SWIPE_RIGHT = (1 << 4),
    EOS_GESTURE_SWIPE_UP    = (1 << 5),
    EOS_GESTURE_SWIPE_DOWN  = (1 << 6),
    EOS_GESTURE_PINCH_IN    = (1 << 7),
    EOS_GESTURE_PINCH_OUT   = (1 << 8),
} eos_gesture_type_t;

typedef struct {
    eos_gesture_type_t type;
    int16_t            start_x;
    int16_t            start_y;
    int16_t            end_x;
    int16_t            end_y;
    uint32_t           duration_ms;
} eos_gesture_event_t;

typedef void (*eos_gesture_callback_t)(const eos_gesture_event_t *event,
                                       void *ctx);

/**
 * Register a callback for one or more gesture types (OR-ed mask).
 *
 * @param types  Bitmask of eos_gesture_type_t values.
 * @param cb     Callback invoked when a matching gesture is detected.
 * @param ctx    User context forwarded to cb.
 * @return 0 on success, negative on failure.
 */
int eos_ui_on_gesture(uint32_t types, eos_gesture_callback_t cb, void *ctx);

/* ============================================================
 * Gesture Configuration
 * ============================================================ */

typedef struct {
    uint16_t swipe_min_distance;    /**< px — minimum drag to count as swipe      */
    uint32_t long_press_time_ms;    /**< ms — hold time for long-press             */
    uint32_t double_tap_time_ms;    /**< ms — max interval between two taps        */
    uint16_t pinch_ratio_pct;       /**< %  — distance change to trigger pinch     */
} eos_gesture_config_t;

int eos_ui_configure_gestures(const eos_gesture_config_t *cfg);

/* ---- Internal helpers (called from ui_display_drv / ui_touch_drv) ---- */
void eos_ui_display_flush_cb(lv_display_t *disp, const void *area,
                             uint8_t *px_map);
void eos_ui_touch_read_cb(lv_indev_t *indev, void *data);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ENABLE_UI */
#endif /* EOS_UI_H */
