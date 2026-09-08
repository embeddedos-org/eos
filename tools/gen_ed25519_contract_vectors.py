#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 EoS Project
"""Generate the shared Ed25519 contract vectors.

embeddedos-org/eos and embeddedos-org/eBoot each carry their own Ed25519
verifier -- different code, different return conventions, the same job. In
August 2026 eos rejected low-order public keys and eBoot did not, and nothing
in either repo could notice: there is no shared build, and the implementations
are too different to diff.

This emits a header of pure test data that both repos compile against, so the
two verifiers are held to one contract.

The digest below is recomputed at run time by each side's driver, over the
vector bytes rather than read back from this header, and compared against a
literal pinned in committed test source. So a hand-edited vector fails as
loudly as a regenerated corpus -- which matters, because the hand edit is the
one a reviewer would not see in a diff of 76 byte arrays.

Regenerate with:
    python3 tools/gen_ed25519_contract_vectors.py > tests/vectors/ed25519_contract_vectors.h
"""

import hashlib
import sys

L = 2**252 + 27742317777372353535851937790883648493

# The eight points of order dividing 8, plus the two non-canonical y >= p
# encodings that decode to low-order points. A key of order n makes
# (R = identity, S = 0) verify whenever n divides SHA-512(R||A||M), i.e. for
# about one message in n -- so each is tried over several messages. A
# regression test pinned to a single message passes against unfixed code.
LOW_ORDER = [
    ("identity_order_1",   "0100000000000000000000000000000000000000000000000000000000000000"),
    ("order_2",            "ECFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7F"),
    ("order_4_zero",       "0000000000000000000000000000000000000000000000000000000000000000"),
    ("order_4_signbit",    "0000000000000000000000000000000000000000000000000000000000000080"),
    ("order_8_a",          "26E8958FC2B227B045C3F489F2EF98F0D5DFAC05D3C63339B13802886D53FC05"),
    ("order_8_b",          "C7176A703D4DD84FBA3C0B760D10670F2A2053FA2C39CCC64EC7FD7792AC037A"),
    ("noncanonical_ec_ff", "ECFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"),
    ("noncanonical_ed_ff", "EDFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"),
]
FORGERY_MESSAGES = [b"firmware image v%d" % i for i in range(8)]

# RFC 8032 section 7.1. Without these a verifier that rejects everything
# satisfies every case above.
RFC = [
    ("rfc8032_test1_empty",
     "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "",
     "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"),
    ("rfc8032_test2_1byte",
     "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
     "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00"),
    ("rfc8032_test3_2byte",
     "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025", "af82",
     "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a"),
]

def vectors():
    out = []
    ident = bytes([1] + [0] * 31)
    for name, pk_hex in LOW_ORDER:
        pk = bytes.fromhex(pk_hex)
        for i, msg in enumerate(FORGERY_MESSAGES):
            out.append((f"loworder_{name}_msg{i}", pk, ident + bytes(32), msg, 0,
                        "low-order key: (R=identity, S=0) must never verify"))
    for name, pk_hex, msg_hex, sig_hex in RFC:
        pk, msg, sig = bytes.fromhex(pk_hex), bytes.fromhex(msg_hex), bytes.fromhex(sig_hex)
        out.append((name, pk, sig, msg, 1, "RFC 8032 section 7.1: a genuine signature must verify"))
        t = bytearray(sig); t[0] ^= 0x01
        out.append((name + "_tampered_R", pk, bytes(t), msg, 0, "one bit flipped in R"))
        t = bytearray(sig); t[63] ^= 0x10
        out.append((name + "_tampered_S", pk, bytes(t), msg, 0, "one bit flipped in S"))
        # S + L is a non-canonical scalar: RFC 8032 5.1.7 requires rejecting it.
        s = int.from_bytes(sig[32:], "little")
        nc = (sig[:32] + ((s + L) % (2**256)).to_bytes(32, "little"))
        out.append((name + "_noncanonical_S", pk, nc, msg, 0,
                    "S + L is non-canonical; RFC 8032 5.1.7 requires rejection"))
    return out

