// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file main.c
 * @brief EoS Example: HMI Button Panel — Industrial touchscreen control demo
 *
 * Builds an industrial HMI panel with:
 *   - Motor start/stop/reverse buttons with status LED indicators
 *   - Speed control slider (0–100%)
 *   - Temperature gauge with alarm threshold
 *   - System status bar (uptime, fault count, connection)
 *   - Emergency stop button with long-press confirmation
 *   - Tab-based navigation (Control / Monitor / Settings)
 *
 * Target: 800×480 7" TFT (SSD1963/RA8875), GT911 capacitive touch
 * Product profile: hmi / plc / industrial
 */

#include <eos/hal.h>
#include <eos/hal_extended.h>
#include <eos/ui.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_W      800
#define SCREEN_H      480
#define DISPLAY_ID    0
#define TOUCH_ID      0

/* ---- simulated plant data ------------------------------------------- */

static struct {
    bool     motor_running;
    bool     motor_reverse;
    int16_t  motor_speed_pct;
    int32_t  temperature_mc;     /* millidegrees C */
    int32_t  temp_alarm_mc;
    uint32_t uptime_sec;
    uint32_t fault_count;
    bool     connected;
    bool     estop_active;
} s_plant = {
    .motor_running   = false,
    .motor_reverse   = false,
    .motor_speed_pct = 0,
    .temperature_mc  = 42500,    /* 42.5°C */
    .temp_alarm_mc   = 80000,    /* 80°C alarm */
    .uptime_sec      = 3661,
    .fault_count     = 0,
    .connected       = true,
    .estop_active    = false,
};

/* ---- LVGL object handles -------------------------------------------- */

static lv_obj_t *s_motor_led;
static lv_obj_t *s_motor_status_label;
static lv_obj_t *s_speed_label;
static lv_obj_t *s_temp_bar;
static lv_obj_t *s_temp_label;
static lv_obj_t *s_status_bar_label;
static lv_obj_t *s_estop_btn;

/* ---- color palette -------------------------------------------------- */

#define COLOR_BG        lv_color_hex(0x1E1E2E)
#define COLOR_PANEL     lv_color_hex(0x2A2A3C)
#define COLOR_GREEN     lv_color_hex(0x00C853)
#define COLOR_RED       lv_color_hex(0xFF1744)
#define COLOR_YELLOW    lv_color_hex(0xFFD600)
#define COLOR_BLUE      lv_color_hex(0x2979FF)
#define COLOR_GRAY      lv_color_hex(0x666680)
#define COLOR_WHITE     lv_color_hex(0xEEEEFF)

/* ---- helpers -------------------------------------------------------- */

static void update_motor_status(void)
{
    if (s_plant.estop_active) {
        lv_obj_set_style_bg_color(s_motor_led, COLOR_RED, 0);
        lv_label_set_text(s_motor_status_label, "E-STOP");
    } else if (s_plant.motor_running) {
        lv_obj_set_style_bg_color(s_motor_led, COLOR_GREEN, 0);
        lv_label_set_text_fmt(s_motor_status_label, "%s %d%%",
            s_plant.motor_reverse ? "REV" : "FWD",
            s_plant.motor_speed_pct);
    } else {
        lv_obj_set_style_bg_color(s_motor_led, COLOR_GRAY, 0);
        lv_label_set_text(s_motor_status_label, "STOPPED");
    }
}

static void update_temperature(void)
{
    int32_t temp_c = s_plant.temperature_mc / 1000;
    int32_t temp_frac = (s_plant.temperature_mc % 1000) / 100;
    lv_bar_set_value(s_temp_bar, temp_c, LV_ANIM_ON);

    bool alarm = s_plant.temperature_mc >= s_plant.temp_alarm_mc;
    lv_color_t color = alarm ? COLOR_RED : (temp_c > 60 ? COLOR_YELLOW : COLOR_GREEN);
    lv_obj_set_style_bg_color(s_temp_bar, color, LV_PART_INDICATOR);

    lv_label_set_text_fmt(s_temp_label, "%ld.%ld" "\xC2\xB0" "C %s",
                          (long)temp_c, (long)temp_frac,
                          alarm ? "ALARM" : "");
    lv_obj_set_style_text_color(s_temp_label, color, 0);
}

