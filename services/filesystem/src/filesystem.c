// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/filesystem.h"
#include <string.h>

#if EOS_ENABLE_FILESYSTEM

#define MAX_INODES 32
#define BLOCK_SIZE 4096

/* `unlinked` marks an inode whose name has been removed but which still has
 * open descriptors: the name is gone from lookups, the storage is not. The
 * inode is released when the last descriptor closes, which is what keeps
 * alloc_inode() from handing the slot to an unrelated file while a live fd
 * still points at it. */
typedef struct { char name[EOS_PATH_MAX]; uint8_t data[BLOCK_SIZE]; uint32_t size, cap; uint8_t in_use, is_dir, unlinked, open_count; } inode_t;
typedef struct { uint8_t in_use, inode; uint32_t pos, flags; } fd_t;
typedef struct { uint8_t in_use; int idx; } dir_ctx_t;

static inode_t g_inodes[MAX_INODES];
static fd_t g_fds[EOS_FILE_MAX];
static dir_ctx_t g_dirs[4];
static int g_init = 0;

/* Helper to validate path length and integrity */
int eos_fs_validate_path(const char *path) {
    if (!path) return -1;
    size_t len = 0;
    while (len < (size_t)EOS_PATH_MAX && path[len]) {
        len++;
    }
    if (len >= (size_t)EOS_PATH_MAX) return -1;
    return 0;
}

int eos_fs_init(const eos_fs_config_t *cfg) {
    (void)cfg;
    memset(g_inodes, 0, sizeof(g_inodes)); memset(g_fds, 0, sizeof(g_fds)); memset(g_dirs, 0, sizeof(g_dirs));
    g_inodes[0].in_use = 1; g_inodes[0].is_dir = 1; strncpy(g_inodes[0].name, "/", EOS_PATH_MAX - 1);
    g_inodes[0].name[EOS_PATH_MAX - 1] = '\0';
    g_init = 1; return 0;
}

void eos_fs_deinit(void) { g_init = 0; }
int eos_fs_format(void) { return eos_fs_init(NULL); }

/* Derived rather than accumulated. A running total has to be adjusted by every
 * path that changes a file's length -- write, truncate, O_TRUNC, remove -- and
 * missing or double-applying one adjustment silently corrupts it. It did:
 * removing a file that still had an open descriptor and then truncating
 * through that descriptor subtracted the same bytes twice, wrapping the
 * unsigned total so that free_bytes came out larger than total_bytes. Summing
 * the inodes cannot get out of step, and the table is 32 entries. */
static uint32_t used_bytes(void) {
    uint32_t used = 0;
    for (int i = 0; i < MAX_INODES; i++)
        if (g_inodes[i].in_use && !g_inodes[i].is_dir) used += g_inodes[i].size;
    return used;
}

int eos_fs_stat(eos_fs_stat_t *st) {
    if (!g_init || !st) return -1;
    uint32_t used = used_bytes();
    st->total_bytes = MAX_INODES * BLOCK_SIZE; st->used_bytes = used; st->free_bytes = st->total_bytes - used;
    return 0;
}

static int find_inode(const char *p) { for (int i = 0; i < MAX_INODES; i++) if (g_inodes[i].in_use && !g_inodes[i].unlinked && strcmp(g_inodes[i].name, p) == 0) return i; return -1; }
static int alloc_inode(void) { for (int i = 0; i < MAX_INODES; i++) if (!g_inodes[i].in_use) return i; return -1; }

/* Drop an inode's name. The storage returns to the pool once the last
 * descriptor closes; until then the slot stays in_use so it cannot be
 * recycled underneath a descriptor that is still open on it. */
static void unlink_inode(int i) {
    if (g_inodes[i].open_count > 0) { g_inodes[i].unlinked = 1; return; }
    memset(&g_inodes[i], 0, sizeof(inode_t));
}
static int alloc_fd(void) { for (int i = 0; i < EOS_FILE_MAX; i++) if (!g_fds[i].in_use) return i; return -1; }

