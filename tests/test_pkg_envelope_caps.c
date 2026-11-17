/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 EoS Project
 *
 * @file test_pkg_envelope_caps.c
 * @brief eos_pkg_run() must enforce the #162 envelope's capabilities, and
 *        the loader must not allocate from an unauthenticated size claim.
 *
 * Two defects, one file (foundation PR-EOS-3, S-03+S-04):
 *
 * 1. Capabilities were signed but never enforced. The v2 envelope signs the
 *    capabilities bitfield, so the bits in a package record are authentic --
 *    but eos_pkg_run() checked only state and binary existence, then
 *    fork+execv'd the binary with the caller's full privileges and no
 *    capability reference at all. A "network" package got the whole
 *    network. This loader has no confinement mechanism (no seccomp,
 *    landlock, namespace, uid separation, or chroot in this translation
 *    unit), so it can enforce no capability: eos_pkg_run() must refuse --
 *    fail-closed -- any package that declares one, including unknown
 *    capability bits, until a platform gains a mechanism.
 *
 * 2. Pre-authentication allocation. verify() and install() did
 *    malloc(hdr.binary_size) before the envelope signature was checked,
 *    so a header declaring a multi-GB binary could force that allocation
 *    on an unauthenticated claim. The declared payload is now bounded
 *    before anything is allocated.
 *
 * The packages below are signed over the v2 envelope (#162) with the same
 * test key test_pkg_trust_anchor.c uses, so every run refusal here is
 * about the capabilities and never about the signature.
 */

#include <stdio.h>
#ifdef _WIN32
#include <eos/eos_windows.h>
#include <direct.h>
#define eos_test_rmdir(p) _rmdir(p)
#define SEP "\\"   /* what install joins paths with; load_db compares the string exactly */
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#define eos_test_rmdir(p) rmdir(p)
#define SEP "/"
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "eos_pkg.h"
#include <eos/crypto.h>
#include <ed25519.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        printf("  %-56s ", #name); \
        fflush(stdout); \
        tests_run++; \
        name(); \
        tests_passed++; \
        printf("[PASS]\n"); \
    } \
    static void name(void)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while (0)

/* ---- signing key: the one test_pkg_trust_anchor.c uses ------------------- */

/* The derived public key was cross-validated against OpenSSL there; sign_v2()
 * asserts the derivation reproduces TEST_PUB, so a drift fails loudly. */
static const uint8_t TEST_SEED[32] = {
 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x00,0x01,0x02,0x03,0x04,0x05,
 0x06,0x07,0x08,0x09,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x00,0x01};
static const uint8_t TEST_PUB[32] = {
 0xc9,0x9a,0xff,0x66,0x7c,0x75,0x78,0x05,0x6b,0xd7,0xd4,0x59,0x0d,0xf9,0x2b,0xf2,
 0xae,0x10,0x8a,0x6a,0x7f,0x33,0xdd,0x52,0x38,0x4a,0x83,0xbf,0x32,0x5f,0x02,0x69};

/* A runnable binary for the launch test: a shell script that marks that it
 * ran, then exits. install() writes it 0755, so fork+execv runs it on
 * POSIX. The marker is how the test knows the child really executed, not
 * just that run() returned 0 -- run() is fire-and-forget, so without the
 * marker the test could pass while the child never ran. */
static const char PAYLOAD_SCRIPT[] = "#!/bin/sh\ntouch test_envelope_caps_ran\n";
#define RAN_MARKER  "test_envelope_caps_ran"
static const uint8_t T2_MSG[1] = { 0x72 };

/* ---- fixture layout, all relative to the test's working directory -------- */

#define EAPP_PATH   "test_envelope_caps.eapp"
#define APPS_DIR    "test_envelope_caps_apps"
#define GOOD_NAME   "caps-test"
#define GOOD_ID     "org.eos.test.caps"
#define NET_ID      "org.eos.test.capnet"
#define UNK_ID      "org.eos.test.capun"
#define CAP_ID_FMT  "org.eos.test.cap%d"   /* the eight defined bits, cap0..cap7 */

static int path_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

/* Fixture files are created 0600 (fopen("wb") would be 0666 masked by
 * umask), the way write_eapp_named() already creates the package. */
static FILE *create_fixture(const char *path)
{
#ifdef _WIN32
    return fopen(path, "wb");
#else
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    FILE *f;
    if (fd < 0) return NULL;
    f = fdopen(fd, "wb");
    if (!f) close(fd);
    return f;
#endif
}

