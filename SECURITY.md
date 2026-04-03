# Security Policy

## Scope

This security policy covers all components of the EoS embedded operating system:

- **EoS source code** — kernel, services, drivers, debug infrastructure
- **HAL backends** — all hardware abstraction layer implementations (`hal/src/hal_*/`)
- **Crypto services** — AES, SHA, RSA, ECC, CRC implementations (`services/crypto/`)
- **Kernel** — task scheduler, IPC, synchronization primitives (`kernel/`)
- **Boot chain** — eboot stage0/stage1, secure boot verification
- **Build system** — CMake configs, CI/CD workflows, toolchain files
- **Documentation** — if a doc error could lead to an insecure deployment

## Supported Versions

| Version | Supported | EOL Date |
|---------|-----------|----------|
| 0.2.x   | Yes       | TBD      |
| 0.1.x   | Yes       | TBD      |

## Reporting a Vulnerability

**Email:** security@embeddedos.org

> ⚠️ Do **NOT** open public GitHub issues for security vulnerabilities.

### PGP Encrypted Reports

For sensitive disclosures, encrypt your report using our PGP key:

```
Fingerprint: XXXX XXXX XXXX XXXX XXXX  XXXX XXXX XXXX XXXX XXXX
Key ID: 0xXXXXXXXX
```

The public key is available at `https://embeddedos.org/.well-known/pgp-key.asc` and on major keyservers.

### What to Include

- Affected component(s) and version(s)
- Step-by-step reproduction instructions
- Proof-of-concept code or crash dump (if available)
- Impact assessment (what an attacker can achieve)
- Suggested fix (optional but appreciated)

## Severity Classification

We use the Common Vulnerability Scoring System (CVSS v3.1):

| Severity | CVSS Score | Examples |
|----------|-----------|---------|
| **Critical** | 9.0 – 10.0 | Remote code execution, secure boot bypass, key extraction |
| **High** | 7.0 – 8.9 | Privilege escalation, memory corruption with exploit path |
| **Medium** | 4.0 – 6.9 | Information disclosure, denial of service |
| **Low** | 0.1 – 3.9 | Minor info leak requiring local access, debug-only issues |

## Response SLA

| Phase | Critical | High | Medium | Low |
|-------|----------|------|--------|-----|
| **Acknowledge** | 24 hours | 24 hours | 24 hours | 24 hours |
| **Triage & confirm** | 72 hours | 72 hours | 7 days | 14 days |
| **Fix released** | 30 days | 60 days | 90 days | Next scheduled release |
| **Public disclosure** | After fix + 14 days | After fix + 14 days | After fix | After fix |

## Embargo & Coordinated Disclosure

- We follow a **90-day coordinated disclosure** policy.
- Once a vulnerability is reported, the reporter and the EoS security team agree on a disclosure date no later than 90 days from the initial report.
- If a fix is released before the 90-day window, disclosure may happen sooner (minimum 14 days after the fix for Critical/High).
- We will request a CVE ID from MITRE for all confirmed vulnerabilities of Medium severity or above.
- Reporters will be credited in the advisory unless they request anonymity.

## Safe Harbor

We consider security research conducted in good faith to be authorized and will not pursue legal action against researchers who:

- Make a good-faith effort to avoid privacy violations, data destruction, and service disruption
- Only interact with accounts they own or have explicit permission to test
- Do not exploit a vulnerability beyond what is necessary to demonstrate it
- Report the vulnerability promptly and do not disclose it publicly before the agreed timeline
- Do not use the vulnerability for financial gain beyond any offered bug bounty

We will not file complaints or pursue legal action against researchers who follow this policy.

## Recognition — Security Hall of Fame

We maintain a public Security Hall of Fame to recognize researchers who responsibly disclose vulnerabilities:

👉 See [`docs/SECURITY_HALL_OF_FAME.md`](docs/SECURITY_HALL_OF_FAME.md)

To be listed, reporters must:
1. Follow this responsible disclosure policy
2. Report a confirmed vulnerability of Low severity or above
3. Consent to public recognition (name/handle and affiliation)

## Standards

- ISO/IEC 20243 — supply chain security
- ISO/IEC/IEEE 15288:2023 — secure development lifecycle
- SPDX license headers on all source files
- CycloneDX SBOM available in ebuild monorepo
