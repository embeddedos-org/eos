# Quality Management System (QMS) — eos
## Aligned with ISO 9001:2015

### Document Control
- All documents version-controlled in Git
- Semantic versioning (v0.1.0)
- Tagged releases with changelog

### Change Management
- All changes via Git commits with DCO sign-off
- PR review required (GitHub branch protection)
- CI must pass before merge

### Configuration Management
- Single source of truth: GitHub repository
- Build configuration: CMakeLists.txt + build.yaml
- Target configuration: ebuild sdk --target <hw>

### Quality Planning
- ISO/IEC 25000 quality model applied
- Quality metrics tracked in CI
- Test coverage goals: >70% per module

### Verification & Validation
- Unit tests: 73+ across 14 suites
- Integration: QEMU boot on 11 board types
- Cross-compilation: 3 architectures (ARM, ARM64, RISC-V)
- Platform: Linux, macOS, Windows

### Corrective & Preventive Action
- Bug tracking via GitHub Issues
- Root cause analysis for failures
- Regression tests added for each fix

### Management Review
- Release review before each version tag
- Gated release: all repos must pass CI before release
- Compliance status reviewed quarterly