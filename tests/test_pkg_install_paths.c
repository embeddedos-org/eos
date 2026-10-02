/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 EoS Project
 *
 * @file test_pkg_install_paths.c
 * @brief A .eapp header names files; it must not name paths.
 *
 * eos_pkg_install() writes the binary to apps_dir/<package_id>/<name> with
 * both fields taken raw from the package header. The signature covers the
 * binary alone, so a genuinely signed package can carry any package_id and
 * name at all: "../escape" put a 0755 binary outside apps_dir, a field with
 * no terminator was printed and joined into paths past the end of the
 * header struct, and because eos_pkg_remove() hands install_path to
 * `rm -rf "..."` through system(), a quote in package_id reached a shell.
 *
 * The packages below are signed with RFC 8032 section 7.1 Test 2, the same
 * vector test_pkg_trust_anchor.c uses, so every refusal here is about the
 * name and never about the signature: the control at the end installs the
 * same payload under a plain name and succeeds.
 */

#include <stdio.h>
#ifdef _WIN32
#include <eos/eos_windows.h>
#include <direct.h>
#define eos_test_rmdir(p) _rmdir(p)
#define eos_test_mkdir(p) _mkdir(p)
#define SEP "\\"   /* what install joins paths with; load_db compares the string exactly */
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#define eos_test_rmdir(p) rmdir(p)
#define eos_test_mkdir(p) mkdir(p, 0755)
#define SEP "/"
#endif
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "eos_pkg.h"
#include <eos/crypto.h>

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

/* ---- RFC 8032 section 7.1, Test 2: a 1-byte message ---------------------- */

static const uint8_t T2_PUB[32] = {
 0x3d,0x40,0x17,0xc3,0xe8,0x43,0x89,0x5a,0x92,0xb7,0x0a,0xa7,0x4d,0x1b,0x7e,0xbc,
 0x9c,0x98,0x2c,0xcf,0x2e,0xc4,0x96,0x8c,0xc0,0xcd,0x55,0xf1,0x2a,0xf4,0x66,0x0c};
static const uint8_t T2_SIG[64] = {
 0x92,0xa0,0x09,0xa9,0xf0,0xd4,0xca,0xb8,0x72,0x0e,0x82,0x0b,0x5f,0x64,0x25,0x40,
 0xa2,0xb2,0x7b,0x54,0x16,0x50,0x3f,0x8f,0xb3,0x76,0x22,0x23,0xeb,0xdb,0x69,0xda,
 0x08,0x5a,0xc1,0xe4,0x3e,0x15,0x99,0x6e,0x45,0x8f,0x36,0x13,0xd0,0xf1,0x1d,0x8c,
 0x38,0x7b,0x2e,0xae,0xb4,0x30,0x2a,0xee,0xb0,0x0d,0x29,0x16,0x12,0xbb,0x0c,0x00};
static const uint8_t T2_MSG[1] = { 0x72 };

/* ---- fixture layout, all relative to the test's working directory -------- */

#define EAPP_PATH   "test_install_paths.eapp"
#define APPS_DIR    "test_install_paths_apps"
#define ESCAPE_DIR  "test_install_paths_escape"   /* APPS_DIR/../ESCAPE_DIR */
#define GOOD_ID     "org.eos.test.paths"
#define GOOD_NAME   "payload-1.0_x"

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

static void remove_tree_best_effort(const char *dir, const char *file)
{
    char p[512];
    snprintf(p, sizeof(p), "%s/%s", dir, file);
    remove(p);
    snprintf(p, sizeof(p), "%s/resources/data.bin", dir);
    remove(p);
    snprintf(p, sizeof(p), "%s/resources", dir);
    eos_test_rmdir(p);
    snprintf(p, sizeof(p), "%s/.eapp_db", dir);
    remove(p);
    eos_test_rmdir(dir);
}

