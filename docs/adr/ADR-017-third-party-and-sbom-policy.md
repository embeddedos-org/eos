---
adr: 17
title: Third-party dependency, vendoring and SBOM policy
status: Proposed
date: 2026-08-28
deciders: Architecture Council (§33), all repository maintainers
source: EmbeddedOS Platform Architecture & Ecosystem Design Document v0.9, §4, §26
---

# ADR-017 — Third-party dependency, vendoring and SBOM policy

**This ADR is a prerequisite for ADR-012 through ADR-016.** None of them may import
anything until this one is Accepted.

## Context

**Observed:** `eos` contains **no third-party libraries at all** — a tree scan for the
usual vendoring conventions and for any of the libraries this ADR set proposes returns
nothing. There is consequently no policy for how one would be added, and no `sbom` in the
repository.

**Observed:** in the absence of a policy, `eos` PR #57 proposed adding 4,815 lines of
third-party cryptographic code with no licence header, no attribution, no upstream
reference and no pinned revision. That is not a contributor failure. It is what happens
when a project with a `NOTICE` file and a `SECURITY.md` has never written down how to
bring code in.

§26 already requires "SBOM generation and dependency/license inventory". Nothing implements
it.

## Decision

1. **`third_party/<name>/` is the only location for external source.** Each directory
   contains the upstream source unmodified, plus:
   - `UPSTREAM` — repository URL, exact tag or commit SHA, import date, importer.
   - `LICENSE` — copied verbatim from upstream.
   - `PATCHES/` — every local modification as a numbered patch file. Modifying vendored
     source in place is prohibited; a change that cannot be expressed as a patch must be
     sent upstream instead.
2. **Pinned revisions only.** A branch name or floating ref is not a pin. Imports use CMake
   `FetchContent` with a SHA, or a submodule, or a checked-in copy with the SHA recorded in
   `UPSTREAM` — never a moving target.
3. **`NOTICE` lists every import** with its licence, updated in the same commit.
4. **Licence allow-list for anything linked into the runtime:** MIT, BSD-2, BSD-3,
   Apache-2.0, ISC, Zlib, and public-domain dedications. **Copyleft licences (GPL, LGPL,
   AGPL) are prohibited in linked runtime code.** Tools invoked as separate processes —
   OpenOCD, dtc, QEMU — are unaffected by this rule, and the distinction must be recorded
   per dependency.
5. **`ebuild sbom` emits CycloneDX** covering every `third_party/` entry and every fetched
   dependency, and release artifacts ship it. This closes the §26 requirement.
6. **A new dependency needs an ADR.** Not a PR description — a record, naming what it
   replaces and what it costs in flash and RAM.
7. **Advisory tracking.** Each import carries an upstream watch so that a CVE reaches this
   project as a task rather than as a customer report.

## Consequences

- PR #57 cannot merge in its present form, and now there is a written reason to point to
  rather than a judgement call.
- Every ADR in this set gains a blocking prerequisite. That is deliberate: importing first
  and inventorying later is how licence obligations get missed.
- Someone must own the advisory watch. An unowned watch is not a control.

## Open

- Submodules vs `FetchContent` vs checked-in copies. Each has a different offline-build
  and reproducibility profile; §26 requires reproducible release builds, which argues for
  checked-in or SHA-pinned fetch with a mirror.
- Whether `third_party/` sits in each repository or in one shared repository consumed by
  all. ADR-019 is the same question for first-party code.
