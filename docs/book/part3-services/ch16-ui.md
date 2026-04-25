# Chapter 16: UI Framework

**Author:** Srikanth Patchava & EmbeddedOS Contributors

---

## 16.1 Overview

The EoS UI service (eos/ui.h) provides an integration layer between the
LVGL graphics library and EoS HAL display/touch drivers. It handles
framebuffer management, input device routing, gesture recognition, and
theme switching -- all gated by the EOS_ENABLE_UI configuration flag.

```
+----------------------------------------------------------+
|                    Application                           |
|            LVGL Widgets and Screens                      |
+----------------------------------------------------------+
|                   EoS UI Service                         |
|   +----------+----------+-----------+----------+        |
|   | Display  |  Touch   | Gesture   |  Theme   |        |
|   | Driver   |  Driver  | Engine    |  Manager |        |
|   +----------+----------+-----------+----------+        |
+----------------------------------------------------------+
|                      LVGL Core                           |
+----------------------------------------------------------+
|                   EoS HAL Drivers                        |
|          Display (SPI/I2C/RGB)  Touch (I2C/SPI)          |
+----------------------------------------------------------+
```

## 16.2 Configuration

### 16.2.1 Framebuffer Modes

| Mode              | Enum               | Description                       |
|-------------------|---------------------|-----------------------------------|
| Full              | EOS_UI_FB_FULL      | Full-screen buffer (one or two)   |
| Partial           | EOS_UI_FB_PARTIAL   | Band/stripe rendering             |
| Direct            | EOS_UI_FB_DIRECT    | No intermediate buffer            |

### 16.2.2 Themes

| Theme             | Enum                | Description                   |
|-------------------|---------------------|-------------------------------|
| Light             | EOS_UI_THEME_LIGHT  | Default light color scheme    |
| Dark              | EOS_UI_THEME_DARK   | Dark mode color scheme        |

### 16.2.3 Display Rotation

| Rotation          | Enum                | Degrees                       |
|-------------------|---------------------|-------------------------------|
| Normal            | EOS_UI_ROTATE_0     | 0 degrees                     |
| Landscape         | EOS_UI_ROTATE_90    | 90 degrees CW                 |
| Inverted          | EOS_UI_ROTATE_180   | 180 degrees                   |
| Landscape Inv.    | EOS_UI_ROTATE_270   | 270 degrees CW                |

### 16.2.4 Configuration Structure

```c
typedef struct {
    uint16_t           screen_width;     /* Display width in px     */
    uint16_t           screen_height;    /* Display height in px    */
    uint8_t            color_depth;      /* 1, 16, or 24 bits       */
    uint8_t            display_id;       /* HAL display device ID   */
    uint8_t            touch_id;         /* HAL touch device ID     */
    eos_ui_fb_mode_t   fb_mode;          /* Framebuffer strategy    */
    eos_ui_rotation_t  rotation;         /* Screen rotation         */
    uint16_t           dpi;              /* Dots per inch (0=auto)  */
    bool               double_buffer;    /* Use two framebuffers?   */
} eos_ui_config_t;
```

## 16.3 Initialization

### 16.3.1 Basic Setup

```c
#include <eos/ui.h>

void ui_setup(void)
{
    eos_ui_config_t cfg = {
        .screen_width   = 320,
        .screen_height  = 240,
        .color_depth    = 16,         /* RGB565                */
        .display_id     = 0,          /* Primary display       */
        .touch_id       = 0,          /* Primary touch panel   */
        .fb_mode        = EOS_UI_FB_FULL,
        .rotation       = EOS_UI_ROTATE_0,
        .dpi            = 0,          /* LVGL default          */
        .double_buffer  = true,       /* Smooth rendering      */
    };

    int rc = eos_ui_init(&cfg);
    if (rc != 0) {
        printf("UI init failed: %d\n", rc);
        return;
    }

    /* Set dark theme */
    eos_ui_set_theme(EOS_UI_THEME_DARK);
}
```

### 16.3.2 Main Loop Integration

```c
void ui_main_loop(void)
{
    while (1) {
        uint32_t sleep_ms = eos_ui_task_handler();
        eos_task_delay_ms(sleep_ms);
    }
}
```

### 16.3.3 RTOS Task Integration

```c
void ui_task(void *arg)
{
    eos_ui_config_t cfg = { /* ... */ };
    eos_ui_init(&cfg);

    /* Create LVGL UI elements here */
    create_main_screen();

    while (1) {
        uint32_t sleep_ms = eos_ui_task_handler();
        eos_task_delay_ms(sleep_ms > 0 ? sleep_ms : 5);
    }
}
```

## 16.4 Display and Input Accessors

After initialization, you can access the underlying LVGL objects:

```c
/* Get LVGL display object for advanced configuration */
lv_display_t *disp = eos_ui_get_display();

/* Get LVGL input device for cursor configuration */
lv_indev_t *indev = eos_ui_get_indev();
```

## 16.5 Theme Switching

