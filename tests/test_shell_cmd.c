// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file test_shell_cmd.c
 * @brief The checked shell-command builder, and the backends through it.
 *
 * Every build backend hands a command to system(). The text is written in
 * the tree; the paths, targets and option values in it come from a
 * project's configuration. These tests pin the two properties the builder
 * exists for: a configuration value that carries shell syntax is refused
 * before anything runs, and a command that does not fit is refused rather
 * than run truncated. The backend cases below drive the real backend
 * functions and rely on nothing but a shell; on a refusal the return code
 * is EOS_ERR_INVALID and no process is spawned, on every platform.
 */

#include "eos/backend.h"
#include "eos/package.h"
#include "eos/shell_cmd.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        printf("  %-60s ", #name); \
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

/* ---- the two rules ------------------------------------------------- */

TEST(test_arg_rule_accepts_ordinary_paths_and_values)
{
    ASSERT(eos_shell_arg_is_safe("/home/me/project/build dir"));
    ASSERT(eos_shell_arg_is_safe("C:/Users/me/out"));
    ASSERT(eos_shell_arg_is_safe("-O2 -g -DNDEBUG=1"));
    ASSERT(eos_shell_arg_is_safe("arm-none-eabi.cmake"));
}

TEST(test_arg_rule_refuses_every_shell_metacharacter)
{
    /* Each of these, inside double quotes, is still read by a POSIX shell. */
    static const char *const hostile[] = {
        "a;b", "a|b", "a&b", "a>b", "a<b", "a$b", "a(b", "a)b",
        "a\"b", "a'b", "a`b", "a\nb", "a\tb", "a\x7f",
#ifdef _WIN32
        /* cmd.exe expands these inside quotes; a trailing backslash escapes
         * the closing quote for the CRT. */
        "a%PATH%b", "a!X!b", "C:\\dir\\",
#else
        "a\\b",
#endif
    };
    for (size_t i = 0; i < sizeof(hostile) / sizeof(hostile[0]); i++)
        ASSERT(!eos_shell_arg_is_safe(hostile[i]));
    ASSERT(!eos_shell_arg_is_safe(NULL));
    ASSERT(eos_shell_arg_is_safe(""));
}

TEST(test_arg_rule_follows_the_host_shell_on_backslashes)
{
#ifdef _WIN32
    /* The path separator: inert inside cmd.exe double quotes. */
    ASSERT(eos_shell_arg_is_safe("C:\\Users\\me\\build dir\\out.bin"));
    ASSERT(eos_shell_arg_is_safe("test_backend_install_tmp_1740\\cargo-ok-i"));
#else
    /* A POSIX shell still reads \ inside double quotes. */
    ASSERT(!eos_shell_arg_is_safe("C:\\Users\\me"));
#endif
}

TEST(test_word_rule_is_an_allowlist)
{
    ASSERT(eos_shell_word_is_safe("qemu_aarch64_virt_defconfig"));
    ASSERT(eos_shell_word_is_safe("aarch64-linux-gnu"));
    ASSERT(eos_shell_word_is_safe("sim:nsh"));
    ASSERT(eos_shell_word_is_safe("nrf52840dk/nrf52840"));
    ASSERT(eos_shell_word_is_safe(""));
    ASSERT(!eos_shell_word_is_safe("-j8"));            /* would become an option */
    ASSERT(!eos_shell_word_is_safe("a b"));
    ASSERT(!eos_shell_word_is_safe("a*"));
    ASSERT(!eos_shell_word_is_safe("a[0]"));
    ASSERT(!eos_shell_word_is_safe("a{b,c}"));
    ASSERT(!eos_shell_word_is_safe("a;b"));
    ASSERT(!eos_shell_word_is_safe(NULL));
}

/* ---- the builder ------------------------------------------------------ */

TEST(test_builder_quotes_arguments_and_leaves_words_bare)
{
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "make -C ");
    eos_shell_cmd_arg(&cmd, "/src dir");
    eos_shell_cmd_text(&cmd, " -j");
    eos_shell_cmd_int(&cmd, 8);
    eos_shell_cmd_text(&cmd, " ");
    eos_shell_cmd_word(&cmd, "defconfig");
    ASSERT(!cmd.fault);
    ASSERT(strcmp(cmd.buf, "make -C \"/src dir\" -j8 defconfig") == 0);
    ASSERT(cmd.len == strlen(cmd.buf));
}

TEST(test_a_refused_value_marks_the_command_and_stops_it_growing)
{
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "cmake -S ");
    eos_shell_cmd_arg(&cmd, "/src\"; touch pwned; \"");
    eos_shell_cmd_text(&cmd, " -B build");
    ASSERT(cmd.fault);
    /* Nothing after the refusal was appended: the line stops at the text
     * before the bad argument, and run() will not execute it. */
    ASSERT(strcmp(cmd.buf, "cmake -S ") == 0);
    ASSERT(eos_shell_cmd_run(&cmd, "test") == EOS_ERR_INVALID);
}

