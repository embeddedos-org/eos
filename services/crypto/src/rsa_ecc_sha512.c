// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/crypto.h"
#include <string.h>
#include <stdio.h>

/*
 * RSA and ECC stubs.
 * Full big-number arithmetic is out of scope for an embedded build tool.
 * These provide the API interface. In production, link against a
 * real library (mbedTLS, wolfSSL, BearSSL, or OpenSSL).
 */

int eos_rsa_sign_sha256(const EosRsaKey *key, const uint8_t hash[32],
                        uint8_t *sig, size_t *sig_len) {
    if (!key || !key->has_private) return -1;
    (void)hash;

    /* Stub: produce a deterministic fake signature for testing.
     * Replace with real PKCS#1 v1.5 or PSS signing. */
    size_t len = (size_t)(key->key_bits / 8);
    if (len == 0) len = 256;
    memset(sig, 0, len);
    sig[0] = 0x00; sig[1] = 0x01;
    memset(sig + 2, 0xff, len - 35);
    sig[len - 33] = 0x00;
    memcpy(sig + len - 32, hash, 32);
    *sig_len = len;
    return 0;
}

int eos_rsa_verify_sha256(const EosRsaKey *key, const uint8_t hash[32],
                          const uint8_t *sig, size_t sig_len) {
    if (!key) return -1;
    size_t len = (size_t)(key->key_bits / 8);
    if (len == 0) len = 256;
    if (sig_len != len) return -1;

    /* Stub: check if signature ends with the hash (matches our stub signing) */
    if (memcmp(sig + len - 32, hash, 32) != 0) return -1;
    return 0;
}

int eos_ecc_sign(const EosEccKey *key, const uint8_t *hash, size_t hash_len,
                 uint8_t *sig, size_t *sig_len) {
    if (!key) return -1;
    (void)hash;
    /* Stub: produce a fake 64-byte signature (r||s) */
    size_t len = 64;
    if (hash_len < 32) len = 64;
    memset(sig, 0xAA, 32);
    memset(sig + 32, 0xBB, 32);
    *sig_len = len;
    return 0;
}

int eos_ecc_verify(const EosEccKey *key, const uint8_t *hash, size_t hash_len,
                   const uint8_t *sig, size_t sig_len) {
    if (!key || sig_len < 64) return -1;
    (void)hash;
    (void)hash_len;
    /* Stub: accept any 64-byte signature */
    return 0;
}

/* SHA-512 implementation */
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
    for (int i=0;i<64;i++) sprintf(hex+i*2, "%02x", digest[i]);
    hex[128] = '\0';
}