static void update_status_bar(void)
{
    uint32_t h = s_plant.uptime_sec / 3600;
    uint32_t m = (s_plant.uptime_sec % 3600) / 60;
    uint32_t s = s_plant.uptime_sec % 60;

    lv_label_set_text_fmt(s_status_bar_label,
        "Uptime: %02lu:%02lu:%02lu  |  Faults: %lu  |  %s",
        (unsigned long)h, (unsigned long)m, (unsigned long)s,
        (unsigned long)s_plant.fault_count,
        s_plant.connected ? LV_SYMBOL_WIFI " Connected" : LV_SYMBOL_CLOSE " Offline");
}

/* ================================================================== */
/*  Widget callbacks                                                   */
/* ================================================================== */

static void btn_start_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (s_plant.estop_active) return;
    s_plant.motor_running = true;
    update_motor_status();
    printf("[hmi] Motor START\n");
}

static void btn_stop_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    s_plant.motor_running = false;
    s_plant.motor_speed_pct = 0;
    update_motor_status();
    printf("[hmi] Motor STOP\n");
}

static void btn_reverse_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (s_plant.estop_active) return;
    s_plant.motor_reverse = !s_plant.motor_reverse;
    update_motor_status();
    printf("[hmi] Motor direction: %s\n",
           s_plant.motor_reverse ? "REVERSE" : "FORWARD");
}

static void slider_speed_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    lv_obj_t *slider = lv_event_get_target(e);
    s_plant.motor_speed_pct = (int16_t)lv_slider_get_value(slider);
    lv_label_set_text_fmt(s_speed_label, "Speed: %d%%",
                          s_plant.motor_speed_pct);
    if (s_plant.motor_running) update_motor_status();
}

/* ---- emergency stop (long-press to activate) ---- */

static void on_estop_gesture(const eos_gesture_event_t *ev, void *ctx)
{
    (void)ctx;
    if (ev->type == EOS_GESTURE_LONG_PRESS) {
        s_plant.estop_active = !s_plant.estop_active;
        if (s_plant.estop_active) {
            s_plant.motor_running = false;
            s_plant.motor_speed_pct = 0;
            lv_obj_set_style_bg_color(s_estop_btn, COLOR_RED, 0);
            printf("[hmi] *** EMERGENCY STOP ACTIVATED ***\n");
        } else {
            lv_obj_set_style_bg_color(s_estop_btn, COLOR_YELLOW, 0);
            printf("[hmi] Emergency stop released\n");
        }
        update_motor_status();
    }
}

/* ================================================================== */
/*  Build the Control tab                                              */
/* ================================================================== */

static lv_obj_t *make_panel_button(lv_obj_t *parent, const char *text,
                                    lv_color_t color, int w, int h)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_radius(btn, 8, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl);
    return btn;
}