static void remove_package_tree(const char *id)
{
    char p[512];
    snprintf(p, sizeof(p), "%s/%s/%s", APPS_DIR, id, GOOD_NAME);
    remove(p);
    snprintf(p, sizeof(p), "%s/%s/resources/data.bin", APPS_DIR, id);
    remove(p);
    snprintf(p, sizeof(p), "%s/%s/resources", APPS_DIR, id);
    eos_test_rmdir(p);
    snprintf(p, sizeof(p), "%s/%s", APPS_DIR, id);
    eos_test_rmdir(p);
}

static void clean_fixture(void)
{
    char id[64];
    int i;

    remove(EAPP_PATH);
    remove(RAN_MARKER);
    remove_package_tree(GOOD_ID);
    remove_package_tree(NET_ID);
    remove_package_tree(UNK_ID);
    for (i = 0; i < 8; i++) {
        snprintf(id, sizeof(id), CAP_ID_FMT, i);
        remove_package_tree(id);
    }
    snprintf(id, sizeof(id), "%s/.eapp_db", APPS_DIR);
    remove(id);
    eos_test_rmdir(APPS_DIR);
}

/* Sign the v2 envelope of (h, payload, res) with the test key and embed the
 * signature. */
static void sign_v2(eapp_header_t *h,
                    const uint8_t *payload, uint32_t payload_len,
                    const uint8_t *res, uint32_t res_len)
{
    uint8_t digest[EAPP_HASH_LEN];
    uint8_t pub[EAPP_PUBKEY_LEN], priv[64];
    uint8_t sig[EAPP_SIGNATURE_LEN];

    ed25519_create_keypair(pub, priv, TEST_SEED);
    ASSERT(memcmp(pub, TEST_PUB, EAPP_PUBKEY_LEN) == 0);
    memset(h->signature, 0, sizeof(h->signature));
    eos_pkg_envelope_digest(h, payload, payload_len, res, res_len, digest);
    ed25519_sign(sig, digest, sizeof(digest), pub, priv);
    memset(priv, 0, sizeof(priv));
    memcpy(h->signature, sig, sizeof(sig));
}

/* Write a genuinely signed package with the given capability bits. The
 * payload is the binary; `res` is the resources blob (NULL when absent).
 * binary_size and resources_size may be overridden after signing through
 * the returned header when a test needs a declared size the file does not
 * hold -- the cap tests do that on purpose, unsigned, because the cap
 * fires before the signature is read. */
static void write_eapp_caps(const char *name, const char *id,
                            uint32_t caps,
                            const uint8_t *payload, uint32_t payload_len,
                            const uint8_t *res, uint32_t res_len,
                            uint32_t binary_size_override,
                            uint32_t resources_size_override)
{
    eapp_header_t h;
    EosSha256 c;
    FILE *f;

    memset(&h, 0, sizeof(h));
    h.magic = EAPP_MAGIC;
    h.version = EAPP_VERSION;
    snprintf(h.name, sizeof(h.name), "%s", name);
    snprintf(h.package_id, sizeof(h.package_id), "%s", id);
    h.ver_major = 1;
    h.capabilities = caps;
    h.arch_count = 1;
    memset(h.supported_archs, 1, sizeof(h.supported_archs));
    h.binary_offset = (uint32_t)sizeof(h);
    h.binary_size = payload_len;
    if (res && res_len > 0) {
        h.resources_offset = h.binary_offset + payload_len;
        h.resources_size = res_len;
    }

    eos_sha256_init(&c);
    eos_sha256_update(&c, payload, payload_len);
    eos_sha256_final(&c, h.hash);
    sign_v2(&h, payload, payload_len, res, res_len);

    if (binary_size_override != 0) h.binary_size = binary_size_override;
    if (resources_size_override != 0) h.resources_size = resources_size_override;

    f = create_fixture(EAPP_PATH);
    ASSERT(f != NULL);
    ASSERT(fwrite(&h, sizeof(h), 1, f) == 1);
    ASSERT(fwrite(payload, 1, payload_len, f) == payload_len);
    if (res && res_len > 0)
        ASSERT(fwrite(res, 1, res_len, f) == res_len);
    fclose(f);
}