static void clean_fixture(void)
{
    char p[512];
    remove(EAPP_PATH);
    remove("test_install_paths_db_sentinel");
    snprintf(p, sizeof(p), "%s/%s/nested/`touch test_install_paths_db_sentinel`", APPS_DIR, GOOD_ID);
    remove(p);
    snprintf(p, sizeof(p), "%s/%s/nested", APPS_DIR, GOOD_ID);
    eos_test_rmdir(p);
    snprintf(p, sizeof(p), "%s/sibling/keep", APPS_DIR);
    remove(p);
    snprintf(p, sizeof(p), "%s/sibling", APPS_DIR);
    eos_test_rmdir(p);
    remove_tree_best_effort(ESCAPE_DIR, GOOD_NAME);
    remove_tree_best_effort(ESCAPE_DIR, "payload");
    snprintf(p, sizeof(p), "%s/%s", APPS_DIR, GOOD_ID);
    remove_tree_best_effort(p, GOOD_NAME);
    remove_tree_best_effort(APPS_DIR, ".eapp_db");
}

/* Write a genuinely signed package whose name fields are exactly `name_raw`
 * and `id_raw` (raw_len bytes each, copied without a terminator so an
 * unterminated field can be produced on purpose). */
static void write_eapp_named(const uint8_t *name_raw, size_t name_len,
                             const uint8_t *id_raw, size_t id_len)
{
    eapp_header_t h;
    EosSha256 c;
    FILE *f;

    ASSERT(name_len <= sizeof(h.name));
    ASSERT(id_len <= sizeof(h.package_id));

    memset(&h, 0, sizeof(h));
    h.magic = EAPP_MAGIC;
    h.version = EAPP_VERSION;
    memcpy(h.name, name_raw, name_len);
    memcpy(h.package_id, id_raw, id_len);
    h.ver_major = 1;
    h.arch_count = 1;
    memset(h.supported_archs, 1, sizeof(h.supported_archs));
    h.binary_offset = (uint32_t)sizeof(h);
    h.binary_size = (uint32_t)sizeof(T2_MSG);
    memcpy(h.signature, T2_SIG, EAPP_SIGNATURE_LEN);

    eos_sha256_init(&c);
    eos_sha256_update(&c, T2_MSG, sizeof(T2_MSG));
    eos_sha256_final(&c, h.hash);

#ifdef _WIN32
    f = fopen(EAPP_PATH, "wb");
#else
    {
        int fd = open(EAPP_PATH, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
        ASSERT(fd >= 0);
        f = fdopen(fd, "wb");
        if (!f) close(fd);
    }
#endif
    ASSERT(f != NULL);
    ASSERT(fwrite(&h, sizeof(h), 1, f) == 1);
    ASSERT(fwrite(T2_MSG, 1, sizeof(T2_MSG), f) == sizeof(T2_MSG));
    fclose(f);
}

static void write_eapp_with_strings(const char *name, const char *id)
{
    write_eapp_named((const uint8_t *)name, strlen(name),
                     (const uint8_t *)id, strlen(id));
}

static void fresh_db(eapp_db_t *db)
{
    clean_fixture();
    ASSERT(eos_pkg_set_trust_anchor(T2_PUB) == 0);
    ASSERT(eos_pkg_init(db, APPS_DIR) == 0);
    ASSERT(db->count == 0);
}

/* ---- tests --------------------------------------------------------------- */

/* The defect as found: a signed package whose package_id climbs out of
 * apps_dir. Before the fix this installed, returned 0, and left a 0755
 * binary at APPS_DIR/../ESCAPE_DIR/payload. */
TEST(test_install_refuses_a_package_id_that_leaves_apps_dir)
{
    eapp_db_t db;
    fresh_db(&db);
    write_eapp_with_strings("payload", "../" ESCAPE_DIR);

    ASSERT(eos_pkg_install(&db, EAPP_PATH) != 0);
    ASSERT(!path_exists(ESCAPE_DIR "/payload"));
    ASSERT(!path_exists(ESCAPE_DIR));
    ASSERT(db.count == 0);
}

/* Same climb through the other field: the binary name. */
TEST(test_install_refuses_a_name_with_a_path_separator)
{
    eapp_db_t db;
    fresh_db(&db);
    write_eapp_with_strings("../../" ESCAPE_DIR "/payload", GOOD_ID);

    ASSERT(eos_pkg_install(&db, EAPP_PATH) != 0);
    ASSERT(!path_exists(ESCAPE_DIR "/payload"));
    ASSERT(!path_exists(APPS_DIR "/" GOOD_ID "/payload"));
    ASSERT(db.count == 0);

    write_eapp_with_strings("sub\\payload", GOOD_ID);
    ASSERT(eos_pkg_install(&db, EAPP_PATH) != 0);
    ASSERT(db.count == 0);
}

/* "." and ".." are components, but not names anything may be installed as. */
TEST(test_install_refuses_dot_and_dotdot)
{
    eapp_db_t db;
    fresh_db(&db);

    write_eapp_with_strings("payload", "..");
    ASSERT(eos_pkg_install(&db, EAPP_PATH) != 0);
    ASSERT(!path_exists("payload"));            /* would have been APPS_DIR/../payload */

    write_eapp_with_strings("payload", ".");
    ASSERT(eos_pkg_install(&db, EAPP_PATH) != 0);
    ASSERT(!path_exists(APPS_DIR "/payload"));

    write_eapp_with_strings("..", GOOD_ID);
    ASSERT(eos_pkg_install(&db, EAPP_PATH) != 0);
    ASSERT(db.count == 0);
}

/* install_path is later handed to `rm -rf "..."` through system(), so the
 * characters a shell reads are refused with the separators. */
TEST(test_install_refuses_shell_metacharacters_in_package_id)
{
    static const char *bad[] = {
        "x\" ; echo pwned ; \"", "x;y", "x y", "x$HOME", "x`id`", "x|y", "x&y",
        "x>y", "x'y", "x*", "x?", "x\n", "x\t",
    };
    eapp_db_t db;
    fresh_db(&db);
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        write_eapp_with_strings("payload", bad[i]);
        ASSERT(eos_pkg_install(&db, EAPP_PATH) != 0);
        ASSERT(db.count == 0);
    }
}

