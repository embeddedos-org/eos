// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_pkg_fetch.c
 * @brief Unit tests for eos_fetch_source()
 *
 * eos_fetch_source() is public API in pkg/include/eos/package.h and had no
 * tests. Its checksum comparison ran only `if (expected_hash &&
 * expected_hash[0])`, so calling it without a digest downloaded an archive
 * and extracted it having compared it against nothing -- and returned
 * EOS_OK, which is what a verified fetch also returns.
 *
 * These cases are offline by construction: every one of them must return
 * before the network is touched, which is the property under test.
 */

#include "eos/package.h"
#include "eos/error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int tests_passed = 0;

#define ASSERT(cond)                                                          \
    do {                                                                      \
        if (!(cond)) {                                                        \
            fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                         \
        }                                                                     \
    } while (0)

#define PASS(name) do { printf("[PASS] %s\n", name); tests_passed++; } while (0)

int main(void)
{
    printf("=== EoS Package Fetch Tests ===\n");

    /* A tarball URL with no digest must be refused, not downloaded. */
    ASSERT(eos_fetch_source("https://example.invalid/src.tar.gz", "/tmp/eos_t",
                            NULL) == EOS_ERR_CHECKSUM);
    PASS("a tarball with no expected hash is refused");

    ASSERT(eos_fetch_source("https://example.invalid/src.tar.gz", "/tmp/eos_t",
                            "") == EOS_ERR_CHECKSUM);
    PASS("an empty expected hash is refused, not treated as absent");

    ASSERT(eos_fetch_source("https://example.invalid/src.tar.xz", "/tmp/eos_t",
                            NULL) == EOS_ERR_CHECKSUM);
    ASSERT(eos_fetch_source("https://example.invalid/src.tgz", "/tmp/eos_t",
                            NULL) == EOS_ERR_CHECKSUM);
    ASSERT(eos_fetch_source("https://example.invalid/src.zip", "/tmp/eos_t",
                            NULL) == EOS_ERR_CHECKSUM);
    PASS(".tar.gz, .tar.xz, .tgz and .zip are all refused without a digest");

    /* The defect this PR was reported as leaving open: a git URL took the
     * git branch, which ignored expected_hash entirely and returned EOS_OK.
     * Both shapes are refused before the network now -- no digest, and (in
     * the guard's eyes) a digest that fetch_git will actually compare. */
    ASSERT(eos_fetch_source("https://example.invalid/proj.git", "/tmp/eos_t",
                            NULL) == EOS_ERR_CHECKSUM);
    ASSERT(eos_fetch_source("git://example.invalid/proj", "/tmp/eos_t",
                            "") == EOS_ERR_CHECKSUM);
    ASSERT(eos_fetch_source("git@example.invalid:proj", "/tmp/eos_t",
                            NULL) == EOS_ERR_CHECKSUM);
    PASS("a git source with no pinned commit is refused, not silently cloned");

    /* is_git_url() was strstr(url, ".git"), so any host under *.github.io --
     * including this organisation's own pages -- and any archive whose path
     * contained ".git" took the git branch and skipped the digest check.
     * These are tarballs and must be treated as tarballs. */
    ASSERT(eos_fetch_source("https://embeddedos-org.github.io/x/src.tar.gz",
                            "/tmp/eos_t", NULL) == EOS_ERR_CHECKSUM);
    ASSERT(eos_fetch_source("https://example.invalid/.github/rel.tar.gz",
                            "/tmp/eos_t", NULL) == EOS_ERR_CHECKSUM);
    PASS("a URL merely containing \".git\" is not mistaken for a git repository");

    /* The refusal must not have widened into rejecting everything: an unsafe
     * URL keeps its own error, and a shell metacharacter is still caught. */
    ASSERT(eos_fetch_source("https://example.invalid/a;rm -rf .tar.gz",
                            "/tmp/eos_t", "deadbeef") == EOS_ERR_FETCH);
    PASS("an unsafe URL still fails as unsafe, not as a checksum error");

    /* An empty URL is a package with nothing to fetch, not a failure. */
    ASSERT(eos_fetch_source("", "/tmp/eos_t", NULL) == EOS_OK);
    ASSERT(eos_fetch_source(NULL, "/tmp/eos_t", NULL) == EOS_OK);
    PASS("no URL is still a no-op rather than an error");

    printf("\n%d/7 tests passed\n", tests_passed);
    return tests_passed == 7 ? 0 : 1;
}
