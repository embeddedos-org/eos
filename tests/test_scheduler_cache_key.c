// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_scheduler_cache_key.c
 * @brief compute_cache_key() against options that do not fit its buffer.
 *
 * compute_cache_key() formats the package identity into a 2048-byte stack
 * buffer. snprintf() returns the length it *would* have written, so
 * accumulating that return value walks the write offset past the end of the
 * buffer as soon as one option truncates -- and `sizeof(input) - len` then
 * underflows size_t, handing snprintf an effectively unbounded size.
 *
 * A package may carry EOS_MAX_OPTIONS (32) options, each a 128-byte key and a
 * 512-byte value, so roughly 20 KiB of option text can be aimed at that 2048
 * byte buffer. Four long options are enough.
 *
 * The overflow itself is undefined behaviour and needs a sanitizer to observe
 * reliably. These tests pin the consequence that is deterministic without one:
 * the content hash must still reach the digest. If options crowd it out, two
 * packages built from *different sources* collapse to the same cache key, and
 * the build reuses the wrong artifact.
 *
 * The file #includes scheduler.c to reach its static, the same way
 * test_ed25519_canonical_s.c reaches ed25519_verify.c's.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../core/src/scheduler.c"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

/** Fill option `i` with a key of `k` characters and a value of `v`.
 * #148 Step 2b: options are interned through eos_pkg_option_add();
 * the package owns the table (never disposed in these short-lived
 * fixtures \u2014 one bounded table per package, freed at exit). */
static void set_option(EosPackage *pkg, int i, int k, int v)
{
    char key[EOS_MAX_NAME];
    char value[EOS_MAX_PATH];
    memset(key, 'k', (size_t)k);
    key[0] = (char)('A' + (i % 26));
    key[k] = '\0';
    memset(value, 'v', (size_t)v);
    value[v] = '\0';
    if (eos_pkg_option_add(pkg, key, value) != EOS_OK) {
        printf("[FAIL] set_option: eos_pkg_option_add failed\n");
        exit(1);
    }
}

/**
 * A package shaped to make the *old* loop truncate mid-iteration.
 *
 * The original guard let an append start whenever len < 2048 - 128, then added
 * snprintf()'s return value -- the length it would have written. Uniform
 * options never expose this: they either all fit, or the guard stops the loop
 * while there is still room. It takes appends that walk len up close to the
 * 1920 guard and then one option too large for what remains:
 *
 *   9 options of ":key=value" = 200 bytes  -> len = 21 + 1800 = 1821  (< 1920)
 *   1 maximal option           = 640 bytes  -> only 227 bytes left, so it
 *                                             truncates, but len += 640
 *
 * len is then 2461 against a 2048-byte buffer, and the hash append writes at
 * input+2461 -- 413 bytes past the end -- with a size argument of
 * 2048 - 2461, which underflows size_t to ~1.8e19.
 */
static void make_pkg(EosPackage *pkg, const char *hash, int medium, int add_max)
{
    memset(pkg, 0, sizeof(*pkg));
    snprintf(pkg->name, sizeof(pkg->name), "zlib");
    snprintf(pkg->version, sizeof(pkg->version), "1.3.1");
    pkg->build_type = EOS_BUILD_CMAKE;
    snprintf(pkg->hash, sizeof(pkg->hash), "%s", hash);

    int n = 0;
    for (; n < medium && n < EOS_MAX_OPTIONS; n++)
        set_option(pkg, n, 98, 100);            /* ":key=value" == 200 bytes */
    if (add_max && n < EOS_MAX_OPTIONS)
        set_option(pkg, n++, EOS_MAX_NAME - 1, EOS_MAX_PATH - 1);  /* 640 */
    /* eos_pkg_option_add() maintains option_count (== n here). */
}

/* Two packages identical but for the source hash must not share a cache key.
 * Before the fix the hash was written at an offset past the end of the buffer
 * once the options overran it, so it never reached the digest and both
 * packages hashed the same 2047 bytes of option text. */
