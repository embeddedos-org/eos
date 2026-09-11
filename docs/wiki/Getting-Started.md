# Getting Started

## Repository purpose

[](https://github.com/embeddedos-org/eos/actions/workflows/ci.yml) [](https://github.com/embeddedos-org/eos/actions/workflows/codeql.yml) [](https://github.com/embeddedos-org/eos/actions/workflows/scorecard.yml) [](https://github.com/embeddedos-org/eos/actions/workflows/release.yml) [](LICENSE)

## First steps

1. Read the [README](https://github.com/embeddedos-org/eos/blob/master/README.md) for the project's supported setup and usage path.
2. Clone the repository and enter its directory:

```bash
git clone https://github.com/embeddedos-org/eos.git
cd eos
```

3. Check the root project inputs below before installing dependencies or selecting a build tool.
4. Review [Development](Development) before changing code, and [Security](Security) before reporting a vulnerability.

## Root project inputs

- `CMakeLists.txt`: CMake build definition.
- `Dockerfile`: Container build definition.
- `requirements-dev.txt`: Python development requirements.

## Scope note

The default branch inspected for this page was `master` at [`d14b62f62d2e`](https://github.com/embeddedos-org/eos/commit/d14b62f62d2ed7a0393d1d99e433949cdca76973). This page intentionally does not invent a universal build command when the repository's own documentation does not provide one.
