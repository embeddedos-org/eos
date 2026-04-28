// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file types.h
 * @brief Core type definitions for EoS build system.
 *
 * Defines all fundamental enums, constants, and utility functions used
 * across the EoS framework: architectures, build types, system kinds,
 * RTOS providers, firmware formats, image formats, and init systems.
 */

#ifndef EOS_TYPES_H
#define EOS_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

/** @defgroup limits Build System Limits
 *  @{ */
#define EOS_MAX_NAME      128   /**< Maximum length of a name string */
#define EOS_MAX_PATH      512   /**< Maximum length of a file path */
#define EOS_MAX_URL       1024  /**< Maximum length of a URL */
#define EOS_MAX_DEPS      64    /**< Maximum dependencies per package */
#define EOS_MAX_PACKAGES  128   /**< Maximum packages in a project */
#define EOS_MAX_LAYERS    32    /**< Maximum layers in a project */
#define EOS_MAX_NODES     256   /**< Maximum nodes in the build graph */
#define EOS_MAX_EDGES     512   /**< Maximum edges in the build graph */
#define EOS_MAX_OPTIONS   32    /**< Maximum build options per package */
#define EOS_MAX_RTOS      4     /**< Maximum RTOS targets in a hybrid system */
#define EOS_HASH_LEN      65    /**< Length of a hash string (SHA256 hex + null) */
/** @} */

/**
 * @brief Target CPU architecture.
 *
 * Identifies the target architecture for cross-compilation.
 * Includes both Linux-capable and bare-metal MCU architectures.
 */
typedef enum {
    EOS_ARCH_ARM64,         /**< ARM 64-bit (AArch64) — Linux-capable */
    EOS_ARCH_ARM_CORTEX_M,  /**< ARM Cortex-M — bare-metal / RTOS */
    EOS_ARCH_ARM_CORTEX_R,  /**< ARM Cortex-R — real-time / safety */
    EOS_ARCH_X86_64,        /**< x86-64 — desktop / server / QEMU */
    EOS_ARCH_RISCV64,       /**< RISC-V 64-bit */
    EOS_ARCH_RISCV32,       /**< RISC-V 32-bit */
    EOS_ARCH_HOST,          /**< Host machine (native) */
    EOS_ARCH_COUNT          /**< Sentinel — number of architectures */
} EosArch;

/**
 * @brief Build system backend type.
 *
 * Each package or firmware target uses one of these build systems.
 * EoS wraps each as a thin adapter — it never rewrites build logic.
 */
typedef enum {
    EOS_BUILD_CMAKE,      /**< CMake + Ninja (first-party C/C++) */
    EOS_BUILD_NINJA,      /**< Direct Ninja invocation */
    EOS_BUILD_MAKE,       /**< GNU Make / Autotools */
    EOS_BUILD_KBUILD,     /**< Linux kernel Kbuild */
    EOS_BUILD_AUTOTOOLS,  /**< Autoconf / Automake */
    EOS_BUILD_ZEPHYR,     /**< Zephyr RTOS (west + CMake) */
    EOS_BUILD_FREERTOS,   /**< FreeRTOS (CMake / Make / vendor SDK) */
    EOS_BUILD_NUTTX,      /**< Apache NuttX (native configure + Make) */
    EOS_BUILD_CUSTOM,     /**< User-defined build script */
    EOS_BUILD_TYPE_COUNT  /**< Sentinel */
} EosBuildType;

/**
 * @brief System kind — determines the build pipeline.
 *
 * - **Linux**: kernel → rootfs → disk image
 * - **RTOS**: firmware binary (.bin/.elf/.hex)
 * - **Hybrid**: Linux image + one or more RTOS firmware targets
 */
typedef enum {
    EOS_SYSTEM_LINUX,       /**< Embedded Linux system */
    EOS_SYSTEM_RTOS,        /**< RTOS firmware (bare-metal) */
    EOS_SYSTEM_HYBRID,      /**< Linux + RTOS on multi-core SoC */
    EOS_SYSTEM_KIND_COUNT   /**< Sentinel */
} EosSystemKind;

/**
 * @brief RTOS provider / kernel identity.
 *
 * Determines which RTOS SDK and build flow to use.
 */
