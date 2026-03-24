// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file main.c
 * @brief EoS Example: Smartwatch Watchface — Round display LVGL demo
 *
 * Builds a digital + analog clock face with:
 *   - Time display (HH:MM:SS) with large font
 *   - Date display (day-of-week, month day)
 *   - Battery percentage arc
 *   - Step counter with daily goal progress bar
 *   - Swipe gestures: left/right to switch faces, down for notifications
 *
 * Target: 240×240 round display (1.28" GC9A01), CST816S capacitive touch
 * Product profile: watch / wearable / fitness
 */

#include <eos/hal.h>
#include <eos/hal_extended.h>
#include <eos/ui.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_SIZE   240
#define DISPLAY_ID    0
#define TOUCH_ID      0

/* ---- simulated data -------------------------------------------------- */

static struct {
    uint8_t  hour, minute, second;
    uint8_t  day, month, weekday;
    uint8_t  battery_pct;
    uint32_t steps;
    uint32_t step_goal;
    uint8_t  heart_rate;
    int      face_index;          /* 0 = digital, 1 = analog */
} s_watch = {
    .hour = 10, .minute = 42, .second = 15,
    .day = 27, .month = 3, .weekday = 4,
    .battery_pct = 73,
    .steps = 6280, .step_goal = 10000,
    .heart_rate = 72,
    .face_index = 0,
};

/* ---- LVGL objects ---------------------------------------------------- */

static lv_obj_t *s_screen_digital;
static lv_obj_t *s_screen_analog;
static lv_obj_t *s_time_label;
static lv_obj_t *s_date_label;
static lv_obj_t *s_battery_arc;
static lv_obj_t *s_battery_label;
static lv_obj_t *s_steps_bar;
static lv_obj_t *s_steps_label;
static lv_obj_t *s_hr_label;
static lv_obj_t *s_analog_hour;
static lv_obj_t *s_analog_min;
static lv_obj_t *s_analog_sec;

/* ---- helpers --------------------------------------------------------- */

static const char *weekday_name(uint8_t wd)
{
    static const char *names[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    return (wd < 7) ? names[wd] : "???";
}

static const char *month_name(uint8_t m)
{
    static const char *names[] = {
        "","Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };
    return (m >= 1 && m <= 12) ? names[m] : "???";
}

/* ================================================================== */
/*  Digital watchface                                                  */
/* ================================================================== */

static void build_digital_face(void)
{
    s_screen_digital = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen_digital, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_screen_digital, LV_OPA_COVER, 0);

    /* Time — large centered label */
    s_time_label = lv_label_create(s_screen_digital);
    lv_label_set_text_fmt(s_time_label, "%02d:%02d",
                          s_watch.hour, s_watch.minute);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_time_label, lv_color_white(), 0);
    lv_obj_align(s_time_label, LV_ALIGN_CENTER, 0, -20);

    /* Seconds — smaller below */
    lv_obj_t *sec = lv_label_create(s_screen_digital);
    lv_label_set_text_fmt(sec, ":%02d", s_watch.second);
    lv_obj_set_style_text_color(sec, lv_color_hex(0x888888), 0);
    lv_obj_align(sec, LV_ALIGN_CENTER, 60, -30);

    /* Date */
    s_date_label = lv_label_create(s_screen_digital);
    lv_label_set_text_fmt(s_date_label, "%s, %s %d",
                          weekday_name(s_watch.weekday),
                          month_name(s_watch.month), s_watch.day);
    lv_obj_set_style_text_color(s_date_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(s_date_label, LV_ALIGN_CENTER, 0, 15);

    /* Battery arc (top-right) */
    s_battery_arc = lv_arc_create(s_screen_digital);
    lv_arc_set_range(s_battery_arc, 0, 100);
    lv_arc_set_value(s_battery_arc, s_watch.battery_pct);
    lv_obj_set_size(s_battery_arc, 50, 50);
    lv_arc_set_bg_angles(s_battery_arc, 0, 360);
    lv_obj_remove_style(s_battery_arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(s_battery_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(s_battery_arc, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
    lv_obj_align(s_battery_arc, LV_ALIGN_TOP_RIGHT, -15, 15);

    s_battery_label = lv_label_create(s_screen_digital);
    lv_label_set_text_fmt(s_battery_label, "%d%%", s_watch.battery_pct);
    lv_obj_set_style_text_color(s_battery_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(s_battery_label, &lv_font_montserrat_10, 0);
    lv_obj_align_to(s_battery_label, s_battery_arc, LV_ALIGN_CENTER, 0, 0);

    /* Steps progress bar (bottom) */
    s_steps_bar = lv_bar_create(s_screen_digital);
    lv_obj_set_size(s_steps_bar, 140, 10);
    lv_bar_set_range(s_steps_bar, 0, (int32_t)s_watch.step_goal);
    lv_bar_set_value(s_steps_bar, (int32_t)s_watch.steps, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_steps_bar, lv_color_hex(0x00AAFF), LV_PART_INDICATOR);
    lv_obj_align(s_steps_bar, LV_ALIGN_BOTTOM_MID, 0, -50);

    s_steps_label = lv_label_create(s_screen_digital);
    lv_label_set_text_fmt(s_steps_label, LV_SYMBOL_SHUFFLE " %lu steps",
                          (unsigned long)s_watch.steps);
    lv_obj_set_style_text_color(s_steps_label, lv_color_hex(0x00AAFF), 0);
    lv_obj_set_style_text_font(s_steps_label, &lv_font_montserrat_12, 0);
    lv_obj_align(s_steps_label, LV_ALIGN_BOTTOM_MID, 0, -35);

    /* Heart rate (top-left) */
    s_hr_label = lv_label_create(s_screen_digital);
    lv_label_set_text_fmt(s_hr_label, LV_SYMBOL_WARNING " %d bpm",
                          s_watch.heart_rate);
    lv_obj_set_style_text_color(s_hr_label, lv_color_hex(0xFF4444), 0);
    lv_obj_set_style_text_font(s_hr_label, &lv_font_montserrat_12, 0);
    lv_obj_align(s_hr_label, LV_ALIGN_TOP_LEFT, 20, 25);
}

/* ================================================================== */
/*  Analog watchface                                                   */
/* ================================================================== */

static void build_analog_face(void)
{
    s_screen_analog = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen_analog, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(s_screen_analog, LV_OPA_COVER, 0);

    /* Hour markers (12 dots around the edge) */
    for (int i = 0; i < 12; i++) {
        lv_obj_t *dot = lv_obj_create(s_screen_analog);
        int r = (i % 3 == 0) ? 6 : 3;
        lv_obj_set_size(dot, r * 2, r * 2);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_white(), 0);
        lv_obj_set_style_border_width(dot, 0, 0);

        int angle_deg = i * 30;
        int cx = SCREEN_SIZE / 2 + (int)(95.0f * lv_trigo_sin(angle_deg * 10) / 32767);
        int cy = SCREEN_SIZE / 2 - (int)(95.0f * lv_trigo_cos(angle_deg * 10) / 32767);
        lv_obj_set_pos(dot, cx - r, cy - r);
    }

    /* Clock hands using lines */
    static lv_point_precise_t hour_pts[2];
    static lv_point_precise_t min_pts[2];
    static lv_point_precise_t sec_pts[2];

    int cx = SCREEN_SIZE / 2;
    int cy = SCREEN_SIZE / 2;

    /* Hour hand */
    int h_angle = (s_watch.hour % 12) * 30 + s_watch.minute / 2;
    hour_pts[0] = (lv_point_precise_t){cx, cy};
    hour_pts[1] = (lv_point_precise_t){
        cx + 50 * lv_trigo_sin(h_angle * 10) / 32767,
        cy - 50 * lv_trigo_cos(h_angle * 10) / 32767
    };
    s_analog_hour = lv_line_create(s_screen_analog);
    lv_line_set_points(s_analog_hour, hour_pts, 2);
    lv_obj_set_style_line_width(s_analog_hour, 4, 0);
    lv_obj_set_style_line_color(s_analog_hour, lv_color_white(), 0);
    lv_obj_set_style_line_rounded(s_analog_hour, true, 0);

    /* Minute hand */
    int m_angle = s_watch.minute * 6;
    min_pts[0] = (lv_point_precise_t){cx, cy};
    min_pts[1] = (lv_point_precise_t){
        cx + 70 * lv_trigo_sin(m_angle * 10) / 32767,
        cy - 70 * lv_trigo_cos(m_angle * 10) / 32767
    };
    s_analog_min = lv_line_create(s_screen_analog);
    lv_line_set_points(s_analog_min, min_pts, 2);
    lv_obj_set_style_line_width(s_analog_min, 3, 0);
    lv_obj_set_style_line_color(s_analog_min, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_line_rounded(s_analog_min, true, 0);

    /* Second hand */
    int s_angle = s_watch.second * 6;
    sec_pts[0] = (lv_point_precise_t){cx, cy};
    sec_pts[1] = (lv_point_precise_t){
        cx + 80 * lv_trigo_sin(s_angle * 10) / 32767,
        cy - 80 * lv_trigo_cos(s_angle * 10) / 32767
    };
    s_analog_sec = lv_line_create(s_screen_analog);
    lv_line_set_points(s_analog_sec, sec_pts, 2);
    lv_obj_set_style_line_width(s_analog_sec, 1, 0);
    lv_obj_set_style_line_color(s_analog_sec, lv_color_hex(0xFF4444), 0);

    /* Center dot */
    lv_obj_t *center = lv_obj_create(s_screen_analog);
    lv_obj_set_size(center, 8, 8);
    lv_obj_set_style_radius(center, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(center, lv_color_hex(0xFF4444), 0);
    lv_obj_set_style_border_width(center, 0, 0);
    lv_obj_align(center, LV_ALIGN_CENTER, 0, 0);

    /* Date label at 3 o'clock position */
    lv_obj_t *date = lv_label_create(s_screen_analog);
    lv_label_set_text_fmt(date, "%d", s_watch.day);
    lv_obj_set_style_text_color(date, lv_color_white(), 0);
    lv_obj_set_style_text_font(date, &lv_font_montserrat_12, 0);
    lv_obj_align(date, LV_ALIGN_CENTER, 45, 0);
}

/* ================================================================== */
/*  Gesture handler — swipe to switch faces                            */
/* ================================================================== */

static void on_gesture(const eos_gesture_event_t *ev, void *ctx)
{
    (void)ctx;

    if (ev->type == EOS_GESTURE_SWIPE_LEFT) {
        s_watch.face_index = 1;
        lv_screen_load_anim(s_screen_analog, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
        printf("[watchface] Switched to analog face\n");
    } else if (ev->type == EOS_GESTURE_SWIPE_RIGHT) {
        s_watch.face_index = 0;
        lv_screen_load_anim(s_screen_digital, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
        printf("[watchface] Switched to digital face\n");
    } else if (ev->type == EOS_GESTURE_SWIPE_DOWN) {
        printf("[watchface] Notification shade (not implemented)\n");
    } else if (ev->type == EOS_GESTURE_SWIPE_UP) {
        printf("[watchface] App drawer (not implemented)\n");
    }
}

/* ================================================================== */
/*  Time update timer                                                  */
/* ================================================================== */

static void time_update_cb(lv_timer_t *timer)
{
    (void)timer;
    s_watch.second++;
    if (s_watch.second >= 60) { s_watch.second = 0; s_watch.minute++; }
    if (s_watch.minute >= 60) { s_watch.minute = 0; s_watch.hour++; }
    if (s_watch.hour >= 24)   { s_watch.hour = 0; }

    /* Update digital face */
    if (s_time_label)
        lv_label_set_text_fmt(s_time_label, "%02d:%02d",
                              s_watch.hour, s_watch.minute);

    /* Simulate step counter */
    s_watch.steps += 3;
    if (s_steps_label)
        lv_label_set_text_fmt(s_steps_label, LV_SYMBOL_SHUFFLE " %lu steps",
                              (unsigned long)s_watch.steps);
    if (s_steps_bar)
        lv_bar_set_value(s_steps_bar, (int32_t)s_watch.steps, LV_ANIM_ON);
}

/* ================================================================== */
/*  main                                                               */
/* ================================================================== */

int main(void)
{
    printf("[watchface] Starting smartwatch demo...\n");

    eos_hal_init();

    /* Display: 240×240 round, RGB565 */
    eos_display_config_t disp = {
        .id = DISPLAY_ID, .width = SCREEN_SIZE, .height = SCREEN_SIZE,
        .color_mode = EOS_DISPLAY_COLOR_RGB565,
    };
    eos_display_init(&disp);

    /* Touch: CST816S capacitive */
    eos_touch_config_t touch = {
        .id = TOUCH_ID, .type = EOS_TOUCH_CAPACITIVE,
        .width = SCREEN_SIZE, .height = SCREEN_SIZE,
        .max_points = 1, .i2c_port = 0, .i2c_addr = 0x15,
    };
    eos_touch_init(&touch);

    /* EoS UI init */
    eos_ui_config_t ui = {
        .screen_width = SCREEN_SIZE, .screen_height = SCREEN_SIZE,
        .color_depth = 16, .display_id = DISPLAY_ID, .touch_id = TOUCH_ID,
        .fb_mode = EOS_UI_FB_PARTIAL, .rotation = EOS_UI_ROTATE_0,
        .dpi = 200, .double_buffer = true,
    };
    if (eos_ui_init(&ui) != 0) {
        printf("[watchface] UI init failed\n");
        return -1;
    }

    /* Gestures — swipe to switch faces */
    eos_ui_on_gesture(EOS_GESTURE_SWIPE_LEFT | EOS_GESTURE_SWIPE_RIGHT
                    | EOS_GESTURE_SWIPE_UP | EOS_GESTURE_SWIPE_DOWN,
                      on_gesture, NULL);

    /* Build both watchfaces */
    build_digital_face();
    build_analog_face();

    /* Start on digital face */
    lv_screen_load(s_screen_digital);

    /* 1-second timer to update time */
    lv_timer_create(time_update_cb, 1000, NULL);

    printf("[watchface] Ready — swipe left/right to switch faces\n");

    /* Main loop */
    for (;;) {
        uint32_t sleep = eos_ui_task_handler();
        (void)sleep;
    }

    return 0;
}
