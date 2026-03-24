// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file driver_framework.c
 * @brief EoS Driver Framework — Registration, lookup, and lifecycle management
 */

#include <eos/drivers.h>
#include <string.h>

static eos_driver_t *driver_table[EOS_MAX_DRIVERS];
static uint32_t driver_count = 0;

int eos_driver_register(eos_driver_t *drv)
{
    if (!drv || !drv->name || !drv->ops) return -1;
    if (driver_count >= EOS_MAX_DRIVERS) return -2;

    /* Check for duplicate */
    for (uint32_t i = 0; i < driver_count; i++) {
        if (driver_table[i] && strcmp(driver_table[i]->name, drv->name) == 0) {
            return -3;
        }
    }

    drv->state = EOS_DRV_STATE_UNINIT;
    driver_table[driver_count++] = drv;
    return 0;
}

int eos_driver_unregister(const char *name)
{
    if (!name) return -1;

    for (uint32_t i = 0; i < driver_count; i++) {
        if (driver_table[i] && strcmp(driver_table[i]->name, name) == 0) {
            if (driver_table[i]->state == EOS_DRV_STATE_ACTIVE) {
                if (driver_table[i]->ops->close) {
                    driver_table[i]->ops->close(driver_table[i]);
                }
            }
            if (driver_table[i]->ops->deinit) {
                driver_table[i]->ops->deinit(driver_table[i]);
            }

            /* Shift remaining entries */
            for (uint32_t j = i; j < driver_count - 1; j++) {
                driver_table[j] = driver_table[j + 1];
            }
            driver_table[--driver_count] = NULL;
            return 0;
        }
    }
    return -1;
}

eos_driver_t *eos_driver_find(const char *name)
{
    if (!name) return NULL;

    for (uint32_t i = 0; i < driver_count; i++) {
        if (driver_table[i] && strcmp(driver_table[i]->name, name) == 0) {
            return driver_table[i];
        }
    }
    return NULL;
}

eos_driver_t *eos_driver_find_by_type(eos_driver_type_t type, uint8_t instance)
{
    for (uint32_t i = 0; i < driver_count; i++) {
        if (driver_table[i] &&
            driver_table[i]->type == type &&
            driver_table[i]->instance == instance) {
            return driver_table[i];
        }
    }
    return NULL;
}

int eos_driver_init_all(void)
{
    int errors = 0;
    for (uint32_t i = 0; i < driver_count; i++) {
        if (driver_table[i] && driver_table[i]->ops->init) {
            int rc = driver_table[i]->ops->init(driver_table[i]);
            if (rc == 0) {
                driver_table[i]->state = EOS_DRV_STATE_READY;
            } else {
                driver_table[i]->state = EOS_DRV_STATE_ERROR;
                errors++;
            }
        }
    }
    return errors;
}

void eos_driver_deinit_all(void)
{
    for (uint32_t i = 0; i < driver_count; i++) {
        if (driver_table[i]) {
            if (driver_table[i]->state == EOS_DRV_STATE_ACTIVE && driver_table[i]->ops->close) {
                driver_table[i]->ops->close(driver_table[i]);
            }
            if (driver_table[i]->ops->deinit) {
                driver_table[i]->ops->deinit(driver_table[i]);
            }
            driver_table[i]->state = EOS_DRV_STATE_UNINIT;
        }
    }
}

int eos_driver_open(eos_driver_t *drv)
{
    if (!drv || !drv->ops) return -1;
    if (drv->state != EOS_DRV_STATE_READY) return -2;
    if (!drv->ops->open) return -1;

    int rc = drv->ops->open(drv);
    if (rc == 0) drv->state = EOS_DRV_STATE_ACTIVE;
    return rc;
}

int eos_driver_close(eos_driver_t *drv)
{
    if (!drv || !drv->ops) return -1;
    if (drv->state != EOS_DRV_STATE_ACTIVE) return -2;
    if (!drv->ops->close) return -1;

    int rc = drv->ops->close(drv);
    if (rc == 0) drv->state = EOS_DRV_STATE_READY;
    return rc;
}

int eos_driver_read(eos_driver_t *drv, void *buf, size_t len)
{
    if (!drv || !drv->ops || !drv->ops->read) return -1;
    if (drv->state != EOS_DRV_STATE_ACTIVE) return -2;
    return drv->ops->read(drv, buf, len);
}

int eos_driver_write(eos_driver_t *drv, const void *buf, size_t len)
{
    if (!drv || !drv->ops || !drv->ops->write) return -1;
    if (drv->state != EOS_DRV_STATE_ACTIVE) return -2;
    return drv->ops->write(drv, buf, len);
}

int eos_driver_ioctl(eos_driver_t *drv, uint32_t cmd, void *arg)
{
    if (!drv || !drv->ops || !drv->ops->ioctl) return -1;
    return drv->ops->ioctl(drv, cmd, arg);
}

int eos_driver_suspend_all(void)
{
    int errors = 0;
    for (uint32_t i = 0; i < driver_count; i++) {
        if (driver_table[i] &&
            driver_table[i]->state == EOS_DRV_STATE_ACTIVE &&
            driver_table[i]->ops->suspend) {
            int rc = driver_table[i]->ops->suspend(driver_table[i]);
            if (rc == 0) {
                driver_table[i]->state = EOS_DRV_STATE_SUSPEND;
            } else {
                errors++;
            }
        }
    }
    return errors;
}

int eos_driver_resume_all(void)
{
    int errors = 0;
    for (uint32_t i = 0; i < driver_count; i++) {
        if (driver_table[i] &&
            driver_table[i]->state == EOS_DRV_STATE_SUSPEND &&
            driver_table[i]->ops->resume) {
            int rc = driver_table[i]->ops->resume(driver_table[i]);
            if (rc == 0) {
                driver_table[i]->state = EOS_DRV_STATE_ACTIVE;
            } else {
                errors++;
            }
        }
    }
    return errors;
}

uint32_t eos_driver_count(void)
{
    return driver_count;
}

void eos_driver_list(eos_driver_t **out, uint32_t max, uint32_t *count)
{
    if (!out || !count) return;

    uint32_t n = (driver_count < max) ? driver_count : max;
    for (uint32_t i = 0; i < n; i++) {
        out[i] = driver_table[i];
    }
    *count = n;
}
