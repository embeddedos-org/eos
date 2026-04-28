// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file boot_params.h
 * @brief Boot parameters passed from eBoot to eos kernel
 */

#ifndef EOS_BOOT_PARAMS_H
#define EOS_BOOT_PARAMS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EOS_BOOT_MAGIC      0x454F5342U  /* "EOSB" */
#define EOS_BOOT_VERSION    1

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t clock_hz;
    uint32_t ram_start;
    uint32_t ram_size;
    uint32_t flash_start;
    uint32_t flash_size;
    uint32_t tick_rate_hz;
    uint32_t boot_flags;
    uint32_t dtb_addr;          /* Device tree blob address (0 if none) */
    uint32_t reserved[6];
    uint32_t checksum;          /* Simple XOR checksum of all fields */
} eos_boot_params_t;

/* Boot flags */
#define EOS_BOOT_FLAG_SECURE    (1U << 0)
#define EOS_BOOT_FLAG_DEBUG     (1U << 1)
#define EOS_BOOT_FLAG_WDT       (1U << 2)
#define EOS_BOOT_FLAG_DTB       (1U << 3)

int eos_boot_params_read(void);
const eos_boot_params_t *eos_boot_params_get(void);
bool eos_boot_params_valid(void);

#ifdef __cplusplus
}
#endif
#endif /* EOS_BOOT_PARAMS_H */
