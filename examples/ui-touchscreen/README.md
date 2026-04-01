# UI Touchscreen — Interactive LVGL Demo

A complete touchscreen application that builds an interactive interface with LVGL through the EoS UI service. Demonstrates display + touch HAL initialization, gesture recognition, theme switching, and RTOS-based rendering.

## What it demonstrates

- **Display HAL** — 480×320 RGB565 display initialization via `eos_display_init()`
- **Touch HAL** — capacitive multi-touch via `eos_touch_init()`
- **EoS UI init** — one-call LVGL setup with `eos_ui_init()` (framebuffer, display driver, input driver)
- **LVGL widgets** — button, label, slider, switch built with standard LVGL API
- **Gesture recognition** — tap, double-tap, long-press, 4-direction swipe, pinch in/out
- **Theme switching** — light ↔ dark via `eos_ui_set_theme()`
- **Brightness control** — slider wired to `eos_display_set_brightness()`
- **RTOS integration** — dedicated UI task calling `eos_ui_task_handler()`

## Screen layout

```
┌────────────────────────────────────────────────┐
│              🏠  EoS UI Demo                   │
│         📍 Swipe Right  (240, 160)  320 ms     │
│                                                │
│   ┌──────────────┐        🖼  Brightness       │
│   │  Count: 42   │        ════════●══          │
│   └──────────────┘                             │
│                                                │
│            Dark mode  [🔘]                     │
│      Swipe / pinch / long-press anywhere       │
└────────────────────────────────────────────────┘
```

## Modules used

| Module | Header | Functions |
|--------|--------|-----------|
| HAL Core | `eos/hal.h` | `eos_hal_init`, `eos_gpio_init`, `eos_gpio_toggle` |
| HAL Display | `eos/hal_extended.h` | `eos_display_init`, `eos_display_set_brightness` |
| HAL Touch | `eos/hal_extended.h` | `eos_touch_init` |
| UI Service | `eos/ui.h` | `eos_ui_init`, `eos_ui_task_handler`, `eos_ui_set_theme`, `eos_ui_on_gesture`, `eos_ui_configure_gestures` |
| Kernel | `eos/kernel.h` | `eos_kernel_init`, `eos_kernel_start`, `eos_task_create` |
| LVGL | `lvgl.h` | `lv_label_create`, `lv_button_create`, `lv_slider_create`, `lv_switch_create` |

## Hardware

- **MCU:** STM32 with LTDC or SPI display (or any MCU with display + I2C touch)
- **Display:** 480×320 RGB565 (ILI9488, ST7796, or similar)
- **Touch:** I2C capacitive panel (FT5x06, FT6236, GT911, CST816) on I2C port 0, address 0x38
- **LED:** Activity indicator on pin 2

## How to build

```bash
# Cross-compile for ARM target
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=../../toolchains/arm-none-eabi.cmake \
  -DEOS_PRODUCT=hmi
cmake --build build

# Host build (simulated peripherals — stubs return -1 but code runs)
cmake -B build
cmake --build build
./build/ui-touchscreen
```

> **Note:** LVGL must be available in your CMake build tree. If it's not pulled in by the EoS build, add it manually:
> ```bash
> cmake -B build -DLVGL_DIR=/path/to/lvgl
> ```

## Expected output

```
[ui-demo] Starting...
[ui-demo] UI built, launching task
[ui-demo] UI task running
[ui-demo] Button pressed, count=1
[ui-demo] Button pressed, count=2
[ui-demo] Brightness: 65%
[ui-demo] Gesture: Swipe Right  (50,160)->(430,155) 320 ms
[ui-demo] Theme: dark
[ui-demo] Gesture: Long-Press  (240,160)->(240,160) 502 ms
[ui-demo] Gesture: Pinch Out  (200,150)->(280,170) 410 ms
```

## Key code walkthrough

### 1. Initialize display and touch through HAL

```c
eos_display_config_t disp_cfg = {
    .id = 0, .width = 480, .height = 320,
    .color_mode = EOS_DISPLAY_COLOR_RGB565,
};
eos_display_init(&disp_cfg);

eos_touch_config_t touch_cfg = {
    .id = 0, .type = EOS_TOUCH_CAPACITIVE,
    .width = 480, .height = 320, .max_points = 5,
    .i2c_port = 0, .i2c_addr = 0x38,
};
eos_touch_init(&touch_cfg);
```

### 2. Bring up LVGL with one call

```c
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
eos_ui_init(&ui_cfg);
```

### 3. Register gesture callbacks

```c
eos_ui_on_gesture(EOS_GESTURE_TAP | EOS_GESTURE_SWIPE_LEFT | ...,
                  on_gesture, NULL);
```

### 4. Build widgets with standard LVGL

```c
lv_obj_t *btn = lv_button_create(lv_screen_active());
lv_obj_add_event_cb(btn, btn_counter_cb, LV_EVENT_CLICKED, NULL);
```

### 5. Run the UI from an RTOS task

```c
static void ui_task(void *arg) {
    for (;;) {
        uint32_t sleep_ms = eos_ui_task_handler();
        eos_task_delay_ms(sleep_ms);
    }
}
eos_task_create("ui", ui_task, NULL, 5, 4096);
```
