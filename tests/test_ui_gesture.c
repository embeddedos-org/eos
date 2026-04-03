// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_ui_gesture.c
 * @brief Unit tests for the EoS UI gesture recognition state machine
 *
 * Exercises every gesture path: tap, double-tap, long-press, swipe
 * (4 directions), pinch-in/out, threshold configuration, callback
 * masking, and edge cases.
 *
 * The gesture source is compiled directly into this translation unit so
 * we can control the mock tick counter and inspect file-scope statics.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/* ---- compile guards --------------------------------------------------
 * When no EOS_PRODUCT_* is defined, eos_config.h enables everything
 * (development build), including EOS_ENABLE_DISPLAY, EOS_ENABLE_TOUCH,
 * and EOS_ENABLE_UI.  No manual overrides needed here.
 * ------------------------------------------------------------------- */

/* ---- mock LVGL types & functions ------------------------------------- */

typedef struct { int32_t x1, y1, x2, y2; } lv_area_t;
typedef struct _lv_display_t lv_display_t;
typedef struct _lv_indev_t   lv_indev_t;

static uint32_t s_mock_tick = 0;
static uint32_t lv_tick_get(void) { return s_mock_tick; }

/* stub — the gesture code never calls these directly */
#define LV_DISPLAY_RENDER_MODE_FULL    0
#define LV_DISPLAY_RENDER_MODE_PARTIAL 1

/* ---- pull in the public header (types only) -------------------------- */
#include <eos/eos_config.h>
#include <eos/hal_extended.h>
#include <eos/ui.h>

/* ---- include the source under test directly -------------------------- */
/* The mock lvgl.h in tests/mocks/ satisfies the #include <lvgl.h> inside
   ui_gesture.c without pulling in the real LVGL library. */
#include "../../services/ui/src/ui_gesture.c"

/* ==================================================================== */
/*  Test framework (matches project conventions)                        */
/* ==================================================================== */

static int tests_run    = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        printf("  %-55s ", #name); \
        name(); \
        tests_passed++; \
        printf("[PASS]\n"); \
    } \
    static void name(void)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while(0)

/* ==================================================================== */
/*  Helper: capture emitted gestures                                    */
/* ==================================================================== */

#define MAX_CAPTURED 16

static eos_gesture_event_t captured[MAX_CAPTURED];
static int                 capture_count;

static void capture_cb(const eos_gesture_event_t *ev, void *ctx)
{
    (void)ctx;
    if (capture_count < MAX_CAPTURED)
        captured[capture_count++] = *ev;
}

/* Reset all static state between tests */
static void reset(void)
{
    memset(&s_gs,  0, sizeof(s_gs));
    memset(s_cbs,  0, sizeof(s_cbs));
    s_cb_count = 0;
    s_gesture_cfg = (eos_gesture_config_t){
        .swipe_min_distance = 50,
        .long_press_time_ms = 400,
        .double_tap_time_ms = 300,
        .pinch_ratio_pct    = 20,
    };
    s_mock_tick   = 0;
    capture_count = 0;
    memset(captured, 0, sizeof(captured));
}

/* Convenience: build a single-finger touch point */
static eos_touch_point_t pt(uint16_t x, uint16_t y, bool pressed)
{
    eos_touch_point_t p = {0};
    p.x = x;
    p.y = y;
    p.pressed = pressed;
    return p;
}

/* ==================================================================== */
/*  Tests                                                               */
/* ==================================================================== */

/* ---- callback registration ------------------------------------------ */

TEST(test_on_gesture_registers_callback)
{
    reset();
    int rc = eos_ui_on_gesture(EOS_GESTURE_TAP, capture_cb, NULL);
    ASSERT(rc == 0);
    ASSERT(s_cb_count == 1);
}

TEST(test_on_gesture_null_cb_rejected)
{
    reset();
    int rc = eos_ui_on_gesture(EOS_GESTURE_TAP, NULL, NULL);
    ASSERT(rc == -1);
    ASSERT(s_cb_count == 0);
}

TEST(test_on_gesture_max_callbacks)
{
    reset();
    for (int i = 0; i < MAX_GESTURE_CBS; i++) {
        ASSERT(eos_ui_on_gesture(EOS_GESTURE_TAP, capture_cb, NULL) == 0);
    }
    ASSERT(eos_ui_on_gesture(EOS_GESTURE_TAP, capture_cb, NULL) == -1);
    ASSERT(s_cb_count == MAX_GESTURE_CBS);
}