/* A field that fills all 64 bytes has no terminator. Before the fix the
 * strings were printed and joined into paths with %s, reading past the
 * header struct on the stack. Both verify() and install() must refuse. */
TEST(test_unterminated_name_fields_are_refused)
{
    uint8_t full[EAPP_MAX_NAME];
    eapp_db_t db;
    fresh_db(&db);
    memset(full, 'A', sizeof(full));

    write_eapp_named(full, sizeof(full), (const uint8_t *)GOOD_ID, strlen(GOOD_ID));
    ASSERT(eos_pkg_verify(EAPP_PATH) != 0);
    ASSERT(eos_pkg_install(&db, EAPP_PATH) != 0);

    write_eapp_named((const uint8_t *)GOOD_NAME, strlen(GOOD_NAME), full, sizeof(full));
    ASSERT(eos_pkg_verify(EAPP_PATH) != 0);
    ASSERT(eos_pkg_install(&db, EAPP_PATH) != 0);
    ASSERT(db.count == 0);
}

/* An empty field is not a name either: an empty package_id would install
 * into apps_dir itself and an empty name would write to the directory. */
TEST(test_empty_name_fields_are_refused)
{
    eapp_db_t db;
    fresh_db(&db);

    write_eapp_with_strings("payload", "");
    ASSERT(eos_pkg_install(&db, EAPP_PATH) != 0);
    write_eapp_with_strings("", GOOD_ID);
    ASSERT(eos_pkg_install(&db, EAPP_PATH) != 0);
    ASSERT(db.count == 0);
}

/* update() reads the header before it verifies anything; the same rule
 * applies there, and an unterminated id must not be looked up or printed. */
