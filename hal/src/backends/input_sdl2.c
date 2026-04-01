// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file input_sdl2.c
 * @brief SDL2 mouse/touch input backend for EoS HAL
 *
 * Converts SDL2 mouse and finger events into eos_touch_point_t.
 */

#ifdef EOS_BACKEND_SDL2

#include <eos/hal_extended.h>
#include <SDL2/SDL.h>
#include <string.h>

#define EOS_SDL2_MAX_TOUCH_DEVS 2

typedef struct {
    uint16_t width;
    uint16_t height;
    bool     mouse_pressed;
    uint16_t mouse_x;
    uint16_t mouse_y;
    bool     active;
} sdl2_touch_t;

static sdl2_touch_t s_touch_devs[EOS_SDL2_MAX_TOUCH_DEVS];

int sdl2_touch_init(const eos_touch_config_t *cfg)
{
    if (!cfg || cfg->id >= EOS_SDL2_MAX_TOUCH_DEVS) return -1;
    sdl2_touch_t *t = &s_touch_devs[cfg->id];
    if (t->active) return -1;

    t->width  = cfg->width;
    t->height = cfg->height;
    t->mouse_pressed = false;
    t->mouse_x = 0;
    t->mouse_y = 0;
    t->active = true;
    return 0;
}

void sdl2_touch_deinit(uint8_t id)
{
    if (id >= EOS_SDL2_MAX_TOUCH_DEVS) return;
    s_touch_devs[id].active = false;
}

int sdl2_touch_read(uint8_t id, eos_touch_point_t *points,
                     uint8_t max_points, uint8_t *count)
{
    if (id >= EOS_SDL2_MAX_TOUCH_DEVS || !s_touch_devs[id].active ||
        !points || !count)
        return -1;

    sdl2_touch_t *t = &s_touch_devs[id];

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_MOUSEBUTTONDOWN:
            t->mouse_pressed = true;
            t->mouse_x = (uint16_t)ev.button.x;
            t->mouse_y = (uint16_t)ev.button.y;
            break;
        case SDL_MOUSEBUTTONUP:
            t->mouse_pressed = false;
            t->mouse_x = (uint16_t)ev.button.x;
            t->mouse_y = (uint16_t)ev.button.y;
            break;
        case SDL_MOUSEMOTION:
            t->mouse_x = (uint16_t)ev.motion.x;
            t->mouse_y = (uint16_t)ev.motion.y;
            break;
        case SDL_FINGERDOWN:
        case SDL_FINGERMOTION:
            t->mouse_pressed = true;
            t->mouse_x = (uint16_t)(ev.tfinger.x * t->width);
            t->mouse_y = (uint16_t)(ev.tfinger.y * t->height);
            break;
        case SDL_FINGERUP:
            t->mouse_pressed = false;
            break;
        default:
            break;
        }
    }

    *count = 0;
    if (max_points > 0) {
        points[0].id       = 0;
        points[0].x        = t->mouse_x;
        points[0].y        = t->mouse_y;
        points[0].pressed  = t->mouse_pressed;
        points[0].pressure = t->mouse_pressed ? 255 : 0;
        *count = 1;
    }
    return 0;
}

int sdl2_touch_set_callback(uint8_t id, eos_touch_callback_t cb, void *ctx)
{
    (void)id; (void)cb; (void)ctx;
    return -1;
}

int sdl2_touch_calibrate(uint8_t id)
{
    (void)id;
    return 0;
}

#endif /* EOS_BACKEND_SDL2 */
