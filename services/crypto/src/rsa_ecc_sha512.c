// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/crypto.h"
#include <string.h>
#include <stdio.h>

/* =========================================================================
 * Big-Integer Arithmetic for RSA
 * Limbs are uint32_t in little-endian order (index 0 = least significant).
 * ========================================================================= */

#define BN_MAX_LIMBS 128  /* 4096-bit keys max */

static void bn_from_bytes(uint32_t *dst, int nw, const uint8_t *src, int nb) {
    memset(dst, 0, (size_t)nw * sizeof(uint32_t));
    for (int i = 0; i < nb && i / 4 < nw; i++)
        dst[i / 4] |= (uint32_t)src[nb - 1 - i] << ((i % 4) * 8);
}

static void bn_to_bytes(const uint32_t *src, int nw, uint8_t *dst, int nb) {
    memset(dst, 0, (size_t)nb);
    for (int i = 0; i < nb && i / 4 < nw; i++)
        dst[nb - 1 - i] = (uint8_t)(src[i / 4] >> ((i % 4) * 8));
}

static int bn_cmp(const uint32_t *a, const uint32_t *b, int n) {
    for (int i = n - 1; i >= 0; i--) {
        if (a[i] > b[i]) return 1;
        if (a[i] < b[i]) return -1;
    }
    return 0;
}

static uint32_t bn_sub(uint32_t *a, const uint32_t *b, int n) {
    uint64_t borrow = 0;
    for (int i = 0; i < n; i++) {
        uint64_t d = (uint64_t)a[i] - b[i] - borrow;
        a[i] = (uint32_t)d;
        borrow = (d >> 63) & 1;
    }
    return (uint32_t)borrow;
}

static int bn_bitlen(const uint32_t *a, int n) {
    for (int i = n - 1; i >= 0; i--) {
        if (a[i] == 0) continue;
        int b = i * 32;
        uint32_t w = a[i];
        while (w) { b++; w >>= 1; }
        return b;
    }
    return 0;
}

/* Schoolbook multiply: result[2*n] = a[n] * b[n] */
static void bn_mul(const uint32_t *a, const uint32_t *b, int n, uint32_t *r) {
    memset(r, 0, (size_t)(2 * n) * sizeof(uint32_t));
    for (int i = 0; i < n; i++) {
        uint64_t carry = 0;
        for (int j = 0; j < n; j++) {
            uint64_t p = (uint64_t)a[i] * b[j] + r[i + j] + carry;
            r[i + j] = (uint32_t)p;
            carry = p >> 32;
        }
        r[i + n] = (uint32_t)carry;
    }
}

/* Montgomery n0inv: returns v such that n[0]*v == -1 (mod 2^32) */
static uint32_t bn_mont_n0inv(uint32_t n0) {
    uint32_t x = 1;
    for (int i = 0; i < 5; i++)
        x *= 2 - n0 * x;
    return (uint32_t)(-(int64_t)x);
}

/* Montgomery reduction in-place: t[2*nw+1] -> result[nw] = t * R^-1 mod n */
static void bn_mont_reduce(uint32_t *t, const uint32_t *n, uint32_t n0inv,
                           int nw, uint32_t *result) {
    for (int i = 0; i < nw; i++) {
        uint32_t m = t[i] * n0inv;
        uint64_t carry = 0;
        for (int j = 0; j < nw; j++) {
            uint64_t p = (uint64_t)m * n[j] + t[i + j] + carry;
            t[i + j] = (uint32_t)p;
            carry = p >> 32;
        }
        for (int j = nw; carry; j++) {
            uint64_t s = (uint64_t)t[i + j] + carry;
            t[i + j] = (uint32_t)s;
            carry = s >> 32;
        }
    }
    memcpy(result, t + nw, (size_t)nw * sizeof(uint32_t));
    if (bn_cmp(result, n, nw) >= 0)
        bn_sub(result, n, nw);
}

/* Montgomery multiply: result = a * b * R^-1 mod n */
static void bn_mont_mul(const uint32_t *a, const uint32_t *b,
                        const uint32_t *n, uint32_t n0inv, int nw,
                        uint32_t *result) {
    uint32_t t[BN_MAX_LIMBS * 2 + 2];
    memset(t, 0, (size_t)(2 * nw + 2) * sizeof(uint32_t));
    bn_mul(a, b, nw, t);
    t[2 * nw] = 0;
    t[2 * nw + 1] = 0;
    bn_mont_reduce(t, n, n0inv, nw, result);
}

/* Compute R^2 mod n by repeated doubling: start=1, double 2*32*nw times */
static void bn_compute_r2(const uint32_t *n, int nw, uint32_t *r2) {
    memset(r2, 0, (size_t)nw * sizeof(uint32_t));
    r2[0] = 1;
    for (int i = 0; i < 2 * 32 * nw; i++) {
        uint32_t carry = 0;
        for (int j = 0; j < nw; j++) {
            uint32_t nc = r2[j] >> 31;
            r2[j] = (r2[j] << 1) | carry;
            carry = nc;
        }
        if (carry || bn_cmp(r2, n, nw) >= 0)
            bn_sub(r2, n, nw);
    }
}

