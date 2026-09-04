/*
 * eos_pkg_trust_anchor.c — eos_pkg's trust-anchor policy
 *
 * Split out of eos_pkg.c (#133): the rest of that file lists directories and
 * spawns processes for install/remove/run, which needs <dirent.h>,
 * <unistd.h> and <sys/wait.h> and is a hard #error under bare-metal newlib.
 * This anchor state is pure in-memory logic with no such dependency, so it
 * lives in its own translation unit and its own CMake target
 * (eos_pkg_trust_anchor) that builds on every target, host or cross. That is
 * what lets a test exercise eos_pkg_set_trust_anchor()/eos_pkg_trust_anchor()
 * themselves outside a host build, instead of only the crypto predicate
 * underneath them.
 *
 * Copyright (c) 2024-2026 EmbeddedOS Project. MIT License.
 */

#include "eos_pkg.h"

#include <stdio.h>
#include <string.h>

#include <eos/crypto.h>
#include <ed25519.h>

/* --------------------------------------------------------------------------
 * Trust anchor
 *
 * This file used to carry
 *
 *     static const uint8_t eos_pkg_public_key[32] = {0};
 *
 * with a comment saying a real system would load it from a keystore. The
 * all-zero encoding is one of Ed25519's eight low-order points, so before
 * services/crypto learned to reject those (#99) every package verified. It
 * now rejects every package instead -- including correctly signed ones --
 * and reports "signature verification failed", which blames the package for
 * a defect in the installation.
 *
 * Neither behaviour is a signature check. A key is required, and there is
 * nowhere in this repository it can honestly be hardcoded from, so it comes
 * from the platform instead:
 *
 *   1. eos_pkg_set_trust_anchor(), which is where a keystore or a
 *      provisioning step installs the key it holds; or
 *   2. EOS_PKG_TRUST_ANCHOR_HEX, 64 hex characters set by the build.
 *
 * With neither, verification refuses rather than appearing to succeed. That
 * is the shape services/crypto already uses for its unimplemented RSA/ECC
 * stubs (EOS_ALLOW_STUB_CRYPTO) and services/ota for an update with no
 * authenticator installed (EOS_ALLOW_UNSIGNED_OTA); EOS_ALLOW_UNSIGNED_PKG
 * is the same opt-in, for tests and for bring-up.
 * -------------------------------------------------------------------------- */

static uint8_t  eos_pkg_anchor[EAPP_PUBKEY_LEN];
static bool     eos_pkg_anchor_set = false;

#ifdef EOS_PKG_TRUST_ANCHOR_HEX
static int eos_pkg_hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decode the build-time anchor. A malformed value is a build configuration
 * error, so it is refused rather than truncated or padded. */
static bool eos_pkg_anchor_from_build(uint8_t out[EAPP_PUBKEY_LEN])
{
    static const char hex[] = EOS_PKG_TRUST_ANCHOR_HEX;
    size_t i;

    if (sizeof(hex) - 1 != EAPP_PUBKEY_LEN * 2) {
        fprintf(stderr,
                "eos-pkg: EOS_PKG_TRUST_ANCHOR_HEX is %zu characters, "
                "expected %d\n", sizeof(hex) - 1, EAPP_PUBKEY_LEN * 2);
        return false;
    }
    for (i = 0; i < EAPP_PUBKEY_LEN; i++) {
        int hi = eos_pkg_hex_nibble(hex[i * 2]);
        int lo = eos_pkg_hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            fprintf(stderr, "eos-pkg: EOS_PKG_TRUST_ANCHOR_HEX is not hex\n");
            return false;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}
#endif

int eos_pkg_set_trust_anchor(const uint8_t public_key[EAPP_PUBKEY_LEN])
{
    if (!public_key) {
        eos_pkg_anchor_set = false;
        memset(eos_pkg_anchor, 0, sizeof(eos_pkg_anchor));
        return 0;
    }

    /* Refuse the key here, where the caller is configuring it and can act on
     * the answer. Accepting it and failing later turns a provisioning mistake
     * into a package that looks forged. */
    if (!ed25519_public_key_is_usable(public_key)) {
        fprintf(stderr,
                "eos-pkg: refusing a trust anchor that is not a prime-order "
                "Ed25519 point; such a key cannot authenticate anything\n");
        return -1;
    }

    memcpy(eos_pkg_anchor, public_key, EAPP_PUBKEY_LEN);
    eos_pkg_anchor_set = true;
    return 0;
}

const uint8_t *eos_pkg_trust_anchor(void)
{
    if (eos_pkg_anchor_set) return eos_pkg_anchor;

#ifdef EOS_PKG_TRUST_ANCHOR_HEX
    {
        uint8_t built[EAPP_PUBKEY_LEN];
        if (eos_pkg_anchor_from_build(built) &&
            ed25519_public_key_is_usable(built)) {
            memcpy(eos_pkg_anchor, built, EAPP_PUBKEY_LEN);
            eos_pkg_anchor_set = true;
            return eos_pkg_anchor;
        }
        fprintf(stderr,
                "eos-pkg: EOS_PKG_TRUST_ANCHOR_HEX does not decode to a "
                "usable Ed25519 public key\n");
    }
#endif
    return NULL;
}