typedef enum {
    EOS_RTOS_FREERTOS,        /**< FreeRTOS */
    EOS_RTOS_ZEPHYR,          /**< Zephyr Project */
    EOS_RTOS_NUTTX,           /**< Apache NuttX */
    EOS_RTOS_THREADX,         /**< Azure ThreadX / Eclipse ThreadX */
    EOS_RTOS_CUSTOM,          /**< User-supplied RTOS */
    EOS_RTOS_PROVIDER_COUNT   /**< Sentinel */
} EosRtosProvider;

/**
 * @brief Firmware output format.
 */
typedef enum {
    EOS_FW_BIN,           /**< Raw binary (.bin) */
    EOS_FW_ELF,           /**< ELF executable (.elf) */
    EOS_FW_HEX,           /**< Intel HEX (.hex) */
    EOS_FW_UF2,           /**< USB Flashing Format (.uf2) */
    EOS_FW_FORMAT_COUNT   /**< Sentinel */
} EosFirmwareFormat;

/**
 * @brief Disk image format for Linux system builds.
 */
typedef enum {
    EOS_IMG_RAW,          /**< Raw disk image (dd-style) */
    EOS_IMG_QCOW2,       /**< QEMU Copy-on-Write v2 */
    EOS_IMG_ISO,          /**< ISO 9660 */
    EOS_IMG_TAR,          /**< Tarball (rootfs archive) */
    EOS_IMG_FORMAT_COUNT  /**< Sentinel */
} EosImageFormat;

/**
 * @brief Filesystem type for rootfs.
 */
typedef enum {
    EOS_FS_EXT4,      /**< ext4 */
    EOS_FS_FAT32,     /**< FAT32 (boot partition) */
    EOS_FS_SQUASHFS,  /**< SquashFS (read-only) */
    EOS_FS_TMPFS,     /**< tmpfs (RAM-based) */
    EOS_FS_COUNT      /**< Sentinel */
} EosFilesystem;

/**
 * @brief Init system for Linux rootfs.
 */
typedef enum {
    EOS_INIT_SYSVINIT,  /**< SysVinit */
    EOS_INIT_SYSTEMD,   /**< systemd */
    EOS_INIT_BUSYBOX,   /**< BusyBox init */
    EOS_INIT_NONE,      /**< No init (custom) */
    EOS_INIT_COUNT      /**< Sentinel */
} EosInitSystem;

/**
 * @brief Generic key-value pair used for build options and metadata.
 */
typedef struct {
    char key[EOS_MAX_NAME];    /**< Option key */
    char value[EOS_MAX_PATH];  /**< Option value */
} EosKeyValue;

/* ---- String conversion utilities ---- */

/** @brief Convert EosArch to string. */
static inline const char *eos_arch_str(EosArch arch) {
    switch (arch) {
        case EOS_ARCH_ARM64:       return "aarch64";
        case EOS_ARCH_ARM_CORTEX_M: return "arm-cortex-m";
        case EOS_ARCH_ARM_CORTEX_R: return "arm-cortex-r";
        case EOS_ARCH_X86_64:      return "x86_64";
        case EOS_ARCH_RISCV64:     return "riscv64";
        case EOS_ARCH_RISCV32:     return "riscv32";
        case EOS_ARCH_HOST:        return "host";
        default:                     return "unknown";
    }
}

/** @brief Convert EosBuildType to string. */
static inline const char *eos_build_type_str(EosBuildType t) {
    switch (t) {
        case EOS_BUILD_CMAKE:     return "cmake";
        case EOS_BUILD_NINJA:     return "ninja";
        case EOS_BUILD_MAKE:      return "make";
        case EOS_BUILD_KBUILD:    return "kbuild";
        case EOS_BUILD_AUTOTOOLS: return "autotools";
        case EOS_BUILD_ZEPHYR:    return "zephyr";
        case EOS_BUILD_FREERTOS:  return "freertos";
        case EOS_BUILD_NUTTX:     return "nuttx";
        case EOS_BUILD_CUSTOM:    return "custom";
        default:                    return "unknown";
    }
}

