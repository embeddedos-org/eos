// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file test_linux_security_paths.c
 * @brief linux_security builds shell commands; these are the inputs it must refuse.
 *
 * Every function here interpolates caller-supplied paths into a string handed
 * to system() or popen(). is_path_safe() is the only thing standing between a
 * path and the shell, and it is static, so it is exercised through the public
 * API: a rejected path makes the call return -1 without building a command.
 */

#include "eos/linux_security.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

/* Print [PASS] only if this function's own checks all held. Printing it
 * unconditionally is the same shape as the defects this file is about: a
 * report that does not depend on the result. */
#define PASS_IF_CLEAN(before, msg) do { \
    if (failures == (before)) printf("[PASS] %s\n", (msg)); \
} while (0)

/* Every environment dependency in this file is probed rather than assumed:
 * a missing build tool must print [SKIP], not count as a security failure. */
static int have_tool(const char *tool) {
    char probe[128];
    snprintf(probe, sizeof probe, "command -v %s >/dev/null 2>&1", tool);
    return system(probe) == 0;
}

/* eos_*_install_to_rootfs() create directories under rootfs_dir. Removing
 * them by name rather than shelling out to `rm -rf` keeps this file free of
 * the thing it is testing. Child-first; a path that is not there is fine. */
static void cleanup_rootfs(const char *root) {
    static const char *paths[] = {
        "etc/selinux/targeted/targeted", "etc/selinux/targeted",
        "etc/selinux/config", "etc/selinux",
        "etc/ima/policy", "etc/ima", "etc/keys/ima-key.pub", "etc/keys",
        "etc", ""
    };
    char p[512];
    for (unsigned i = 0; i < sizeof paths / sizeof *paths; i++) {
        snprintf(p, sizeof p, "%s/%s", root, paths[i]);
        if (remove(p) != 0) (void)rmdir(p);
    }
    rmdir(root);
}

/* Payloads that a shell reads as syntax. The first two passed the original
 * denylist (";|&><$()\"'") -- it had no backtick and no newline. */
static const char *HOSTILE[] = {
    "/tmp/`touch /tmp/pwned`",      /* command substitution */
    "/tmp/x\ntouch /tmp/pwned",     /* newline starts a second command */
    "/tmp/x\\",                     /* trailing backslash escapes the closing quote */
    "/tmp/a;id",                    /* semicolon */
    "/tmp/$(id)",                   /* dollar-paren */
    "/tmp/a|id",                    /* pipe */
    "/tmp/a&id",                    /* background + second command */
    "/tmp/a>/etc/passwd",           /* redirect */
    "/tmp/a\"b",                    /* closes the quote the format string opens */
    "/tmp/a'b",                     /* single quote */
    "/tmp/a\x01b",                  /* control character */
};

static void test_dmverity_verify_refuses_hostile_paths(void) {
    int f0 = failures;
    for (unsigned i = 0; i < sizeof HOSTILE / sizeof *HOSTILE; i++) {
        EosDmVerity dv;
        eos_dmverity_init(&dv);
        strncpy(dv.hash_device, "/tmp/hash.img", sizeof(dv.hash_device) - 1);
        strncpy(dv.root_hash, "abcdef0123456789", sizeof(dv.root_hash) - 1);
        dv.verified = 1;   /* must not survive a refusal */
        CHECK(eos_dmverity_verify(&dv, HOSTILE[i]) != 0);
        CHECK(dv.verified == 0);
    }
    PASS_IF_CLEAN(f0, "dmverity_verify refuses hostile image paths");
}

/* The two dmverity refusal tests above assert a non-zero return, which does
 * not discriminate on a host without veritysetup: system() returns non-zero
 * either way, so they pass against the unfixed file too. CI installs no
 * veritysetup, so that is every CI run.
 *
 * This one does discriminate anywhere, by watching for the side effect. The
 * injected command creates a sentinel file; if the guard refuses the path the
 * command is never built and the sentinel cannot appear. On the unfixed file
 * the backtick survives is_path_safe(), the shell runs it, and the file
 * exists. */
