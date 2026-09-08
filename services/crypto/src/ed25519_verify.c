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
int ed25519_verify(const unsigned char *signature, const unsigned char *message, size_t message_len, const unsigned char *public_key) {
    unsigned char h[64];
    unsigned char checker[32];
    sha512_context hash;
    ge_p3 A;
    ge_p2 R;

    if (signature[63] & 224) {
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