/* Modular exponentiation: result = base^exp mod n */
static void bn_powmod(const uint32_t *base, const uint32_t *exp,
                      const uint32_t *n, int nw, uint32_t *result) {
    uint32_t n0inv = bn_mont_n0inv(n[0]);
    uint32_t r2[BN_MAX_LIMBS];
    bn_compute_r2(n, nw, r2);

    uint32_t one[BN_MAX_LIMBS];
    memset(one, 0, sizeof(one));
    one[0] = 1;

    /* Convert base to Montgomery form */
    uint32_t bm[BN_MAX_LIMBS];
    bn_mont_mul(base, r2, n, n0inv, nw, bm);

    /* result_mont = R mod n = Mont(1, R^2) */
    uint32_t rm[BN_MAX_LIMBS];
    bn_mont_mul(one, r2, n, n0inv, nw, rm);

    /* Square-and-multiply (MSB to LSB) */
    int bits = bn_bitlen(exp, nw);
    uint32_t tmp[BN_MAX_LIMBS];
    for (int i = bits - 1; i >= 0; i--) {
        bn_mont_mul(rm, rm, n, n0inv, nw, tmp);
        memcpy(rm, tmp, (size_t)nw * sizeof(uint32_t));
        if ((exp[i / 32] >> (i % 32)) & 1) {
            bn_mont_mul(rm, bm, n, n0inv, nw, tmp);
            memcpy(rm, tmp, (size_t)nw * sizeof(uint32_t));
        }
    }

    /* Convert back from Montgomery form */
    bn_mont_mul(rm, one, n, n0inv, nw, result);
}

/* =========================================================================
 * RSA PKCS#1 v1.5 with SHA-256
 * ========================================================================= */

/* DER-encoded DigestInfo prefix for SHA-256 (19 bytes) */
static const uint8_t sha256_digest_prefix[] = {
    0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
    0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
    0x00, 0x04, 0x20
};

int eos_rsa_sign_sha256(const EosRsaKey *key, const uint8_t hash[32],
                        uint8_t *sig, size_t *sig_len) {
    if (!key || !key->has_private || !hash || !sig || !sig_len) return -1;

    int klen = key->key_bits / 8;
    if (klen < 64 || klen > EOS_RSA_MAX_KEY_BYTES) return -1;

    int di_len = (int)sizeof(sha256_digest_prefix) + 32;
    int ps_len = klen - 3 - di_len;
    if (ps_len < 8) return -1;

    /* Build PKCS#1 v1.5 padded message */
    uint8_t padded[EOS_RSA_MAX_KEY_BYTES];
    padded[0] = 0x00;
    padded[1] = 0x01;
    memset(padded + 2, 0xff, (size_t)ps_len);
    padded[2 + ps_len] = 0x00;
    memcpy(padded + 3 + ps_len, sha256_digest_prefix,
           sizeof(sha256_digest_prefix));
    memcpy(padded + 3 + ps_len + (int)sizeof(sha256_digest_prefix), hash, 32);

    int nw = klen / 4;
    uint32_t m[BN_MAX_LIMBS], d[BN_MAX_LIMBS], n[BN_MAX_LIMBS];
    uint32_t s[BN_MAX_LIMBS];
    bn_from_bytes(m, nw, padded, klen);
    bn_from_bytes(d, nw, key->d, klen);
    bn_from_bytes(n, nw, key->n, klen);

    /* sig = padded^d mod n */
    bn_powmod(m, d, n, nw, s);
    bn_to_bytes(s, nw, sig, klen);
    *sig_len = (size_t)klen;
    return 0;
}

int eos_rsa_verify_sha256(const EosRsaKey *key, const uint8_t hash[32],
                          const uint8_t *sig, size_t sig_len) {
    if (!key || !hash || !sig) return -1;

    int klen = key->key_bits / 8;
    if (klen < 64 || klen > EOS_RSA_MAX_KEY_BYTES) return -1;
    if (sig_len != (size_t)klen) return -1;

    int nw = klen / 4;
    uint32_t s[BN_MAX_LIMBS], e[BN_MAX_LIMBS], n[BN_MAX_LIMBS];
    uint32_t m[BN_MAX_LIMBS];
    bn_from_bytes(s, nw, sig, klen);
    bn_from_bytes(e, nw, key->e, klen);
    bn_from_bytes(n, nw, key->n, klen);

    if (bn_cmp(s, n, nw) >= 0) return -1;

    /* m = sig^e mod n */
    bn_powmod(s, e, n, nw, m);

    uint8_t dec[EOS_RSA_MAX_KEY_BYTES];
    bn_to_bytes(m, nw, dec, klen);

    /* Verify PKCS#1 v1.5 structure */
    if (dec[0] != 0x00 || dec[1] != 0x01) return -1;

    int i = 2;
    while (i < klen && dec[i] == 0xff) i++;
    if (i < 4 || i >= klen || dec[i] != 0x00) return -1;
    i++;

    int remaining = klen - i;
    int expected = (int)sizeof(sha256_digest_prefix) + 32;
    if (remaining != expected) return -1;

    if (memcmp(dec + i, sha256_digest_prefix,
               sizeof(sha256_digest_prefix)) != 0)
        return -1;
    if (memcmp(dec + i + (int)sizeof(sha256_digest_prefix), hash, 32) != 0)
        return -1;

    return 0;
}

