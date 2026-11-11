#ifndef EOS_PKG_H
#define EOS_PKG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* .eapp package format constants */
#define EAPP_MAGIC          0x45415050  /* "EAPP" */
/* Format version 2. In version 1 the Ed25519 signature covered the binary
 * payload alone, so every other header field (capabilities, version triple,
 * name, package_id, arch list, resources offset/size) and the resources blob
 * itself were unsigned and rewritable on a genuinely signed package (#162).
 * In version 2 the signature is Ed25519 over the envelope digest computed by
 * eos_pkg_envelope_digest(): SHA-256 over the header with the signature field
 * zeroed, followed by the binary payload, followed by the resources blob.
 * The verifier refuses any version other than this one. */
#define EAPP_VERSION        2
#define EAPP_MAX_NAME       64
#define EAPP_MAX_PATH       256
#define EAPP_MAX_PACKAGES   64
#define EAPP_SIGNATURE_LEN  64  /* Ed25519 */
#define EAPP_PUBKEY_LEN     32  /* Ed25519 public key */
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
/* Every capability bit this loader knows about. A package that sets a bit
 * outside this mask declares a capability no released loader understands,
 * and eos_pkg_run() refuses it: an unknown capability must never run
 * unenforced by accident (foundation PR-EOS-3, S-03). */
#define EAPP_CAP_KNOWN_MASK 0xFFu

/* Largest combined binary + resources payload the loader will allocate
 * before the signature is verified (foundation PR-EOS-3, S-04). The
 * binary and the resources blob are both read into memory before the
 * envelope signature is checked, so a header declaring a multi-GB
 * binary_size could force that allocation on an unauthenticated claim.
 * verify() and install() refuse a package whose declared payload is
 * larger than this before allocating anything. 64 MiB is generous for
 * the MCU-class packages this loader serves; a platform that genuinely
 * ships larger packages raises it with -DEOS_PKG_MAX_PAYLOAD_BYTES=...
 * The bound is on the combined size: both blobs are live at once while
 * the signature is checked. */
#ifndef EOS_PKG_MAX_PAYLOAD_BYTES
#define EOS_PKG_MAX_PAYLOAD_BYTES (64u * 1024u * 1024u)
#endif

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
    EAPP_ARCH_RISCV32,
    EAPP_ARCH_WASM,
    EAPP_ARCH_JVM,
    EAPP_ARCH_MIPS,
    EAPP_ARCH_MIPS64,
    EAPP_ARCH_POWERPC,
    EAPP_ARCH_PPC64,
    EAPP_ARCH_SPARC,
    EAPP_ARCH_SPARC64,
    EAPP_ARCH_M68K,
    EAPP_ARCH_SH,
    EAPP_ARCH_AVR,
    EAPP_ARCH_AVR32,
    EAPP_ARCH_PIC16,
    EAPP_ARCH_PIC18,
    EAPP_ARCH_PIC24,
    EAPP_ARCH_DSPIC,
    EAPP_ARCH_PIC32,
    EAPP_ARCH_MSP430,
    EAPP_ARCH_C28X,
    EAPP_ARCH_C6000,
    EAPP_ARCH_PRU,
    EAPP_ARCH_RL78,
    EAPP_ARCH_RX,
    EAPP_ARCH_TRICORE,
    EAPP_ARCH_C166,
    EAPP_ARCH_MICROBLAZE,
    EAPP_ARCH_NIOS2,
    EAPP_ARCH_OPENRISC,
    EAPP_ARCH_LM32,
    EAPP_ARCH_BLACKFIN,
    EAPP_ARCH_SHARC,
    EAPP_ARCH_HEXAGON,
    EAPP_ARCH_CEVA,
    EAPP_ARCH_ARC,
    EAPP_ARCH_8051,
    EAPP_ARCH_LOONGARCH,
    EAPP_ARCH_PARISC,
    EAPP_ARCH_IA64,
    EAPP_ARCH_ALPHA,
    EAPP_ARCH_S390,
    EAPP_ARCH_CRIS,
    EAPP_ARCH_KALIMBA,
    EAPP_ARCH_XTENSA,
    EAPP_ARCH_COUNT,
} eapp_arch_t;

