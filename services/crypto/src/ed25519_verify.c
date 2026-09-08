#include "ed25519.h"
#include "sha512.h"
#include "ge.h"
#include "sc.h"

static int consttime_equal(const unsigned char *x, const unsigned char *y) {
    unsigned char r = 0;

    r = x[0] ^ y[0];
    #define F(i) r |= x[i] ^ y[i]
    F(1);
    F(2);
    F(3);
    F(4);
    F(5);
    F(6);
    F(7);
    F(8);
    F(9);
    F(10);
    F(11);
    F(12);
    F(13);
    F(14);
    F(15);
    F(16);
    F(17);
    F(18);
    F(19);
    F(20);
    F(21);
    F(22);
    F(23);
    F(24);
    F(25);
    F(26);
    F(27);
    F(28);
    F(29);
    F(30);
    F(31);
    #undef F

    return !r;
}


/* L = 2^252 + 27742317777372353535851937790883648493, the prime order of the
 * Ed25519 base-point subgroup, little-endian. */
static const unsigned char ED25519_ORDER_L[32] = {
    0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
    0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
};

/* Reject public keys outside the prime-order subgroup.
 *
 * Decoding a point is not enough. Ed25519 has eight low-order points, and for
 * any of them every term of the verification equation collapses to the identity
 * regardless of the message -- so a signature of all zeros verifies against any
 * content at all. That is not a weakened signature; it is no signature.
 *
 * The all-zero encoding is one of the eight, which mattered here: eos_pkg
 * shipped eos_pkg_public_key[32] = {0} and consequently accepted every package
 * handed to it.
 *
 * Tested by multiplying the key by L and requiring the identity, rather than by
 * comparing against a table of the eight encodings. Both are correct; this one
 * cannot be wrong in a way that silently rejects valid keys because a constant
 * was transcribed incorrectly.
 *
 * Multiplying by L alone is not sufficient: the identity has order 1, which
 * divides L, so [L]identity = identity and it passes the subgroup test. It is
 * rejected separately. Verified rather than assumed -- the first version of
 * this check had only the subgroup test and still accepted an identity key.
 *
 * A is negated by ge_frombytes_negate_vartime(). [L](-A) = -[L]A, and the
 * identity is its own negation, so neither test is affected.
 */
static int ed25519_point_is_identity(const unsigned char packed[32]) {
    int i;
    if (packed[0] != 1) {
        return 0;
    }
    for (i = 1; i < 32; i++) {
        if (packed[i] != 0) {
            return 0;
        }
    }
    return 1;
}

static int ed25519_key_has_prime_order(const ge_p3 *A) {
    static const unsigned char zero_scalar[32] = {0};
    unsigned char packed[32];
    ge_p2 A2;
    ge_p2 LA;

    /* The identity is order 1, so [L]identity = identity and the subgroup
     * test below would admit it. Reject it first. */
    ge_p3_to_p2(&A2, A);
    ge_tobytes(packed, &A2);
    if (ed25519_point_is_identity(packed)) {
        return 0;
    }

    /* [L]A + [0]B */
    ge_double_scalarmult_vartime(&LA, ED25519_ORDER_L, A, zero_scalar);
    ge_tobytes(packed, &LA);

    /* A key of prime order L multiplies to the identity. */
    return ed25519_point_is_identity(packed);
}

/* Is this encoding usable as a trust anchor?
 *
 * ed25519_verify() runs the same two tests, but only once a signature is in
 * hand, and a failure there is indistinguishable from a bad signature. A
 * caller that stores a key -- eos_pkg's trust anchor, see #98 -- needs to ask
 * about the key alone, at the moment it is configured, so an unusable anchor
 * is reported as an unusable anchor rather than as a stream of packages that
 * all appear to be forged.
 */
int ed25519_public_key_is_usable(const unsigned char *public_key) {
    ge_p3 A;

    if (public_key == 0) {
        return 0;
    }
    if (ge_frombytes_negate_vartime(&A, public_key) != 0) {
        return 0;
    }
    return ed25519_key_has_prime_order(&A);
}

/* Reject a signature whose S is not the canonical representative mod L.
 *
 * The check above this one -- signature[63] & 224 -- is ref10's partial test.
 * It rejects any S with a bit set above 2^252, but L is 2^252 +
 * 27742317777372353535851937790883648493, so every S in [L, 2^253) passes it
 * while being non-canonical. Adding L to a valid S lands squarely in that band:
 * (R, S) and (R, S + L) both verified, which is signature malleability. Anything
 * that treats a signature as a unique identifier -- deduplication, replay
 * caches, logging by signature hash -- sees two distinct signatures over one
 * message and one key.
 *
 * RFC 8032 5.1.7 requires "reject the signature if S is not in the range
 * [0, L)". eBoot's core/ed25519_verify.c already does this; this side did not,
 * which is precisely the kind of divergence the shared contract corpus in
 * tests/vectors/ed25519_contract_vectors.h exists to surface -- and it is how
 * this was found.
 *
 * Compared most-significant byte first, which is a comparison against a public
 * constant and reveals nothing about any secret: S is public.
 */
/* Through the public API this boundary is unreachable: a signature with
 * S == L and one that simply fails the verification equation both return 0,
 * so the corpus cannot tell an off-by-one here from an ordinary rejection
 * -- and an off-by-one that refused a valid S = L - 1 would satisfy every
 * vector in the corpus while rejecting real signatures in the field. It is
 * a pure function of 32 bytes, so it can be tested directly and without
 * generating any signatures.
 *
 * It stays static. tests/test_ed25519_canonical_s.c includes this translation
 * unit to reach it, rather than the file being compiled twice with a macro
 * that changes this symbol's linkage -- that arrangement made the function
 * look unreachable to static analysis, and a linkage that varies by build is
 * a poor thing to have in a crypto source file. */
static int sc_is_canonical(const unsigned char s[32]) {
    /* L, little-endian. */
    static const unsigned char L[32] = {
        0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
        0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
    };
    int i;
    for (i = 31; i >= 0; i--) {
        if (s[i] < L[i]) return 1;
        if (s[i] > L[i]) return 0;
    }
    return 0;   /* S == L is not in [0, L) either */
}

int ed25519_verify(const unsigned char *signature, const unsigned char *message, size_t message_len, const unsigned char *public_key) {
    unsigned char h[64];
    unsigned char checker[32];
    sha512_context hash;
    ge_p3 A;
    ge_p2 R;

    if (signature[63] & 224) {
        return 0;
    }

    /* The full range check RFC 8032 5.1.7 asks for; the test above is only
     * its top three bits. */
    if (!sc_is_canonical(signature + 32)) {
        return 0;
    }

    if (ge_frombytes_negate_vartime(&A, public_key) != 0) {
        return 0;
    }

    if (!ed25519_key_has_prime_order(&A)) {
        return 0;
    }

    sha512_init(&hash);
    sha512_update(&hash, signature, 32);
    sha512_update(&hash, public_key, 32);
    sha512_update(&hash, message, message_len);
    sha512_final(&hash, h);
    
    sc_reduce(h);
    ge_double_scalarmult_vartime(&R, h, &A, signature + 32);
    ge_tobytes(checker, &R);

    if (!consttime_equal(checker, signature)) {
        return 0;
    }

    return 1;
}