/* =========================================================================
 * P-256 (secp256r1) Field Arithmetic - 256-bit mod p
 * ========================================================================= */

#define P256_LIMBS 8

static const uint32_t P256_P[8] = {
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000,
    0x00000000, 0x00000000, 0x00000001, 0xFFFFFFFF
};

static const uint32_t P256_N[8] = {
    0xFC632551, 0xF3B9CAC2, 0xA7179E84, 0xBCE6FAAD,
    0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF
};

static const uint32_t P256_GX[8] = {
    0xD898C296, 0xF4A13945, 0x2DEB33A0, 0x77037D81,
    0x63A440F2, 0xF8BCE6E5, 0xE12C4247, 0x6B17D1F2
};

static const uint32_t P256_GY[8] = {
    0x37BF51F5, 0xCBB64068, 0x6B315ECE, 0x2BCE3357,
    0x7C0F9E16, 0x8EE7EB4A, 0xFE1A7F9B, 0x4FE342E2
};

static int u256_cmp(const uint32_t *a, const uint32_t *b) {
    for (int i = 7; i >= 0; i--) {
        if (a[i] > b[i]) return 1;
        if (a[i] < b[i]) return -1;
    }
    return 0;
}

static int u256_is_zero(const uint32_t *a) {
    for (int i = 0; i < 8; i++)
        if (a[i]) return 0;
    return 1;
}

static uint32_t u256_add(uint32_t *r, const uint32_t *a, const uint32_t *b) {
    uint64_t c = 0;
    for (int i = 0; i < 8; i++) {
        c += (uint64_t)a[i] + b[i];
        r[i] = (uint32_t)c;
        c >>= 32;
    }
    return (uint32_t)c;
}

static uint32_t u256_sub(uint32_t *r, const uint32_t *a, const uint32_t *b) {
    uint64_t bw = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t d = (uint64_t)a[i] - b[i] - bw;
        r[i] = (uint32_t)d;
        bw = (d >> 63) & 1;
    }
    return (uint32_t)bw;
}

static void u256_from_bytes(uint32_t *dst, const uint8_t *src) {
    for (int i = 0; i < 8; i++)
        dst[i] = ((uint32_t)src[31 - 4*i]) |
                 ((uint32_t)src[30 - 4*i] << 8) |
                 ((uint32_t)src[29 - 4*i] << 16) |
                 ((uint32_t)src[28 - 4*i] << 24);
}

static void u256_to_bytes(const uint32_t *src, uint8_t *dst) {
    for (int i = 0; i < 8; i++) {
        dst[31 - 4*i]     = (uint8_t)(src[i]);
        dst[30 - 4*i] = (uint8_t)(src[i] >> 8);
        dst[29 - 4*i] = (uint8_t)(src[i] >> 16);
        dst[28 - 4*i] = (uint8_t)(src[i] >> 24);
    }
}

/* Schoolbook 256x256 -> 512 bit multiply */
static void u256_mul_512(const uint32_t *a, const uint32_t *b, uint32_t r[16]) {
    memset(r, 0, 16 * sizeof(uint32_t));
    for (int i = 0; i < 8; i++) {
        uint64_t carry = 0;
        for (int j = 0; j < 8; j++) {
            uint64_t p = (uint64_t)a[i] * b[j] + r[i + j] + carry;
            r[i + j] = (uint32_t)p;
            carry = p >> 32;
        }
        r[i + 8] = (uint32_t)carry;
    }
}

