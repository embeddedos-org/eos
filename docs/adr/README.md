# Architecture Decision Records

One file per decision. A record is never edited after it reaches **Accepted** — it is
superseded by a later record that names it.

| Status | Meaning |
|---|---|
| Proposed | Written, not yet ratified by the maintainers named in `deciders`. |
| Accepted | Ratified. Binding on new code. |
| Superseded | Replaced; the replacing ADR is named in the header. |

## Index

| ADR | Title | Status |
|---|---|---|
| 012 | [Cryptographic provider selection](ADR-012-cryptographic-provider-selection.md) | Proposed |
| 013 | [Flattened device tree parsing via libfdt](ADR-013-devicetree-via-libfdt.md) | Proposed |
| 014 | [TCP/IP via lwIP](ADR-014-tcpip-via-lwip.md) | Proposed |
| 015 | [Vendor HAL adapter policy](ADR-015-vendor-hal-adapter-policy.md) | Proposed |
| 016 | [Board and feature description via devicetree and Kconfig](ADR-016-devicetree-and-kconfig.md) | Proposed |
| 017 | [Third-party dependency, vendoring and SBOM policy](ADR-017-third-party-and-sbom-policy.md) | Proposed |
| 018 | [Test framework and machine-readable CI results](ADR-018-test-framework.md) | Proposed |
| 019 | [ebuild consumes eos and eBoot as pinned dependencies](ADR-019-ebuild-pinned-dependencies.md) | Proposed |

## Note on numbering

ADR-001 through ADR-011 exist on the `reconcile/tier-1` branch and have not yet been
pushed to `master`. This set starts at 012 so that those numbers are not reused. ADR-012
closes a question ADR-011 left explicitly Open; merging this set without ADR-001–011 will
leave those cross-references dangling until that branch lands.
