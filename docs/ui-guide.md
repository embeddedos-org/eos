# UI Service — LVGL Integration Guide

The EoS UI service bridges [LVGL](https://lvgl.io/) to the EoS HAL display and touch drivers. It provides one-call initialization, a gesture recognition engine, and theme switching — so you can go from raw pixels to interactive widgets with minimal glue code.

Requires `EOS_ENABLE_UI` (which implies `EOS_ENABLE_DISPLAY`). Touch features require `EOS_ENABLE_TOUCH`.

---

## Quick Start

### Minimal example — 240×320 display, RGB565, no touch

```c
#include <eos/hal.h>
#include <eos/ui.h>
#include <lvgl.h>

int main(void)
{
    eos_hal_init();

    /* Configure the display through HAL first */
    eos_display_config_t disp_cfg = {
        .id         = 0,
        .width      = 240,
        .height     = 320,
        .color_mode = EOS_DISPLAY_COLOR_RGB565,
    };
    eos_display_init(&disp_cfg);

    /* Initialize the UI layer */
    eos_ui_config_t ui_cfg = {
        .screen_width  = 240,
        .screen_height = 320,
        .color_depth   = 16,           /* RGB565 */
        .display_id    = 0,
        .fb_mode       = EOS_UI_FB_PARTIAL,
        .rotation      = EOS_UI_ROTATE_0,
        .dpi           = 0,            /* use LVGL default */
        .double_buffer = false,
    };

    if (eos_ui_init(&ui_cfg) != 0) {
        /* handle error */
        return -1;
    }

    /* Create a simple label */
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello, EoS!");
    lv_obj_center(label);

    /* Main loop */
    for (;;) {
        uint32_t sleep_ms = eos_ui_task_handler();
        eos_delay_ms(sleep_ms);
    }
}
```

### Full example — display + touch + gestures

```c
#include <eos/hal.h>
#include <eos/ui.h>
#include <lvgl.h>
#include <stdio.h>

/* Called when a gesture is detected */
static void on_gesture(const eos_gesture_event_t *ev, void *ctx)
{
    (void)ctx;
    switch (ev->type) {
        case EOS_GESTURE_TAP:
            printf("Tap at (%d, %d)\n", ev->start_x, ev->start_y);
            break;
        case EOS_GESTURE_SWIPE_LEFT:
            printf("Swipe left (%d ms)\n", ev->duration_ms);
            break;
        case EOS_GESTURE_SWIPE_RIGHT:
            printf("Swipe right (%d ms)\n", ev->duration_ms);
            break;
        case EOS_GESTURE_LONG_PRESS:
            printf("Long press at (%d, %d)\n", ev->start_x, ev->start_y);
            break;
        case EOS_GESTURE_PINCH_IN:
            printf("Pinch in\n");
            break;
        case EOS_GESTURE_PINCH_OUT:
            printf("Pinch out\n");
            break;
        default:
            break;
    }
}

int main(void)
{
    eos_hal_init();

    /* --- HAL: initialize display --- */
    eos_display_config_t disp_cfg = {
        .id         = 0,
        .width      = 480,
        .height     = 320,
        .color_mode = EOS_DISPLAY_COLOR_RGB565,
    };
    eos_display_init(&disp_cfg);

    /* --- HAL: initialize touch panel --- */
    eos_touch_config_t touch_cfg = {
        .id         = 0,
        .type       = EOS_TOUCH_CAPACITIVE,
        .width      = 480,
        .height     = 320,
        .max_points = 5,
        .i2c_port   = 0,
        .i2c_addr   = 0x38,   /* FT5x06 default */
    };
    eos_touch_init(&touch_cfg);

    /* --- UI: bring up LVGL --- */
    eos_ui_config_t ui_cfg = {
        .screen_width  = 480,
        .screen_height = 320,
        .color_depth   = 16,
        .display_id    = 0,
        .touch_id      = 0,
        .fb_mode       = EOS_UI_FB_FULL,
        .rotation      = EOS_UI_ROTATE_0,
        .dpi           = 130,
        .double_buffer = true,
    };

    if (eos_ui_init(&ui_cfg) != 0) {
        return -1;
    }

    /* --- UI: set dark theme --- */
    eos_ui_set_theme(EOS_UI_THEME_DARK);

    /* --- UI: register gesture callbacks --- */
    uint32_t gestures = EOS_GESTURE_TAP
                      | EOS_GESTURE_SWIPE_LEFT
                      | EOS_GESTURE_SWIPE_RIGHT
                      | EOS_GESTURE_LONG_PRESS
                      | EOS_GESTURE_PINCH_IN
                      | EOS_GESTURE_PINCH_OUT;

    eos_ui_on_gesture(gestures, on_gesture, NULL);

    /* --- UI: tune gesture thresholds (optional) --- */
    eos_gesture_config_t gcfg = {
        .swipe_min_distance = 60,    /* px */
        .long_press_time_ms = 500,   /* ms */
        .double_tap_time_ms = 250,   /* ms */
        .pinch_ratio_pct    = 25,    /* % distance change */
    };
    eos_ui_configure_gestures(&gcfg);

    /* --- Build a simple UI --- */
    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_center(btn);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "Press Me");
    lv_obj_center(lbl);

    /* --- Main loop --- */
    for (;;) {
        uint32_t sleep_ms = eos_ui_task_handler();
        eos_delay_ms(sleep_ms);
    }
}
```

---

## API Reference

### Configuration Types

```c
typedef struct {
    uint16_t           screen_width;
    uint16_t           screen_height;
    uint8_t            color_depth;       /* 1, 16, or 24 */
    uint8_t            display_id;        /* HAL display id   */
    uint8_t            touch_id;          /* HAL touch id     */
    eos_ui_fb_mode_t   fb_mode;           /* FULL, PARTIAL, or DIRECT */
    eos_ui_rotation_t  rotation;          /* 0°, 90°, 180°, 270° */
    uint16_t           dpi;               /* 0 = LVGL default */
    bool               double_buffer;     /* two framebuffers? */
} eos_ui_config_t;
```

| Field | Description |
|-------|-------------|
| `screen_width` / `screen_height` | Display resolution in pixels |
| `color_depth` | Bits per pixel: `1` (monochrome), `16` (RGB565), `24` (RGB888) |
| `display_id` | Must match the `id` passed to `eos_display_init()` |
| `touch_id` | Must match the `id` passed to `eos_touch_init()` (ignored if touch disabled) |
| `fb_mode` | `EOS_UI_FB_FULL` — full-screen buffer, best quality. `EOS_UI_FB_PARTIAL` — small band buffer, lowest RAM. `EOS_UI_FB_DIRECT` — no intermediate buffer (advanced). |
| `rotation` | Applies coordinate rotation to touch input |
| `double_buffer` | Allocate two framebuffers for tear-free rendering |

### Lifecycle Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_ui_init` | `int eos_ui_init(const eos_ui_config_t *cfg)` | Initialize LVGL, create display/input drivers, allocate framebuffer(s). Returns 0 on success. |
| `eos_ui_deinit` | `void eos_ui_deinit(void)` | Free all resources, call `lv_deinit()`. |
| `eos_ui_task_handler` | `uint32_t eos_ui_task_handler(void)` | Run one LVGL tick. Returns recommended sleep time (ms). |
| `eos_ui_get_display` | `lv_display_t *eos_ui_get_display(void)` | Get LVGL display object for widget creation. |
| `eos_ui_get_indev` | `lv_indev_t *eos_ui_get_indev(void)` | Get LVGL input device for event routing. |
| `eos_ui_set_theme` | `int eos_ui_set_theme(eos_ui_theme_t theme)` | Switch between `EOS_UI_THEME_LIGHT` and `EOS_UI_THEME_DARK`. |

### Gesture Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `eos_ui_on_gesture` | `int eos_ui_on_gesture(uint32_t types, eos_gesture_callback_t cb, void *ctx)` | Register a callback for an OR-ed mask of gesture types. |
| `eos_ui_configure_gestures` | `int eos_ui_configure_gestures(const eos_gesture_config_t *cfg)` | Override default thresholds for gesture detection. |

### Gesture Types

| Gesture | Constant | Description |
|---------|----------|-------------|
| Tap | `EOS_GESTURE_TAP` | Single brief touch |
| Double Tap | `EOS_GESTURE_DOUBLE_TAP` | Two taps within `double_tap_time_ms` |
| Long Press | `EOS_GESTURE_LONG_PRESS` | Hold for `long_press_time_ms` |
| Swipe Left | `EOS_GESTURE_SWIPE_LEFT` | Horizontal drag left ≥ `swipe_min_distance` |
| Swipe Right | `EOS_GESTURE_SWIPE_RIGHT` | Horizontal drag right ≥ `swipe_min_distance` |
| Swipe Up | `EOS_GESTURE_SWIPE_UP` | Vertical drag up ≥ `swipe_min_distance` |
| Swipe Down | `EOS_GESTURE_SWIPE_DOWN` | Vertical drag down ≥ `swipe_min_distance` |
| Pinch In | `EOS_GESTURE_PINCH_IN` | Two-finger pinch inward |
| Pinch Out | `EOS_GESTURE_PINCH_OUT` | Two-finger spread outward |

---

## Framebuffer Modes

| Mode | RAM Usage | Quality | Use Case |
|------|-----------|---------|----------|
| `EOS_UI_FB_FULL` | High (full screen × bpp) | Best — no tearing with double-buffer | Devices with ≥64 KB spare RAM |
| `EOS_UI_FB_PARTIAL` | Low (~1/10 of full) | Good — renders in bands | RAM-constrained MCUs |
| `EOS_UI_FB_DIRECT` | Zero | Depends on HAL | When HAL provides its own framebuffer |

---

## RTOS Integration

Instead of a bare-metal `for(;;)` loop, create a dedicated LVGL task:

```c
static void ui_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t sleep_ms = eos_ui_task_handler();
        eos_task_delay_ms(sleep_ms);
    }
}

int main(void)
{
    eos_hal_init();
    eos_kernel_init();

    /* ... display + touch init ... */

    eos_ui_config_t ui_cfg = { /* ... */ };
    eos_ui_init(&ui_cfg);

    eos_task_create("ui", ui_task, NULL, 5, 4096);
    eos_kernel_start();
}
```

---

## Build Integration

Link against `eos_ui` and provide LVGL:

```cmake
# CMakeLists.txt for your application
add_executable(my_app main.c)

target_link_libraries(my_app
    eos_hal
    eos_ui
    lvgl          # LVGL must be available in your build tree
)
```

If LVGL is already a target in your CMake build, `eos_ui` links it automatically.

---

## Source Files

| File | Purpose |
|------|---------|
| `services/ui/include/eos/ui.h` | Public API header |
| `services/ui/src/ui.c` | Core LVGL lifecycle, framebuffer management |
| `services/ui/src/ui_display_drv.c` | Display flush callback → HAL bridge |
| `services/ui/src/ui_touch_drv.c` | Touch read callback → HAL bridge, rotation |
| `services/ui/src/ui_gesture.c` | Gesture state machine (tap, swipe, pinch) |