/* NIST P-256 fast reduction: reduce 512-bit c[] mod p into r[] */
static void fp_reduce(const uint32_t c[16], uint32_t r[8]) {
    int64_t a[8];
    for (int i = 0; i < 8; i++) a[i] = (int64_t)c[i];

    /* +2*s2: {0,0,0,c11,c12,c13,c14,c15} */
    a[3] += 2*(int64_t)c[11]; a[4] += 2*(int64_t)c[12];
    a[5] += 2*(int64_t)c[13]; a[6] += 2*(int64_t)c[14];
    a[7] += 2*(int64_t)c[15];
    /* +2*s3: {0,0,0,c12,c13,c14,c15,0} */
    a[3] += 2*(int64_t)c[12]; a[4] += 2*(int64_t)c[13];
    a[5] += 2*(int64_t)c[14]; a[6] += 2*(int64_t)c[15];
    /* +s4: {c8,c9,c10,0,0,0,c14,c15} */
    a[0] += c[8]; a[1] += c[9]; a[2] += c[10];
    a[6] += c[14]; a[7] += c[15];
    /* +s5: {c9,c10,c11,c13,c14,c15,c13,c8} */
    a[0] += c[9]; a[1] += c[10]; a[2] += c[11];
    a[3] += c[13]; a[4] += c[14]; a[5] += c[15];
    a[6] += c[13]; a[7] += c[8];
    /* -d1: {c11,c12,c13,0,0,0,c8,c10} */
    a[0] -= c[11]; a[1] -= c[12]; a[2] -= c[13];
    a[6] -= c[8]; a[7] -= c[10];
    /* -d2: {c12,0,0,c14,c15,0,c9,c11} */
    a[0] -= c[12]; a[3] -= c[14]; a[4] -= c[15];
    a[6] -= c[9]; a[7] -= c[11];
    /* -d3: {c13,c14,c15,c8,c9,c10,0,c12} */
    a[0] -= c[13]; a[1] -= c[14]; a[2] -= c[15];
    a[3] -= c[8]; a[4] -= c[9]; a[5] -= c[10];
    a[7] -= c[12];
    /* -d4: {c14,c15,0,c9,c10,c11,0,c13} */
    a[0] -= c[14]; a[1] -= c[15];
    a[3] -= c[9]; a[4] -= c[10]; a[5] -= c[11];
    a[7] -= c[13];

    /* Propagate carries */
    int64_t carry = 0;
    for (int i = 0; i < 8; i++) {
        a[i] += carry;
        r[i] = (uint32_t)((uint64_t)a[i] & 0xFFFFFFFFULL);
        carry = a[i] >> 32;
    }

    /* Normalize: add/subtract p until in [0, p) */
    while (carry > 0) {
        uint32_t tmp[8];
        u256_sub(tmp, r, P256_P);
        memcpy(r, tmp, 32);
        carry--;
    }
    while (carry < 0) {
        uint32_t tmp[8];
        u256_add(tmp, r, P256_P);
        memcpy(r, tmp, 32);
        carry++;
    }
    if (u256_cmp(r, P256_P) >= 0) {
        uint32_t tmp[8];
        u256_sub(tmp, r, P256_P);
        memcpy(r, tmp, 32);
    }
}

static void fp_add(const uint32_t *a, const uint32_t *b, uint32_t *r) {
    uint32_t c = u256_add(r, a, b);
    if (c || u256_cmp(r, P256_P) >= 0) {
        uint32_t tmp[8];
        u256_sub(tmp, r, P256_P);
        memcpy(r, tmp, 32);
    }
}

static void fp_sub(const uint32_t *a, const uint32_t *b, uint32_t *r) {
    uint32_t bw = u256_sub(r, a, b);
    if (bw) {
        uint32_t tmp[8];
        u256_add(tmp, r, P256_P);
        memcpy(r, tmp, 32);
    }
}

static void fp_mul(const uint32_t *a, const uint32_t *b, uint32_t *r) {
    uint32_t t[16];
    u256_mul_512(a, b, t);
    fp_reduce(t, r);
}

static void fp_sqr(const uint32_t *a, uint32_t *r) {
    fp_mul(a, a, r);
}

/* Fermat inversion: r = a^(p-2) mod p */
static void fp_inv(const uint32_t *a, uint32_t *r) {
    uint32_t pm2[8];
    memcpy(pm2, P256_P, 32);
    pm2[0] -= 2;  /* p - 2 */

    uint32_t u[8];
    memset(r, 0, 32);
    r[0] = 1;
    for (int i = 255; i >= 0; i--) {
        fp_sqr(r, u);
        if ((pm2[i / 32] >> (i % 32)) & 1)
            fp_mul(u, a, r);
        else
            memcpy(r, u, 32);
    }
}

/* =========================================================================
 * Scalar (mod N) Arithmetic for ECDSA
 * ========================================================================= */

static void fn_mod_reduce(const uint32_t c[16], uint32_t r[8]) {
    uint32_t w[17];
    memcpy(w, c, 64);
    w[16] = 0;

    int w_bits = bn_bitlen(w, 16);
    int m_bits = bn_bitlen((const uint32_t *)P256_N, 8);
    if (w_bits <= m_bits && bn_cmp(w, P256_N, 8) < 0) {
        memcpy(r, w, 32);
        return;
    }

    int shift = w_bits - m_bits;
    if (shift < 0) shift = 0;

    uint32_t ms[17];
    memset(ms, 0, sizeof(ms));
    int ws = shift / 32;
    int bs = shift % 32;
    for (int i = 0; i < 8; i++) {
        if (i + ws < 17) {
            if (bs)
                ms[i + ws] |= P256_N[i] << bs;
            else
                ms[i + ws] |= P256_N[i];
        }
        if (bs && i + ws + 1 < 17)
            ms[i + ws + 1] |= P256_N[i] >> (32 - bs);
    }

    for (int i = shift; i >= 0; i--) {
        if (bn_cmp(w, ms, 17) >= 0)
            bn_sub(w, ms, 17);
        for (int j = 0; j < 16; j++)
            ms[j] = (ms[j] >> 1) | (ms[j + 1] << 31);
        ms[16] >>= 1;
    }
    memcpy(r, w, 32);
}

