// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file system.h
 * @brief System build pipeline — Linux, RTOS firmware, and hybrid.
 *
 * Defines the build contexts and functions for all three system kinds:
 * - **EosSystem**: Linux kernel + rootfs + disk image pipeline
 * - **EosFirmware**: RTOS firmware build pipeline
 * - **EosHybridSystem**: Combined Linux + RTOS for multi-core SoCs
 */

#ifndef EOS_SYSTEM_H
#define EOS_SYSTEM_H

#include "eos/types.h"
#include "eos/error.h"
#include "eos/config.h"

/**
 * @brief Linux system build context.
 *
 * Holds all state needed to build a complete Linux image:
 * kernel, rootfs, init system, and disk image output.
 */
typedef struct {
    char kernel_provider[EOS_MAX_NAME];   /**< Kernel build provider (kbuild, buildroot) */
    char kernel_defconfig[EOS_MAX_PATH];  /**< Kernel defconfig name */
    char kernel_src[EOS_MAX_PATH];        /**< Kernel source directory */
    char kernel_build_dir[EOS_MAX_PATH];  /**< Kernel build output directory */
    char kernel_image[EOS_MAX_PATH];      /**< Built kernel image path */

    char rootfs_dir[EOS_MAX_PATH];        /**< Rootfs assembly directory */
    char rootfs_provider[EOS_MAX_NAME];   /**< Rootfs provider (eos, buildroot) */
    EosInitSystem init_system;            /**< Init system (busybox, systemd, sysvinit) */
    char hostname[EOS_MAX_NAME];          /**< System hostname */

    EosImageFormat image_format;          /**< Output image format (raw, qcow2, tar) */
    char image_output[EOS_MAX_PATH];      /**< Output image file path */
    size_t image_size_mb;                 /**< Image size in megabytes */

    char staging_dir[EOS_MAX_PATH];       /**< Package staging directory */
    char install_dir[EOS_MAX_PATH];       /**< Package install directory */

    int dry_run;                          /**< If true, log pipeline without executing */
    int verbose;                          /**< Enable verbose logging */
} EosSystem;

/**
 * @brief RTOS firmware build context.
 *
 * Holds all state needed to build a firmware binary for a specific
 * RTOS provider and target board.
 */
typedef struct {
    EosRtosProvider provider;             /**< RTOS provider (freertos, zephyr, nuttx) */
    char board[EOS_MAX_NAME];             /**< Target board identifier */
    char core[EOS_MAX_NAME];             /**< CPU core name (for multi-core SoCs) */
    char entry[EOS_MAX_PATH];            /**< Application entry point / source path */
    char output[EOS_MAX_PATH];           /**< Output firmware file path */
    EosFirmwareFormat format;            /**< Output format (bin, elf, hex, uf2) */

    char build_dir[EOS_MAX_PATH];        /**< Firmware build directory */
    char src_dir[EOS_MAX_PATH];          /**< Firmware source directory */
    char install_dir[EOS_MAX_PATH];      /**< Firmware install/output directory */
    char toolchain_target[EOS_MAX_NAME]; /**< Cross-compilation target triple */

    int dry_run;                         /**< If true, log pipeline without executing */
    int verbose;                         /**< Enable verbose logging */
} EosFirmware;

/**
 * @brief Hybrid build context — Linux + RTOS.
 *
 * Coordinates building both a Linux system and one or more RTOS
 * firmware targets, typically for multi-core SoCs like STM32MP1.
 */
typedef struct {
    EosSystemKind kind;                   /**< Should be EOS_SYSTEM_HYBRID */
    EosSystem linux_sys;                  /**< Linux system build context */
    EosFirmware firmware[EOS_MAX_RTOS];   /**< RTOS firmware targets */
    int firmware_count;                   /**< Number of firmware targets */
    int dry_run;                          /**< If true, log pipeline without executing */
    int verbose;                          /**< Enable verbose logging */
} EosHybridSystem;

/* ---- Linux system functions ---- */

/** @brief Initialize Linux system context from config. */
void eos_system_init(EosSystem *sys, const EosConfig *cfg);

/** @brief Run the complete Linux build pipeline (kernel → rootfs → image). */
EosResult eos_system_build(EosSystem *sys);

/** @brief Build the Linux kernel using the configured provider. */
EosResult eos_system_build_kernel(EosSystem *sys);

/** @brief Assemble the rootfs (FHS skeleton, init scripts, users). */
EosResult eos_system_build_rootfs(EosSystem *sys);

/** @brief Create the final disk image from kernel + rootfs. */
EosResult eos_system_build_image(EosSystem *sys);

/** @brief Print Linux system configuration to stdout. */
void eos_system_dump(const EosSystem *sys);

/* ---- RTOS firmware functions ---- */

/**
 * @brief Initialize firmware context from RTOS config.
 * @param fw       Firmware context to initialize
 * @param rtos_cfg RTOS configuration (provider, board, entry, output)
 * @param cfg      Global project config (workspace dirs, toolchain)
 */
void eos_firmware_init(EosFirmware *fw, const EosRtosConfig *rtos_cfg,
                       const EosConfig *cfg);

/** @brief Run the firmware build pipeline (configure → build → output). */
EosResult eos_firmware_build(EosFirmware *fw);

/** @brief Flash firmware to target board (scaffold — not yet wired). */
EosResult eos_firmware_flash(const EosFirmware *fw);

/** @brief Print firmware configuration to stdout. */
void eos_firmware_dump(const EosFirmware *fw);

/* ---- Hybrid system functions ---- */

/** @brief Initialize hybrid context (Linux + RTOS) from config. */
void eos_hybrid_init(EosHybridSystem *hybrid, const EosConfig *cfg);

/** @brief Build hybrid system: Linux first, then each RTOS firmware. */
EosResult eos_hybrid_build(EosHybridSystem *hybrid);

/** @brief Print hybrid system configuration to stdout. */
void eos_hybrid_dump(const EosHybridSystem *hybrid);

/* ---- Documentation build functions ---- */

/**
 * @brief Build product documentation as part of the build pipeline.
 *
 * Generates API docs (via Doxygen or configured tool) and user docs
 * (via MkDocs or configured tool), then packages the output alongside
 * the product image/firmware artifacts.
 *
 * @param docs_cfg  Documentation configuration from eos.yaml
 * @param cfg       Global project config (for project name, build dirs)
 * @param dry_run   If true, log steps without executing
 * @return EOS_OK on success
 */
EosResult eos_docs_build(const EosDocsConfig *docs_cfg, const EosConfig *cfg,
                         int dry_run);

/** @brief Print docs configuration to stdout. */
void eos_docs_dump(const EosDocsConfig *docs_cfg, const EosConfig *cfg);

#endif /* EOS_SYSTEM_H */