```c
/* Switch to dark mode at runtime */
eos_ui_set_theme(EOS_UI_THEME_DARK);

/* Switch back to light mode */
eos_ui_set_theme(EOS_UI_THEME_LIGHT);
```

## 16.6 Gesture Recognition

The gesture engine processes raw touch events and recognizes common
gestures. It supports nine gesture types as a bitmask:

### 16.6.1 Gesture Types

| Gesture              | Bitmask    | Description              |
|----------------------|------------|--------------------------|
| EOS_GESTURE_NONE     | 0x000      | No gesture               |
| EOS_GESTURE_TAP      | 0x001      | Single tap               |
| EOS_GESTURE_DOUBLE_TAP | 0x002   | Double tap               |
| EOS_GESTURE_LONG_PRESS | 0x004   | Long press and hold      |
| EOS_GESTURE_SWIPE_LEFT | 0x008   | Swipe left               |
| EOS_GESTURE_SWIPE_RIGHT| 0x010   | Swipe right              |
| EOS_GESTURE_SWIPE_UP   | 0x020   | Swipe up                 |
| EOS_GESTURE_SWIPE_DOWN | 0x040   | Swipe down               |
| EOS_GESTURE_PINCH_IN   | 0x080   | Pinch inward (zoom out)  |
| EOS_GESTURE_PINCH_OUT  | 0x100   | Pinch outward (zoom in)  |

### 16.6.2 Gesture Event Structure

```c
typedef struct {
    eos_gesture_type_t type;       /* Which gesture detected    */
    int16_t            start_x;    /* Touch start X coordinate  */
    int16_t            start_y;    /* Touch start Y coordinate  */
    int16_t            end_x;      /* Touch end X coordinate    */
    int16_t            end_y;      /* Touch end Y coordinate    */
    uint32_t           duration_ms;/* Gesture duration          */
} eos_gesture_event_t;
```

### 16.6.3 Registering Gesture Callbacks

```c
void on_swipe(const eos_gesture_event_t *event, void *ctx)
{
    switch (event->type) {
    case EOS_GESTURE_SWIPE_LEFT:
        printf("Swipe left: (%d,%d)->(%d,%d) %ums\n",
               event->start_x, event->start_y,
               event->end_x, event->end_y,
               event->duration_ms);
        navigate_next_screen();
        break;
    case EOS_GESTURE_SWIPE_RIGHT:
        navigate_prev_screen();
        break;
    default:
        break;
    }
}

void on_tap(const eos_gesture_event_t *event, void *ctx)
{
    if (event->type == EOS_GESTURE_LONG_PRESS) {
        show_context_menu(event->start_x, event->start_y);
    }
}

void setup_gestures(void)
{
    /* Register swipe callbacks */
    eos_ui_on_gesture(
        EOS_GESTURE_SWIPE_LEFT | EOS_GESTURE_SWIPE_RIGHT,
        on_swipe, NULL
    );

    /* Register tap and long-press */
    eos_ui_on_gesture(
        EOS_GESTURE_TAP | EOS_GESTURE_LONG_PRESS,
        on_tap, NULL
    );
}
```

### 16.6.4 Gesture Configuration

Fine-tune gesture detection thresholds:

```c
typedef struct {
    uint16_t swipe_min_distance;    /* px: min drag for swipe      */
    uint32_t long_press_time_ms;    /* ms: hold time for long-press */
    uint32_t double_tap_time_ms;    /* ms: max interval for double  */
    uint16_t pinch_ratio_pct;       /* %: distance change for pinch */
} eos_gesture_config_t;
```

```c
eos_gesture_config_t gcfg = {
    .swipe_min_distance  = 50,     /* 50 px minimum swipe     */
    .long_press_time_ms  = 800,    /* 800 ms long press       */
    .double_tap_time_ms  = 300,    /* 300 ms double-tap gap   */
    .pinch_ratio_pct     = 20,     /* 20% distance change     */
};
eos_ui_configure_gestures(&gcfg);
```

## 16.7 Display Rendering Pipeline

```
  Touch Input            LVGL Timer
      |                      |
      v                      v
+-------------+    +------------------+
| touch_read  |    | eos_ui_task_     |
| _cb()       |    | handler()        |
+------+------+    +--------+---------+
       |                    |
       v                    v
  +----------+     +----------------+
  | Gesture  |     | LVGL Render    |
  | Engine   |     | Pipeline       |
  +----------+     +--------+-------+
                            |
                            v
                   +----------------+
                   | display_flush  |
                   | _cb()          |
                   +--------+-------+
                            |
                            v
                   +----------------+
                   | HAL Display    |
                   | DMA Transfer   |
                   +----------------+
```

## 16.8 Internal Driver Callbacks

These functions are called internally by the display and touch drivers.
Application code should not call them directly:

```c
/* Called by LVGL when a display area needs flushing */
void eos_ui_display_flush_cb(lv_display_t *disp,
                             const void *area,
                             uint8_t *px_map);

/* Called by LVGL to poll touch input */
void eos_ui_touch_read_cb(lv_indev_t *indev, void *data);
```

