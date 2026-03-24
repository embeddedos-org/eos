/**
 * @file filesystem.c
 * @brief EoS File System — Stub implementation
 */

#include <eos/filesystem.h>
#include <string.h>

#if EOS_ENABLE_FILESYSTEM

static bool fs_initialized = false;

int eos_fs_init(const eos_fs_config_t *cfg)
{
    (void)cfg;
    fs_initialized = true;
    return 0;
}

void eos_fs_deinit(void) { fs_initialized = false; }
int eos_fs_format(void) { return fs_initialized ? 0 : -1; }

int eos_fs_stat(eos_fs_stat_t *stat)
{
    if (!fs_initialized || !stat) return -1;
    memset(stat, 0, sizeof(*stat));
    return 0;
}

eos_file_t eos_fs_open(const char *path, uint32_t flags)
{
    (void)path; (void)flags;
    return EOS_FILE_INVALID;
}

int eos_fs_close(eos_file_t fd) { (void)fd; return -1; }

int eos_fs_read(eos_file_t fd, void *buf, size_t len)
{
    (void)fd; (void)buf; (void)len;
    return -1;
}

int eos_fs_write(eos_file_t fd, const void *data, size_t len)
{
    (void)fd; (void)data; (void)len;
    return -1;
}

int eos_fs_seek(eos_file_t fd, int32_t offset, eos_seek_whence_t whence)
{
    (void)fd; (void)offset; (void)whence;
    return -1;
}

int eos_fs_tell(eos_file_t fd, uint32_t *pos)
{
    (void)fd; (void)pos;
    return -1;
}

int eos_fs_truncate(eos_file_t fd, uint32_t size)
{
    (void)fd; (void)size;
    return -1;
}

int eos_fs_sync(eos_file_t fd) { (void)fd; return -1; }

int eos_fs_mkdir(const char *path) { (void)path; return -1; }

eos_dir_t eos_fs_opendir(const char *path)
{
    (void)path;
    return EOS_DIR_INVALID;
}

int eos_fs_readdir(eos_dir_t dir, eos_dirent_t *entry)
{
    (void)dir; (void)entry;
    return -1;
}

int eos_fs_closedir(eos_dir_t dir) { (void)dir; return -1; }

int eos_fs_remove(const char *path) { (void)path; return -1; }

int eos_fs_rename(const char *old_path, const char *new_path)
{
    (void)old_path; (void)new_path;
    return -1;
}

bool eos_fs_exists(const char *path) { (void)path; return false; }

#endif /* EOS_ENABLE_FILESYSTEM */