/* ---- configuration -------------------------------------------------- */

TEST(test_configure_gestures_null_rejected)
{
    reset();
    ASSERT(eos_ui_configure_gestures(NULL) == -1);
}

TEST(test_configure_gestures_applies)
{
    reset();
    eos_gesture_config_t cfg = {
        .swipe_min_distance = 100,
        .long_press_time_ms = 800,
        .double_tap_time_ms = 200,
        .pinch_ratio_pct    = 30,
    };
    ASSERT(eos_ui_configure_gestures(&cfg) == 0);
    ASSERT(s_gesture_cfg.swipe_min_distance == 100);
    ASSERT(s_gesture_cfg.long_press_time_ms == 800);
    ASSERT(s_gesture_cfg.double_tap_time_ms == 200);
    ASSERT(s_gesture_cfg.pinch_ratio_pct    == 30);
}

/* ---- tap ------------------------------------------------------------ */

TEST(test_tap_basic)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    s_mock_tick = 100;
    eos_touch_point_t p = pt(100, 100, true);
    eos_ui_gesture_feed(&p, 1);                       /* touch down */

    s_mock_tick = 120;
    p = pt(100, 100, false);
    eos_ui_gesture_feed(&p, 1);                       /* release */

    ASSERT(capture_count == 1);
    ASSERT(captured[0].type == EOS_GESTURE_TAP);
    ASSERT(captured[0].start_x == 100);
    ASSERT(captured[0].start_y == 100);
}

TEST(test_tap_callback_mask_filters)
{
    reset();
    /* Register only for swipe-left — tap should NOT be delivered */
    eos_ui_on_gesture(EOS_GESTURE_SWIPE_LEFT, capture_cb, NULL);

    s_mock_tick = 0;
    eos_touch_point_t p = pt(50, 50, true);
    eos_ui_gesture_feed(&p, 1);
    s_mock_tick = 10;
    p = pt(50, 50, false);
    eos_ui_gesture_feed(&p, 1);

    ASSERT(capture_count == 0);
}

/* ---- double tap ----------------------------------------------------- */

TEST(test_double_tap)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    /* First tap */
    s_mock_tick = 100;
    eos_touch_point_t p = pt(100, 100, true);
    eos_ui_gesture_feed(&p, 1);
    s_mock_tick = 120;
    p = pt(100, 100, false);
    eos_ui_gesture_feed(&p, 1);

    ASSERT(capture_count == 1);
    ASSERT(captured[0].type == EOS_GESTURE_TAP);

    /* Second tap within double_tap_time_ms (300 default) */
    s_mock_tick = 200;
    p = pt(105, 105, true);
    eos_ui_gesture_feed(&p, 1);
    s_mock_tick = 220;
    p = pt(105, 105, false);
    eos_ui_gesture_feed(&p, 1);

    ASSERT(capture_count == 2);
    ASSERT(captured[1].type == EOS_GESTURE_DOUBLE_TAP);
}

TEST(test_double_tap_too_slow_becomes_two_taps)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    /* First tap */
    s_mock_tick = 100;
    eos_touch_point_t p = pt(100, 100, true);
    eos_ui_gesture_feed(&p, 1);
    s_mock_tick = 120;
    p = pt(100, 100, false);
    eos_ui_gesture_feed(&p, 1);
    ASSERT(captured[0].type == EOS_GESTURE_TAP);

    /* Second tap after 500ms — outside double-tap window */
    s_mock_tick = 620;
    p = pt(100, 100, true);
    eos_ui_gesture_feed(&p, 1);
    s_mock_tick = 640;
    p = pt(100, 100, false);
    eos_ui_gesture_feed(&p, 1);

    ASSERT(capture_count == 2);
    ASSERT(captured[1].type == EOS_GESTURE_TAP);
}

/* ---- long press ----------------------------------------------------- */

TEST(test_long_press)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    s_mock_tick = 0;
    eos_touch_point_t p = pt(200, 200, true);
    eos_ui_gesture_feed(&p, 1);

    /* Hold — still pressed, tick advances past 400ms threshold */
    s_mock_tick = 450;
    p = pt(200, 200, true);
    eos_ui_gesture_feed(&p, 1);

    ASSERT(capture_count == 1);
    ASSERT(captured[0].type == EOS_GESTURE_LONG_PRESS);
    ASSERT(captured[0].duration_ms == 450);
}

/* ---- swipe left ----------------------------------------------------- */