TEST(test_a_line_that_does_not_fit_is_refused_not_truncated)
{
    EosShellCmd cmd;
    char big[EOS_SHELL_CMD_MAX];
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';

    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "cp ");
    eos_shell_cmd_arg(&cmd, big);          /* 2 quotes + 4095 x: cannot fit */
    ASSERT(cmd.fault);
    ASSERT(strcmp(cmd.buf, "cp ") == 0);   /* left as it was, never partial */
    ASSERT(eos_shell_cmd_run(&cmd, "test") == EOS_ERR_INVALID);

    /* Exactly filling the buffer is fine; one more byte is not. */
    eos_shell_cmd_init(&cmd);
    big[EOS_SHELL_CMD_MAX - 1 - 0] = '\0';
    big[EOS_SHELL_CMD_MAX - 1] = '\0';
    eos_shell_cmd_text(&cmd, big);          /* 4095 bytes + terminator = 4096 */
    ASSERT(!cmd.fault);
    ASSERT(cmd.len == EOS_SHELL_CMD_MAX - 1);
    eos_shell_cmd_text(&cmd, "y");
    ASSERT(cmd.fault);
}

TEST(test_run_reports_the_exit_status_of_a_command_that_ran)
{
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "exit 0");
    ASSERT(eos_shell_cmd_run(&cmd, "test") == EOS_OK);

    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "exit 3");
    ASSERT(eos_shell_cmd_run(&cmd, "test") == EOS_ERR_BUILD);
}

/* A command the shell cannot run at all is an environment problem, and is
 * reported apart from a command that ran and failed: on POSIX the shell
 * exits 127 and run() returns EOS_ERR_SYSTEM. cmd.exe reports an unknown
 * command through an ordinary non-zero status, so there the claim is only
 * that it is not success. */
TEST(test_run_tells_a_missing_command_from_a_failed_one)
{
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "eos-shell-cmd-no-such-command-4c1e");
#ifdef _WIN32
    ASSERT(eos_shell_cmd_run(&cmd, "test") != EOS_OK);
#else
    ASSERT(eos_shell_cmd_run(&cmd, "test") == EOS_ERR_SYSTEM);
#endif
}

/* ---- the backends, through the builder ------------------------------- */

/* A build directory that would run a command if it reached a shell
 * unchecked. Every backend must return EOS_ERR_INVALID for it without
 * spawning anything -- EOS_ERR_BUILD would mean a shell saw it. */
static const char *const HOSTILE_DIR = "/tmp/build\"; touch pwned; \"";

static void assert_every_operation_refuses(EosBackend *b)
{
    EosPackage no_options;
    memset(&no_options, 0, sizeof(no_options));
    if (b->configure)
        ASSERT(b->configure(b, HOSTILE_DIR, HOSTILE_DIR, NULL, &no_options)
               != EOS_ERR_BUILD);
    ASSERT(b->build(b, HOSTILE_DIR, 4) == EOS_ERR_INVALID);
    ASSERT(b->install(b, HOSTILE_DIR, HOSTILE_DIR) == EOS_ERR_INVALID);
    ASSERT(b->clean(b, HOSTILE_DIR) == EOS_ERR_INVALID);
}

TEST(test_every_backend_refuses_a_hostile_directory)
{
    EosBackend b;
    void (*const inits[])(EosBackend *) = {
        eos_backend_cmake_init, eos_backend_ninja_init, eos_backend_make_init,
        eos_backend_kbuild_init, eos_backend_zephyr_init, eos_backend_freertos_init,
        eos_backend_nuttx_init, eos_backend_meson_init, eos_backend_autotools_init,
        eos_backend_buildroot_init, eos_backend_cargo_init,
    };
    for (size_t i = 0; i < sizeof(inits) / sizeof(inits[0]); i++) {
        inits[i](&b);
        assert_every_operation_refuses(&b);
    }
}

/* Option values reach the configure line too. A value that would end the
 * quoted argument and start a command is refused; a value with a space,
 * which used to split into two arguments, is now one quoted argument. */
TEST(test_configure_refuses_a_hostile_option_and_quotes_a_spaced_one)
{
    EosBackend b;
    eos_backend_cmake_init(&b);

    EosPackage hostile;
    memset(&hostile, 0, sizeof(hostile));
    strncpy(hostile.options[0].key, "CMAKE_C_FLAGS", EOS_MAX_NAME - 1);
    strncpy(hostile.options[0].value, "-O2\"; touch pwned; \"", EOS_MAX_PATH - 1);
    hostile.option_count = 1;
    ASSERT(b.configure(&b, "/src", "/build", NULL, &hostile) == EOS_ERR_INVALID);

    EosPackage bad_key;
    memset(&bad_key, 0, sizeof(bad_key));
    strncpy(bad_key.options[0].key, "X Y", EOS_MAX_NAME - 1);   /* not one word */
    strncpy(bad_key.options[0].value, "1", EOS_MAX_PATH - 1);
    bad_key.option_count = 1;
    ASSERT(b.configure(&b, "/src", "/build", NULL, &bad_key) == EOS_ERR_INVALID);

    /* The shape of the line a spaced value produces, checked on the builder
     * rather than by running cmake. */
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, " -D");
    eos_shell_cmd_word(&cmd, "CMAKE_C_FLAGS");
    eos_shell_cmd_text(&cmd, "=");
    eos_shell_cmd_arg(&cmd, "-O2 -g");
    ASSERT(!cmd.fault);
    ASSERT(strcmp(cmd.buf, " -DCMAKE_C_FLAGS=\"-O2 -g\"") == 0);
}

