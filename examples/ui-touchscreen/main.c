// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file main.c
 * @brief EoS Example: UI Touchscreen — Interactive LVGL demo
 *
 * Sets up a 480×320 RGB565 display with capacitive touch, initialises
 * the EoS UI layer, and builds a small interactive interface:
 *   - Status label showing gesture events and touch coordinates
 *   - A counter button that increments on each press
 *   - A brightness slider wired to eos_display_set_brightness()
 *   - A theme-toggle switch (light ↔ dark)
 *   - Gesture callbacks for swipe / long-press / pinch
 *
 * Demonstrates: HAL display + touch, eos_ui lifecycle, LVGL widgets,
 * gesture recognition, theme switching, and RTOS task integration.
 */

#include <eos/hal.h>
#include <eos/hal_extended.h>
#include <eos/kernel.h>
#include <eos/ui.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

/* ---- hardware constants ---------------------------------------------- */

#define DISPLAY_ID     0
#define TOUCH_ID       0
#define SCREEN_W       480
#define SCREEN_H       320
#define LED_PIN        2       /* activity LED */

/* ---- UI handles (file-scope so callbacks can reach them) ------------- */

static lv_obj_t *g_status_label;
static lv_obj_t *g_counter_label;
static uint32_t  g_counter;

/* ================================================================== */
/*  Gesture callback                                                  */
/* ================================================================== */

static void on_gesture(const eos_gesture_event_t *ev, void *ctx)
{
    (void)ctx;
    const char *name = "?";

    switch (ev->type) {
        case EOS_GESTURE_TAP:         name = "Tap";         break;
        case EOS_GESTURE_DOUBLE_TAP:  name = "Double-Tap";  break;
        case EOS_GESTURE_LONG_PRESS:  name = "Long-Press";  break;
        case EOS_GESTURE_SWIPE_LEFT:  name = "Swipe Left";  break;
        case EOS_GESTURE_SWIPE_RIGHT: name = "Swipe Right"; break;
        case EOS_GESTURE_SWIPE_UP:    name = "Swipe Up";    break;
        case EOS_GESTURE_SWIPE_DOWN:  name = "Swipe Down";  break;
        case EOS_GESTURE_PINCH_IN:    name = "Pinch In";    break;
        case EOS_GESTURE_PINCH_OUT:   name = "Pinch Out";   break;
        default: break;
    }

    printf("[ui-demo] Gesture: %s  (%d,%d)->(%d,%d) %lu ms\n",
           name, ev->start_x, ev->start_y, ev->end_x, ev->end_y,
           (unsigned long)ev->duration_ms);

    if (g_status_label) {
        lv_label_set_text_fmt(g_status_label,
            LV_SYMBOL_GPS " %s  (%d, %d)  %lu ms",
            name, ev->end_x, ev->end_y,
            (unsigned long)ev->duration_ms);
    }
}

/* ================================================================== */
/*  Widget event handlers                                             */
/* ================================================================== */

static void btn_counter_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    g_counter++;
    lv_label_set_text_fmt(g_counter_label, "Count: %lu",
                          (unsigned long)g_counter);
    eos_gpio_toggle(LED_PIN);
    printf("[ui-demo] Button pressed, count=%lu\n",
           (unsigned long)g_counter);
}

static void slider_brightness_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    eos_display_set_brightness(DISPLAY_ID, (uint8_t)val);
    printf("[ui-demo] Brightness: %ld%%\n", (long)val);
}

static void sw_theme_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    lv_obj_t *sw = lv_event_get_target(e);
    bool dark = lv_obj_has_state(sw, LV_STATE_CHECKED);
    eos_ui_set_theme(dark ? EOS_UI_THEME_DARK : EOS_UI_THEME_LIGHT);
    printf("[ui-demo] Theme: %s\n", dark ? "dark" : "light");
}

/* ================================================================== */
/*  Screen builder                                                    */
/* ================================================================== */