TEST(test_swipe_left)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    s_mock_tick = 0;
    eos_touch_point_t p = pt(200, 100, true);
    eos_ui_gesture_feed(&p, 1);

    /* Drag left by 80px (> swipe_min_distance / 2 to enter MOVING) */
    s_mock_tick = 50;
    p = pt(120, 100, true);
    eos_ui_gesture_feed(&p, 1);

    /* Release */
    s_mock_tick = 100;
    p = pt(120, 100, false);
    eos_ui_gesture_feed(&p, 1);

    ASSERT(capture_count == 1);
    ASSERT(captured[0].type == EOS_GESTURE_SWIPE_LEFT);
    ASSERT(captured[0].start_x == 200);
    ASSERT(captured[0].end_x   == 120);
}

/* ---- swipe right ---------------------------------------------------- */

TEST(test_swipe_right)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    s_mock_tick = 0;
    eos_touch_point_t p = pt(100, 100, true);
    eos_ui_gesture_feed(&p, 1);

    s_mock_tick = 50;
    p = pt(200, 100, true);
    eos_ui_gesture_feed(&p, 1);

    s_mock_tick = 100;
    p = pt(200, 100, false);
    eos_ui_gesture_feed(&p, 1);

    ASSERT(capture_count == 1);
    ASSERT(captured[0].type == EOS_GESTURE_SWIPE_RIGHT);
}

/* ---- swipe up ------------------------------------------------------- */

TEST(test_swipe_up)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    s_mock_tick = 0;
    eos_touch_point_t p = pt(100, 200, true);
    eos_ui_gesture_feed(&p, 1);

    s_mock_tick = 50;
    p = pt(100, 100, true);
    eos_ui_gesture_feed(&p, 1);

    s_mock_tick = 100;
    p = pt(100, 100, false);
    eos_ui_gesture_feed(&p, 1);

    ASSERT(capture_count == 1);
    ASSERT(captured[0].type == EOS_GESTURE_SWIPE_UP);
}

/* ---- swipe down ----------------------------------------------------- */

TEST(test_swipe_down)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    s_mock_tick = 0;
    eos_touch_point_t p = pt(100, 100, true);
    eos_ui_gesture_feed(&p, 1);

    s_mock_tick = 50;
    p = pt(100, 200, true);
    eos_ui_gesture_feed(&p, 1);

    s_mock_tick = 100;
    p = pt(100, 200, false);
    eos_ui_gesture_feed(&p, 1);

    ASSERT(capture_count == 1);
    ASSERT(captured[0].type == EOS_GESTURE_SWIPE_DOWN);
}

/* ---- movement below threshold → tap, not swipe ---------------------- */

TEST(test_small_movement_is_tap_not_swipe)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    s_mock_tick = 0;
    eos_touch_point_t p = pt(100, 100, true);
    eos_ui_gesture_feed(&p, 1);

    /* Move only 5px — well below swipe threshold */
    s_mock_tick = 30;
    p = pt(105, 100, true);
    eos_ui_gesture_feed(&p, 1);

    s_mock_tick = 60;
    p = pt(105, 100, false);
    eos_ui_gesture_feed(&p, 1);

    ASSERT(capture_count == 1);
    ASSERT(captured[0].type == EOS_GESTURE_TAP);
}

/* ---- pinch out ------------------------------------------------------ */

TEST(test_pinch_out)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    /* Two fingers close together (100px apart) */
    s_mock_tick = 0;
    eos_touch_point_t pts[2];
    pts[0] = pt(100, 100, true);
    pts[1] = pt(200, 100, true);
    eos_ui_gesture_feed(pts, 2);              /* establishes initial_dist2 */

    /* Spread apart (250px apart — >20% increase from 100) */
    s_mock_tick = 50;
    pts[0] = pt(75, 100, true);
    pts[1] = pt(325, 100, true);
    eos_ui_gesture_feed(pts, 2);

    ASSERT(capture_count == 1);
    ASSERT(captured[0].type == EOS_GESTURE_PINCH_OUT);
}

/* ---- pinch in ------------------------------------------------------- */

TEST(test_pinch_in)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    /* Two fingers 200px apart */
    s_mock_tick = 0;
    eos_touch_point_t pts[2];
    pts[0] = pt(100, 100, true);
    pts[1] = pt(300, 100, true);
    eos_ui_gesture_feed(pts, 2);

    /* Pinch together (100px apart — >20% decrease from 200) */
    s_mock_tick = 50;
    pts[0] = pt(150, 100, true);
    pts[1] = pt(200, 100, true);
    eos_ui_gesture_feed(pts, 2);

    ASSERT(capture_count == 1);
    ASSERT(captured[0].type == EOS_GESTURE_PINCH_IN);
}

