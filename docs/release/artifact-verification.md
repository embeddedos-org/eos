# Artifact Verification Guide

This document describes how to verify the integrity and authenticity of EoS release artifacts.

---

## Overview

Every official EoS release includes:

| Artifact | Purpose |
|----------|---------|
| `eos-vX.Y.Z.tar.gz` | Source archive |
| `eos-vX.Y.Z.tar.gz.sig` | GPG detached signature |
| `eos-vX.Y.Z.tar.gz.sha256` | SHA-256 checksum |
| `eos-vX.Y.Z.tar.gz.cosign.sig` | Cosign (Sigstore) signature |
| `eos-vX.Y.Z.tar.gz.cosign.bundle` | Cosign verification bundle |
| `eos-vX.Y.Z.sbom.json` | CycloneDX SBOM |

---

## 1. Cosign / Sigstore Verification

[Cosign](https://github.com/sigstore/cosign) provides keyless signing via Sigstore's transparency log. This is the **recommended** verification method.

### Install Cosign

```bash
# macOS
brew install cosign

# Linux (download binary)
curl -LO https://github.com/sigstore/cosign/releases/latest/download/cosign-linux-amd64
chmod +x cosign-linux-amd64
sudo mv cosign-linux-amd64 /usr/local/bin/cosign

# Verify cosign installation
cosign version
```

### Verify a Release Artifact

```bash
# Download the release artifact and its cosign signature
curl -LO https://github.com/embedded-os/eos/releases/download/vX.Y.Z/eos-vX.Y.Z.tar.gz
curl -LO https://github.com/embedded-os/eos/releases/download/vX.Y.Z/eos-vX.Y.Z.tar.gz.cosign.sig
curl -LO https://github.com/embedded-os/eos/releases/download/vX.Y.Z/eos-vX.Y.Z.tar.gz.cosign.bundle

# Verify using Sigstore's keyless signing (GitHub Actions OIDC identity)
cosign verify-blob \
  --signature eos-vX.Y.Z.tar.gz.cosign.sig \
  --bundle eos-vX.Y.Z.tar.gz.cosign.bundle \
  --certificate-oidc-issuer https://token.actions.githubusercontent.com \
  --certificate-identity-regexp "github.com/embedded-os/eos" \
  eos-vX.Y.Z.tar.gz
```

**Expected output:**
```
Verified OK
```

If verification fails, **do not use the artifact**. Report the issue to security@embeddedos.org.

---

## 2. GPG Signature Verification

### Import the EoS Release Signing Key

```bash
# Download the public key
curl -LO https://embeddedos.org/.well-known/pgp-key.asc

# Import into your GPG keyring
gpg --import pgp-key.asc

# Verify the key fingerprint matches:
# XXXX XXXX XXXX XXXX XXXX  XXXX XXXX XXXX XXXX XXXX
gpg --fingerprint security@embeddedos.org
```

### Verify the Signature

```bash
# Download the artifact and its detached signature
curl -LO https://github.com/embedded-os/eos/releases/download/vX.Y.Z/eos-vX.Y.Z.tar.gz
curl -LO https://github.com/embedded-os/eos/releases/download/vX.Y.Z/eos-vX.Y.Z.tar.gz.sig

# Verify
gpg --verify eos-vX.Y.Z.tar.gz.sig eos-vX.Y.Z.tar.gz
```

**Expected output:**
```
gpg: Signature made <date> using RSA key ID XXXXXXXX
gpg: Good signature from "EoS Release Signing Key <security@embeddedos.org>"
```

⚠️ If you see `BAD signature`, the artifact has been tampered with. Do not use it.

---

## 3. SHA-256 Checksum Verification

### Verify the Checksum

```bash
# Download the artifact and checksum file
curl -LO https://github.com/embedded-os/eos/releases/download/vX.Y.Z/eos-vX.Y.Z.tar.gz
curl -LO https://github.com/embedded-os/eos/releases/download/vX.Y.Z/eos-vX.Y.Z.tar.gz.sha256

# Verify on Linux
sha256sum -c eos-vX.Y.Z.tar.gz.sha256

# Verify on macOS
shasum -a 256 -c eos-vX.Y.Z.tar.gz.sha256

# Verify on Windows (PowerShell)
(Get-FileHash eos-vX.Y.Z.tar.gz -Algorithm SHA256).Hash -eq (Get-Content eos-vX.Y.Z.tar.gz.sha256).Split(' ')[0]
```

**Expected output:**
```
eos-vX.Y.Z.tar.gz: OK
```

### Manual Verification

```bash
# Compute the hash yourself and compare
sha256sum eos-vX.Y.Z.tar.gz
# Compare the output hash with the contents of eos-vX.Y.Z.tar.gz.sha256
```

> **Note:** SHA-256 verification alone only confirms integrity (no corruption), not authenticity (who built it). Always combine with GPG or Cosign verification for full assurance.

---

## 4. SBOM Verification

The Software Bill of Materials (SBOM) lists all components included in the release.

### View the SBOM

```bash
# Download the SBOM
curl -LO https://github.com/embedded-os/eos/releases/download/vX.Y.Z/eos-vX.Y.Z.sbom.json

# Pretty-print
python3 -m json.tool eos-vX.Y.Z.sbom.json

# Or use a CycloneDX tool
pip install cyclonedx-bom
cyclonedx-py validate --input-file eos-vX.Y.Z.sbom.json
```

### Verify SBOM Contents

```bash
# List all components in the SBOM
jq '.components[] | {name: .name, version: .version, type: .type}' eos-vX.Y.Z.sbom.json

# Check for known vulnerabilities using grype
grype sbom:eos-vX.Y.Z.sbom.json

# Check using trivy
trivy sbom eos-vX.Y.Z.sbom.json
```

### Verify SBOM Signature

The SBOM itself is also signed:

```bash
# Verify SBOM signature with cosign
cosign verify-blob \
  --signature eos-vX.Y.Z.sbom.json.cosign.sig \
  --bundle eos-vX.Y.Z.sbom.json.cosign.bundle \
  --certificate-oidc-issuer https://token.actions.githubusercontent.com \
  --certificate-identity-regexp "github.com/embedded-os/eos" \
  eos-vX.Y.Z.sbom.json
```

---

## Verification Checklist

For maximum assurance, perform **all** of the following:

- [ ] **Cosign** — verify the artifact was built by the official CI pipeline
- [ ] **GPG** — verify the artifact was signed by the release maintainer
- [ ] **SHA-256** — verify the artifact was not corrupted in transit
- [ ] **SBOM** — review the component list and scan for known vulnerabilities

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `gpg: No public key` | Import the signing key: `gpg --import pgp-key.asc` |
| `cosign: no matching signatures` | Ensure you're using the correct `--certificate-identity-regexp` |
| `sha256sum: WARNING: 1 computed checksum did NOT match` | Re-download the artifact; if persistent, report to security@embeddedos.org |
| `grype` reports CVEs | Check if the CVE applies to EoS's usage of the component; consult the security advisory |
