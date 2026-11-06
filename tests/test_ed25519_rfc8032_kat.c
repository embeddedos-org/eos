// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file test_ed25519_rfc8032_kat.c
 * @brief Key generation and signing reproduce RFC 8032 section 7.1 exactly.
 *
 * test_ed25519_contract proves the verifier accepts the RFC 8032 signatures,
 * and the package tests sign and then verify their own output. Neither pins
 * what ed25519_create_keypair() and ed25519_sign() produce: a change that
 * altered the field or scalar arithmetic on both sides alike would still
 * round-trip. Ed25519 is deterministic, so the exact bytes are checkable.
 *
 * This matters because the ref10 arithmetic under services/crypto/src used to
 * left-shift negative signed values and overflow int32_t when packing bytes --
 * undefined behaviour a compiler may miscompile. Those were rewritten as
 * multiplications and unsigned shifts; this test is what says the rewrite
 * produced the same numbers. Run it under -fsanitize=undefined as well, as the
 * nightly Sanitizers job does, to say there is no UB left on these paths.
 *
 * Public keys, messages and signatures come from the shared contract corpus,
 * so they are not duplicated here. The corpus carries no secrets, so the three
 * seeds are RFC 8032's own, checked against that corpus with an independent
 * implementation before being committed.
 */

#include "ed25519.h"

#include "vectors/ed25519_contract_vectors.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char    *name;      /* entry in eos_ed25519_contract_vectors */
    unsigned char  seed[32];  /* RFC 8032 section 7.1 SECRET KEY */
} rfc8032_seed_t;

static const rfc8032_seed_t seeds[] = {
    { "rfc8032_test1_empty",
      {0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,
       0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60} },
    { "rfc8032_test2_1byte",
      {0x4c,0xcd,0x08,0x9b,0x28,0xff,0x96,0xda,0x9d,0xb6,0xc3,0x46,0xec,0x11,0x4e,0x0f,
       0x5b,0x8a,0x31,0x9f,0x35,0xab,0xa6,0x24,0xda,0x8c,0xf6,0xed,0x4f,0xb8,0xa6,0xfb} },
    { "rfc8032_test3_2byte",
      {0xc5,0xaa,0x8d,0xf4,0x3f,0x9f,0x83,0x7b,0xed,0xb7,0x44,0x2f,0x31,0xdc,0xb7,0xb1,
       0x66,0xd3,0x85,0x35,0x07,0x6f,0x09,0x4b,0x85,0xce,0x3a,0x2e,0x0b,0x44,0x58,0xf7} },
};

static const eos_ed25519_contract_vector_t *find_vector(const char *name) {
    for (int i = 0; i < EOS_ED25519_CONTRACT_COUNT; i++) {
        if (strcmp(eos_ed25519_contract_vectors[i].name, name) == 0)
            return &eos_ed25519_contract_vectors[i];
    }
    return NULL;
}

static void print_hex(const char *label, const unsigned char *p, size_t n) {
    printf("    %-9s ", label);
    for (size_t i = 0; i < n; i++) printf("%02x", p[i]);
    printf("\n");
}

int main(void) {
    unsigned failed = 0;
    const size_t count = sizeof seeds / sizeof seeds[0];

    printf("=== Ed25519 RFC 8032 known answers (keygen + sign) ===\n");

    for (size_t i = 0; i < count; i++) {
        const eos_ed25519_contract_vector_t *v = find_vector(seeds[i].name);
        unsigned char pub[32], priv[64], sig[64];

        printf("  %-28s ", seeds[i].name);

        /* The corpus is generated; if an entry is renamed or dropped, say so
         * rather than silently testing fewer vectors. */
        if (!v || !v->expect_accept) {
            printf("[FAIL] not an accept vector in the contract corpus\n");
            failed++;
            continue;
        }

        ed25519_create_keypair(pub, priv, seeds[i].seed);
        if (memcmp(pub, v->public_key, sizeof pub) != 0) {
            printf("[FAIL] public key\n");
            print_hex("expected", v->public_key, sizeof pub);
            print_hex("got", pub, sizeof pub);
            failed++;
            continue;
        }

        ed25519_sign(sig, v->message, v->message_len, pub, priv);
        if (memcmp(sig, v->signature, sizeof sig) != 0) {
            printf("[FAIL] signature\n");
            print_hex("expected", v->signature, sizeof sig);
            print_hex("got", sig, sizeof sig);
            failed++;
            continue;
        }

        /* Close the loop through the verifier on the bytes just produced. */
        if (ed25519_verify(sig, v->message, v->message_len, pub) != 1) {
            printf("[FAIL] own signature refused\n");
            failed++;
            continue;
        }

        printf("[PASS]\n");
    }

    if (failed) {
        printf("[FAIL] %u of %zu RFC 8032 vectors\n", failed, count);
        return 1;
    }
    printf("[PASS] all %zu RFC 8032 vectors: keygen and signing are bit-exact\n", count);
    return 0;
}