static void write_runnable_eapp(const char *id, uint32_t caps)
{
    write_eapp_caps(GOOD_NAME, id, caps,
                    (const uint8_t *)PAYLOAD_SCRIPT,
                    (uint32_t)sizeof(PAYLOAD_SCRIPT) - 1,
                    NULL, 0, 0, 0);
}

static void fresh_db(eapp_db_t *db)
{
    clean_fixture();
    ASSERT(eos_pkg_set_trust_anchor(TEST_PUB) == 0);
    ASSERT(eos_pkg_init(db, APPS_DIR) == 0);
    ASSERT(db->count == 0);
}

/* ---- tests --------------------------------------------------------------- */

TEST(test_payload_size_bound_accepts_small_payloads)
{
    /* The bound is on the combined binary + resources size: both blobs are
     * live at once while the signature is checked. */
    ASSERT(eos_pkg_check_payload_sizes(0, 0) == 0);
    ASSERT(eos_pkg_check_payload_sizes(1, 0) == 0);
    ASSERT(eos_pkg_check_payload_sizes(0, 1) == 0);
    ASSERT(eos_pkg_check_payload_sizes(EOS_PKG_MAX_PAYLOAD_BYTES, 0) == 0);
    ASSERT(eos_pkg_check_payload_sizes(0, EOS_PKG_MAX_PAYLOAD_BYTES) == 0);
    ASSERT(eos_pkg_check_payload_sizes(EOS_PKG_MAX_PAYLOAD_BYTES - 1, 1) == 0);
}

TEST(test_payload_size_bound_refuses_over_cap)
{
    ASSERT(eos_pkg_check_payload_sizes(EOS_PKG_MAX_PAYLOAD_BYTES + 1, 0) != 0);
    ASSERT(eos_pkg_check_payload_sizes(0, EOS_PKG_MAX_PAYLOAD_BYTES + 1) != 0);
    ASSERT(eos_pkg_check_payload_sizes(EOS_PKG_MAX_PAYLOAD_BYTES,
                                       EOS_PKG_MAX_PAYLOAD_BYTES) != 0);
    /* Two near-4 GiB declarations must not wrap past the bound. */
    ASSERT(eos_pkg_check_payload_sizes(UINT32_MAX, UINT32_MAX) != 0);
    ASSERT(eos_pkg_check_payload_sizes(UINT32_MAX, 0) != 0);
    ASSERT(eos_pkg_check_payload_sizes(0, UINT32_MAX) != 0);
}

TEST(test_verify_and_install_refuse_a_binary_over_the_cap)
{
    /* The header declares a binary larger than the cap while the file holds
     * one byte. The signature over that header is not valid either -- but
     * the cap fires before any allocation and before the signature is
     * read, so the refusal is by the cap either way, and no allocation of
     * the declared size happens. */
    eapp_db_t db;
    fresh_db(&db);
    write_eapp_caps(GOOD_NAME, GOOD_ID, 0, T2_MSG, (uint32_t)sizeof(T2_MSG),
                    NULL, 0, EOS_PKG_MAX_PAYLOAD_BYTES + 1, 0);

    ASSERT(eos_pkg_verify(EAPP_PATH) != 0);
    ASSERT(eos_pkg_install(&db, EAPP_PATH) != 0);
    ASSERT(db.count == 0);
    ASSERT(!path_exists(APPS_DIR "/" GOOD_ID "/" GOOD_NAME));
}

TEST(test_verify_and_install_refuse_resources_over_the_cap)
{
    /* Same, through the resources blob: a small binary, a resources size
     * over the cap. The combined size is what is bounded. */
    eapp_db_t db;
    fresh_db(&db);
    write_eapp_caps(GOOD_NAME, GOOD_ID, 0, T2_MSG, (uint32_t)sizeof(T2_MSG),
                    NULL, 0, 0, EOS_PKG_MAX_PAYLOAD_BYTES + 1);

    ASSERT(eos_pkg_verify(EAPP_PATH) != 0);
    ASSERT(eos_pkg_install(&db, EAPP_PATH) != 0);
    ASSERT(db.count == 0);
    ASSERT(!path_exists(APPS_DIR "/" GOOD_ID "/" GOOD_NAME));
}