TEST(test_update_refuses_a_bad_package_id_before_looking_it_up)
{
    uint8_t full[EAPP_MAX_NAME];
    eapp_db_t db;
    fresh_db(&db);
    memset(full, 'B', sizeof(full));

    write_eapp_with_strings("payload", "../" ESCAPE_DIR);
    ASSERT(eos_pkg_update(&db, EAPP_PATH) != 0);
    write_eapp_named((const uint8_t *)GOOD_NAME, strlen(GOOD_NAME), full, sizeof(full));
    ASSERT(eos_pkg_update(&db, EAPP_PATH) != 0);
    ASSERT(!path_exists(ESCAPE_DIR));
}

/* The db is the other ingress. A record written by an earlier version of
 * this tool from an unvalidated header carries whatever the header did, and
 * remove() used to hand its install_path to `rm -rf "..."` through
 * system(): install_path = apps/x" ; touch sentinel ; echo " ran the touch
 * and reported the package removed. load_db() now drops a record whose
 * names are not names, or whose install_path is not the one install builds,
 * so remove() and stop() never see it. */
#define DB_SENTINEL "test_install_paths_db_sentinel"

TEST(test_load_db_drops_a_record_that_would_reach_a_shell)
{
    eapp_db_t db;
    fresh_db(&db);
    remove(DB_SENTINEL);

    /* Write the db the way the pre-fix tool would have: a record whose
     * install_path carries a shell escape, and one whose name does. */
    eapp_package_t *legacy = &db.packages[0];
    memset(legacy, 0, sizeof(*legacy));
    snprintf(legacy->name, sizeof(legacy->name), "%s", "payload");
    snprintf(legacy->package_id, sizeof(legacy->package_id), "%s", "legacy");
    snprintf(legacy->install_path, sizeof(legacy->install_path),
             "%s/x\" ; touch " DB_SENTINEL " ; echo \"", APPS_DIR);
    legacy->state = EAPP_STATE_INSTALLED;
    eapp_package_t *hostile = &db.packages[1];
    memset(hostile, 0, sizeof(*hostile));
    snprintf(hostile->name, sizeof(hostile->name), "%s", "x\" ; touch " DB_SENTINEL " ; \"");
    snprintf(hostile->package_id, sizeof(hostile->package_id), "%s", "hostile");
    snprintf(hostile->install_path, sizeof(hostile->install_path), "%s" SEP "hostile", APPS_DIR);
    hostile->state = EAPP_STATE_RUNNING;
    eapp_package_t *fine = &db.packages[2];
    memset(fine, 0, sizeof(*fine));
    snprintf(fine->name, sizeof(fine->name), "%s", GOOD_NAME);
    snprintf(fine->package_id, sizeof(fine->package_id), "%s", GOOD_ID);
    snprintf(fine->install_path, sizeof(fine->install_path), "%s" SEP "%s", APPS_DIR, GOOD_ID);
    fine->state = EAPP_STATE_INSTALLED;
    db.count = 3;
    ASSERT(eos_pkg_save_db(&db) == 0);

    /* Reload: two records dropped, the well-formed one kept. */
    ASSERT(eos_pkg_init(&db, APPS_DIR) == 0);
    ASSERT(db.count == 1);
    ASSERT(strcmp(db.packages[0].package_id, GOOD_ID) == 0);
    ASSERT(eos_pkg_find(&db, "legacy") == NULL);
    ASSERT(eos_pkg_find(&db, "hostile") == NULL);

    /* Neither can be acted on, and nothing ran. */
    ASSERT(eos_pkg_remove(&db, "legacy") != 0);
    ASSERT(eos_pkg_stop(&db, "hostile") != 0);
    ASSERT(!path_exists(DB_SENTINEL));
}

/* remove() takes the package directory down with a walk in C: a file whose
 * name would have been shell syntax is just a file, and only the directory
 * install built is touched -- a sibling stays. */