eos_file_t eos_fs_open(const char *path, uint32_t flags) {
    if (!g_init || eos_fs_validate_path(path) != 0) return EOS_FILE_INVALID;
    int inode = find_inode(path);
    int created = 0;
    if (inode < 0) {
        if (!(flags & EOS_O_CREATE)) return EOS_FILE_INVALID;
        inode = alloc_inode(); if (inode < 0) return EOS_FILE_INVALID;
        memset(&g_inodes[inode], 0, sizeof(inode_t));
        strncpy(g_inodes[inode].name, path, EOS_PATH_MAX - 1);
        g_inodes[inode].in_use = 1; g_inodes[inode].cap = BLOCK_SIZE;
        created = 1;
    }
    int fd = alloc_fd();
    if (fd < 0) {
        /* An inode created a moment ago has no name anyone can reach and no
         * descriptor: release it rather than stranding the slot. */
        if (created) memset(&g_inodes[inode], 0, sizeof(inode_t));
        return EOS_FILE_INVALID;
    }
    g_fds[fd].in_use = 1; g_fds[fd].inode = (uint8_t)inode; g_fds[fd].pos = 0; g_fds[fd].flags = flags;
    g_inodes[inode].open_count++;
    if (flags & EOS_O_TRUNC) g_inodes[inode].size = 0;
    if (flags & EOS_O_APPEND) g_fds[fd].pos = g_inodes[inode].size;
    return (eos_file_t)fd;
}

int eos_fs_close(eos_file_t fd) {
    if (fd < 0 || fd >= EOS_FILE_MAX || !g_fds[fd].in_use) return -1;
    inode_t *n = &g_inodes[g_fds[fd].inode];
    g_fds[fd].in_use = 0;
    if (n->open_count > 0) n->open_count--;
    if (n->open_count == 0 && n->unlinked) memset(n, 0, sizeof(*n));
    return 0;
}

int eos_fs_read(eos_file_t fd, void *buf, size_t len) {
    if (fd < 0 || fd >= EOS_FILE_MAX || !g_fds[fd].in_use || !buf) return -1;
    inode_t *n = &g_inodes[g_fds[fd].inode]; uint32_t p = g_fds[fd].pos;
    if (p >= n->size) return 0;
    uint32_t avail = n->size - p; if (len > avail) len = avail;
    memcpy(buf, n->data + p, len); g_fds[fd].pos += (uint32_t)len;
    return (int)len;
}

int eos_fs_write(eos_file_t fd, const void *data, size_t len) {
    if (fd < 0 || fd >= EOS_FILE_MAX || !g_fds[fd].in_use || !data) return -1;
    inode_t *n = &g_inodes[g_fds[fd].inode]; uint32_t p = g_fds[fd].pos;
    /* Clamp to the remaining capacity, then bail if nothing is left. These are
     * two independent statements; sharing a line made GCC warn that the second
     * looked guarded by the first `if`, and a maintainer could easily misread
     * it the same way. The `p > n->cap` guard is defensive: callers maintain
     * pos <= cap, but if that ever slipped, `n->cap - p` would wrap (both are
     * unsigned) and hand memcpy a huge length. */
    if (p > n->cap) return -1;
    if (p + len > n->cap) len = n->cap - p;
    if (len == 0) return 0;
    memcpy(n->data + p, data, len); g_fds[fd].pos += (uint32_t)len;
    if (g_fds[fd].pos > n->size) n->size = g_fds[fd].pos;
    return (int)len;
}

int eos_fs_seek(eos_file_t fd, int32_t off, eos_seek_whence_t w) {
    if (fd < 0 || fd >= EOS_FILE_MAX || !g_fds[fd].in_use) return -1;
    inode_t *n = &g_inodes[g_fds[fd].inode]; int32_t np;
    switch (w) { case EOS_SEEK_SET: np = off; break; case EOS_SEEK_CUR: np = (int32_t)g_fds[fd].pos + off; break; case EOS_SEEK_END: np = (int32_t)n->size + off; break; default: return -1; }
    /* Clamp into [0, cap]; two independent statements, one per line. */
    if (np < 0) np = 0;
    if ((uint32_t)np > n->cap) np = (int32_t)n->cap;
    g_fds[fd].pos = (uint32_t)np; return 0;
}

