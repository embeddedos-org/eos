// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file backend.h
 * @brief Build backend interface and registry.
 *
 * Each build system (CMake, Ninja, Make, Kbuild, Zephyr, FreeRTOS, NuttX)
 * is wrapped as an EosBackend with four operations: configure, build,
 * install, and clean. Backends are registered at startup and looked up
 * by name or EosBuildType when the scheduler needs to build a node.
 */

#ifndef EOS_BACKEND_H
#define EOS_BACKEND_H

#include "eos/types.h"
#include "eos/error.h"

#define EOS_MAX_BACKENDS 16  /**< Maximum registered backends */

/**
 * @brief Build backend — thin adapter around a native build system.
 *
 * Each backend provides four function pointers following the same
 * lifecycle: configure → build → install → clean.
 */
typedef struct EosBackend {
    char name[EOS_MAX_NAME];   /**< Backend identifier (e.g., "cmake", "zephyr") */
    EosBuildType type;          /**< Corresponding build type enum */

    /** @brief Configure the build (e.g., cmake -S ... -B ...) */
    EosResult (*configure)(struct EosBackend *self, const char *src_dir,
                             const char *build_dir, const char *toolchain_file,
                             const EosKeyValue *options, int option_count);

    /** @brief Execute the build (e.g., cmake --build ...) */
    EosResult (*build)(struct EosBackend *self, const char *build_dir, int jobs);

    /** @brief Install built artifacts to a destination directory */
    EosResult (*install)(struct EosBackend *self, const char *build_dir,
                           const char *install_dir);

    /** @brief Clean build artifacts */
    EosResult (*clean)(struct EosBackend *self, const char *build_dir);
} EosBackend;

/** @brief Initialize the global backend registry. Must be called before register. */
void eos_backend_registry_init(void);

/** @brief Register a backend in the global registry. */
EosResult eos_backend_register(EosBackend *backend);

/** @brief Find a registered backend by name. Returns NULL if not found. */
EosBackend *eos_backend_find(const char *name);

/** @brief Find a registered backend by build type. Returns NULL if not found. */
EosBackend *eos_backend_find_by_type(EosBuildType type);

/* ---- Built-in backend constructors ---- */

void eos_backend_cmake_init(EosBackend *b);    /**< CMake + Ninja */
void eos_backend_ninja_init(EosBackend *b);    /**< Direct Ninja */
void eos_backend_make_init(EosBackend *b);     /**< GNU Make / Autotools */
void eos_backend_kbuild_init(EosBackend *b);   /**< Linux Kbuild */

/* ---- RTOS backend constructors ---- */

void eos_backend_zephyr_init(EosBackend *b);   /**< Zephyr RTOS (west/CMake) */
void eos_backend_freertos_init(EosBackend *b); /**< FreeRTOS (CMake/Make/vendor) */
void eos_backend_nuttx_init(EosBackend *b);    /**< Apache NuttX (native) */

/* ---- Additional backend constructors ---- */

void eos_backend_meson_init(EosBackend *b);     /**< Meson + Ninja */
void eos_backend_autotools_init(EosBackend *b); /**< Autoconf / Automake */
void eos_backend_buildroot_init(EosBackend *b); /**< Buildroot (kernel+rootfs+image) */
void eos_backend_cargo_init(EosBackend *b);     /**< Rust Cargo */

#endif /* EOS_BACKEND_H */