/* ---- pinch within threshold → no event ------------------------------ */

TEST(test_pinch_below_threshold_no_event)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    /* Two fingers 200px apart */
    s_mock_tick = 0;
    eos_touch_point_t pts[2];
    pts[0] = pt(100, 100, true);
    pts[1] = pt(300, 100, true);
    eos_ui_gesture_feed(pts, 2);

    /* Move only 5px closer — well within 20% threshold */
    s_mock_tick = 50;
    pts[0] = pt(102, 100, true);
    pts[1] = pt(298, 100, true);
    eos_ui_gesture_feed(pts, 2);

    ASSERT(capture_count == 0);
}

/* ---- zero-count feed is no-op --------------------------------------- */

TEST(test_zero_count_noop)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    eos_touch_point_t p = pt(100, 100, true);
    eos_ui_gesture_feed(&p, 0);

    ASSERT(capture_count == 0);
}

/* ---- custom thresholds ---------------------------------------------- */

TEST(test_custom_swipe_threshold)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    /* Set swipe threshold to 200px */
    eos_gesture_config_t cfg = {
        .swipe_min_distance = 200,
        .long_press_time_ms = 400,
        .double_tap_time_ms = 300,
        .pinch_ratio_pct    = 20,
    };
    eos_ui_configure_gestures(&cfg);

    /* Move 80px — would be a swipe with default 50, but not with 200 */
    s_mock_tick = 0;
    eos_touch_point_t p = pt(100, 100, true);
    eos_ui_gesture_feed(&p, 1);

    s_mock_tick = 50;
    p = pt(180, 100, true);
    eos_ui_gesture_feed(&p, 1);

    s_mock_tick = 100;
    p = pt(180, 100, false);
    eos_ui_gesture_feed(&p, 1);

    /* Should NOT have emitted a swipe */
    for (int i = 0; i < capture_count; i++) {
        ASSERT(captured[i].type != EOS_GESTURE_SWIPE_RIGHT);
        ASSERT(captured[i].type != EOS_GESTURE_SWIPE_LEFT);
    }
}

TEST(test_custom_long_press_threshold)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    /* Set long-press to 1000ms */
    eos_gesture_config_t cfg = {
        .swipe_min_distance = 50,
        .long_press_time_ms = 1000,
        .double_tap_time_ms = 300,
        .pinch_ratio_pct    = 20,
    };
    eos_ui_configure_gestures(&cfg);

    s_mock_tick = 0;
    eos_touch_point_t p = pt(100, 100, true);
    eos_ui_gesture_feed(&p, 1);

    /* 500ms is not enough for a 1000ms threshold */
    s_mock_tick = 500;
    p = pt(100, 100, true);
    eos_ui_gesture_feed(&p, 1);
    ASSERT(capture_count == 0);

    /* 1100ms is enough */
    s_mock_tick = 1100;
    p = pt(100, 100, true);
    eos_ui_gesture_feed(&p, 1);
    ASSERT(capture_count == 1);
    ASSERT(captured[0].type == EOS_GESTURE_LONG_PRESS);
}

/* ---- event fields populated correctly ------------------------------- */

TEST(test_event_fields_swipe)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    s_mock_tick = 1000;
    eos_touch_point_t p = pt(50, 150, true);
    eos_ui_gesture_feed(&p, 1);

    s_mock_tick = 1050;
    p = pt(250, 150, true);
    eos_ui_gesture_feed(&p, 1);

    s_mock_tick = 1200;
    p = pt(250, 150, false);
    eos_ui_gesture_feed(&p, 1);

    ASSERT(capture_count == 1);
    ASSERT(captured[0].type     == EOS_GESTURE_SWIPE_RIGHT);
    ASSERT(captured[0].start_x  == 50);
    ASSERT(captured[0].start_y  == 150);
    ASSERT(captured[0].end_x    == 250);
    ASSERT(captured[0].end_y    == 150);
    ASSERT(captured[0].duration_ms == 200);   /* 1200 - 1000 */
}

/* ---- callback context pointer forwarded ----------------------------- */

static int    s_ctx_val = 42;
static void  *s_received_ctx = NULL;

static void ctx_capture_cb(const eos_gesture_event_t *ev, void *ctx)
{
    (void)ev;
    s_received_ctx = ctx;
}