int eos_fs_tell(eos_file_t fd, uint32_t *pos) { if (fd < 0 || fd >= EOS_FILE_MAX || !g_fds[fd].in_use || !pos) return -1; *pos = g_fds[fd].pos; return 0; }

int eos_fs_truncate(eos_file_t fd, uint32_t size) {
    if (fd < 0 || fd >= EOS_FILE_MAX || !g_fds[fd].in_use) return -1;
    inode_t *n = &g_inodes[g_fds[fd].inode];
    if (size > n->cap) size = n->cap;
    if (size < n->size) memset(n->data + size, 0, n->size - size);
    n->size = size; if (g_fds[fd].pos > size) g_fds[fd].pos = size;
    return 0;
}

int eos_fs_sync(eos_file_t fd) { if (fd < 0 || fd >= EOS_FILE_MAX || !g_fds[fd].in_use) return -1; return 0; }

int eos_fs_mkdir(const char *path) {
    if (!g_init || eos_fs_validate_path(path) != 0 || find_inode(path) >= 0) return -1;
    int i = alloc_inode(); if (i < 0) return -1;
    memset(&g_inodes[i], 0, sizeof(inode_t));
    strncpy(g_inodes[i].name, path, EOS_PATH_MAX - 1);
    g_inodes[i].in_use = 1; g_inodes[i].is_dir = 1; return 0;
}

eos_dir_t eos_fs_opendir(const char *path) {
    if (!g_init || eos_fs_validate_path(path) != 0) return EOS_DIR_INVALID;
    for (int i = 0; i < 4; i++) if (!g_dirs[i].in_use) { g_dirs[i].in_use = 1; g_dirs[i].idx = 0; return i; }
    return EOS_DIR_INVALID;
}

int eos_fs_readdir(eos_dir_t d, eos_dirent_t *e) {
    if (d < 0 || d >= 4 || !g_dirs[d].in_use || !e) return -1;
    while (g_dirs[d].idx < MAX_INODES) {
        int i = g_dirs[d].idx++;
        if (g_inodes[i].in_use && !g_inodes[i].unlinked) {
            strncpy(e->name, g_inodes[i].name, EOS_PATH_MAX - 1);
            e->size = g_inodes[i].size; e->is_dir = g_inodes[i].is_dir; return 0;
        }
    }
    return -1;
}

int eos_fs_closedir(eos_dir_t d) { if (d < 0 || d >= 4 || !g_dirs[d].in_use) return -1; g_dirs[d].in_use = 0; return 0; }

int eos_fs_remove(const char *path) {
    if (!g_init || eos_fs_validate_path(path) != 0) return -1;
    int i = find_inode(path); if (i < 0) return -1;
    unlink_inode(i); return 0;
}

int eos_fs_rename(const char *old_path, const char *new_path) {
    if (!g_init || eos_fs_validate_path(old_path) != 0 || eos_fs_validate_path(new_path) != 0) return -1;
    int i = find_inode(old_path); if (i < 0) return -1;
    int existing = find_inode(new_path);
    if (existing == i) return 0;
    /* Without this the new name was written over the old one with no regard
     * for what was already there, leaving two in_use inodes carrying the same
     * path: find_inode() saw one of them, readdir() listed both, and removing
     * the name once left the other behind still answering eos_fs_exists().
     * POSIX rename() replaces the target, so unlink it first -- which also
     * keeps any descriptor still open on it readable until it is closed. */
    if (existing >= 0) unlink_inode(existing);
    strncpy(g_inodes[i].name, new_path, EOS_PATH_MAX - 1);
    g_inodes[i].name[EOS_PATH_MAX - 1] = 0;
    return 0;
}

bool eos_fs_exists(const char *path) {
    if (!g_init || eos_fs_validate_path(path) != 0) return false;
    return find_inode(path) >= 0;
}

#endif /* EOS_ENABLE_FILESYSTEM */