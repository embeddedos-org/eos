// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file ui.c
 * @brief EoS UI Service — Core LVGL lifecycle management
 *
 * Initialises LVGL, creates the display and input-device drivers,
 * manages framebuffer allocation, and exposes the tick / task-handler
 * wrapper that the application calls from its main loop or RTOS task.
 */

#include <eos/ui.h>

#if EOS_ENABLE_UI

#include <eos/hal_extended.h>
#include <lvgl.h>
#include <stdlib.h>
#include <string.h>

/* ---- internal state --------------------------------------------------- */

static struct {
    bool           initialised;
    eos_ui_config_t cfg;
    lv_display_t  *display;
    lv_indev_t    *indev;
    uint8_t       *fb1;
    uint8_t       *fb2;
    uint32_t       fb_size;
    uint32_t       last_tick_ms;
} s_ui;

/* ---- helpers ---------------------------------------------------------- */

static uint32_t fb_bytes(const eos_ui_config_t *c)
{
    uint32_t pixels = (uint32_t)c->screen_width * c->screen_height;
    switch (c->color_depth) {
        case 1:  return (pixels + 7) / 8;
        case 16: return pixels * 2;
        case 24: return pixels * 4;        /* LVGL uses 32-bit for RGB888 */
        default: return pixels * 2;
    }
}

/* ---- public API ------------------------------------------------------- */

int eos_ui_init(const eos_ui_config_t *cfg)
{
    if (!cfg || s_ui.initialised)
        return -1;

    memset(&s_ui, 0, sizeof(s_ui));
    s_ui.cfg = *cfg;

    /* 1. Initialise LVGL */
    lv_init();

    /* 2. Allocate framebuffer(s) */
    if (cfg->fb_mode != EOS_UI_FB_DIRECT) {
        s_ui.fb_size = (cfg->fb_mode == EOS_UI_FB_PARTIAL)
                        ? fb_bytes(cfg) / 10   /* ~10 lines */
                        : fb_bytes(cfg);
        s_ui.fb1 = (uint8_t *)malloc(s_ui.fb_size);
        if (!s_ui.fb1) return -2;

        if (cfg->double_buffer) {
            s_ui.fb2 = (uint8_t *)malloc(s_ui.fb_size);
            if (!s_ui.fb2) { free(s_ui.fb1); return -2; }
        }
    }

    /* 3. Create LVGL display driver */
    s_ui.display = lv_display_create(cfg->screen_width, cfg->screen_height);
    if (!s_ui.display) {
        free(s_ui.fb1); free(s_ui.fb2);
        return -3;
    }

    lv_display_set_flush_cb(s_ui.display,
        (lv_display_flush_cb_t)eos_ui_display_flush_cb);

    if (s_ui.fb1) {
        lv_display_set_buffers(s_ui.display, s_ui.fb1, s_ui.fb2,
                               s_ui.fb_size,
                               cfg->fb_mode == EOS_UI_FB_FULL
                                   ? LV_DISPLAY_RENDER_MODE_FULL
                                   : LV_DISPLAY_RENDER_MODE_PARTIAL);
    }

    if (cfg->dpi > 0)
        lv_display_set_dpi(s_ui.display, cfg->dpi);

    /* 4. Create LVGL input device (touch) */
#if EOS_ENABLE_TOUCH
    s_ui.indev = lv_indev_create();
    if (s_ui.indev) {
        lv_indev_set_type(s_ui.indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(s_ui.indev,
            (lv_indev_read_cb_t)eos_ui_touch_read_cb);
    }
#endif

    s_ui.initialised = true;
    return 0;
}

void eos_ui_deinit(void)
{
    if (!s_ui.initialised) return;

    if (s_ui.indev)   lv_indev_delete(s_ui.indev);
    if (s_ui.display) lv_display_delete(s_ui.display);

    free(s_ui.fb1);
    free(s_ui.fb2);

    lv_deinit();
    memset(&s_ui, 0, sizeof(s_ui));
}

uint32_t eos_ui_task_handler(void)
{
    if (!s_ui.initialised) return 100;

    uint32_t now = lv_tick_get();
    uint32_t elapsed = now - s_ui.last_tick_ms;
    s_ui.last_tick_ms = now;
    (void)elapsed;

    return lv_timer_handler();
}

lv_display_t *eos_ui_get_display(void)
{
    return s_ui.display;
}

lv_indev_t *eos_ui_get_indev(void)
{
    return s_ui.indev;
}

int eos_ui_set_theme(eos_ui_theme_t theme)
{
    if (!s_ui.initialised) return -1;
    (void)theme;
    /* Theme application is LVGL-version-dependent.
       Placeholder — integrate lv_theme_default_init() here. */
    return 0;
}

#endif /* EOS_ENABLE_UI */