def carr(b):
    return ",".join("0x%02x" % x for x in b)

def main():
    # The vectors are generated into the build tree rather than committed:
    # this file is the only copy either repo carries, and the header it emits
    # is 54 KB of generated data that used to be checked in to both.
    out = sys.stdout
    argv = sys.argv[1:]
    if len(argv) == 2 and argv[0] in ("-o", "--output"):
        out = open(argv[1], "w", newline="\n")
    elif argv:
        sys.stderr.write("usage: gen_ed25519_contract_vectors.py [-o OUTPUT]\n")
        return 2

    vs = vectors()
    digest_src = b"".join(pk + sig + msg + bytes([exp]) for _, pk, sig, msg, exp, _ in vs)
    digest = hashlib.sha256(digest_src).hexdigest()
    max_msg = max(len(m) for _, _, _, m, _, _ in vs)

    w = out.write
    w("/* SPDX-License-Identifier: MIT\n")
    w(" * Copyright (c) 2026 EoS Project\n */\n\n")
    w("/* GENERATED FILE -- do not edit by hand.\n")
    w(" * tools/gen_ed25519_contract_vectors.py\n *\n")
    w(" * The Ed25519 contract shared by embeddedos-org/eos and\n")
    w(" * embeddedos-org/eBoot. Each repo has its own verifier -- different code,\n")
    w(" * opposite return conventions, the same job -- and for a while only one of\n")
    w(" * them rejected low-order public keys. Nothing could notice: there is no\n")
    w(" * shared build, and the two are far too different to diff.\n *\n")
    w(" * Both repos compile this identical file. Each side's driver recomputes\n")
    w(" * the digest over these vectors and compares it to a literal pinned in\n")
    w(" * its own committed source, so an edit here fails there. Divergence is\n")
    w(" * change on one side that does not reach the other is visible in review as\n")
    w(" * two different digests.\n */\n\n")
    w("#ifndef EOS_ED25519_CONTRACT_VECTORS_H\n#define EOS_ED25519_CONTRACT_VECTORS_H\n\n")
    w("#include <stddef.h>\n\n")
    w('#define EOS_ED25519_CONTRACT_DIGEST "%s"\n' % digest)
    w("#define EOS_ED25519_CONTRACT_COUNT  %d\n" % len(vs))
    w("#define EOS_ED25519_CONTRACT_MAXMSG %d\n\n" % max_msg)
    w("typedef struct {\n")
    w("    const char    *name;\n")
    w("    unsigned char  public_key[32];\n")
    w("    unsigned char  signature[64];\n")
    w("    unsigned char  message[EOS_ED25519_CONTRACT_MAXMSG];\n")
    w("    size_t         message_len;\n")
    w("    int            expect_accept;   /* 1 = must verify, 0 = must be refused */\n")
    w("    const char    *why;\n")
    w("} eos_ed25519_contract_vector_t;\n\n")
    w("static const eos_ed25519_contract_vector_t\n")
    w("eos_ed25519_contract_vectors[EOS_ED25519_CONTRACT_COUNT] = {\n")
    for name, pk, sig, msg, exp, why in vs:
        padded = msg + bytes(max_msg - len(msg))
        w('    { "%s",\n' % name)
        w("      {%s},\n" % carr(pk))
        w("      {%s},\n" % carr(sig))
        w("      {%s},\n" % carr(padded))
        w('      %d, %d, "%s" },\n' % (len(msg), exp, why))
    w("};\n\n")
    w("#endif /* EOS_ED25519_CONTRACT_VECTORS_H */\n")
    sys.stderr.write("vectors: %d  accept: %d  reject: %d\ndigest: %s\n" % (
        len(vs), sum(v[4] for v in vs), sum(1 for v in vs if not v[4]), digest))
    if out is not sys.stdout:
        out.close()
    return 0

if __name__ == "__main__":
    sys.exit(main() or 0)
