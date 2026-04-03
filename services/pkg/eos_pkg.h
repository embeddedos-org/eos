#ifndef EOS_PKG_H
#define EOS_PKG_H

#include <stdint.h>
#include <stdbool.h>

/* .eapp package format constants */
#define EAPP_MAGIC          0x45415050  /* "EAPP" */
#define EAPP_VERSION        1
#define EAPP_MAX_NAME       64
#define EAPP_MAX_PATH       256
#define EAPP_MAX_PACKAGES   64
#define EAPP_SIGNATURE_LEN  64  /* Ed25519 */
#define EAPP_HASH_LEN       32  /* SHA-256 */

/* Package capabilities */
#define EAPP_CAP_DISPLAY    (1 << 0)
#define EAPP_CAP_NETWORK    (1 << 1)
#define EAPP_CAP_STORAGE    (1 << 2)
#define EAPP_CAP_AUDIO      (1 << 3)
#define EAPP_CAP_CAMERA     (1 << 4)
#define EAPP_CAP_BLUETOOTH  (1 << 5)
#define EAPP_CAP_GPIO       (1 << 6)
#define EAPP_CAP_USB        (1 << 7)

/* Package state */
typedef enum {
    EAPP_STATE_NOT_INSTALLED = 0,
    EAPP_STATE_INSTALLED,
    EAPP_STATE_RUNNING,
    EAPP_STATE_DISABLED,
    EAPP_STATE_UPDATING,
    EAPP_STATE_ERROR,
} eapp_state_t;

/* Architecture target */
typedef enum {
    EAPP_ARCH_ARM64 = 0,
    EAPP_ARCH_ARMV7,
    EAPP_ARCH_X86_64,
    EAPP_ARCH_RISCV64,
    EAPP_ARCH_WASM,
    EAPP_ARCH_JVM,
    EAPP_ARCH_XTENSA,
    EAPP_ARCH_COUNT,
} eapp_arch_t;

/* .eapp file header (at offset 0 of the .eapp ZIP archive manifest) */
typedef struct __attribute__((packed)) {
    uint32_t magic;             /* EAPP_MAGIC */
    uint8_t  version;           /* EAPP_VERSION */
    char     name[EAPP_MAX_NAME];
    char     package_id[EAPP_MAX_NAME]; /* e.g. "com.eos.eapps.ecal" */
    uint8_t  ver_major;
    uint8_t  ver_minor;
    uint8_t  ver_patch;
    uint32_t capabilities;      /* bitfield of EAPP_CAP_* */
    uint8_t  arch_count;        /* number of architectures included */
    uint8_t  supported_archs[EAPP_ARCH_COUNT]; /* 1 if binary for this arch is present */
    uint32_t binary_offset;     /* offset to binary data in archive */
    uint32_t binary_size;       /* size of binary for current arch */
    uint32_t resources_offset;  /* offset to resources */
    uint32_t resources_size;
    uint8_t  signature[EAPP_SIGNATURE_LEN]; /* Ed25519 signature */
    uint8_t  hash[EAPP_HASH_LEN];          /* SHA-256 hash of binary */
    bool     has_gui;           /* true if app has GUI mode */
    bool     has_cli;           /* true if app has CLI mode */
} eapp_header_t;

/* Installed package record */
typedef struct {
    char     name[EAPP_MAX_NAME];
    char     package_id[EAPP_MAX_NAME];
    char     install_path[EAPP_MAX_PATH];
    uint8_t  ver_major, ver_minor, ver_patch;
    uint32_t capabilities;
    eapp_arch_t arch;
    eapp_state_t state;
    uint32_t installed_size;    /* bytes */
    uint32_t install_time;      /* unix timestamp */
    bool     has_gui;
    bool     has_cli;
} eapp_package_t;

/* Package database */
typedef struct {
    eapp_package_t packages[EAPP_MAX_PACKAGES];
    uint32_t count;
    char     apps_dir[EAPP_MAX_PATH];  /* e.g. "/apps" or "/data/apps" */
    char     db_path[EAPP_MAX_PATH];   /* e.g. "/apps/.eapp_db" */
} eapp_db_t;

/* eos-pkg API */
int  eos_pkg_init(eapp_db_t *db, const char *apps_dir);
int  eos_pkg_install(eapp_db_t *db, const char *eapp_path);
int  eos_pkg_remove(eapp_db_t *db, const char *package_id);
int  eos_pkg_update(eapp_db_t *db, const char *eapp_path);
int  eos_pkg_list(const eapp_db_t *db);
int  eos_pkg_info(const eapp_db_t *db, const char *package_id);
int  eos_pkg_verify(const char *eapp_path);
int  eos_pkg_run(const eapp_db_t *db, const char *package_id, int argc, char **argv);
int  eos_pkg_stop(const eapp_db_t *db, const char *package_id);
int  eos_pkg_enable(eapp_db_t *db, const char *package_id);
int  eos_pkg_disable(eapp_db_t *db, const char *package_id);
eapp_package_t* eos_pkg_find(const eapp_db_t *db, const char *package_id);
int  eos_pkg_save_db(const eapp_db_t *db);
int  eos_pkg_load_db(eapp_db_t *db);
eapp_arch_t eos_pkg_detect_arch(void);
const char* eos_pkg_arch_name(eapp_arch_t arch);
const char* eos_pkg_state_name(eapp_state_t state);

#endif /* EOS_PKG_H */
