#ifndef EOS_LOCKFILE_H
#define EOS_LOCKFILE_H

#include "eos/types.h"
#include "eos/error.h"
#include "eos/config.h"

typedef struct {
    char name[EOS_MAX_NAME];
    char version[EOS_MAX_NAME];
    char hash[EOS_HASH_LEN];
    char source[EOS_MAX_URL];
    EosBuildType build_type;
    char resolved_version[EOS_MAX_NAME];
} EosLockEntry;

typedef struct {
    char project_name[EOS_MAX_NAME];
    char project_version[EOS_MAX_NAME];
    EosLockEntry entries[EOS_MAX_PACKAGES];
    int count;
} EosLockfile;

EosResult eos_lockfile_load(EosLockfile *lock, const char *path);
EosResult eos_lockfile_save(const EosLockfile *lock, const char *path);
EosResult eos_lockfile_generate(EosLockfile *lock, const EosConfig *cfg);
int eos_lockfile_is_current(const EosLockfile *lock, const EosConfig *cfg);
void eos_lockfile_dump(const EosLockfile *lock);

#endif /* EOS_LOCKFILE_H */