/** @brief Parse build type from string. Returns EOS_BUILD_CMAKE for unknown input. */
static inline EosBuildType eos_build_type_from_str(const char *s) {
    if (!s) return EOS_BUILD_CMAKE;
    if (strcmp(s, "cmake") == 0)     return EOS_BUILD_CMAKE;
    if (strcmp(s, "ninja") == 0)     return EOS_BUILD_NINJA;
    if (strcmp(s, "make") == 0)      return EOS_BUILD_MAKE;
    if (strcmp(s, "kbuild") == 0)    return EOS_BUILD_KBUILD;
    if (strcmp(s, "autotools") == 0) return EOS_BUILD_AUTOTOOLS;
    if (strcmp(s, "zephyr") == 0)    return EOS_BUILD_ZEPHYR;
    if (strcmp(s, "freertos") == 0)  return EOS_BUILD_FREERTOS;
    if (strcmp(s, "nuttx") == 0)     return EOS_BUILD_NUTTX;
    if (strcmp(s, "custom") == 0)    return EOS_BUILD_CUSTOM;
    return EOS_BUILD_CMAKE;
}

/** @brief Convert EosSystemKind to string. */
static inline const char *eos_system_kind_str(EosSystemKind k) {
    switch (k) {
        case EOS_SYSTEM_LINUX:  return "linux";
        case EOS_SYSTEM_RTOS:   return "rtos";
        case EOS_SYSTEM_HYBRID: return "hybrid";
        default:                  return "unknown";
    }
}

/** @brief Parse system kind from string. Returns EOS_SYSTEM_LINUX for unknown input. */
static inline EosSystemKind eos_system_kind_from_str(const char *s) {
    if (!s) return EOS_SYSTEM_LINUX;
    if (strcmp(s, "linux") == 0)  return EOS_SYSTEM_LINUX;
    if (strcmp(s, "rtos") == 0)   return EOS_SYSTEM_RTOS;
    if (strcmp(s, "hybrid") == 0) return EOS_SYSTEM_HYBRID;
    return EOS_SYSTEM_LINUX;
}

/** @brief Convert EosRtosProvider to string. */
static inline const char *eos_rtos_provider_str(EosRtosProvider p) {
    switch (p) {
        case EOS_RTOS_FREERTOS: return "freertos";
        case EOS_RTOS_ZEPHYR:   return "zephyr";
        case EOS_RTOS_NUTTX:    return "nuttx";
        case EOS_RTOS_THREADX:  return "threadx";
        case EOS_RTOS_CUSTOM:   return "custom";
        default:                  return "unknown";
    }
}

/** @brief Parse RTOS provider from string. Returns EOS_RTOS_FREERTOS for unknown input. */
static inline EosRtosProvider eos_rtos_provider_from_str(const char *s) {
    if (!s) return EOS_RTOS_FREERTOS;
    if (strcmp(s, "freertos") == 0) return EOS_RTOS_FREERTOS;
    if (strcmp(s, "zephyr") == 0)   return EOS_RTOS_ZEPHYR;
    if (strcmp(s, "nuttx") == 0)    return EOS_RTOS_NUTTX;
    if (strcmp(s, "threadx") == 0)  return EOS_RTOS_THREADX;
    if (strcmp(s, "custom") == 0)   return EOS_RTOS_CUSTOM;
    return EOS_RTOS_FREERTOS;
}

/** @brief Convert EosFirmwareFormat to string. */
static inline const char *eos_firmware_format_str(EosFirmwareFormat f) {
    switch (f) {
        case EOS_FW_BIN: return "bin";
        case EOS_FW_ELF: return "elf";
        case EOS_FW_HEX: return "hex";
        case EOS_FW_UF2: return "uf2";
        default:           return "unknown";
    }
}

/** @brief Parse firmware format from string. Returns EOS_FW_BIN for unknown input. */
static inline EosFirmwareFormat eos_firmware_format_from_str(const char *s) {
    if (!s) return EOS_FW_BIN;
    if (strcmp(s, "bin") == 0) return EOS_FW_BIN;
    if (strcmp(s, "elf") == 0) return EOS_FW_ELF;
    if (strcmp(s, "hex") == 0) return EOS_FW_HEX;
    if (strcmp(s, "uf2") == 0) return EOS_FW_UF2;
    return EOS_FW_BIN;
}

#endif /* EOS_TYPES_H */
