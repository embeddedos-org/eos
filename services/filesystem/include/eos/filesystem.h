// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file filesystem.h
 * @brief EoS File System Abstraction
 *
 * Unified file API over flash storage, SD cards, or RAM disks.
 */

#ifndef EOS_FILESYSTEM_H
#define EOS_FILESYSTEM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <eos/eos_config.h>

#if EOS_ENABLE_FILESYSTEM

#ifdef __cplusplus
extern "C" {
#endif

#define EOS_PATH_MAX  128
#define EOS_FILE_MAX  8

typedef enum {
    EOS_FS_FAT    = 0,
    EOS_FS_LITTLEFS = 1,
    EOS_FS_SPIFFS = 2,
    EOS_FS_RAMFS  = 3,
} eos_fs_type_t;

typedef enum {
    EOS_O_READ   = (1 << 0),
    EOS_O_WRITE  = (1 << 1),
    EOS_O_CREATE = (1 << 2),
    EOS_O_APPEND = (1 << 3),
    EOS_O_TRUNC  = (1 << 4),
} eos_open_flags_t;

typedef enum {
    EOS_SEEK_SET = 0,
    EOS_SEEK_CUR = 1,
    EOS_SEEK_END = 2,
} eos_seek_whence_t;

typedef struct {
    char     name[EOS_PATH_MAX];
    uint32_t size;
    bool     is_dir;
} eos_dirent_t;

typedef struct {
    uint32_t total_bytes;
    uint32_t used_bytes;
    uint32_t free_bytes;
} eos_fs_stat_t;

typedef int eos_file_t;
typedef int eos_dir_t;

#define EOS_FILE_INVALID (-1)
#define EOS_DIR_INVALID  (-1)

typedef struct {
    eos_fs_type_t type;
    uint8_t       flash_id;     /* for flash-backed FS */
    uint32_t      base_addr;
    uint32_t      size;
} eos_fs_config_t;

int  eos_fs_init(const eos_fs_config_t *cfg);
void eos_fs_deinit(void);
int  eos_fs_format(void);
int  eos_fs_stat(eos_fs_stat_t *stat);

/* File operations */
eos_file_t eos_fs_open(const char *path, uint32_t flags);
int  eos_fs_close(eos_file_t fd);
int  eos_fs_read(eos_file_t fd, void *buf, size_t len);
int  eos_fs_write(eos_file_t fd, const void *data, size_t len);
int  eos_fs_seek(eos_file_t fd, int32_t offset, eos_seek_whence_t whence);
int  eos_fs_tell(eos_file_t fd, uint32_t *pos);
int  eos_fs_truncate(eos_file_t fd, uint32_t size);
int  eos_fs_sync(eos_file_t fd);

/* Directory operations */
int  eos_fs_mkdir(const char *path);
eos_dir_t eos_fs_opendir(const char *path);
int  eos_fs_readdir(eos_dir_t dir, eos_dirent_t *entry);
int  eos_fs_closedir(eos_dir_t dir);

/* Path operations */
int  eos_fs_remove(const char *path);
int  eos_fs_rename(const char *old_path, const char *new_path);
bool eos_fs_exists(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* EOS_ENABLE_FILESYSTEM */
#endif /* EOS_FILESYSTEM_H */