## 16.9 LVGL Widget Examples

Once the EoS UI layer is initialized, use standard LVGL APIs to create
widgets. The EoS layer manages the display driver -- you focus on UI:

### 16.9.1 Button with Label

```c
void create_button(void)
{
    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn, 120, 50);
    lv_obj_center(btn);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Press Me");
    lv_obj_center(label);

    lv_obj_add_event_cb(btn, btn_event_handler,
                        LV_EVENT_CLICKED, NULL);
}

void btn_event_handler(lv_event_t *e)
{
    printf("Button clicked!\n");
}
```

### 16.9.2 Sensor Dashboard

```c
void create_dashboard(void)
{
    /* Temperature gauge */
    lv_obj_t *meter = lv_meter_create(lv_screen_active());
    lv_obj_set_size(meter, 200, 200);
    lv_obj_center(meter);

    lv_meter_scale_t *scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_range(meter, scale, 0, 100, 270, 135);

    lv_meter_indicator_t *indic =
        lv_meter_add_needle_line(meter, scale, 4,
                                 lv_palette_main(LV_PALETTE_RED), -10);

    /* Update periodically */
    lv_meter_set_indicator_value(meter, indic, 42);
}
```

## 16.10 Memory Considerations

| Configuration        | RAM Usage (320x240 16-bit)        |
|----------------------|-----------------------------------|
| Single buffer, full  | ~150 KB (1 framebuffer)           |
| Double buffer, full  | ~300 KB (2 framebuffers)          |
| Partial (10 lines)   | ~6.4 KB per buffer                |
| Direct mode          | 0 KB (renders to display RAM)     |

### 16.10.1 Choosing Framebuffer Mode

```
  Available RAM?
       |
  +----+----+
  |         |
 <64KB    >256KB
  |         |
  v         v
Partial   Full + Double Buffer
or Direct   (smoothest)
```

## 16.11 Complete Application Example

```c
#include <eos/ui.h>
#include <eos/sensor.h>
#include <lvgl.h>

static lv_obj_t *temp_label;
static lv_obj_t *hum_label;

void create_main_screen(void)
{
    lv_obj_t *scr = lv_screen_active();

    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "EoS Sensor Monitor");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    /* Temperature display */
    temp_label = lv_label_create(scr);
    lv_label_set_text(temp_label, "Temp: --.- C");
    lv_obj_align(temp_label, LV_ALIGN_CENTER, 0, -20);

    /* Humidity display */
    hum_label = lv_label_create(scr);
    lv_label_set_text(hum_label, "Hum:  --.-%");
    lv_obj_align(hum_label, LV_ALIGN_CENTER, 0, 20);
}

void update_display(float temp, float hum)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "Temp: %.1f C", temp);
    lv_label_set_text(temp_label, buf);

    snprintf(buf, sizeof(buf), "Hum:  %.1f%%", hum);
    lv_label_set_text(hum_label, buf);
}

void app_main(void)
{
    /* Initialize UI */
    eos_ui_config_t cfg = {
        .screen_width  = 320,
        .screen_height = 240,
        .color_depth   = 16,
        .display_id    = 0,
        .touch_id      = 0,
        .fb_mode       = EOS_UI_FB_FULL,
        .rotation      = EOS_UI_ROTATE_0,
        .double_buffer = true,
    };
    eos_ui_init(&cfg);
    eos_ui_set_theme(EOS_UI_THEME_DARK);

    /* Set up gesture navigation */
    eos_ui_on_gesture(
        EOS_GESTURE_SWIPE_LEFT | EOS_GESTURE_SWIPE_RIGHT,
        on_swipe, NULL
    );

    /* Create UI */
    create_main_screen();

    /* Main loop */
    while (1) {
        eos_sensor_reading_t temp, hum;
        eos_sensor_read_filtered(0, &temp);
        eos_sensor_read_filtered(1, &hum);
        update_display(temp.value, hum.value);

        eos_ui_task_handler();
        eos_task_delay_ms(33);   /* ~30 FPS */
    }
}
```

## 16.12 API Reference Summary

| Function                     | Description                          |
|------------------------------|--------------------------------------|
| eos_ui_init                  | Initialize LVGL + display + touch    |
| eos_ui_deinit                | Tear down UI layer                   |
| eos_ui_task_handler          | Run one LVGL timer iteration         |
| eos_ui_get_display           | Get LVGL display object              |
| eos_ui_get_indev             | Get LVGL input device object         |
| eos_ui_set_theme             | Switch light/dark theme              |
| eos_ui_on_gesture            | Register gesture callback            |
| eos_ui_configure_gestures    | Set gesture thresholds               |
| eos_ui_display_flush_cb      | Internal: display flush callback     |
| eos_ui_touch_read_cb         | Internal: touch read callback        |

---

*End of Part III: Services*