static void fn_add(const uint32_t *a, const uint32_t *b, uint32_t *r) {
    uint32_t c = u256_add(r, a, b);
    if (c || u256_cmp(r, P256_N) >= 0) {
        uint32_t tmp[8];
        u256_sub(tmp, r, P256_N);
        memcpy(r, tmp, 32);
    }
}

static void fn_mul(const uint32_t *a, const uint32_t *b, uint32_t *r) {
    uint32_t t[16];
    u256_mul_512(a, b, t);
    fn_mod_reduce(t, r);
}

/* Fermat inversion mod N: r = a^(N-2) mod N */
static void fn_inv(const uint32_t *a, uint32_t *r) {
    uint32_t nm2[8];
    memcpy(nm2, P256_N, 32);
    uint64_t borrow = 2;
    for (int i = 0; i < 8 && borrow; i++) {
        uint64_t d = (uint64_t)nm2[i] - (borrow & 0xFFFFFFFF);
        nm2[i] = (uint32_t)d;
        borrow = (d >> 63) & 1;
    }

    uint32_t u[8];
    memset(r, 0, 32);
    r[0] = 1;
    for (int i = 255; i >= 0; i--) {
        fn_mul(r, r, u);
        if ((nm2[i / 32] >> (i % 32)) & 1)
            fn_mul(u, a, r);
        else
            memcpy(r, u, 32);
    }
}

/* =========================================================================
 * P-256 Point Operations (Jacobian coordinates: X, Y, Z)
 * Affine (x,y) = (X/Z^2, Y/Z^3)
 * ========================================================================= */

typedef struct {
    uint32_t x[8], y[8], z[8];
} P256Point;

static void p256_set_infinity(P256Point *p) {
    memset(p, 0, sizeof(*p));
    p->y[0] = 1;
}

static int p256_is_infinity(const P256Point *p) {
    return u256_is_zero(p->z);
}

/* Point doubling (a = -3 for P-256) */
static void p256_double(const P256Point *p, P256Point *r) {
    if (p256_is_infinity(p)) { *r = *p; return; }

    uint32_t s[8], m[8], t[8], u[8], v[8];

    /* S = 4*X*Y^2 */
    fp_sqr(p->y, t);
    fp_mul(p->x, t, s);
    fp_add(s, s, s);
    fp_add(s, s, s);

    /* M = 3*X^2 + a*Z^4  (a = -3) */
    fp_sqr(p->x, u);
    fp_add(u, u, v);
    fp_add(v, u, m);       /* 3*X^2 */
    fp_sqr(p->z, u);
    fp_sqr(u, v);          /* Z^4 */
    fp_add(v, v, u);
    fp_add(u, v, u);       /* 3*Z^4 */
    fp_sub(m, u, m);       /* 3*X^2 - 3*Z^4 */

    /* X' = M^2 - 2*S */
    fp_sqr(m, r->x);
    fp_sub(r->x, s, r->x);
    fp_sub(r->x, s, r->x);

    /* Y' = M*(S - X') - 8*Y^4 */
    fp_sub(s, r->x, u);
    fp_mul(m, u, r->y);
    fp_sqr(t, v);          /* Y^4 */
    fp_add(v, v, u);
    fp_add(u, u, u);
    fp_add(u, u, u);       /* 8*Y^4 */
    fp_sub(r->y, u, r->y);

    /* Z' = 2*Y*Z */
    fp_mul(p->y, p->z, r->z);
    fp_add(r->z, r->z, r->z);
}

/* Point addition (full Jacobian) */
static void p256_add(const P256Point *p, const P256Point *q, P256Point *r) {
    if (p256_is_infinity(p)) { *r = *q; return; }
    if (p256_is_infinity(q)) { *r = *p; return; }

    uint32_t z1z1[8], z2z2[8], u1[8], u2[8], s1[8], s2[8];
    uint32_t h[8], hh[8], hhh[8], rr[8], v[8], t[8];

    fp_sqr(p->z, z1z1);
    fp_sqr(q->z, z2z2);
    fp_mul(p->x, z2z2, u1);
    fp_mul(q->x, z1z1, u2);
    fp_mul(p->y, z2z2, s1); fp_mul(s1, q->z, s1);
    fp_mul(q->y, z1z1, s2); fp_mul(s2, p->z, s2);

    if (u256_cmp(u1, u2) == 0) {
        if (u256_cmp(s1, s2) == 0) { p256_double(p, r); return; }
        p256_set_infinity(r);
        return;
    }

    fp_sub(u2, u1, h);
    fp_sqr(h, hh);
    fp_mul(hh, h, hhh);
    fp_sub(s2, s1, rr);
    fp_mul(u1, hh, v);

    fp_sqr(rr, r->x);
    fp_sub(r->x, hhh, r->x);
    fp_sub(r->x, v, r->x);
    fp_sub(r->x, v, r->x);

    fp_sub(v, r->x, t);
    fp_mul(rr, t, r->y);
    fp_mul(s1, hhh, t);
    fp_sub(r->y, t, r->y);

    fp_mul(p->z, q->z, t);
    fp_mul(t, h, r->z);
}

