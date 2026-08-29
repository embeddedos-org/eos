---
adr: 12
title: Cryptographic provider selection
status: Proposed
date: 2026-08-28
deciders: Architecture Council (§33), Boot & Security maintainers
source: EmbeddedOS Platform Architecture & Ecosystem Design Document v0.9, §4, §8, §11, §21, §25
supersedes: none
closes: ADR-011 "Open" item 1 — which provider, and what it costs in flash
---

# ADR-012 — Cryptographic provider selection

**Status: Proposed.** ADR-011 decided that no cryptographic primitive is implemented in
this organisation. It left the provider unchosen. This record chooses one.

## Context

The gap ADR-011 documented is still open, and a contributor has now tried to close it by
hand.

**Observed — the tree as it stands on `master`:**

| Location | State |
|---|---|
| `eos/services/crypto/` | Four hand-written implementations: `aes.c`, `sha256.c`, `rsa_ecc_sha512.c`, `crc.c` |
| `eBoot/core/` | A second, independent SHA-512 and a hand-rolled Ed25519 verify |
| Third-party crypto anywhere in `eos` | **None.** A tree scan for vendored dependencies returns nothing |

**Observed — `eos` PR #57** proposes a third set: 4,815 added lines of vendored Curve25519
field arithmetic, precomputed tables and a third SHA-512. A search of that diff for
`copyright`, `license`, or `public domain` returns **zero matches**, and it adds no test
vectors for the verification path it introduces.

PR #57 has the right instinct and the wrong execution. ADR-011 item 2 says to delete the
hand-rolled Ed25519 and link a reviewed implementation — not to paste an unattributed one
into the tree. The distinction that matters is *provenance*: a reviewed provider is
reviewed because it is identifiable, versioned, and tracked for advisories. A copy with no
upstream reference has none of those properties, whatever its origin.

## Decision

1. **Mbed TLS is the provider for `eos`**, consumed through the **PSA Crypto API** rather
   than the legacy `mbedtls_*` interface. PSA is the interface silicon vendors map their
   hardware accelerators to, so the same call reaches a software implementation on one part
   and a crypto co-processor on another. `eos/services/crypto/include/eos/crypto.h` stays
   as the EoS-facing API and becomes a thin shim over PSA.

2. **eBoot links a verify-only subset.** The bootloader takes the smallest reviewed
   configuration that provides SHA-512 and Ed25519 verification — Mbed TLS with everything
   else compiled out. No signing, no TLS, no AEAD in the boot chain.

3. **Deletion, not coexistence.** When PSA covers a primitive, the hand-written file is
   deleted in the same commit. A hand-rolled implementation that stays in the tree "for
   reference" is one `#include` away from being linked again.

4. **CRC-32 is out of scope.** It is not a cryptographic primitive and stays as it is.

5. **No import without conformance vectors in CI.** Per ADR-011 item 4, each adopted
   primitive lands with published vectors (FIPS 180-4, FIPS-197, RFC 8032 §7.1) executing
   in the test suite. The RFC 8032 vectors that currently fail become the acceptance test
   for this work.

6. **PR #57 is closed in favour of this record**, with thanks — it identified a real defect
   that ADR-011 had documented and nothing had acted on.

## Consequences

- **Footprint is the real cost and it is not yet measured.** ADR-011 named this and it
  remains unquantified. Before this ADR moves to Accepted, flash and RAM deltas must be
  measured on the smallest §21 Nano target, in both the eBoot verify-only and the full
  `eos` configuration. *This is a blocking prerequisite, not a follow-up.*
- **License obligation.** Mbed TLS is Apache-2.0 (**Inferred**, re-check at import).
  Apache-2.0 combines with this MIT project but carries attribution requirements, so
  ADR-017's `NOTICE` and SBOM work must land first.
- Ed25519 secure boot becomes functional for the first time. Until it does, §25 status for
  `EOS_SIG_ED25519` stays **non-functional** exactly as ADR-011 requires.
- Anything ever signed with a key whose signatures nothing could verify remains an open
  question inherited from ADR-011 and is not resolved here.

## Alternatives considered

- **BearSSL** — smaller and constant-time by construction, but a narrower ecosystem and no
  PSA mapping, which forfeits the hardware-accelerator argument that motivates this choice.
- **libsodium verify-only** — excellent for the eBoot half, but does not cover the AES and
  RSA surface `eos` exposes, so it would mean two providers.
- **wolfSSL** — GPLv2-or-commercial. **Rejected**: incompatible with shipping an MIT
  runtime without a commercial licence.
- **Repair the existing code** — explicitly rejected by ADR-011 item 2, and that reasoning
  is adopted unchanged here.

## Open

- Measured flash/RAM cost on the smallest supported part (blocking, see above).
- Whether `eos`'s RSA/ECDSA stubs are deleted outright or migrated, inherited from ADR-011.
- Whether the PSA shim can be thin enough that `eos/crypto.h` needs no source changes in
  callers. **Unknown** until attempted.
