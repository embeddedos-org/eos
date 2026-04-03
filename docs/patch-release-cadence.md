# EoS Patch & Release Cadence

This document defines the versioning strategy, release schedule, long-term support (LTS) policy, and backport procedures for the EoS embedded operating system.

---

## Versioning: Semantic Versioning (SemVer)

EoS follows [Semantic Versioning 2.0.0](https://semver.org/):

```
MAJOR.MINOR.PATCH

MAJOR — Breaking API changes (removed functions, changed signatures, renamed config flags)
MINOR — New features, new peripherals, new services (backward-compatible)
PATCH — Bug fixes, documentation updates, performance improvements (no API changes)
```

### Pre-release and Build Metadata

```
v1.2.3-rc.1       — Release candidate 1 for v1.2.3
v1.2.3-beta.2     — Beta 2 for v1.2.3
v1.2.3+build.456  — Build metadata (CI build number)
```

---

## Release Schedule

| Release Type | Cadence | Scope | Branch |
|-------------|---------|-------|--------|
| **Patch** (X.Y.Z) | Monthly | Bug fixes, doc updates, minor improvements | `release/vX.Y` |
| **Minor** (X.Y.0) | Quarterly | New features, new peripheral support, new services | `release/vX.Y` |
| **Major** (X.0.0) | Annual | Breaking API changes, architecture updates | `release/vX.0` |

### Typical Annual Timeline

| Month | Release | Notes |
|-------|---------|-------|
| Jan | vX.Y.1 | Patch — post-holiday fixes |
| Feb | vX.Y.2 | Patch |
| Mar | vX.(Y+1).0 | **Minor** — Q1 features |
| Apr | vX.(Y+1).1 | Patch |
| May | vX.(Y+1).2 | Patch |
| Jun | vX.(Y+2).0 | **Minor** — Q2 features |
| Jul | vX.(Y+2).1 | Patch |
| Aug | vX.(Y+2).2 | Patch |
| Sep | vX.(Y+3).0 | **Minor** — Q3 features |
| Oct | vX.(Y+3).1 | Patch |
| Nov | vX.(Y+3).2 | Patch |
| Dec | v(X+1).0.0 | **Major** — annual breaking release |

---

## Security Patches: Out-of-Band Releases

Security vulnerabilities with **Critical** or **High** severity are released out-of-band, independent of the regular schedule:

| Severity | Release Timeline | Process |
|----------|-----------------|---------|
| **Critical** (CVSS 9.0-10.0) | Within 30 days of confirmation | Emergency patch release to all supported branches |
| **High** (CVSS 7.0-8.9) | Within 60 days of confirmation | Expedited patch release to all supported branches |
| **Medium** (CVSS 4.0-6.9) | Within 90 days of confirmation | Included in next scheduled patch release |
| **Low** (CVSS 0.1-3.9) | Next scheduled release | Included in next scheduled patch release |

Security patches:
- Are tagged with a `.Z+1` patch version (e.g., `v1.2.4` if `v1.2.3` was latest).
- Include only the security fix and minimal related changes.
- Are accompanied by a security advisory (GitHub Security Advisory + CVE).
- Are backported to **all** supported branches (see LTS Policy).

---

## Long-Term Support (LTS) Policy

| Branch | LTS Duration | Security Fixes | Bug Fixes | New Features |
|--------|-------------|----------------|-----------|-------------|
| **v1.x** | 2 years from v1.0.0 release | ✅ Full duration | ✅ First 18 months | ❌ |
| **v2.x** | 3 years from v2.0.0 release | ✅ Full duration | ✅ First 24 months | ❌ |
| **v3.x+** | 3 years from vX.0.0 release | ✅ Full duration | ✅ First 24 months | ❌ |

### LTS Branch Lifecycle

```
v1.0.0 ──────────────────────────────────────────── v1.x EOL
  │   Active development   │  Maintenance (security only)  │
  │      18 months          │         6 months              │
  ├────────────────────────┼───────────────────────────────┤
  │  Bug fixes + security  │  Security fixes only          │
```

---

## End-of-Life (EOL) Timeline & Migration

### EOL Process

1. **12 months before EOL**: Announce deprecation in release notes and README.
2. **6 months before EOL**: Stop accepting non-security bug fixes for the branch.
3. **3 months before EOL**: Final patch release with all accumulated security fixes.
4. **EOL date**: Branch marked as unsupported. No further releases.
5. **EOL + 3 months**: Branch archived (read-only).

### Migration Guidance

When a major version reaches EOL, users should migrate to the next supported major version:

1. Review the migration guide (`docs/migration/vX-to-vY.md`).
2. Update `eos_config.h` for renamed/removed configuration flags.
3. Update API calls per the breaking changes list in `CHANGELOG.md`.
4. Run the full test suite against the new version.
5. Test on target hardware before deploying to production.

---

## Backport Policy

Security fixes are backported to **all currently supported branches**:

```
main (development)
  │
  ├── release/v2.3  (current stable)     ← backport
  ├── release/v2.2  (previous stable)    ← backport
  ├── release/v2.1  (LTS maintenance)    ← backport (security only)
  ├── release/v1.9  (v1.x LTS)          ← backport (security only)
  │
  └── release/v0.9  (EOL)               ✗ no backport
```

### Backport Criteria

| Fix Type | Backported To |
|----------|--------------|
| Critical/High security fix | All supported branches |
| Medium security fix | Current stable + LTS branches |
| Low security fix | Current stable only |
| Critical bug fix (data loss, crash) | Current stable + previous stable |
| Regular bug fix | Current stable only |
| New feature | Never backported |

### Backport Process

1. Fix is developed and merged to `main`.
2. Maintainer creates cherry-pick PRs to each supported release branch.
3. Cherry-pick PRs go through the same CI checks as regular PRs.
4. Backported fix is tagged as a new patch release on each branch.
5. Release notes reference the original fix and CVE (if applicable).

```bash
# Example: backport commit abc123 to release/v1.9
git checkout release/v1.9
git cherry-pick abc123
git push origin release/v1.9
# CI runs → tag v1.9.X → release published
```