/* Scalar multiplication: R = k * P */
static void p256_scalar_mul(const uint32_t k[8], const P256Point *p,
                            P256Point *r) {
    P256Point acc, tmp;
    p256_set_infinity(&acc);
    for (int i = 255; i >= 0; i--) {
        p256_double(&acc, &tmp);
        acc = tmp;
        if ((k[i / 32] >> (i % 32)) & 1) {
            p256_add(&acc, p, &tmp);
            acc = tmp;
        }
    }
    *r = acc;
}

/* Convert Jacobian to affine */
static void p256_to_affine(const P256Point *p, uint32_t *ax, uint32_t *ay) {
    uint32_t zi[8], zi2[8], zi3[8];
    fp_inv(p->z, zi);
    fp_sqr(zi, zi2);
    fp_mul(zi, zi2, zi3);
    fp_mul(p->x, zi2, ax);
    fp_mul(p->y, zi3, ay);
}

/* =========================================================================
 * ECDSA with P-256 (deterministic k via RFC 6979 / HMAC-SHA256)
 * ========================================================================= */

static void hmac_sha256(const uint8_t *key, size_t key_len,
                        const uint8_t *msg, size_t msg_len,
                        uint8_t out[32]) {
    uint8_t k_pad[64];
    EosSha256 ctx;
    uint8_t khash[32];

    if (key_len > 64) {
        eos_sha256_init(&ctx);
        eos_sha256_update(&ctx, key, key_len);
        eos_sha256_final(&ctx, khash);
        key = khash;
        key_len = 32;
    }

    memset(k_pad, 0x36, 64);
    for (size_t i = 0; i < key_len; i++) k_pad[i] ^= key[i];
    eos_sha256_init(&ctx);
    eos_sha256_update(&ctx, k_pad, 64);
    eos_sha256_update(&ctx, msg, msg_len);
    uint8_t inner[32];
    eos_sha256_final(&ctx, inner);

    memset(k_pad, 0x5c, 64);
    for (size_t i = 0; i < key_len; i++) k_pad[i] ^= key[i];
    eos_sha256_init(&ctx);
    eos_sha256_update(&ctx, k_pad, 64);
    eos_sha256_update(&ctx, inner, 32);
    eos_sha256_final(&ctx, out);
}

/* RFC 6979 deterministic k generation */
static void rfc6979_generate_k(const uint8_t priv[32], const uint8_t *hash,
                               size_t hash_len, uint32_t k_out[8]) {
    uint8_t V[32], K[32];
    memset(V, 0x01, 32);
    memset(K, 0x00, 32);

    uint8_t h1[32];
    memset(h1, 0, 32);
    if (hash_len >= 32)
        memcpy(h1, hash, 32);
    else
        memcpy(h1 + (32 - hash_len), hash, hash_len);

    /* K = HMAC_K(V || 0x00 || priv || h1) */
    uint8_t buf[97];
    memcpy(buf, V, 32);
    buf[32] = 0x00;
    memcpy(buf + 33, priv, 32);
    memcpy(buf + 65, h1, 32);
    hmac_sha256(K, 32, buf, 97, K);
    hmac_sha256(K, 32, V, 32, V);

    /* K = HMAC_K(V || 0x01 || priv || h1) */
    memcpy(buf, V, 32);
    buf[32] = 0x01;
    memcpy(buf + 33, priv, 32);
    memcpy(buf + 65, h1, 32);
    hmac_sha256(K, 32, buf, 97, K);
    hmac_sha256(K, 32, V, 32, V);

    for (;;) {
        hmac_sha256(K, 32, V, 32, V);
        u256_from_bytes(k_out, V);
        if (!u256_is_zero(k_out) && u256_cmp(k_out, P256_N) < 0)
            return;
        memcpy(buf, V, 32);
        buf[32] = 0x00;
        hmac_sha256(K, 32, buf, 33, K);
        hmac_sha256(K, 32, V, 32, V);
    }
}

int eos_ecc_sign(const EosEccKey *key, const uint8_t *hash, size_t hash_len,
                 uint8_t *sig, size_t *sig_len) {
    if (!key || !hash || !sig || !sig_len) return -1;

    uint32_t d[8], z[8], k[8];
    u256_from_bytes(d, key->priv);

    uint8_t h32[32];
    memset(h32, 0, 32);
    if (hash_len >= 32)
        memcpy(h32, hash, 32);
    else
        memcpy(h32 + (32 - hash_len), hash, hash_len);
    u256_from_bytes(z, h32);

    if (u256_cmp(z, P256_N) >= 0) {
        uint32_t tmp[8];
        u256_sub(tmp, z, P256_N);
        memcpy(z, tmp, 32);
    }

    rfc6979_generate_k(key->priv, hash, hash_len, k);

    P256Point G, R;
    memcpy(G.x, P256_GX, 32);
    memcpy(G.y, P256_GY, 32);
    memset(G.z, 0, 32); G.z[0] = 1;

    p256_scalar_mul(k, &G, &R);

    uint32_t rx[8], ry[8];
    p256_to_affine(&R, rx, ry);

    uint32_t r_sig[8];
    memcpy(r_sig, rx, 32);
    if (u256_cmp(r_sig, P256_N) >= 0) {
        uint32_t tmp[8];
        u256_sub(tmp, r_sig, P256_N);
        memcpy(r_sig, tmp, 32);
    }
    if (u256_is_zero(r_sig)) return -1;

    /* s = k^-1 * (z + r * d) mod N */
    uint32_t ki[8], rd[8], zrd[8], s_sig[8];
    fn_inv(k, ki);
    fn_mul(r_sig, d, rd);
    fn_add(z, rd, zrd);
    fn_mul(ki, zrd, s_sig);
    if (u256_is_zero(s_sig)) return -1;

    u256_to_bytes(r_sig, sig);
    u256_to_bytes(s_sig, sig + 32);
    *sig_len = 64;
    return 0;
}