static int test_hash_still_separates_packages_when_options_overflow(void)
{
    EosPackage a, b;
    char key_a[EOS_MAX_NAME + EOS_HASH_LEN];
    char key_b[EOS_MAX_NAME + EOS_HASH_LEN];

    /* 9 medium options then one maximal one: the old loop hands the hash
     * append an offset of 2461 into a 2048-byte buffer. */
    make_pkg(&a, "1111111111111111111111111111111111111111111111111111111111111111", 9, 1);
    make_pkg(&b, "2222222222222222222222222222222222222222222222222222222222222222", 9, 1);

    compute_cache_key(&a, "host", key_a, sizeof(key_a));
    compute_cache_key(&b, "host", key_b, sizeof(key_b));

    ASSERT(strcmp(key_a, key_b) != 0);
    return 0;
}

/* The same at the exact boundary the old guard reached for: enough option text
 * to land inside `sizeof(input) - 128`, where the loop stopped but the hash was
 * still appended into whatever room was left. */
static int test_hash_still_separates_packages_at_the_reserve_boundary(void)
{
    EosPackage a, b;
    char key_a[EOS_MAX_NAME + EOS_HASH_LEN];
    char key_b[EOS_MAX_NAME + EOS_HASH_LEN];

    make_pkg(&a, "3333333333333333333333333333333333333333333333333333333333333333", 9, 0);
    make_pkg(&b, "4444444444444444444444444444444444444444444444444444444444444444", 9, 0);

    compute_cache_key(&a, "host", key_a, sizeof(key_a));
    compute_cache_key(&b, "host", key_b, sizeof(key_b));

    ASSERT(strcmp(key_a, key_b) != 0);
    return 0;
}

/* Every option count from none to the maximum must produce a key, and the key
 * must stay a function of its inputs. This is the loop that walked off the
 * end; it is exercised across the whole range rather than at one width. */
static int test_key_is_produced_and_stable_for_every_option_count(void)
{
    for (int n = 0; n < EOS_MAX_OPTIONS; n++) {
        EosPackage p;
        char first[EOS_MAX_NAME + EOS_HASH_LEN];
        char again[EOS_MAX_NAME + EOS_HASH_LEN];

        make_pkg(&p, "5555555555555555555555555555555555555555555555555555555555555555", n, n % 2);
        compute_cache_key(&p, "arm-none-eabi", first, sizeof(first));
        compute_cache_key(&p, "arm-none-eabi", again, sizeof(again));

        ASSERT(first[0] != '\0');
        ASSERT(strcmp(first, again) == 0);
    }
    return 0;
}

/* A package with no hash at all still keys, and the toolchain still counts:
 * the reserve arithmetic is skipped in that case and must not misbehave. */
static int test_no_hash_still_keys_and_toolchain_separates(void)
{
    EosPackage p;
    char host_key[EOS_MAX_NAME + EOS_HASH_LEN];
    char cross_key[EOS_MAX_NAME + EOS_HASH_LEN];

    make_pkg(&p, "", 9, 1);
    compute_cache_key(&p, "host", host_key, sizeof(host_key));
    compute_cache_key(&p, "arm-none-eabi", cross_key, sizeof(cross_key));

    ASSERT(host_key[0] != '\0');
    ASSERT(strcmp(host_key, cross_key) != 0);
    return 0;
}

#define RUN(fn) do { \
    printf("  %-62s ", #fn); \
    tests_run++; \
    if (fn() != 0) return 1; \
    tests_passed++; \
    printf("[PASS]\n"); \
} while (0)

int main(void)
{
    printf("=== EoS: scheduler cache-key tests ===\n\n");

    RUN(test_hash_still_separates_packages_when_options_overflow);
    RUN(test_hash_still_separates_packages_at_the_reserve_boundary);
    RUN(test_key_is_produced_and_stable_for_every_option_count);
    RUN(test_no_hash_still_keys_and_toolchain_separates);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