static void build_ui(void)
{
    lv_obj_t *scr = lv_screen_active();

    /* ---- title --------------------------------------------------- */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, LV_SYMBOL_HOME " EoS UI Demo");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    /* ---- status label (updated by gestures) ---------------------- */
    g_status_label = lv_label_create(scr);
    lv_label_set_text(g_status_label,
                      LV_SYMBOL_GPS " Waiting for gesture...");
    lv_obj_align(g_status_label, LV_ALIGN_TOP_MID, 0, 40);

    /* ---- counter button ------------------------------------------ */
    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 160, 50);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, 30, -20);
    lv_obj_add_event_cb(btn, btn_counter_cb, LV_EVENT_CLICKED, NULL);

    g_counter_label = lv_label_create(btn);
    lv_label_set_text(g_counter_label, "Count: 0");
    lv_obj_center(g_counter_label);

    /* ---- brightness slider --------------------------------------- */
    lv_obj_t *bright_lbl = lv_label_create(scr);
    lv_label_set_text(bright_lbl, LV_SYMBOL_IMAGE " Brightness");
    lv_obj_align(bright_lbl, LV_ALIGN_RIGHT_MID, -30, -50);

    lv_obj_t *slider = lv_slider_create(scr);
    lv_obj_set_width(slider, 160);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 80, LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_RIGHT_MID, -30, -20);
    lv_obj_add_event_cb(slider, slider_brightness_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);

    /* ---- theme switch -------------------------------------------- */
    lv_obj_t *theme_lbl = lv_label_create(scr);
    lv_label_set_text(theme_lbl, "Dark mode");
    lv_obj_align(theme_lbl, LV_ALIGN_BOTTOM_MID, -40, -20);

    lv_obj_t *sw = lv_switch_create(scr);
    lv_obj_align(sw, LV_ALIGN_BOTTOM_MID, 40, -20);
    lv_obj_add_event_cb(sw, sw_theme_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* ---- footer -------------------------------------------------- */
    lv_obj_t *footer = lv_label_create(scr);
    lv_label_set_text(footer, "Swipe / pinch / long-press anywhere");
    lv_obj_set_style_text_color(footer, lv_color_hex(0x888888), 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -4);
}

/* ================================================================== */
/*  UI task (runs on RTOS)                                            */
/* ================================================================== */

static void ui_task(void *arg)
{
    (void)arg;
    printf("[ui-demo] UI task running\n");

    for (;;) {
        uint32_t sleep_ms = eos_ui_task_handler();
        eos_task_delay_ms(sleep_ms);
    }
}

/* ================================================================== */
/*  main                                                              */
/* ================================================================== */

int main(void)
{
    printf("[ui-demo] Starting...\n");

    /* ---- platform init ------------------------------------------- */
    eos_hal_init();
    eos_kernel_init();

    /* Activity LED */
    eos_gpio_config_t led = {
        .pin  = LED_PIN,
        .mode = EOS_GPIO_OUTPUT,
        .pull = EOS_GPIO_PULL_NONE,
    };
    eos_gpio_init(&led);

    /* ---- HAL: display -------------------------------------------- */
    eos_display_config_t disp_cfg = {
        .id         = DISPLAY_ID,
        .width      = SCREEN_W,
        .height     = SCREEN_H,
        .color_mode = EOS_DISPLAY_COLOR_RGB565,
    };
    if (eos_display_init(&disp_cfg) != 0) {
        printf("[ui-demo] display init failed\n");
    }

    /* ---- HAL: touch ---------------------------------------------- */
    eos_touch_config_t touch_cfg = {
        .id         = TOUCH_ID,
        .type       = EOS_TOUCH_CAPACITIVE,
        .width      = SCREEN_W,
        .height     = SCREEN_H,
        .max_points = 5,
        .i2c_port   = 0,
        .i2c_addr   = 0x38,       /* FT5x06 / FT6236 default */
    };
    if (eos_touch_init(&touch_cfg) != 0) {
        printf("[ui-demo] touch init failed\n");
    }

    /* ---- EoS UI: bring up LVGL ----------------------------------- */
    eos_ui_config_t ui_cfg = {
        .screen_width  = SCREEN_W,
        .screen_height = SCREEN_H,
        .color_depth   = 16,           /* RGB565 */
        .display_id    = DISPLAY_ID,
        .touch_id      = TOUCH_ID,
        .fb_mode       = EOS_UI_FB_FULL,
        .rotation      = EOS_UI_ROTATE_0,
        .dpi           = 130,
        .double_buffer = true,
    };

    int rc = eos_ui_init(&ui_cfg);
    if (rc != 0) {
        printf("[ui-demo] eos_ui_init failed: %d\n", rc);
        return -1;
    }

    /* ---- theme --------------------------------------------------- */
    eos_ui_set_theme(EOS_UI_THEME_LIGHT);

    /* ---- gestures ------------------------------------------------ */
    uint32_t gesture_mask = EOS_GESTURE_TAP
                          | EOS_GESTURE_DOUBLE_TAP
                          | EOS_GESTURE_LONG_PRESS
                          | EOS_GESTURE_SWIPE_LEFT
                          | EOS_GESTURE_SWIPE_RIGHT
                          | EOS_GESTURE_SWIPE_UP
                          | EOS_GESTURE_SWIPE_DOWN
                          | EOS_GESTURE_PINCH_IN
                          | EOS_GESTURE_PINCH_OUT;

    eos_ui_on_gesture(gesture_mask, on_gesture, NULL);

    eos_gesture_config_t gcfg = {
        .swipe_min_distance = 50,
        .long_press_time_ms = 500,
        .double_tap_time_ms = 300,
        .pinch_ratio_pct    = 20,
    };
    eos_ui_configure_gestures(&gcfg);

    /* ---- build widgets ------------------------------------------- */
    build_ui();
    printf("[ui-demo] UI built, launching task\n");

    /* ---- start RTOS ---------------------------------------------- */
    eos_task_create("ui", ui_task, NULL, 5, 4096);
    eos_kernel_start();

    return 0;
}
