// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_ui_touch_drv.c
 * @brief Unit tests for the LVGL touch read callback (ui_touch_drv.c)
 *
 * The driver source is compiled into this translation unit with a fake
 * eos_touch_read() (the touch controller) and a fake eos_ui_gesture_feed()
 * that records what the gesture recogniser is given.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/* ---- mock LVGL indev types ------------------------------------------- */

typedef struct _lv_indev_t lv_indev_t;
typedef struct {
    struct { int16_t x, y; } point;
    int state;
} lv_indev_data_t;
#define LV_INDEV_STATE_RELEASED 0
#define LV_INDEV_STATE_PRESSED  1

#include <eos/eos_config.h>
#include <eos/hal_extended.h>
#include <eos/ui.h>

/* ---- fake touch controller and gesture recogniser --------------------- */

static eos_touch_point_t s_panel[5];
static uint8_t           s_panel_count;

int eos_touch_read(uint8_t id, eos_touch_point_t *points, uint8_t max_points,
                   uint8_t *count)
{
    (void)id;
    uint8_t n = s_panel_count < max_points ? s_panel_count : max_points;
    memcpy(points, s_panel, n * sizeof(points[0]));
    *count = n;
    return 0;
}

static eos_touch_point_t s_fed[5];
static uint8_t           s_fed_count;

void eos_ui_gesture_feed(const eos_touch_point_t *points, uint8_t count)
{
    memcpy(s_fed, points, count * sizeof(points[0]));
    s_fed_count = count;
}

/* The mock lvgl.h in tests/mocks/ satisfies the driver's #include <lvgl.h>. */
#include "../services/ui/src/ui_touch_drv.c"

/* ==================================================================== */

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static void touch(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    memset(s_panel, 0, sizeof(s_panel));
    s_panel[0] = (eos_touch_point_t){ .x = x0, .y = y0, .id = 0, .pressed = true };
    s_panel[1] = (eos_touch_point_t){ .x = x1, .y = y1, .id = 1, .pressed = true };
    s_panel_count = 2;
    s_fed_count = 0;
}

/* With rotation 0 nothing moves. */
static void test_rotate_0(void)
{
    lv_indev_data_t d;
    printf("  test_rotate_0\n");
    eos_ui_touch_set_params(0, EOS_UI_ROTATE_0, 320, 240);
    touch(10, 20, 100, 50);
    eos_ui_touch_read_cb(NULL, &d);
    CHECK(d.state == LV_INDEV_STATE_PRESSED);
    CHECK(d.point.x == 10 && d.point.y == 20);
    CHECK(s_fed_count == 2);
    CHECK(s_fed[0].x == 10 && s_fed[0].y == 20);
    CHECK(s_fed[1].x == 100 && s_fed[1].y == 50);
}

/* The gesture recogniser gets the same screen-space point LVGL gets, and
 * every other point is rotated the same way. */
static void test_rotate_180_all_points(void)
{
    lv_indev_data_t d;
    printf("  test_rotate_180_all_points\n");
    eos_ui_touch_set_params(0, EOS_UI_ROTATE_180, 320, 240);
    touch(10, 20, 100, 50);
    eos_ui_touch_read_cb(NULL, &d);
    CHECK(d.point.x == 309 && d.point.y == 219);
    CHECK(s_fed_count == 2);
    CHECK(s_fed[0].x == 309 && s_fed[0].y == 219);
    CHECK(s_fed[1].x == 219 && s_fed[1].y == 189);
    CHECK(s_fed[1].id == 1 && s_fed[1].pressed);
}

static void test_rotate_90_all_points(void)
{
    lv_indev_data_t d;
    printf("  test_rotate_90_all_points\n");
    eos_ui_touch_set_params(0, EOS_UI_ROTATE_90, 320, 240);
    touch(10, 20, 100, 50);
    eos_ui_touch_read_cb(NULL, &d);
    CHECK(d.point.x == 20 && d.point.y == 309);
    CHECK(s_fed[0].x == d.point.x && s_fed[0].y == d.point.y);
    CHECK(s_fed[1].x == 50 && s_fed[1].y == 219);
}

static void test_rotate_270_all_points(void)
{
    lv_indev_data_t d;
    printf("  test_rotate_270_all_points\n");
    eos_ui_touch_set_params(0, EOS_UI_ROTATE_270, 320, 240);
    touch(10, 20, 100, 50);
    eos_ui_touch_read_cb(NULL, &d);
    CHECK(d.point.x == 219 && d.point.y == 10);
    CHECK(s_fed[0].x == d.point.x && s_fed[0].y == d.point.y);
    CHECK(s_fed[1].x == 189 && s_fed[1].y == 100);
}

int main(void)
{
    printf("test_ui_touch_drv\n");
    test_rotate_0();
    test_rotate_180_all_points();
    test_rotate_90_all_points();
    test_rotate_270_all_points();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
