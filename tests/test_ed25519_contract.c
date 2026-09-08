// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file test_ed25519_contract.c
 * @brief eos's half of the Ed25519 contract shared with eBoot.
 *
 * eos and eBoot each carry their own Ed25519 verifier. They are different
 * implementations with opposite return conventions -- ed25519_verify() here
 * returns 1 to accept, eBoot's eos_ed25519_verify() returns EOS_OK -- doing the
 * same job on the same wire format. For a while only this side rejected
 * low-order public keys, and nothing in either repo could notice: there is no
 * shared build, and the two are far too different for a source diff to mean
 * anything.
 *
 * tests/vectors/ed25519_contract_vectors.h is the part that can be shared: pure
 * data, byte-identical in both repos, with a digest over it. Each side runs its
 * own verifier against every vector and prints that digest. A change to one copy
 * that does not reach the other shows up as two different digests.
 *
 * The eBoot-side twin is tests/unit/test_ed25519_contract.c in
 * embeddedos-org/eBoot. Both should always report the digest below.
 */

#include "ed25519.h"
#include "eos/crypto.h"

#include "vectors/ed25519_contract_vectors.h"

#include <stdio.h>
#include <string.h>

/* The corpus digest, pinned in committed source.
 *
 * EOS_ED25519_CONTRACT_DIGEST comes from the generated header, which this
 * repo's own copy of tools/gen_ed25519_contract_vectors.py just wrote -- a
 * hash the generator took over its own output. Comparing it against nothing
 * made the drift guard unfalsifiable: edit the generator here and not in
 * eBoot and this repo regenerates a self-consistent corpus with a new digest,
 * the test still exits 0, and the divergence is visible only to a human
 * reading two CI logs in two repositories.
 *
 * Recomputed over the vector bytes at run time, so an edited vector fails as
 * loudly as a changed generator. This value must stay byte-identical to the
 * one in eBoot's tests/unit/test_ed25519_contract.c. The count and accept-count are pinned
 * for the same reason: both are generated, so a corpus that silently shrank
 * would otherwise still pass.
 */
#define EOS_ED25519_CONTRACT_EXPECTED \
    "1059febedae2b3e3dfaa1ef5b419fb37ed7f0d53b377a3250b53b95a546282a2"
#define EOS_ED25519_CONTRACT_EXPECTED_COUNT     76
#define EOS_ED25519_CONTRACT_EXPECTED_ACCEPTS   3

/* SHA-256 over the corpus, in the generator's serialisation order.
 *
 * Recomputed here rather than read back from the generated header. The
 * previous version compared EOS_ED25519_CONTRACT_DIGEST -- a #define written
 * into that same header -- against the literal below, so nothing hashed the
 * vectors: editing a committed vector changed the data and left the value it
 * was compared against untouched, and the suite still passed. Same fix as
 * eBoot's tests/unit/test_ed25519_contract.c, and the serialisation matches
 * tools/gen_ed25519_contract_vectors.py exactly.
 */
static void contract_digest(char out_hex[65]) {
    EosSha256 sha;
    uint8_t digest[EOS_SHA256_DIGEST_SIZE];
    int i, j;

    eos_sha256_init(&sha);
    for (i = 0; i < EOS_ED25519_CONTRACT_COUNT; i++) {
        const eos_ed25519_contract_vector_t *v =
            &eos_ed25519_contract_vectors[i];
        uint8_t accept = (uint8_t)v->expect_accept;

        eos_sha256_update(&sha, v->public_key, sizeof v->public_key);
        eos_sha256_update(&sha, v->signature, sizeof v->signature);
        eos_sha256_update(&sha, v->message, v->message_len);
        eos_sha256_update(&sha, &accept, 1);
    }
    eos_sha256_final(&sha, digest);

    for (j = 0; j < EOS_SHA256_DIGEST_SIZE; j++)
        snprintf(out_hex + j * 2, 3, "%02x", digest[j]);
    out_hex[64] = '\0';
}

int main(void) {
    unsigned failed = 0, accepted = 0, refused = 0, positives = 0;
    int i;

    printf("Ed25519 contract vectors\n");
    printf("  digest: %s\n", EOS_ED25519_CONTRACT_DIGEST);
    printf("  count:  %d\n\n", EOS_ED25519_CONTRACT_COUNT);

    char computed[65];
    contract_digest(computed);

    if (strcmp(computed, EOS_ED25519_CONTRACT_EXPECTED) != 0) {
        printf("[FAIL] corpus digest changed\n"
               "       expected %s\n"
               "       got      %s\n"
               "       A vector was edited, the generator was changed, or the\n"
               "       two repos' copies have diverged. Re-derive, confirm eos\n"
               "       and eBoot agree, then update the pin in BOTH.\n",
               EOS_ED25519_CONTRACT_EXPECTED, computed);
        return 1;
    }

    /* The header's own claim must agree too: if it does not, the committed
     * corpus and the header describing it came from different runs. */
    if (strcmp(computed, EOS_ED25519_CONTRACT_DIGEST) != 0) {
        printf("[FAIL] the header's digest (%s) does not describe the vectors "
               "it ships with (%s)\n",
               EOS_ED25519_CONTRACT_DIGEST, computed);
        return 1;
    }

    if (EOS_ED25519_CONTRACT_COUNT != EOS_ED25519_CONTRACT_EXPECTED_COUNT) {
        printf("[FAIL] corpus is %d vectors, expected %d\n",
               EOS_ED25519_CONTRACT_COUNT,
               EOS_ED25519_CONTRACT_EXPECTED_COUNT);
        return 1;
    }

    for (i = 0; i < EOS_ED25519_CONTRACT_COUNT; i++) {
        const eos_ed25519_contract_vector_t *v = &eos_ed25519_contract_vectors[i];

        /* eos convention: 1 accepts, 0 refuses -- the inverse of eBoot's. */
        int got_accept = (ed25519_verify(v->signature, v->message,
                                         v->message_len, v->public_key) == 1);

        if (got_accept) accepted++; else refused++;
        if (v->expect_accept) positives++;

        if (got_accept != v->expect_accept) {
            printf("  [FAIL] %s\n         expected %s, got %s\n         %s\n",
                   v->name,
                   v->expect_accept ? "ACCEPT" : "refuse",
                   got_accept ? "ACCEPT" : "refuse",
                   v->why);
            failed++;
        }
    }

    printf("  %u accepted, %u refused, %u wrong\n\n", accepted, refused, failed);

    if (failed) {
        printf("[FAIL] %u of %d contract vectors behaved incorrectly\n",
               failed, EOS_ED25519_CONTRACT_COUNT);
        return 1;
    }

    /* A verifier that refuses everything satisfies all 73 negative vectors, so
     * the three RFC 8032 signatures carry the whole weight of proving this
     * still accepts real ones. Fail loudly if they are ever dropped. */
    if (positives != EOS_ED25519_CONTRACT_EXPECTED_ACCEPTS) {
        printf("[FAIL] the corpus has %u accept vectors, expected %d; "
               "a verifier that refuses everything would otherwise pass\n",
               positives, EOS_ED25519_CONTRACT_EXPECTED_ACCEPTS);
        return 1;
    }

    printf("[PASS] all %d contract vectors behaved as specified (%u must accept)\n",
           EOS_ED25519_CONTRACT_COUNT, positives);
    return 0;
}
