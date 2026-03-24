# Risk Register — eos

| Risk ID | Category | Description | Likelihood | Impact | Mitigation | Status |
|---------|----------|-------------|------------|--------|------------|--------|
| RSK-001 | Security | Buffer overflow in HAL | Low | Critical | No malloc in kernel, stack bounds | Mitigated |
| RSK-002 | Security | Firmware tampering | Medium | Critical | SHA-256 verification, secure boot | Mitigated |
| RSK-003 | Security | Supply chain injection | Low | Critical | SPDX headers, SBOM, build reproducibility | Mitigated |
| RSK-004 | Security | Replay attack on IPC | Medium | High | EIPC HMAC-SHA256 + nonce tracking | Mitigated |
| RSK-005 | Reliability | Kernel crash | Medium | Critical | Core dump handler, ring buffer logging | Mitigated |
| RSK-006 | Reliability | OTA brick | Low | Critical | A/B slots, rollback, recovery partition | Mitigated |
| RSK-007 | Reliability | Watchdog timeout | Medium | High | Service manager auto-restart | Mitigated |
| RSK-008 | Quality | Untested code paths | Medium | Medium | 73+ unit tests, CI on 3 platforms | Monitored |
| RSK-009 | Quality | API contract violations | Low | High | Header-first development, NIST test vectors | Mitigated |
| RSK-010 | Compliance | License violation | Low | Medium | SPDX on all files, REUSE dep5 | Mitigated |
| RSK-011 | Portability | Platform-specific failures | Medium | Medium | Cross-compile 3 archs, QEMU 11 boards | Mitigated |
| RSK-012 | Performance | Real-time deadline miss | Medium | High | O(n) scheduler, no malloc, fixed structs | Monitored |

## Risk Assessment Method
- Likelihood: Low / Medium / High
- Impact: Low / Medium / High / Critical
- Status: Open / Monitored / Mitigated / Closed