int eos_ecc_verify(const EosEccKey *key, const uint8_t *hash, size_t hash_len,
                   const uint8_t *sig, size_t sig_len) {
    if (!key || !hash || !sig || sig_len < 64) return -1;

    uint32_t r_sig[8], s_sig[8], z[8];
    u256_from_bytes(r_sig, sig);
    u256_from_bytes(s_sig, sig + 32);

    if (u256_is_zero(r_sig) || u256_cmp(r_sig, P256_N) >= 0) return -1;
    if (u256_is_zero(s_sig) || u256_cmp(s_sig, P256_N) >= 0) return -1;

    uint8_t h32[32];
    memset(h32, 0, 32);
    if (hash_len >= 32)
        memcpy(h32, hash, 32);
    else
        memcpy(h32 + (32 - hash_len), hash, hash_len);
    u256_from_bytes(z, h32);
    if (u256_cmp(z, P256_N) >= 0) {
        uint32_t tmp[8];
        u256_sub(tmp, z, P256_N);
        memcpy(z, tmp, 32);
    }

    uint32_t w[8];
    fn_inv(s_sig, w);

    uint32_t u1[8], u2[8];
    fn_mul(z, w, u1);
    fn_mul(r_sig, w, u2);

    P256Point G, Q, R1, R2, R;
    memcpy(G.x, P256_GX, 32); memcpy(G.y, P256_GY, 32);
    memset(G.z, 0, 32); G.z[0] = 1;

    u256_from_bytes(Q.x, key->pub);
    u256_from_bytes(Q.y, key->pub + 32);
    memset(Q.z, 0, 32); Q.z[0] = 1;

    p256_scalar_mul(u1, &G, &R1);
    p256_scalar_mul(u2, &Q, &R2);
    p256_add(&R1, &R2, &R);

    if (p256_is_infinity(&R)) return -1;

    uint32_t rx[8], ry[8];
    p256_to_affine(&R, rx, ry);

    if (u256_cmp(rx, P256_N) >= 0) {
        uint32_t tmp[8];
        u256_sub(tmp, rx, P256_N);
        memcpy(rx, tmp, 32);
    }

    if (u256_cmp(rx, r_sig) != 0) return -1;
    return 0;
}

/* =========================================================================
 * SHA-512 Implementation (unchanged)
 * ========================================================================= */

static const uint64_t K512[80] = {
    0x428a2f98d728ae22ULL,0x7137449123ef65cdULL,0xb5c0fbcfec4d3b2fULL,0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL,0x59f111f1b605d019ULL,0x923f82a4af194f9bULL,0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL,0x12835b0145706fbeULL,0x243185be4ee4b28cULL,0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL,0x80deb1fe3b1696b1ULL,0x9bdc06a725c71235ULL,0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL,0xefbe4786384f25e3ULL,0x0fc19dc68b8cd5b5ULL,0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL,0x4a7484aa6ea6e483ULL,0x5cb0a9dcbd41fbd4ULL,0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL,0xa831c66d2db43210ULL,0xb00327c898fb213fULL,0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL,0xd5a79147930aa725ULL,0x06ca6351e003826fULL,0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL,0x2e1b21385c26c926ULL,0x4d2c6dfc5ac42aedULL,0x53380d139d95b3dfULL,
    0x650a73548baf63deULL,0x766a0abb3c77b2a8ULL,0x81c2c92e47edaee6ULL,0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL,0xa81a664bbc423001ULL,0xc24b8b70d0f89791ULL,0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL,0xd69906245565a910ULL,0xf40e35855771202aULL,0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL,0x1e376c085141ab53ULL,0x2748774cdf8eeb99ULL,0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL,0x4ed8aa4ae3418acbULL,0x5b9cca4f7763e373ULL,0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL,0x78a5636f43172f60ULL,0x84c87814a1f0ab72ULL,0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL,0xa4506cebde82bde9ULL,0xbef9a3f7b2c67915ULL,0xc67178f2e372532bULL,
    0xca273eceea26619cULL,0xd186b8c721c0c207ULL,0xeada7dd6cde0eb1eULL,0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL,0x0a637dc5a2c898a6ULL,0x113f9804bef90daeULL,0x1b710b35131c471bULL,
    0x28db77f523047d84ULL,0x32caab7b40c72493ULL,0x3c9ebe0a15c9bebcULL,0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL,0x597f299cfc657e2aULL,0x5fcb6fab3ad6faecULL,0x6c44198c4a475817ULL
};