/* .eapp file header (at offset 0 of the .eapp ZIP archive manifest) */
#if defined(_MSC_VER)
#pragma pack(push, 1)
#define EOS_EAPP_PACKED
#else
#define EOS_EAPP_PACKED __attribute__((packed))
#endif
typedef struct EOS_EAPP_PACKED {
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
#if defined(_MSC_VER)
#pragma pack(pop)
#endif
#undef EOS_EAPP_PACKED

/* This header is written to and read from .eapp files verbatim. Keep this
 * contract in lockstep across every compiler that can produce a package. */
_Static_assert(EAPP_ARCH_COUNT == 48, "eapp header layout requires 48 architectures");
_Static_assert(sizeof(bool) == 1, "eapp header wire format requires one-byte bool");
_Static_assert(sizeof(eapp_header_t) == 303, "eapp header wire size changed");
_Static_assert(offsetof(eapp_header_t, magic) == 0, "eapp magic offset changed");
_Static_assert(offsetof(eapp_header_t, name) == 5, "eapp name offset changed");
_Static_assert(offsetof(eapp_header_t, package_id) == 69, "eapp package_id offset changed");
_Static_assert(offsetof(eapp_header_t, capabilities) == 136, "eapp capabilities offset changed");
_Static_assert(offsetof(eapp_header_t, binary_offset) == 189, "eapp binary offset changed");
_Static_assert(offsetof(eapp_header_t, signature) == 205, "eapp signature offset changed");
_Static_assert(offsetof(eapp_header_t, hash) == 269, "eapp hash offset changed");
_Static_assert(offsetof(eapp_header_t, has_gui) == 301, "eapp has_gui offset changed");
_Static_assert(offsetof(eapp_header_t, has_cli) == 302, "eapp has_cli offset changed");

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

/**
 * @brief Install the Ed25519 public key that package signatures are checked
 *        against, or NULL to remove it.
 *
 * This is the seam a keystore or a provisioning step fills; there is no key
 * that can honestly be compiled into this repository. The key is validated
 * here rather than at verification time -- a key outside the prime-order
 * subgroup authenticates nothing, and reporting that as "signature
 * verification failed" per package blames the package for a defect in the
 * installation.
 *
 * @return 0 if the key was installed, -1 if it is unusable as an anchor.
 */
int  eos_pkg_set_trust_anchor(const uint8_t public_key[EAPP_PUBKEY_LEN]);

/**
 * @brief The anchor in force: the one set at runtime, else the one the build
 *        supplied via EOS_PKG_TRUST_ANCHOR_HEX, else NULL.
 *
 * With no anchor, eos_pkg_verify() and eos_pkg_install() refuse rather than
 * appear to verify, unless the build defines EOS_ALLOW_UNSIGNED_PKG. That
 * mirrors EOS_ALLOW_STUB_CRYPTO in services/crypto and EOS_ALLOW_UNSIGNED_OTA
 * in services/ota.
 */
const uint8_t *eos_pkg_trust_anchor(void);
/**
 * @brief SHA-256 digest of the v2 signature envelope: the header with the
 *        signature field zeroed, followed by the binary payload, followed by
 *        the resources blob (when present).
 *
 * The producer signs this digest with Ed25519; eos_pkg_verify() and
 * eos_pkg_install() recompute it and check the header's signature against
 * it. Signing the digest keeps the signed message at 32 bytes and lets the
 * digest stream, so large payloads never need a second in-memory copy.
 */
void eos_pkg_envelope_digest(const eapp_header_t *hdr,
                             const uint8_t *binary_data, uint32_t binary_size,
                             const uint8_t *res_data, uint32_t res_size,
                             uint8_t digest[EAPP_HASH_LEN]);

/**
 * @brief Whether a declared binary + resources payload fits the
 *        pre-authentication allocation bound.
 *
 * The loader reads both blobs into memory before the envelope signature
 * is checked, so the check must run on the header's declared sizes --
 * before any allocation, and before the signature. The bound is on the
 * combined size (see EOS_PKG_MAX_PAYLOAD_BYTES): both blobs are live at
 * once while the signature is checked. The sum is computed in 64 bits,
 * so two near-4 GiB declarations cannot wrap past the bound.
 *
 * @return 0 when the combined size is within the bound, -1 otherwise.
 */
int eos_pkg_check_payload_sizes(uint32_t binary_size, uint32_t resources_size);

/**
 * @brief Launch an installed package binary, within its signed capabilities.
 *
 * The #162 v2 envelope signs the capabilities, so the bits in the package
 * record are the ones its publisher asked for -- but signing is not
 * enforcement: this loader runs the binary with the caller's full
 * privileges and has no confinement mechanism (no seccomp, landlock,
 * namespace, uid separation, or chroot in this translation unit), so
 * today it can enforce no capability at all. eos_pkg_run() therefore
 * refuses -- fail-closed -- any package whose capabilities are not within
 * the enforceable set (currently empty), including unknown capability
 * bits: a package that needs a device capability cannot run until the
 * loader can confine it to that capability. A package that declares no
 * capabilities runs: it asks for nothing the loader would have to
 * enforce. The refusal leaves the package installed, not in error: the
 * package is fine, this loader cannot run it yet.
 *
 * @return 0 when the package launched, -1 when it was not found, is
 *         disabled or already running, has no binary, or declares
 *         capabilities the loader cannot enforce.
 */
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