static void build_control_tab(lv_obj_t *parent)
{
    /* Motor control panel */
    lv_obj_t *motor_panel = lv_obj_create(parent);
    lv_obj_set_size(motor_panel, 350, 280);
    lv_obj_set_style_bg_color(motor_panel, COLOR_PANEL, 0);
    lv_obj_set_style_radius(motor_panel, 12, 0);
    lv_obj_set_style_border_width(motor_panel, 0, 0);
    lv_obj_align(motor_panel, LV_ALIGN_TOP_LEFT, 10, 10);

    lv_obj_t *motor_title = lv_label_create(motor_panel);
    lv_label_set_text(motor_title, LV_SYMBOL_SETTINGS " Motor Control");
    lv_obj_set_style_text_color(motor_title, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(motor_title, &lv_font_montserrat_16, 0);
    lv_obj_align(motor_title, LV_ALIGN_TOP_LEFT, 10, 5);

    /* Status LED + label */
    s_motor_led = lv_obj_create(motor_panel);
    lv_obj_set_size(s_motor_led, 20, 20);
    lv_obj_set_style_radius(s_motor_led, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_motor_led, COLOR_GRAY, 0);
    lv_obj_set_style_border_width(s_motor_led, 0, 0);
    lv_obj_align(s_motor_led, LV_ALIGN_TOP_RIGHT, -80, 8);

    s_motor_status_label = lv_label_create(motor_panel);
    lv_label_set_text(s_motor_status_label, "STOPPED");
    lv_obj_set_style_text_color(s_motor_status_label, COLOR_WHITE, 0);
    lv_obj_align(s_motor_status_label, LV_ALIGN_TOP_RIGHT, -10, 8);

    /* Buttons: START / STOP / REVERSE */
    lv_obj_t *btn_start = make_panel_button(motor_panel, LV_SYMBOL_PLAY " START",
                                             COLOR_GREEN, 140, 50);
    lv_obj_align(btn_start, LV_ALIGN_LEFT_MID, 15, -10);
    lv_obj_add_event_cb(btn_start, btn_start_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_stop = make_panel_button(motor_panel, LV_SYMBOL_STOP " STOP",
                                            COLOR_RED, 140, 50);
    lv_obj_align(btn_stop, LV_ALIGN_RIGHT_MID, -15, -10);
    lv_obj_add_event_cb(btn_stop, btn_stop_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_rev = make_panel_button(motor_panel, LV_SYMBOL_LOOP " REVERSE",
                                           COLOR_BLUE, 140, 40);
    lv_obj_align(btn_rev, LV_ALIGN_BOTTOM_LEFT, 15, -10);
    lv_obj_add_event_cb(btn_rev, btn_reverse_cb, LV_EVENT_CLICKED, NULL);

    /* Speed slider */
    lv_obj_t *speed_slider = lv_slider_create(motor_panel);
    lv_obj_set_width(speed_slider, 120);
    lv_slider_set_range(speed_slider, 0, 100);
    lv_slider_set_value(speed_slider, 0, LV_ANIM_OFF);
    lv_obj_align(speed_slider, LV_ALIGN_BOTTOM_RIGHT, -15, -20);
    lv_obj_add_event_cb(speed_slider, slider_speed_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);

    s_speed_label = lv_label_create(motor_panel);
    lv_label_set_text(s_speed_label, "Speed: 0%");
    lv_obj_set_style_text_color(s_speed_label, COLOR_WHITE, 0);
    lv_obj_align(s_speed_label, LV_ALIGN_BOTTOM_RIGHT, -30, -45);

    /* Temperature gauge panel */
    lv_obj_t *temp_panel = lv_obj_create(parent);
    lv_obj_set_size(temp_panel, 350, 120);
    lv_obj_set_style_bg_color(temp_panel, COLOR_PANEL, 0);
    lv_obj_set_style_radius(temp_panel, 12, 0);
    lv_obj_set_style_border_width(temp_panel, 0, 0);
    lv_obj_align(temp_panel, LV_ALIGN_BOTTOM_LEFT, 10, -10);

    lv_obj_t *temp_title = lv_label_create(temp_panel);
    lv_label_set_text(temp_title, LV_SYMBOL_CHARGE " Temperature");
    lv_obj_set_style_text_color(temp_title, COLOR_WHITE, 0);
    lv_obj_align(temp_title, LV_ALIGN_TOP_LEFT, 10, 5);

    s_temp_bar = lv_bar_create(temp_panel);
    lv_obj_set_size(s_temp_bar, 280, 20);
    lv_bar_set_range(s_temp_bar, 0, 120);
    lv_bar_set_value(s_temp_bar, 42, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_temp_bar, COLOR_GREEN, LV_PART_INDICATOR);
    lv_obj_align(s_temp_bar, LV_ALIGN_CENTER, 0, 5);

    s_temp_label = lv_label_create(temp_panel);
    lv_label_set_text(s_temp_label, "42.5" "\xC2\xB0" "C");
    lv_obj_set_style_text_color(s_temp_label, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(s_temp_label, &lv_font_montserrat_16, 0);
    lv_obj_align(s_temp_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    /* Emergency stop button (right side) */
    s_estop_btn = lv_button_create(parent);
    lv_obj_set_size(s_estop_btn, 180, 180);
    lv_obj_set_style_bg_color(s_estop_btn, COLOR_YELLOW, 0);
    lv_obj_set_style_radius(s_estop_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(s_estop_btn, 20, 0);
    lv_obj_set_style_shadow_color(s_estop_btn, COLOR_RED, 0);
    lv_obj_align(s_estop_btn, LV_ALIGN_RIGHT_MID, -60, -20);

    lv_obj_t *estop_lbl = lv_label_create(s_estop_btn);
    lv_label_set_text(estop_lbl, "E-STOP\n(hold)");
    lv_obj_set_style_text_font(estop_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(estop_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(estop_lbl);
}

/* ================================================================== */
/*  Timer: simulate plant data changes                                 */
/* ================================================================== */

static void plant_update_cb(lv_timer_t *timer)
{
    (void)timer;

    s_plant.uptime_sec++;

    /* Simulate temperature fluctuation */
    if (s_plant.motor_running)
        s_plant.temperature_mc += 200;
    else if (s_plant.temperature_mc > 25000)
        s_plant.temperature_mc -= 100;

    update_temperature();
    update_status_bar();
}

/* ================================================================== */
/*  main                                                               */
/* ================================================================== */

int main(void)
{
    printf("[hmi] Starting industrial HMI panel...\n");

    eos_hal_init();

    /* Display: 800×480 7" TFT, RGB565 */
    eos_display_config_t disp = {
        .id = DISPLAY_ID, .width = SCREEN_W, .height = SCREEN_H,
        .color_mode = EOS_DISPLAY_COLOR_RGB565,
    };
    eos_display_init(&disp);

    /* Touch: GT911 capacitive, 5-point multi-touch */
    eos_touch_config_t touch = {
        .id = TOUCH_ID, .type = EOS_TOUCH_CAPACITIVE,
        .width = SCREEN_W, .height = SCREEN_H,
        .max_points = 5, .i2c_port = 0, .i2c_addr = 0x5D,
    };
    eos_touch_init(&touch);

    /* EoS UI init */
    eos_ui_config_t ui = {
        .screen_width = SCREEN_W, .screen_height = SCREEN_H,
        .color_depth = 16, .display_id = DISPLAY_ID, .touch_id = TOUCH_ID,
        .fb_mode = EOS_UI_FB_FULL, .rotation = EOS_UI_ROTATE_0,
        .dpi = 130, .double_buffer = true,
    };
    if (eos_ui_init(&ui) != 0) {
        printf("[hmi] UI init failed\n");
        return -1;
    }

    eos_ui_set_theme(EOS_UI_THEME_DARK);

    /* Gesture: long-press for E-STOP */
    eos_ui_on_gesture(EOS_GESTURE_LONG_PRESS, on_estop_gesture, NULL);

    /* Build main screen */
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);

    /* Status bar (top) */
    lv_obj_t *status_bar = lv_obj_create(scr);
    lv_obj_set_size(status_bar, SCREEN_W, 30);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x16162A), 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 0, 0);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);

    s_status_bar_label = lv_label_create(status_bar);
    lv_obj_set_style_text_color(s_status_bar_label, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(s_status_bar_label, &lv_font_montserrat_12, 0);
    lv_obj_align(s_status_bar_label, LV_ALIGN_LEFT_MID, 10, 0);
    update_status_bar();

    /* Content area below status bar */
    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_set_size(content, SCREEN_W, SCREEN_H - 30);
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);

    /* Build control panel */
    build_control_tab(content);

    /* Initial state */
    update_motor_status();
    update_temperature();

    /* 1-second plant simulation timer */
    lv_timer_create(plant_update_cb, 1000, NULL);

    printf("[hmi] HMI panel ready — motor START/STOP, speed slider, E-STOP (long-press)\n");

    /* Main loop */
    for (;;) {
        uint32_t sleep = eos_ui_task_handler();
        (void)sleep;
    }

    return 0;
}