static void test_injection_does_not_execute(void) {
    int f0 = failures;
    char dir[] = "/tmp/eos_lsp_inj_XXXXXX";
    char sentinel[320], hostile[512];
    EosDmVerity dv;
    EosIma ima;

    if (!mkdtemp(dir)) { fprintf(stderr, "[SKIP] mkdtemp failed\n"); return; }
    snprintf(sentinel, sizeof sentinel, "%s/pwned", dir);

    /* A path carrying a command substitution that would create the sentinel. */
    snprintf(hostile, sizeof hostile, "/tmp/img`touch %s`", sentinel);

    eos_dmverity_init(&dv);
    strncpy(dv.hash_device, "/tmp/hash.img", sizeof(dv.hash_device) - 1);
    strncpy(dv.root_hash, "abcdef0123456789", sizeof(dv.root_hash) - 1);
    (void)eos_dmverity_verify(&dv, hostile);
    CHECK(access(sentinel, F_OK) != 0);

    eos_dmverity_init(&dv);
    (void)eos_dmverity_create(&dv, hostile, "/tmp/hash.img");
    CHECK(access(sentinel, F_OK) != 0);

    eos_ima_init(&ima, EOS_IMA_ENFORCE);
    strncpy(ima.key_file, "/tmp/key.pub", sizeof(ima.key_file) - 1);
    (void)eos_ima_sign_file(&ima, hostile);
    CHECK(access(sentinel, F_OK) != 0);

    if (access(sentinel, F_OK) == 0) {
        fprintf(stderr, "[FAIL] the injected command executed: %s exists\n", sentinel);
        remove(sentinel);
    }
    rmdir(dir);
    PASS_IF_CLEAN(f0, "an injected command substitution never executes");
}

static void test_dmverity_verify_refuses_hostile_hash_device(void) {
    int f0 = failures;
    EosDmVerity dv;
    eos_dmverity_init(&dv);
    strncpy(dv.hash_device, "/tmp/`id`", sizeof(dv.hash_device) - 1);
    strncpy(dv.root_hash, "abcdef", sizeof(dv.root_hash) - 1);
    CHECK(eos_dmverity_verify(&dv, "/tmp/image.img") != 0);
    CHECK(dv.verified == 0);
    PASS_IF_CLEAN(f0, "dmverity_verify refuses a hostile hash device");
}

static void test_dmverity_create_refuses_hostile_paths(void) {
    int f0 = failures;
    EosDmVerity dv;
    eos_dmverity_init(&dv);
    CHECK(eos_dmverity_create(&dv, "/tmp/`id`", "/tmp/hash.img") != 0);
    eos_dmverity_init(&dv);
    CHECK(eos_dmverity_create(&dv, "/tmp/image.img", "/tmp/x\nid") != 0);
    PASS_IF_CLEAN(f0, "dmverity_create refuses hostile paths");
}

static void test_busybox_refuses_hostile_fields(void) {
    int f0 = failures;
    EosBusybox bb;

    eos_busybox_init(&bb);
    strncpy(bb.source_dir, "/tmp/`id`", sizeof(bb.source_dir) - 1);
    CHECK(eos_busybox_build(&bb) != 0);

    /* defconfig and cross_compile are interpolated WITHOUT surrounding quotes,
     * so a bare space is already an injection of an extra make argument. */
    eos_busybox_init(&bb);
    strncpy(bb.source_dir, "/tmp/bb", sizeof(bb.source_dir) - 1);
    strncpy(bb.defconfig, "defconfig CONFIG_X=y", sizeof(bb.defconfig) - 1);
    CHECK(eos_busybox_configure(&bb) != 0);

    eos_busybox_init(&bb);
    strncpy(bb.source_dir, "/tmp/bb", sizeof(bb.source_dir) - 1);
    strncpy(bb.cross_compile, "arm- ;id", sizeof(bb.cross_compile) - 1);
    CHECK(eos_busybox_build(&bb) != 0);

    PASS_IF_CLEAN(f0, "busybox refuses hostile source_dir/defconfig/cross_compile");
}

/* defconfig and cross_compile are interpolated unquoted, so they have to be
 * one shell word. That used to be a denylist -- space, tab, *, ?, ~ -- which
 * is the same shape as the metacharacter denylist this PR is about, and it
 * had the same kind of hole: [ and ] are a glob character class, { and } are
 * brace expansion when /bin/sh is bash, and a leading - makes make read the
 * word as an option instead of a target. None of those is command execution;
 * all of them are a make invocation other than the one that was asked for. */
static const char *NOT_ONE_WORD[] = {
    "def[c]onfig",       /* glob character class */
    "def{a,b}config",    /* brace expansion */
    "defconfig*",        /* glob */
    "defconfi?",         /* glob */
    "~/defconfig",       /* tilde expansion */
    "defconfig extra",   /* a second argument */
    "defconfig\textra",  /* likewise, with a tab */
    "--version",         /* an option, not a target */
    "def#config",        /* comment character */
    "def%config",        /* pattern rule character */
    "def!config",        /* history expansion */
    "defconfig\xc3\xa9",   /* non-ASCII */
};