/* A path that fits EOS_MAX_PATH but not the old 1024-byte command buffers
 * three of the install commands used. Those ran truncated; now the line is
 * refused when it does not fit and runs whole when it does. */
TEST(test_install_never_runs_a_truncated_line)
{
    /* A 511-byte path (the longest EOS_MAX_PATH holds) that no platform can
     * create: /dev/null is not a directory on POSIX, and NUL is a reserved
     * device name on Windows, so the mkdir in the command fails and nothing
     * is left behind. */
    static const char prefix[] = "/dev/null/NUL/";
    char longdir[EOS_MAX_PATH];
    memset(longdir, 'd', sizeof(longdir) - 1);
    memcpy(longdir, prefix, sizeof(prefix) - 1);
    longdir[sizeof(longdir) - 1] = '\0';

    /* A line over the old 1024-byte buffers but inside the 4096-byte builder
     * must reach the shell whole. Under the old buffers it was cut inside a
     * quoted path and the shell died on the unterminated quote.
     *
     * The discriminator cannot be the exit status: a line that ran whole and
     * a line the shell could not parse both exit non-zero. Until #166 the
     * POSIX line ended in `|| true`, which made a whole line exit 0 and so
     * told the two apart; that tail is gone, so the test now observes what
     * only a whole line can do. */
    EosBackend b;
    eos_backend_nuttx_init(&b);
#ifdef _WIN32
    /* cmd.exe names the directories four times, so the 511-byte path is
     * ~2000 bytes and fits. mkdir fails on the reserved NUL name: a known
     * reason, from a line that ran. EOS_ERR_INVALID would mean refused. */
    ASSERT(b.install(&b, longdir, longdir) == EOS_ERR_BUILD);
#else
    /* The POSIX line names them eight times since #166 made the .bin -> .elf
     * fallback explicit, so 511 bytes would no longer fit (4303 bytes) and
     * the builder would refuse it. A 300-byte path gives a 2615-byte line:
     * over the old limit, inside the new one.
     *
     * The directory is creatable and initially absent. A whole line runs
     * `mkdir -p` first, then finds no nuttx image and exits 1. A truncated
     * line fails to parse, and a shell executes nothing of a line it cannot
     * parse -- so the directory existing afterwards is proof the line ran
     * whole, and EOS_ERR_BUILD is the known reason it then stopped. */
    char base[] = "/tmp/eos_trunc_XXXXXX";
    ASSERT(mkdtemp(base) != NULL);
    char fitdir[EOS_MAX_PATH];
    int n = snprintf(fitdir, sizeof(fitdir), "%s/", base);
    for (; n < 300; n++) {                 /* components stay under NAME_MAX */
        fitdir[n] = (n % 100 == 0) ? '/' : 'd';
    }
    fitdir[n] = '\0';

    struct stat st;
    ASSERT(stat(fitdir, &st) != 0);                     /* absent before */
    ASSERT(b.install(&b, fitdir, fitdir) == EOS_ERR_BUILD);
    ASSERT(stat(fitdir, &st) == 0 && S_ISDIR(st.st_mode)); /* ran whole */

    /* Best-effort removal of the scratch tree. Not a verification: every
     * assertion above has already run, and a leftover under /tmp changes no
     * result. */
    char rm[EOS_MAX_PATH + 16];
    snprintf(rm, sizeof(rm), "rm -rf %s", base);
    (void)system(rm);
#endif

    /* And the builder refuses when even one more argument would not fit. */
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    for (int i = 0; i < 8; i++) {
        eos_shell_cmd_text(&cmd, " ");
        eos_shell_cmd_arg(&cmd, longdir);
    }
    ASSERT(cmd.fault);
    ASSERT(eos_shell_cmd_run(&cmd, "test") == EOS_ERR_INVALID);
}

int main(void)
{
    printf("=== EoS shell command builder + backends ===\n\n");

    run_test_arg_rule_accepts_ordinary_paths_and_values();
    run_test_arg_rule_refuses_every_shell_metacharacter();
    run_test_arg_rule_follows_the_host_shell_on_backslashes();
    run_test_word_rule_is_an_allowlist();
    run_test_builder_quotes_arguments_and_leaves_words_bare();
    run_test_a_refused_value_marks_the_command_and_stops_it_growing();
    run_test_a_line_that_does_not_fit_is_refused_not_truncated();
    run_test_run_reports_the_exit_status_of_a_command_that_ran();
    run_test_run_tells_a_missing_command_from_a_failed_one();
    run_test_every_backend_refuses_a_hostile_directory();
    run_test_configure_refuses_a_hostile_option_and_quotes_a_spaced_one();
    run_test_install_never_runs_a_truncated_line();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
