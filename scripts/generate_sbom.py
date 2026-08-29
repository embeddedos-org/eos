#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 EoS Contributors
"""Generate a CycloneDX 1.5 SBOM from third_party/ and the project metadata.

§26 requires "SBOM generation and dependency/license inventory". scripts/cve_check.sh
already looks for sbom.json at the repository root and feeds it to grype; this produces
that file, so the CVE path works the moment a dependency exists.

Usage:
    scripts/generate_sbom.py               # print to stdout
    scripts/generate_sbom.py --write       # write sbom.json
    scripts/generate_sbom.py --check       # fail if sbom.json is stale

Exit codes:
  0 — success, or --check found the file current
  1 — --check found sbom.json missing or stale
  2 — script error
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_third_party import parse_upstream  # noqa: E402

PROJECT_ROOT = Path(__file__).resolve().parent.parent
THIRD_PARTY = PROJECT_ROOT / "third_party"
SBOM_PATH = PROJECT_ROOT / "sbom.json"
CMAKELISTS = PROJECT_ROOT / "CMakeLists.txt"


def project_version() -> str:
    """Read VERSION from the top-level project() call."""
    if not CMAKELISTS.is_file():
        return "0.0.0"
    match = re.search(r"^\s*VERSION\s+([0-9]+(?:\.[0-9]+)*)", CMAKELISTS.read_text(
        encoding="utf-8"), re.M)
    return match.group(1) if match else "0.0.0"


def purl(fields: dict[str, str]) -> str:
    """Best-effort package URL. Generic type keeps it honest for source imports."""
    name = fields.get("name", "unknown")
    version = fields.get("version", "")
    repository = fields.get("repository", "")
    revision = fields.get("revision", "")
    qualifiers = []
    if repository:
        qualifiers.append(f"vcs_url=git+{repository}")
    if revision:
        qualifiers.append(f"checksum=sha1:{revision}")
    tail = "?" + "&".join(qualifiers) if qualifiers else ""
    return f"pkg:generic/{name}@{version or revision[:12] or 'unknown'}{tail}"


# CycloneDX 1.5 carries model weights as their own component type, so an ML-BOM
# consumer can tell a shipped model from a linked library.
_TYPE_BY_KIND = {
    "source": "library",
    "tool": "application",
    "weights": "machine-learning-model",
}


def component(fields: dict[str, str]) -> dict:
    licence = fields.get("license", "")
    kind = fields.get("kind", "source").lower()
    entry = {
        "type": _TYPE_BY_KIND.get(kind, "library"),
        "bom-ref": purl(fields),
        "name": fields.get("name", "unknown"),
        "version": fields.get("version", "") or fields.get("revision", "")[:12],
        "purl": purl(fields),
        "scope": "required" if fields.get("linked", "").lower() == "yes" else "optional",
        "description": fields.get("purpose", ""),
    }
    if licence:
        entry["licenses"] = [{"license": {"id": licence}}]
    externals = []
    if fields.get("repository"):
        externals.append({"type": "vcs", "url": fields["repository"]})
    if externals:
        entry["externalReferences"] = externals
    if fields.get("revision"):
        properties = [
            {"name": "eos:revision", "value": fields["revision"]},
            {"name": "eos:imported", "value": fields.get("imported", "")},
            {"name": "eos:linked", "value": fields.get("linked", "")},
            {"name": "eos:kind", "value": kind},
        ]
        if kind == "weights":
            # Whether we may ship the file matters to anyone consuming this SBOM,
            # and it is not derivable from the licence id alone.
            properties.append(
                {"name": "eos:redistribution", "value": fields.get("redistribution", "")}
            )
        entry["properties"] = properties
    return entry


def build_sbom() -> dict:
    components = []
    if THIRD_PARTY.is_dir():
        for directory in sorted(d for d in THIRD_PARTY.iterdir() if d.is_dir()):
            upstream = directory / "UPSTREAM"
            if not upstream.is_file():
                continue
            components.append(component(parse_upstream(upstream)))

    return {
        "bomFormat": "CycloneDX",
        "specVersion": "1.5",
        "version": 1,
        # No timestamp: a regenerated-but-unchanged SBOM must compare equal, or --check
        # fails on every run and the signal is lost.
        "metadata": {
            "component": {
                "type": "operating-system",
                "bom-ref": "pkg:generic/eos",
                "name": "eos",
                "version": project_version(),
                "description": "EoS — embedded operating system",
                "licenses": [{"license": {"id": "MIT"}}],
            },
            "tools": [{"vendor": "EoS Project", "name": "generate_sbom.py"}],
        },
        "components": components,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--write", action="store_true", help="write sbom.json")
    group.add_argument("--check", action="store_true", help="fail if sbom.json is stale")
    args = parser.parse_args()

    rendered = json.dumps(build_sbom(), indent=2, sort_keys=False) + "\n"

    if args.check:
        if not SBOM_PATH.is_file():
            print("FAIL: sbom.json does not exist. Run scripts/generate_sbom.py --write.")
            return 1
        if SBOM_PATH.read_text(encoding="utf-8") != rendered:
            print("FAIL: sbom.json is stale — third_party/ has changed since it was written.")
            print("Run scripts/generate_sbom.py --write and commit the result.")
            return 1
        count = len(json.loads(rendered)["components"])
        print(f"OK: sbom.json is current ({count} component(s)).")
        return 0

    if args.write:
        SBOM_PATH.write_text(rendered, encoding="utf-8")
        count = len(json.loads(rendered)["components"])
        print(f"Wrote {SBOM_PATH.relative_to(PROJECT_ROOT)} ({count} component(s)).")
        return 0

    sys.stdout.write(rendered)
    return 0


if __name__ == "__main__":
    sys.exit(main())
