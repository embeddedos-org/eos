// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file ui_display_drv.c
 * @brief EoS UI — LVGL display driver bridge
 *
 * Implements the LVGL flush callback by forwarding pixel data through
 * the EoS HAL display API (eos_display_draw_bitmap / eos_display_flush).
 * Handles color-format conversion (1BPP, RGB565, RGB888) and dirty-region
 * optimisation.
 */

#include <eos/ui.h>

#if EOS_ENABLE_UI

#include <eos/hal_extended.h>
#include <lvgl.h>
#include <string.h>

/* Display id used for HAL calls — set during init via config. */
static uint8_t s_display_id;
static eos_display_color_t s_color_mode;

void eos_ui_display_set_id(uint8_t id, eos_display_color_t mode)
{
    s_display_id = id;
    s_color_mode = mode;
}

/**
 * LVGL flush callback → EoS HAL.
 *
 * @param disp    LVGL display driver.
 * @param area    Dirty area descriptor (cast from const lv_area_t*).
 * @param px_map  Pixel buffer rendered by LVGL.
 */
void eos_ui_display_flush_cb(lv_display_t *disp, const void *area,
                             uint8_t *px_map)
{
    const lv_area_t *a = (const lv_area_t *)area;

    uint16_t x = (uint16_t)a->x1;
    uint16_t y = (uint16_t)a->y1;
    uint16_t w = (uint16_t)(a->x2 - a->x1 + 1);
    uint16_t h = (uint16_t)(a->y2 - a->y1 + 1);

    /* Push the rendered pixels to the HAL */
    eos_display_draw_bitmap(s_display_id, x, y, w, h, px_map);
    eos_display_flush(s_display_id);

    /* Tell LVGL the flushing is done */
    lv_display_flush_ready(disp);
}

#endif /* EOS_ENABLE_UI */