#define RR64(x,n) (((x)>>(n))|((x)<<(64-(n))))
#define CH64(x,y,z) (((x)&(y))^((~(x))&(z)))
#define MAJ64(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define EP0_512(x) (RR64(x,28)^RR64(x,34)^RR64(x,39))
#define EP1_512(x) (RR64(x,14)^RR64(x,18)^RR64(x,41))
#define SIG0_512(x) (RR64(x,1)^RR64(x,8)^((x)>>7))
#define SIG1_512(x) (RR64(x,19)^RR64(x,61)^((x)>>6))

void eos_sha512_init(EosSha512 *ctx) {
    if (!ctx) return;
    ctx->state[0]=0x6a09e667f3bcc908ULL; ctx->state[1]=0xbb67ae8584caa73bULL;
    ctx->state[2]=0x3c6ef372fe94f82bULL; ctx->state[3]=0xa54ff53a5f1d36f1ULL;
    ctx->state[4]=0x510e527fade682d1ULL; ctx->state[5]=0x9b05688c2b3e6c1fULL;
    ctx->state[6]=0x1f83d9abfb41bd6bULL; ctx->state[7]=0x5be0cd19137e2179ULL;
    ctx->total = 0;
}

static void sha512_transform(EosSha512 *ctx, const uint8_t *block) {
    uint64_t w[80], a,b,c,d,e,f,g,h,t1,t2;
    for (int i=0;i<16;i++)
        w[i]=(uint64_t)block[i*8]<<56|(uint64_t)block[i*8+1]<<48|
             (uint64_t)block[i*8+2]<<40|(uint64_t)block[i*8+3]<<32|
             (uint64_t)block[i*8+4]<<24|(uint64_t)block[i*8+5]<<16|
             (uint64_t)block[i*8+6]<<8|(uint64_t)block[i*8+7];
    for (int i=16;i<80;i++)
        w[i]=SIG1_512(w[i-2])+w[i-7]+SIG0_512(w[i-15])+w[i-16];
    a=ctx->state[0];b=ctx->state[1];c=ctx->state[2];d=ctx->state[3];
    e=ctx->state[4];f=ctx->state[5];g=ctx->state[6];h=ctx->state[7];
    for (int i=0;i<80;i++){
        t1=h+EP1_512(e)+CH64(e,f,g)+K512[i]+w[i];
        t2=EP0_512(a)+MAJ64(a,b,c);
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    ctx->state[0]+=a;ctx->state[1]+=b;ctx->state[2]+=c;ctx->state[3]+=d;
    ctx->state[4]+=e;ctx->state[5]+=f;ctx->state[6]+=g;ctx->state[7]+=h;
}

void eos_sha512_update(EosSha512 *ctx, const void *data, size_t len) {
    if (!ctx || !data || len == 0) return;
    const uint8_t *p = (const uint8_t *)data;
    size_t off = (size_t)(ctx->total % 128);
    ctx->total += len;
    while (len > 0) {
        size_t n = 128 - off;
        if (n > len) n = len;
        memcpy(ctx->buf + off, p, n);
        p += n; len -= n; off += n;
        if (off == 128) { sha512_transform(ctx, ctx->buf); off = 0; }
    }
}

void eos_sha512_final(EosSha512 *ctx, uint8_t digest[EOS_SHA512_DIGEST_SIZE]) {
    if (!ctx || !digest) return;
    uint64_t bits = ctx->total * 8;
    size_t off = (size_t)(ctx->total % 128);
    ctx->buf[off++] = 0x80;
    if (off > 112) {
        memset(ctx->buf + off, 0, 128 - off);
        sha512_transform(ctx, ctx->buf);
        off = 0;
    }
    memset(ctx->buf + off, 0, 120 - off);
    for (int i=0;i<8;i++) ctx->buf[120+i]=(uint8_t)(bits>>(56-i*8));
    sha512_transform(ctx, ctx->buf);
    for (int i=0;i<8;i++) {
        digest[i*8]  =(uint8_t)(ctx->state[i]>>56);
        digest[i*8+1]=(uint8_t)(ctx->state[i]>>48);
        digest[i*8+2]=(uint8_t)(ctx->state[i]>>40);
        digest[i*8+3]=(uint8_t)(ctx->state[i]>>32);
        digest[i*8+4]=(uint8_t)(ctx->state[i]>>24);
        digest[i*8+5]=(uint8_t)(ctx->state[i]>>16);
        digest[i*8+6]=(uint8_t)(ctx->state[i]>>8);
        digest[i*8+7]=(uint8_t)(ctx->state[i]);
    }
}

void eos_sha512_hex(const uint8_t digest[EOS_SHA512_DIGEST_SIZE],
                    char hex[EOS_SHA512_HEX_SIZE]) {
    if (!digest || !hex) return;
    for (int i=0;i<64;i++) sprintf(hex+i*2, "%02x", digest[i]);
    hex[128] = '\0';
}
