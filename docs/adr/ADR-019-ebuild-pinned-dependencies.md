---
adr: 19
title: ebuild consumes eos and eBoot as pinned dependencies
status: Proposed
date: 2026-08-28
deciders: Architecture Council (§33), Build & Tooling maintainers, EoS maintainers
source: EmbeddedOS Platform Architecture & Ecosystem Design Document v0.9, §6, §9, §22
---

# ADR-019 — ebuild consumes eos and eBoot as pinned dependencies

## Context

§9 makes `ebuild` the developer control plane, and §6 makes it a developer tool rather
than a runtime dependency. Both are the right calls. Neither describes what the repository
currently contains.

**Verified — measured on `origin/master` of both repositories, 28 Aug 2026, by comparing
git blob hashes:**

| Copy | Files | Still identical | **Already drifted** |
|---|---|---|---|
| `ebuild/core/eos/` ← `eos` | 350 | 306 | **44** |
| `ebuild/core/eboot/` ← `eBoot` | 167 | 120 | **46** |

Across the wider organisation, **322 C/H files are byte-identical between two or more
repositories** (**Verified**, same method).

`ebuild` carries a full snapshot of two other repositories, and ninety of those files have
already diverged from their sources. Every fix merged into `eos` — including the 21 open
pull requests — silently misses the copy. A security fix landed in one is absent from the
other, and nothing anywhere reports that.

## Decision

1. **`ebuild/core/eos/` and `ebuild/core/eboot/` are replaced by pinned dependencies** on
   the `eos` and `eBoot` repositories, using the mechanism ADR-017 settles.
2. **Until that lands, a drift guard is mandatory.** `ebuild` CI fails when
   `core/eos/**` or `core/eboot/**` differs from the pinned upstream revision. Visible
   drift is not a fix, but invisible drift is the actual danger.
3. **The 90 drifted files are reconciled explicitly**, file by file. Each divergence is
   either a fix that belongs upstream — sent there as a PR — or an unintended edit, which
   is reverted. Neither outcome is "leave it".
4. **The compatibility matrix §26 requires is the pin.** `ebuild` declares which `eos` and
   `eBoot` revisions it is tested against, and that declaration is the matrix rather than a
   separate document that drifts on its own.

## Consequences

- `ebuild` stops being able to silently patch the OS underneath a user. That is the
  intended loss.
- A change spanning tool and OS becomes two PRs and a pin bump. Slower per change, and the
  only way the dependency direction in §6 is actually enforced rather than described.
- The other 322-file duplication cluster across the organisation needs the same treatment.
  It is out of scope here and needs its own record.

## Open

- Submodule, `FetchContent`, or a west-style manifest. A manifest scales better to the
  ~19-repository organisation and is the likely answer, but it is a larger change.
- Whether the drift guard blocks merges or only warns, during the reconciliation window.