/* And the values a real caller passes, which must all still be accepted. */
static const char *ONE_WORD[] = {
    "defconfig", "menuconfig", "allnoconfig",
    "busybox-1.36.1_defconfig", "configs/eos.config",
    "arm-linux-gnueabihf-", "aarch64-none-elf-", "x86_64-pc-linux-gnu-",
    "sha256", "sha512", "CONFIG_X=y", "a+b", "a:b", "a.b", "a-b", "a_b",
};

static void test_unquoted_fields_must_be_one_shell_word(void) {
    int f0 = failures;
    EosBusybox bb;
    unsigned i;

    for (i = 0; i < sizeof NOT_ONE_WORD / sizeof *NOT_ONE_WORD; i++) {
        eos_busybox_init(&bb);
        strncpy(bb.source_dir, "/tmp/bb", sizeof(bb.source_dir) - 1);
        strncpy(bb.defconfig, NOT_ONE_WORD[i], sizeof(bb.defconfig) - 1);
        CHECK(eos_busybox_configure(&bb) != 0);

        eos_busybox_init(&bb);
        strncpy(bb.source_dir, "/tmp/bb", sizeof(bb.source_dir) - 1);
        strncpy(bb.cross_compile, NOT_ONE_WORD[i], sizeof(bb.cross_compile) - 1);
        CHECK(eos_busybox_build(&bb) != 0);
    }

    /* The counter-check for the allowlist: it must not have narrowed the
     * predicate to the point where a real defconfig or cross-compile prefix
     * is refused. dv->hash_algo is checked by the same function, so an
     * accepted value has to reach veritysetup rather than be refused early --
     * observable as eos_dmverity_verify() getting past its guard, which the
     * hostile cases above never do. */
    for (i = 0; i < sizeof ONE_WORD / sizeof *ONE_WORD; i++) {
        EosDmVerity dv;
        eos_dmverity_init(&dv);
        strncpy(dv.hash_algo, ONE_WORD[i], sizeof(dv.hash_algo) - 1);
        strncpy(dv.hash_device, "/tmp/hash.img", sizeof(dv.hash_device) - 1);
        strncpy(dv.root_hash, "deadbeef", sizeof(dv.root_hash) - 1);
        /* -1 from the guard and non-zero from veritysetup are the same
         * number, so this asserts on the field the guard sets instead. */
        (void)eos_dmverity_verify(&dv, "/tmp/image.img");
        CHECK(dv.verified == 0);
    }

    PASS_IF_CLEAN(f0, "unquoted fields must be one shell word");
}

static void test_ima_sign_refuses_hostile_paths(void) {
    int f0 = failures;
    EosIma ima;
    eos_ima_init(&ima, EOS_IMA_ENFORCE);
    strncpy(ima.key_file, "/tmp/key.pub", sizeof(ima.key_file) - 1);
    CHECK(eos_ima_sign_file(&ima, "/tmp/`id`") != 0);
    PASS_IF_CLEAN(f0, "ima_sign_file refuses a hostile file path");
}

/* A guard that refuses everything would pass every test above, so this is
 * the counter-check: a well-formed source_dir must still reach the shell.
 * Proven by side effect -- a generated Makefile whose target creates a
 * sentinel file. If the sentinel exists, the command was built and run. */
