// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file lvgl.h
 * @brief Mock LVGL header for unit testing
 *
 * Provides only the include guard so that #include <lvgl.h> inside
 * ui_gesture.c resolves without the real LVGL library.  The test
 * harness defines lv_tick_get() as a static mock before including
 * the source under test.
 */

#ifndef LVGL_H
#define LVGL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#endif /* LVGL_H */