TEST(test_remove_deletes_the_package_tree_and_nothing_else)
{
    eapp_db_t db;
    char p[512];
    FILE *f;
    fresh_db(&db);
    write_eapp_with_strings(GOOD_NAME, GOOD_ID);
    ASSERT(eos_pkg_install(&db, EAPP_PATH) == 0);
    ASSERT(db.count == 1);

    /* Extra content under the install dir, including a nested directory
     * and a file with backticks in its name. */
    snprintf(p, sizeof(p), "%s/%s/nested", APPS_DIR, GOOD_ID);
    ASSERT(eos_test_mkdir(p) == 0);
    snprintf(p, sizeof(p), "%s/%s/nested/`touch " DB_SENTINEL "`", APPS_DIR, GOOD_ID);
    f = create_fixture(p); ASSERT(f != NULL); fputs("x", f); fclose(f);
    /* A sibling package directory that must survive. */
    snprintf(p, sizeof(p), "%s/sibling", APPS_DIR);
    ASSERT(eos_test_mkdir(p) == 0);
    snprintf(p, sizeof(p), "%s/sibling/keep", APPS_DIR);
    f = create_fixture(p); ASSERT(f != NULL); fputs("x", f); fclose(f);
#ifndef _WIN32
    /* And a symlink from inside the package to the sibling: the walk must
     * remove the link, not what it points at. */
    snprintf(p, sizeof(p), "%s/%s/nested/link-to-sibling", APPS_DIR, GOOD_ID);
    ASSERT(symlink("../../sibling", p) == 0);
#endif

    remove(DB_SENTINEL);
    ASSERT(eos_pkg_remove(&db, GOOD_ID) == 0);
    ASSERT(db.count == 0);
    snprintf(p, sizeof(p), "%s/%s", APPS_DIR, GOOD_ID);
    ASSERT(!path_exists(p));
    snprintf(p, sizeof(p), "%s/sibling/keep", APPS_DIR);
    ASSERT(path_exists(p));
    ASSERT(!path_exists(DB_SENTINEL));

    /* And a record whose install_path is not the directory install built is
     * refused at the point of removal too, even if it got into memory. */
    fresh_db(&db);
    write_eapp_with_strings(GOOD_NAME, GOOD_ID);
    ASSERT(eos_pkg_install(&db, EAPP_PATH) == 0);
    snprintf(db.packages[0].install_path, sizeof(db.packages[0].install_path), "%s", APPS_DIR);
    ASSERT(eos_pkg_remove(&db, GOOD_ID) != 0);
    ASSERT(path_exists(APPS_DIR));
    snprintf(p, sizeof(p), "%s/sibling/keep", APPS_DIR);
    remove(p);
    snprintf(p, sizeof(p), "%s/sibling", APPS_DIR);
    eos_test_rmdir(p);
}

/* The control: the same signed payload under a plain reverse-DNS id and a
 * name with every allowed punctuation character installs where it should. */
TEST(test_install_accepts_a_plain_name)
{
    eapp_db_t db;
    fresh_db(&db);
    write_eapp_with_strings(GOOD_NAME, GOOD_ID);

    ASSERT(eos_pkg_verify(EAPP_PATH) == 0);
    ASSERT(eos_pkg_install(&db, EAPP_PATH) == 0);
    ASSERT(db.count == 1);
    ASSERT(path_exists(APPS_DIR "/" GOOD_ID "/" GOOD_NAME));
    ASSERT(strcmp(db.packages[0].package_id, GOOD_ID) == 0);
    ASSERT(strcmp(db.packages[0].name, GOOD_NAME) == 0);
}

int main(void)
{
    printf("=== test_pkg_install_paths ===\n");
    run_test_install_refuses_a_package_id_that_leaves_apps_dir();
    run_test_install_refuses_a_name_with_a_path_separator();
    run_test_install_refuses_dot_and_dotdot();
    run_test_install_refuses_shell_metacharacters_in_package_id();
    run_test_unterminated_name_fields_are_refused();
    run_test_empty_name_fields_are_refused();
    run_test_update_refuses_a_bad_package_id_before_looking_it_up();
    run_test_load_db_drops_a_record_that_would_reach_a_shell();
    run_test_remove_deletes_the_package_tree_and_nothing_else();
    run_test_install_accepts_a_plain_name();
    clean_fixture();
    printf("%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