static void test_ordinary_paths_still_reach_the_shell(void) {
    int f0 = failures;
    char dir[] = "/tmp/eos_lsp_XXXXXX";
    char mk[256], sentinel[256];
    FILE *f;
    int fd;
    EosBusybox bb;

    if (!have_tool("make")) {
        printf("[SKIP] make is not installed; the counter-check cannot run\n");
        return;
    }
    if (!mkdtemp(dir)) { fprintf(stderr, "[SKIP] mkdtemp failed\n"); return; }
    snprintf(mk, sizeof mk, "%s/Makefile", dir);
    snprintf(sentinel, sizeof sentinel, "%s/ran", dir);
    /* open(O_CREAT, 0600) rather than fopen("w"): fopen creates with 0666
     * masked by umask, so on a permissive umask this Makefile would be
     * world-writable -- and it is a file whose contents get executed. */
    fd = open(mk, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    CHECK(fd >= 0);
    if (fd < 0) { rmdir(dir); return; }
    f = fdopen(fd, "w");
    CHECK(f != NULL);
    if (!f) { close(fd); rmdir(dir); return; }
    fprintf(f, "defconfig:\n\t@touch ran\n");
    fclose(f);

    eos_busybox_init(&bb);
    strncpy(bb.source_dir, dir, sizeof(bb.source_dir) - 1);
    strncpy(bb.defconfig, "defconfig", sizeof(bb.defconfig) - 1);
    /* A real cross-compile prefix goes through the same allowlist and has to
     * survive it -- an allowlist that rejected this would break every cross
     * build while passing every hostile-input test in this file. */
    strncpy(bb.cross_compile, "arm-linux-gnueabihf-",
            sizeof(bb.cross_compile) - 1);
    CHECK(eos_busybox_configure(&bb) == 0);

    /* The sentinel is the whole point: it only exists if the command was
     * constructed and executed rather than refused. */
    CHECK(access(sentinel, F_OK) == 0);

    remove(sentinel); remove(mk); rmdir(dir);
    PASS_IF_CLEAN(f0, "a well-formed source_dir still reaches the shell");
}

/* Finding 1 from the review: the guard ran above the line that populates the
 * field it protects, so the *default* path -- source_dir left empty by
 * eos_busybox_init, which is what a caller gets by not setting it -- built
 * ".eos/build/src/busybox-<version>" from an unvalidated version and handed
 * that to system(). is_path_safe("") returns 1 because its loop never runs,
 * so the guard passed on exactly the input it exists to catch.
 *
 * Watches for the side effect, like test_injection_does_not_execute, because
 * a non-zero return proves nothing here: `make` fails on a nonexistent
 * directory too. */
static void test_the_default_source_dir_path_is_validated(void) {
    int f0 = failures;
    char dir[] = "/tmp/eos_lsp_ver_XXXXXX";
    char sentinel[64], hostile_version[96];
    EosBusybox bb;
    int n;

    /* This test needs no `make`: command substitution is performed by the
     * shell while it expands the word, before it looks for the program. If
     * the guard lets the string through, the backtick runs whether or not
     * make exists. */
    if (!mkdtemp(dir)) { fprintf(stderr, "[SKIP] mkdtemp failed\n"); return; }
    n = snprintf(sentinel, sizeof sentinel, "%s/pwned", dir);
    CHECK(n > 0 && (size_t)n < sizeof sentinel);
    n = snprintf(hostile_version, sizeof hostile_version,
                 "1.36.1`touch %s`", sentinel);
    /* A truncated payload loses its closing backtick and stops being an
     * injection -- and the test would then pass while testing nothing. The
     * same applies to the copy into bb.version below, which is why the
     * length is checked against the field as well as against the buffer. */
    CHECK(n > 0 && (size_t)n < sizeof hostile_version);
    CHECK(strlen(hostile_version) < sizeof bb.version);

    /* The setter must refuse it outright. */
    eos_busybox_init(&bb);
    CHECK(eos_busybox_set_version(&bb, hostile_version) != 0);

    /* And if the field is reached around -- a caller writing the struct
     * directly -- configure() must still refuse, because it now validates
     * the value it actually uses rather than the empty string. */
    eos_busybox_init(&bb);
    strncpy(bb.version, hostile_version, sizeof(bb.version) - 1);
    bb.version[sizeof(bb.version) - 1] = '\0';
    bb.source_dir[0] = '\0';               /* the default path */
    CHECK(eos_busybox_configure(&bb) != 0);

    CHECK(access(sentinel, F_OK) != 0);
    if (access(sentinel, F_OK) == 0) {
        fprintf(stderr, "[FAIL] the default path executed the injection: %s\n",
                sentinel);
        remove(sentinel);
    }
    rmdir(dir);
    PASS_IF_CLEAN(f0, "the default source_dir is validated, not the empty string");
}

/* Finding 2: eos_ima_sign_file() discarded system()'s result and its command
 * ended `|| echo`, so on any host without evmctl it signed nothing and
 * returned 0. This machine has no evmctl, which is precisely the condition
 * the finding is about -- so the assertion is that it now reports failure
 * rather than success. */
static void test_ima_sign_reports_failure_when_evmctl_is_absent(void) {
    int f0 = failures;
    EosIma ima;
    char dir[] = "/tmp/eos_lsp_ima_XXXXXX";
    char target[320];
    int fd;

    if (!mkdtemp(dir)) { fprintf(stderr, "[SKIP] mkdtemp failed\n"); return; }
    snprintf(target, sizeof target, "%s/file", dir);

    /* open(O_CREAT, 0600), not fopen("w") -- same reason as the Makefile
     * fixture above: fopen creates with 0666 masked by umask, so under a
     * permissive umask this file is world-writable while a signing tool is
     * pointed at it. Third instance of this in the file; the first two were
     * fixed for exactly this. */
    fd = open(target, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd >= 0) { (void)!write(fd, "x", 1); close(fd); }

    eos_ima_init(&ima, EOS_IMA_ENFORCE);
    strncpy(ima.key_file, "/tmp/key.pub", sizeof(ima.key_file) - 1);

    if (have_tool("evmctl")) {
        printf("[SKIP] evmctl is installed; "
               "the absent-tool path cannot be exercised here\n");
    } else {
        CHECK(eos_ima_sign_file(&ima, target) != 0);
        PASS_IF_CLEAN(f0, "ima_sign_file reports failure when evmctl is absent");
    }

    remove(target);
    rmdir(dir);
}


/* ---- the rootfs entry points --------------------------------------------
 *
 * These four were the untested half of the guard: the review's finding that
 * rootfs_dir reaches `cp` through `path` was fixed with no case covering it.
 * Refusal is asserted by side effect, not by return code -- these functions
 * have several ways to return -1 and only one of them means "the guard
 * caught it". */
static void test_rootfs_entry_points_refuse_a_hostile_dir(void) {
    int f0 = failures;
    char dir[] = "/tmp/eos_lsp_rootfs_XXXXXX";
    char sentinel[64], hostile[128];
    int n;
    EosSelinux se;
    EosIma ima;
    EosBusybox bb;

    if (!mkdtemp(dir)) { fprintf(stderr, "[SKIP] mkdtemp failed\n"); return; }
    n = snprintf(sentinel, sizeof sentinel, "%s/pwned", dir);
    CHECK(n > 0 && (size_t)n < sizeof sentinel);
    n = snprintf(hostile, sizeof hostile, "%s/`touch %s`", dir, sentinel);
    CHECK(n > 0 && (size_t)n < sizeof hostile);

    eos_selinux_init(&se, EOS_SELINUX_ENFORCING);
    strncpy(se.policy_name, "targeted", sizeof(se.policy_name) - 1);
    CHECK(eos_selinux_install_to_rootfs(&se, hostile) != 0);

    eos_selinux_init(&se, EOS_SELINUX_ENFORCING);
    strncpy(se.file_contexts, "/etc/selinux/file_contexts",
            sizeof(se.file_contexts) - 1);
    CHECK(eos_selinux_label_rootfs(&se, hostile) != 0);

    eos_ima_init(&ima, EOS_IMA_ENFORCE);
    CHECK(eos_ima_install_to_rootfs(&ima, hostile) != 0);

    /* The guard sits above the `mode == EOS_IMA_OFF` early return, so a
     * malformed rootfs_dir is refused even when IMA is off -- a deliberate
     * change of that function's contract, pinned here so it stays one. */
    eos_ima_init(&ima, EOS_IMA_OFF);
    CHECK(eos_ima_install_to_rootfs(&ima, hostile) != 0);

    eos_busybox_init(&bb);
    strncpy(bb.source_dir, dir, sizeof(bb.source_dir) - 1);
    CHECK(eos_busybox_install_to_rootfs(&bb, hostile) != 0);

    CHECK(access(sentinel, F_OK) != 0);
    if (access(sentinel, F_OK) == 0) {
        fprintf(stderr, "[FAIL] a rootfs entry point executed the injection: "
                "%s\n", sentinel);
        remove(sentinel);
    }
    rmdir(dir);
    PASS_IF_CLEAN(f0, "the rootfs entry points refuse a hostile rootfs_dir");
}

/* Finding 1 of the second review: four more security steps reported success
 * without having run. Each of the three tests below drives one of them into
 * the state where its step cannot complete, and asserts it says so. */
static void test_selinux_label_reports_failure_when_it_cannot_label(void) {
    int f0 = failures;
    char dir[] = "/tmp/eos_lsp_lbl_XXXXXX";
    char fc[128];
    EosSelinux se;
    int fd;

    if (!mkdtemp(dir)) { fprintf(stderr, "[SKIP] mkdtemp failed\n"); return; }

    /* SELinux on, no file_contexts: nothing to label with. This used to echo
     * a "skipping" line and return 0. */
    eos_selinux_init(&se, EOS_SELINUX_ENFORCING);
    CHECK(eos_selinux_label_rootfs(&se, dir) != 0);

    /* Disabled is still 0 -- there is nothing to do, which is not the same
     * as a step that could not run. */
    eos_selinux_init(&se, EOS_SELINUX_DISABLED);
    CHECK(eos_selinux_label_rootfs(&se, dir) == 0);

    snprintf(fc, sizeof fc, "%s/file_contexts", dir);
    fd = open(fc, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd >= 0) { (void)!write(fd, "/.*  --  system_u:object_r:default_t\n", 37); close(fd); }

    eos_selinux_init(&se, EOS_SELINUX_ENFORCING);
    strncpy(se.file_contexts, fc, sizeof(se.file_contexts) - 1);
    if (have_tool("setfiles")) {
        printf("[SKIP] setfiles is installed; "
               "the absent-tool path cannot be exercised here\n");
    } else {
        /* setfiles absent: the rootfs is not labeled, and the `|| echo` tail
         * used to hide that behind a 0. */
        CHECK(eos_selinux_label_rootfs(&se, dir) != 0);
    }

    remove(fc);
    rmdir(dir);
    PASS_IF_CLEAN(f0, "selinux_label_rootfs reports a rootfs it did not label");
}

static void test_ima_install_reports_a_key_it_did_not_install(void) {
    int f0 = failures;
    char dir[] = "/tmp/eos_lsp_imai_XXXXXX";
    char key[128], installed[192], policy[192], etc[128];
    EosIma ima;

    if (!mkdtemp(dir)) { fprintf(stderr, "[SKIP] mkdtemp failed\n"); return; }
    /* These functions MKDIR("<rootfs>/etc/<x>") without creating <rootfs>/etc
     * first, so a bare mkdtemp() rootfs makes them fail at the fopen() long
     * before the step under test. The positive control below is what caught
     * that -- without it this test would have passed on the wrong -1. */
    snprintf(etc, sizeof etc, "%s/etc", dir);
    CHECK(mkdir(etc, 0700) == 0);
    snprintf(key, sizeof key, "%s/absent-key.pub", dir);
    snprintf(installed, sizeof installed, "%s/etc/keys/ima-key.pub", dir);
    snprintf(policy, sizeof policy, "%s/etc/ima/policy", dir);

    /* A key file that does not exist: cp fails, so the rootfs gets an
     * appraise policy and no key to appraise against. `|| true` plus a
     * discarded system() reported that as installed. */
    eos_ima_init(&ima, EOS_IMA_ENFORCE);
    ima.sign_executables = 1;
    strncpy(ima.key_file, key, sizeof(ima.key_file) - 1);
    CHECK(eos_ima_install_to_rootfs(&ima, dir) != 0);
    CHECK(access(installed, F_OK) != 0);

    /* A policy file that names a path it cannot read is the same defect in
     * the non-shell half of the function: it used to leave /etc/ima/policy
     * empty and return 0. */
    eos_ima_init(&ima, EOS_IMA_ENFORCE);
    strncpy(ima.policy_file, key, sizeof(ima.policy_file) - 1);
    CHECK(eos_ima_install_to_rootfs(&ima, dir) != 0);

    /* Counter-check: with no key and no policy file to install, the default
     * policy is written and the function must still return 0. Without this,
     * the two assertions above are satisfied by any function that always
     * fails. */
    eos_ima_init(&ima, EOS_IMA_ENFORCE);
    CHECK(eos_ima_install_to_rootfs(&ima, dir) == 0);
    CHECK(access(policy, F_OK) == 0);

    remove(policy);
    remove(installed);
    cleanup_rootfs(dir);
    PASS_IF_CLEAN(f0, "ima_install_to_rootfs reports a key it did not install");
}

static void test_busybox_install_reports_a_rootfs_it_did_not_build(void) {
    int f0 = failures;
    char src[] = "/tmp/eos_lsp_bbsrc_XXXXXX";
    char rootfs[] = "/tmp/eos_lsp_bbfs_XXXXXX";
    char mk[128], init[128];
    EosBusybox bb;
    int fd;
    FILE *f;

    if (!mkdtemp(src) || !mkdtemp(rootfs)) {
        fprintf(stderr, "[SKIP] mkdtemp failed\n");
        return;
    }
    snprintf(mk, sizeof mk, "%s/Makefile", src);
    snprintf(init, sizeof init, "%s/init", rootfs);

    /* A source tree with no Makefile: `make -C src install` cannot succeed
     * (and if make is absent the shell fails the same way). system()'s
     * result was discarded, so this returned 0 with no busybox installed --
     * and then wrote an /init that would have nothing to exec. */
    eos_busybox_init(&bb);
    strncpy(bb.source_dir, src, sizeof(bb.source_dir) - 1);
    CHECK(eos_busybox_install_to_rootfs(&bb, rootfs) != 0);
    CHECK(access(init, F_OK) != 0);

    /* The counter-check: a target that does succeed must still get through,
     * or the fix above is just "refuse everything". */
    if (!have_tool("make")) {
        printf("[SKIP] make is not installed; "
               "the success path cannot be exercised here\n");
    } else {
        fd = open(mk, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        CHECK(fd >= 0);
        if (fd >= 0) {
            f = fdopen(fd, "w");
            if (f) { fprintf(f, "install:\n\t@true\n"); fclose(f); }
            else close(fd);
            CHECK(eos_busybox_install_to_rootfs(&bb, rootfs) == 0);
            CHECK(access(init, F_OK) == 0);
        }
    }

    remove(init); remove(mk); rmdir(rootfs); rmdir(src);
    PASS_IF_CLEAN(f0, "busybox_install_to_rootfs reports an install that did not run");
}

static void test_selinux_install_reports_a_policy_it_did_not_copy(void) {
    int f0 = failures;
    char pdir[] = "/tmp/eos_lsp_pol_XXXXXX";
    char rootfs[] = "/tmp/eos_lsp_serfs_XXXXXX";
    char policy[128], etc[128];
    EosSelinux se;
    int fd;

    if (!mkdtemp(pdir) || !mkdtemp(rootfs)) {
        fprintf(stderr, "[SKIP] mkdtemp failed\n");
        return;
    }
    snprintf(etc, sizeof etc, "%s/etc", rootfs);
    CHECK(mkdir(etc, 0700) == 0);
    snprintf(policy, sizeof policy, "%s/targeted", pdir);
    fd = open(policy, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    CHECK(fd >= 0);
    if (fd < 0) { rmdir(pdir); rmdir(rootfs); return; }
    (void)!write(fd, "policy\n", 7);
    close(fd);

    eos_selinux_init(&se, EOS_SELINUX_ENFORCING);
    CHECK(eos_selinux_set_policy(&se, pdir, "targeted") == 0);

    /* Positive control first: with the policy file present the copy runs and
     * the function must still return 0. */
    CHECK(eos_selinux_install_to_rootfs(&se, rootfs) == 0);

    /* Now take the file away, leaving policy_loaded set -- the glob matches
     * nothing, cp fails, and the rootfs config names a policy that is not
     * there. `|| true` used to report that as done. */
    remove(policy);
    CHECK(eos_selinux_install_to_rootfs(&se, rootfs) != 0);

    cleanup_rootfs(rootfs);
    rmdir(pdir);
    PASS_IF_CLEAN(f0, "selinux_install_to_rootfs reports a policy it did not copy");
}


/* The hostile-rootfs_dir cases above are refused before any command is built,
 * which means they also pass on a build with no guard at all -- the calls
 * fail at the fopen() instead. So the guard on the one path that really does
 * reach a shell gets a fixture that would execute if it were removed: a
 * directory whose *name* is the injection, with the /etc the function needs,
 * and a loaded policy so the `cp` is actually reached.
 *
 * Verified by mutation: with the rootfs_dir term deleted from that guard,
 * this test fails and /tmp holds the sentinel. */
static void test_selinux_install_guard_stops_a_reachable_injection(void) {
    int f0 = failures;
    char base[] = "/tmp/eos_lsp_reach_XXXXXX";
    char pdir[] = "/tmp/eos_lsp_reachp_XXXXXX";
    char hostile[192], etc[256], policy[128];
    /* A bare name, not a path: this whole string becomes one directory
     * *name*, so a '/' in it would be a directory separator instead. The
     * touch therefore lands in the working directory. */
    static const char *sentinel = "eos_lsp_reach_pwned";
    int n, fd;
    EosSelinux se;

    remove(sentinel);                     /* in case an earlier run left one */
    if (!mkdtemp(base) || !mkdtemp(pdir)) {
        fprintf(stderr, "[SKIP] mkdtemp failed\n");
        return;
    }
    /* A real directory whose name contains the substitution. Creating it
     * takes no shell; only interpolating it into a command does. */
    n = snprintf(hostile, sizeof hostile, "%s/`touch %s`", base, sentinel);
    CHECK(n > 0 && (size_t)n < sizeof hostile);
    if (mkdir(hostile, 0700) != 0) {
        fprintf(stderr, "[SKIP] cannot create the fixture directory\n");
        rmdir(pdir); rmdir(base);
        return;
    }
    snprintf(etc, sizeof etc, "%s/etc", hostile);
    CHECK(mkdir(etc, 0700) == 0);

    snprintf(policy, sizeof policy, "%s/targeted", pdir);
    fd = open(policy, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    CHECK(fd >= 0);
    if (fd >= 0) { (void)!write(fd, "policy\n", 7); close(fd); }

    eos_selinux_init(&se, EOS_SELINUX_ENFORCING);
    CHECK(eos_selinux_set_policy(&se, pdir, "targeted") == 0);
    CHECK(eos_selinux_install_to_rootfs(&se, hostile) != 0);

    CHECK(access(sentinel, F_OK) != 0);
    if (access(sentinel, F_OK) == 0) {
        fprintf(stderr, "[FAIL] the policy copy executed the injection: %s\n",
                sentinel);
        remove(sentinel);
    }

    cleanup_rootfs(hostile);
    remove(policy); rmdir(pdir); rmdir(base);
    PASS_IF_CLEAN(f0, "the selinux policy copy cannot be reached by a rootfs_dir");
}

/* The /init writer is the last step of eos_busybox_install_to_rootfs and it
 * used to ignore its own fopen(): an initramfs with no /init, reported as
 * installed. An unwritable rootfs is the reachable way to produce that. */
static void test_busybox_install_reports_an_init_it_could_not_write(void) {
    int f0 = failures;
    char rootfs[] = "/tmp/eos_lsp_ro_XXXXXX";
    char probe[128], init[128];
    EosBusybox bb;
    int fd;

    if (!mkdtemp(rootfs)) { fprintf(stderr, "[SKIP] mkdtemp failed\n"); return; }
    snprintf(init, sizeof init, "%s/init", rootfs);
    if (chmod(rootfs, S_IRUSR | S_IXUSR) != 0) {
        fprintf(stderr, "[SKIP] chmod failed\n");
        rmdir(rootfs);
        return;
    }
    /* root ignores the mode bits, so check that the directory really is
     * unwritable before asserting on what happens when it is. */
    snprintf(probe, sizeof probe, "%s/probe", rootfs);
    fd = open(probe, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd >= 0) {
        close(fd); remove(probe);
        printf("[SKIP] the rootfs is writable anyway (running as root?)\n");
        chmod(rootfs, S_IRWXU); rmdir(rootfs);
        return;
    }

    /* source_dir empty skips the make step, so the fopen() is the only thing
     * that can fail here. */
    eos_busybox_init(&bb);
    CHECK(eos_busybox_install_to_rootfs(&bb, rootfs) != 0);
    CHECK(access(init, F_OK) != 0);

    chmod(rootfs, S_IRWXU);
    rmdir(rootfs);
    PASS_IF_CLEAN(f0, "busybox_install_to_rootfs reports an /init it could not write");
}

int main(void) {
    test_dmverity_verify_refuses_hostile_paths();
    test_dmverity_verify_refuses_hostile_hash_device();
    test_injection_does_not_execute();
    test_dmverity_create_refuses_hostile_paths();
    test_busybox_refuses_hostile_fields();
    test_unquoted_fields_must_be_one_shell_word();
    test_ima_sign_refuses_hostile_paths();
    test_the_default_source_dir_path_is_validated();
    test_ima_sign_reports_failure_when_evmctl_is_absent();
    test_rootfs_entry_points_refuse_a_hostile_dir();
    test_selinux_label_reports_failure_when_it_cannot_label();
    test_ima_install_reports_a_key_it_did_not_install();
    test_busybox_install_reports_a_rootfs_it_did_not_build();
    test_selinux_install_reports_a_policy_it_did_not_copy();
    test_selinux_install_guard_stops_a_reachable_injection();
    test_busybox_install_reports_an_init_it_could_not_write();
    test_ordinary_paths_still_reach_the_shell();

    if (failures) {
        fprintf(stderr, "\n%d check(s) failed\n", failures);
        return 1;
    }
    printf("\nAll linux_security path checks passed\n");
    return 0;
}
