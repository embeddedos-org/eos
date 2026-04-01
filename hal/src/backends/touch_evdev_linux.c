// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file touch_evdev_linux.c
 * @brief Linux evdev multitouch input backend for EoS HAL
 *
 * Reads touch/mouse events from /dev/input/eventN using Linux input subsystem.
 * Supports multitouch protocol B (ABS_MT_SLOT, ABS_MT_TRACKING_ID).
 */

#ifdef __linux__

#include <eos/hal_extended.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <linux/input.h>

#define EOS_EVDEV_MAX_SLOTS     10
#define EOS_EVDEV_MAX_DEVICES   4

typedef struct {
    int      fd;
    uint16_t abs_x_max;
    uint16_t abs_y_max;
    uint16_t width;
    uint16_t height;
    int      current_slot;
    struct {
        int16_t  x;
        int16_t  y;
        int32_t  tracking_id;
        bool     active;
    } slots[EOS_EVDEV_MAX_SLOTS];
    bool     active;
} evdev_touch_t;

static evdev_touch_t s_touch_devs[EOS_EVDEV_MAX_DEVICES];

static int evdev_find_touch_device(void)
{
    char path[64];
    for (int i = 0; i < 16; i++) {
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        struct input_absinfo abs_x;
        if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &abs_x) == 0) {
            return fd;
        }
        struct input_absinfo abs_btn;
        if (ioctl(fd, EVIOCGABS(ABS_X), &abs_btn) == 0) {
            return fd;
        }
        close(fd);
    }
    return -1;
}

int evdev_touch_init(const eos_touch_config_t *cfg)
{
    if (!cfg || cfg->id >= EOS_EVDEV_MAX_DEVICES) return -1;

    evdev_touch_t *t = &s_touch_devs[cfg->id];
    if (t->active) return -1;

    t->fd = evdev_find_touch_device();
    if (t->fd < 0) return -1;

    struct input_absinfo abs_x, abs_y;
    if (ioctl(t->fd, EVIOCGABS(ABS_MT_POSITION_X), &abs_x) == 0 &&
        ioctl(t->fd, EVIOCGABS(ABS_MT_POSITION_Y), &abs_y) == 0) {
        t->abs_x_max = (uint16_t)abs_x.maximum;
        t->abs_y_max = (uint16_t)abs_y.maximum;
    } else if (ioctl(t->fd, EVIOCGABS(ABS_X), &abs_x) == 0 &&
               ioctl(t->fd, EVIOCGABS(ABS_Y), &abs_y) == 0) {
        t->abs_x_max = (uint16_t)abs_x.maximum;
        t->abs_y_max = (uint16_t)abs_y.maximum;
    } else {
        t->abs_x_max = cfg->width;
        t->abs_y_max = cfg->height;
    }

    t->width  = cfg->width;
    t->height = cfg->height;
    t->current_slot = 0;
    memset(t->slots, 0, sizeof(t->slots));
    for (int i = 0; i < EOS_EVDEV_MAX_SLOTS; i++)
        t->slots[i].tracking_id = -1;

    t->active = true;
    return 0;
}

void evdev_touch_deinit(uint8_t id)
{
    if (id >= EOS_EVDEV_MAX_DEVICES) return;
    evdev_touch_t *t = &s_touch_devs[id];
    if (!t->active) return;

    close(t->fd);
    t->active = false;
}

int evdev_touch_read(uint8_t id, eos_touch_point_t *points,
                     uint8_t max_points, uint8_t *count)
{
    if (id >= EOS_EVDEV_MAX_DEVICES || !s_touch_devs[id].active ||
        !points || !count)
        return -1;

    evdev_touch_t *t = &s_touch_devs[id];
    struct input_event ev;

    while (read(t->fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        if (ev.type == EV_ABS) {
            int slot = t->current_slot;
            if (slot < 0 || slot >= EOS_EVDEV_MAX_SLOTS) continue;

            switch (ev.code) {
            case ABS_MT_SLOT:
                t->current_slot = ev.value;
                break;
            case ABS_MT_TRACKING_ID:
                t->slots[slot].tracking_id = ev.value;
                t->slots[slot].active = (ev.value >= 0);
                break;
            case ABS_MT_POSITION_X:
            case ABS_X:
                if (t->abs_x_max > 0)
                    t->slots[slot].x = (int16_t)((ev.value * t->width) /
                                                  t->abs_x_max);
                else
                    t->slots[slot].x = (int16_t)ev.value;
                break;
            case ABS_MT_POSITION_Y:
            case ABS_Y:
                if (t->abs_y_max > 0)
                    t->slots[slot].y = (int16_t)((ev.value * t->height) /
                                                  t->abs_y_max);
                else
                    t->slots[slot].y = (int16_t)ev.value;
                break;
            }
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            t->slots[0].active = (ev.value != 0);
        }
    }

    *count = 0;
    for (int i = 0; i < EOS_EVDEV_MAX_SLOTS && *count < max_points; i++) {
        if (t->slots[i].active) {
            points[*count].id      = (uint8_t)i;
            points[*count].x       = (uint16_t)t->slots[i].x;
            points[*count].y       = (uint16_t)t->slots[i].y;
            points[*count].pressed = true;
            points[*count].pressure = 255;
            (*count)++;
        }
    }
    return 0;
}

int evdev_touch_set_callback(uint8_t id, eos_touch_callback_t cb, void *ctx)
{
    (void)id; (void)cb; (void)ctx;
    return -1;
}

int evdev_touch_calibrate(uint8_t id)
{
    (void)id;
    return 0;
}

#endif /* __linux__ */