TEST(test_callback_context_forwarded)
{
    reset();
    s_received_ctx = NULL;

    eos_ui_on_gesture(EOS_GESTURE_TAP, ctx_capture_cb, &s_ctx_val);

    s_mock_tick = 0;
    eos_touch_point_t p = pt(10, 10, true);
    eos_ui_gesture_feed(&p, 1);
    s_mock_tick = 10;
    p = pt(10, 10, false);
    eos_ui_gesture_feed(&p, 1);

    ASSERT(s_received_ctx == &s_ctx_val);
}

/* ---- multiple callbacks with different masks ------------------------ */

static int s_tap_only_count  = 0;
static int s_swipe_only_count = 0;

static void tap_only_cb(const eos_gesture_event_t *ev, void *ctx)
{
    (void)ctx;
    if (ev->type == EOS_GESTURE_TAP) s_tap_only_count++;
}

static void swipe_only_cb(const eos_gesture_event_t *ev, void *ctx)
{
    (void)ctx;
    if (ev->type == EOS_GESTURE_SWIPE_RIGHT) s_swipe_only_count++;
}

TEST(test_multiple_callbacks_selective_delivery)
{
    reset();
    s_tap_only_count  = 0;
    s_swipe_only_count = 0;

    eos_ui_on_gesture(EOS_GESTURE_TAP, tap_only_cb, NULL);
    eos_ui_on_gesture(EOS_GESTURE_SWIPE_RIGHT, swipe_only_cb, NULL);

    /* Generate a tap */
    s_mock_tick = 0;
    eos_touch_point_t p = pt(50, 50, true);
    eos_ui_gesture_feed(&p, 1);
    s_mock_tick = 10;
    p = pt(50, 50, false);
    eos_ui_gesture_feed(&p, 1);

    ASSERT(s_tap_only_count   == 1);
    ASSERT(s_swipe_only_count == 0);

    /* Generate a swipe right */
    s_mock_tick = 500;
    p = pt(50, 50, true);
    eos_ui_gesture_feed(&p, 1);
    s_mock_tick = 520;
    p = pt(200, 50, true);
    eos_ui_gesture_feed(&p, 1);
    s_mock_tick = 550;
    p = pt(200, 50, false);
    eos_ui_gesture_feed(&p, 1);

    ASSERT(s_tap_only_count   == 1);
    ASSERT(s_swipe_only_count == 1);
}

/* ---- diagonal movement: dominant axis wins -------------------------- */

TEST(test_diagonal_dominant_axis)
{
    reset();
    eos_ui_on_gesture(0xFFFFFFFF, capture_cb, NULL);

    /* Move diagonally with more X than Y */
    s_mock_tick = 0;
    eos_touch_point_t p = pt(100, 100, true);
    eos_ui_gesture_feed(&p, 1);

    s_mock_tick = 50;
    p = pt(220, 130, true);
    eos_ui_gesture_feed(&p, 1);

    s_mock_tick = 100;
    p = pt(220, 130, false);
    eos_ui_gesture_feed(&p, 1);

    ASSERT(capture_count == 1);
    ASSERT(captured[0].type == EOS_GESTURE_SWIPE_RIGHT);  /* X dominant */
}

/* ==================================================================== */
/*  main                                                                */
/* ==================================================================== */

int main(void)
{
    printf("=== EoS: UI Gesture State Machine Unit Tests ===\n\n");

    run_test_on_gesture_registers_callback();
    run_test_on_gesture_null_cb_rejected();
    run_test_on_gesture_max_callbacks();
    run_test_configure_gestures_null_rejected();
    run_test_configure_gestures_applies();
    run_test_tap_basic();
    run_test_tap_callback_mask_filters();
    run_test_double_tap();
    run_test_double_tap_too_slow_becomes_two_taps();
    run_test_long_press();
    run_test_swipe_left();
    run_test_swipe_right();
    run_test_swipe_up();
    run_test_swipe_down();
    run_test_small_movement_is_tap_not_swipe();
    run_test_pinch_out();
    run_test_pinch_in();
    run_test_pinch_below_threshold_no_event();
    run_test_zero_count_noop();
    run_test_custom_swipe_threshold();
    run_test_custom_long_press_threshold();
    run_test_event_fields_swipe();
    run_test_callback_context_forwarded();
    run_test_multiple_callbacks_selective_delivery();
    run_test_diagonal_dominant_axis();

    tests_run = 25;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
