<!--
SPDX-License-Identifier: MIT
SPDX-FileCopyrightText: 2026 EoS Contributors
-->

# third_party/

External source lives here and nowhere else.

This is the operational form of **ADR-017**. That record says *why*; this file says
*how*. If the two disagree, the ADR wins and this file is the defect.

## Why the rule exists

§4 of the architecture document says: *"Do not invent new cryptographic primitives;
integrate reviewed cryptographic implementations."* §26 requires *"SBOM generation and
dependency/license inventory."*

Neither was achievable, because there was no way to bring code in. In the absence of a
route, a contributor took the only one available and pasted 4,815 lines of third-party
cryptography into a pull request with no licence header, no attribution and no upstream
reference. That is not a contributor failure. It is what a project with a `NOTICE` file
and no import path produces.

## Layout

One directory per dependency:

```
third_party/
  mbedtls/
    UPSTREAM          # required — where it came from and exactly which revision
    LICENSE           # required — copied verbatim from upstream
    PATCHES/          # optional — every local change, as numbered patch files
      0001-....patch
    <upstream source, unmodified>
```

### `UPSTREAM`

Plain `key: value`, one per line. Every field is required.

```
name: mbedtls
version: 3.6.2
repository: https://github.com/Mbed-TLS/mbedtls.git
revision: 8c89224991adff88d53cd380f42a2baa36f91454
license: Apache-2.0
linked: yes
imported: 2026-08-28
importer: srikanth@embeddedos.org
purpose: PSA Crypto provider (ADR-012)
```

| Field | Meaning |
|---|---|
| `name` | Directory name. Must match. |
| `kind` | `source`, `weights`, or `tool`. Model weights follow different rules — see below. |
| `version` | Upstream release, or `none` for an untagged revision. |
| `repository` | Clone URL. |
| `revision` | **Full 40-character commit SHA.** A tag or branch name is not a pin. |
| `license` | SPDX identifier. Must be on the allow-list below. |
| `linked` | `yes` if it is compiled into a shipped artifact, `no` if it is a build-time or test-only tool. The licence allow-list is stricter for `yes`. |
| `imported` | ISO date. |
| `importer` | Who to ask. |
| `purpose` | One line, naming the ADR that authorised it. |
| `redistribution` | **Weights only.** `yes` if this project may ship the file; `no` if users must fetch it themselves. |
| `acknowledged_by` | **Weights only,** and only for a restricted licence with `redistribution: yes`. A named human who has read the terms. |

## The rules

1. **Pinned revisions only.** A branch name or floating tag is not a pin. If the fetch is
   not reproducible, the release build is not reproducible, and §26 requires that it is.

2. **Never modify vendored source in place.** A local change goes in `PATCHES/` as a
   numbered patch, applied at build time. A change that cannot be expressed as a patch
   belongs upstream, not here. Patches are a debt: each one is re-applied on every version
   bump, and each one is a divergence nobody upstream will maintain for us.

3. **`NOTICE` lists every import**, updated in the same commit. `scripts/check_third_party.py`
   fails the build otherwise.

4. **Licence allow-list.** For anything with `linked: yes` — code compiled into a shipped
   artifact:

   | Allowed | |
   |---|---|
   | MIT, BSD-2-Clause, BSD-3-Clause, Apache-2.0, ISC, Zlib | Permissive, attribution only |
   | Unlicense, CC0-1.0, 0BSD | Public-domain dedications |

   **GPL, LGPL, AGPL and MPL are prohibited in linked code.** This project ships MIT; a
   copyleft dependency changes what downstream users may do with it, which is not a
   decision a dependency gets to make on their behalf.

   Tools invoked as separate processes — OpenOCD, `dtc`, QEMU — are unaffected. Record
   those with `linked: no`, where the allow-list does not apply.

5. **Model weights are not source, and the source rules do not cover them.**

   A weight file arrives under a bespoke agreement, not an SPDX identifier. Several of
   the common ones — Llama Community, Gemma Terms, Mistral Research, anything `-NC-` —
   restrict redistribution, commercial use, or use as training data for another model.
   None of that is expressible as a licence id, and none of it can be checked
   mechanically.

   So weights carry two extra fields. `redistribution` says whether this project may
   ship the file at all, as opposed to telling users to fetch it themselves — getting
   that wrong is a licence breach, not a build failure. And a restricted licence
   combined with `redistribution: yes` needs `acknowledged_by`: a named person who has
   actually read the terms. The script cannot read them for you; it can only refuse to
   let the claim be anonymous.

   Weights appear in the SBOM as CycloneDX `machine-learning-model` components, so an
   ML-BOM consumer can tell a shipped model from a linked library.

6. **A new dependency needs an ADR**, not a pull request description. The record names
   what the dependency replaces and what it costs in flash and RAM. See ADR-012 for the
   shape.

7. **Every import carries an upstream watch.** A CVE must reach this project as a task,
   not as a customer report. `scripts/cve_check.sh` consumes the generated `sbom.json`.

## Adding a dependency

```bash
# 1. The ADR is merged and Accepted first. No exceptions.

# 2. Clone at an exact revision.
git clone https://github.com/Mbed-TLS/mbedtls.git third_party/mbedtls
git -C third_party/mbedtls checkout <full-sha>
rm -rf third_party/mbedtls/.git

# 3. Record the pin and copy the licence.
$EDITOR third_party/mbedtls/UPSTREAM
cp third_party/mbedtls/LICENSE third_party/mbedtls/LICENSE   # already there for most

# 4. Add it to NOTICE, then verify.
python3 scripts/check_third_party.py
python3 scripts/generate_sbom.py --write

# 5. Commit the source, the pin, the NOTICE entry and sbom.json together.
```

## Updating a dependency

Bump `revision` and `version`, re-apply the patches, re-run both scripts, and say in the
commit message what changed upstream and why the bump is wanted. A version bump that
exists only to be current is churn; a version bump that fixes a CVE names the CVE.

## Removing a dependency

Delete the directory, the `NOTICE` entry, and any `PATCHES/`, and regenerate the SBOM. If
an ADR authorised it, supersede that ADR — do not silently leave it saying the dependency
is in use.