TEST(test_run_refuses_a_package_with_an_unenforceable_capability)
{
    /* The defect as found, stated as a test: a genuinely signed package
     * that declares network. Install still accepts it -- installing stores
     * files, it grants nothing -- but run must refuse: this loader has no
     * confinement mechanism, so it cannot run a network package within its
     * capability. The refusal leaves the package installed, not in error:
     * the package is fine, this loader cannot run it yet. */
    eapp_db_t db;
    eapp_package_t *pkg;
    fresh_db(&db);
    write_runnable_eapp(NET_ID, EAPP_CAP_NETWORK);

    ASSERT(eos_pkg_install(&db, EAPP_PATH) == 0);
    ASSERT(db.count == 1);
    ASSERT(eos_pkg_run(&db, NET_ID, 0, NULL) != 0);

    pkg = eos_pkg_find(&db, NET_ID);
    ASSERT(pkg != NULL);
    ASSERT(pkg->state == EAPP_STATE_INSTALLED);
}

TEST(test_run_refuses_unknown_capability_bits)
{
    /* A bit no released loader defines. The envelope signature covers the
     * header, so the bit is authentic -- and still refused: an unknown
     * capability must never run unenforced by accident. */
    eapp_db_t db;
    eapp_package_t *pkg;
    fresh_db(&db);
    write_runnable_eapp(UNK_ID, 1u << 31);

    ASSERT(eos_pkg_install(&db, EAPP_PATH) == 0);
    ASSERT(eos_pkg_run(&db, UNK_ID, 0, NULL) != 0);

    pkg = eos_pkg_find(&db, UNK_ID);
    ASSERT(pkg != NULL);
    ASSERT(pkg->state == EAPP_STATE_INSTALLED);
}

TEST(test_run_refuses_every_defined_capability)
{
    /* Each of the eight defined bits on its own package: none is
     * enforceable by this loader, so every one is refused at run. */
    eapp_db_t db;
    char id[64];
    int i;
    fresh_db(&db);
    for (i = 0; i < 8; i++) {
        snprintf(id, sizeof(id), CAP_ID_FMT, i);
        write_runnable_eapp(id, 1u << i);
        ASSERT(eos_pkg_install(&db, EAPP_PATH) == 0);
    }
    ASSERT(db.count == 8);
    for (i = 0; i < 8; i++) {
        eapp_package_t *pkg;
        snprintf(id, sizeof(id), CAP_ID_FMT, i);
        ASSERT(eos_pkg_run(&db, id, 0, NULL) != 0);
        pkg = eos_pkg_find(&db, id);
        ASSERT(pkg != NULL);
        ASSERT(pkg->state == EAPP_STATE_INSTALLED);
    }
}

#ifndef _WIN32
TEST(test_run_launches_a_package_with_no_capabilities)
{
    /* The positive control: a package that declares no capabilities asks
     * for nothing the loader would have to enforce, so it runs. Without
     * this the tests above are satisfied by a run() that refuses
     * everything, which is the bug in its other form. The payload is a
     * shell script that touches a marker file; the test waits for the
     * marker, so it proves the child really executed -- and waits before
     * any cleanup deletes the script out from under it. */
    eapp_db_t db;
    eapp_package_t *pkg;
    int waited;
    fresh_db(&db);
    write_runnable_eapp(GOOD_ID, 0);

    ASSERT(eos_pkg_install(&db, EAPP_PATH) == 0);
    ASSERT(eos_pkg_run(&db, GOOD_ID, 0, NULL) == 0);

    pkg = eos_pkg_find(&db, GOOD_ID);
    ASSERT(pkg != NULL);
    ASSERT(pkg->state == EAPP_STATE_RUNNING);

    /* The child runs concurrently; give it up to 2 s to touch the marker. */
    waited = 0;
    while (!path_exists(RAN_MARKER) && waited < 40) {
        usleep(50000);
        waited++;
    }
    ASSERT(path_exists(RAN_MARKER));
    remove(RAN_MARKER);
}
#endif

int main(void)
{
    printf("eos_pkg envelope capabilities\n");

    run_test_payload_size_bound_accepts_small_payloads();
    run_test_payload_size_bound_refuses_over_cap();
    run_test_verify_and_install_refuse_a_binary_over_the_cap();
    run_test_verify_and_install_refuse_resources_over_the_cap();
    run_test_run_refuses_a_package_with_an_unenforceable_capability();
    run_test_run_refuses_unknown_capability_bits();
    run_test_run_refuses_every_defined_capability();
#ifndef _WIN32
    run_test_run_launches_a_package_with_no_capabilities();
#endif

    clean_fixture();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
