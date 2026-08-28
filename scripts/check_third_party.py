#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 EoS Contributors
"""Validate every dependency under third_party/ against the ADR-017 policy.

Checks, per dependency:

  * UPSTREAM exists and carries every required field
  * revision is a full 40-character SHA, not a tag or branch
  * LICENSE exists and is not empty
  * the SPDX identifier is on the allow-list, when the code is linked
  * model weights declare redistribution rights, since their licences are not
    source licences and several forbid redistribution outright
  * the directory name matches the recorded name
  * NOTICE mentions it

Exit codes:
  0 — every dependency conforms (including when there are none)
  1 — at least one violation
  2 — script error
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
THIRD_PARTY = PROJECT_ROOT / "third_party"
NOTICE = PROJECT_ROOT / "NOTICE"

REQUIRED_FIELDS = (
    "name",
    "kind",
    "version",
    "repository",
    "revision",
    "license",
    "linked",
    "imported",
    "importer",
    "purpose",
)

# ADR-017 decision item 4. Applies to anything compiled into a shipped artifact.
PERMISSIVE = {
    "MIT",
    "BSD-2-Clause",
    "BSD-3-Clause",
    "Apache-2.0",
    "ISC",
    "Zlib",
    "Unlicense",
    "CC0-1.0",
    "0BSD",
}

# Named explicitly so the failure message can say why, rather than "not on the list".
COPYLEFT = {
    "GPL-2.0-only", "GPL-2.0-or-later", "GPL-3.0-only", "GPL-3.0-or-later",
    "LGPL-2.1-only", "LGPL-2.1-or-later", "LGPL-3.0-only", "LGPL-3.0-or-later",
    "AGPL-3.0-only", "AGPL-3.0-or-later",
    "MPL-2.0",
}

KINDS = {"source", "weights", "tool"}

# Model weights are not source. Their licences are bespoke agreements, not SPDX
# identifiers, and several restrict redistribution, commercial use, or use as
# training data for other models. A weight file waved through the source
# allow-list is a licence breach the SBOM would not show.
WEIGHTS_OSI = {"Apache-2.0", "MIT", "CC-BY-4.0", "CC-BY-SA-4.0", "OpenRAIL-M"}

# Permitted, but only with redistribution explicitly recorded and acknowledged.
WEIGHTS_RESTRICTED = {
    "Llama-3-Community",
    "Llama-4-Community",
    "Gemma-Terms",
    "Qwen-License",
    "DeepSeek-License",
    "Mistral-Research",
    "CC-BY-NC-4.0",
}

SHA_RE = re.compile(r"^[0-9a-f]{40}$")
DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")


def parse_upstream(path: Path) -> dict[str, str]:
    """Parse the key: value UPSTREAM file. Blank lines and # comments are ignored."""
    fields: dict[str, str] = {}
    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if ":" not in line:
            raise ValueError(f"{path}:{lineno}: expected 'key: value', got {raw!r}")
        key, _, value = line.partition(":")
        fields[key.strip()] = value.strip()
    return fields


def check_weights(name: str, fields: dict[str, str], licence: str) -> list[str]:
    """Extra rules for model weights, which source licence rules do not cover."""
    problems: list[str] = []

    redistribution = fields.get("redistribution", "").lower()
    if redistribution not in {"yes", "no"}:
        problems.append(
            f"{name}: weights must declare 'redistribution: yes|no' — whether this "
            "project may ship the weight file itself, as opposed to telling users to "
            "fetch it. Getting this wrong is a licence breach, not a build failure."
        )

    if not licence:
        return problems

    if licence in WEIGHTS_RESTRICTED:
        if redistribution == "yes" and not fields.get("acknowledged_by"):
            problems.append(
                f"{name}: {licence} is a restricted model licence and this record "
                "claims the right to redistribute. That needs a named human in "
                "'acknowledged_by' who has read the terms — acceptable-use clauses, "
                "naming requirements and downstream restrictions are not SPDX and "
                "cannot be checked mechanically."
            )
    elif licence not in WEIGHTS_OSI:
        problems.append(
            f"{name}: {licence} is not a recognised model-weight licence. Add it to "
            "WEIGHTS_OSI or WEIGHTS_RESTRICTED in this script, and say which in the ADR "
            "that authorised the import."
        )

    return problems


def check_dependency(directory: Path, notice_text: str) -> list[str]:
    """Return a list of problems with one dependency. Empty means it conforms."""
    problems: list[str] = []
    name = directory.name

    upstream = directory / "UPSTREAM"
    if not upstream.is_file():
        return [f"{name}: no UPSTREAM file — see third_party/README.md for the format"]

    try:
        fields = parse_upstream(upstream)
    except ValueError as exc:
        return [f"{name}: {exc}"]

    for field in REQUIRED_FIELDS:
        if not fields.get(field):
            problems.append(f"{name}: UPSTREAM is missing required field '{field}'")

    if fields.get("name") and fields["name"] != name:
        problems.append(
            f"{name}: UPSTREAM says name '{fields['name']}' but the directory is '{name}'"
        )

    revision = fields.get("revision", "")
    if revision and not SHA_RE.match(revision):
        problems.append(
            f"{name}: revision '{revision}' is not a full 40-character SHA. "
            "A tag or branch is not a pin — it moves, and the build stops being reproducible."
        )

    imported = fields.get("imported", "")
    if imported and not DATE_RE.match(imported):
        problems.append(f"{name}: imported '{imported}' is not an ISO date (YYYY-MM-DD)")

    linked = fields.get("linked", "").lower()
    if linked and linked not in {"yes", "no"}:
        problems.append(f"{name}: linked must be 'yes' or 'no', got '{fields['linked']}'")

    kind = fields.get("kind", "").lower()
    if kind and kind not in KINDS:
        problems.append(
            f"{name}: kind must be one of {', '.join(sorted(KINDS))}, got '{fields['kind']}'"
        )

    licence = fields.get("license", "")

    if kind == "weights":
        problems.extend(check_weights(name, fields, licence))
    elif licence and linked == "yes":
        if licence in COPYLEFT:
            problems.append(
                f"{name}: {licence} is copyleft and this code is linked into a shipped "
                "artifact. EoS ships MIT; a copyleft dependency changes what downstream "
                "users may do. Invoke it as a separate process (linked: no), or find an "
                "alternative."
            )
        elif licence not in PERMISSIVE:
            problems.append(
                f"{name}: {licence} is not on the allow-list for linked code "
                f"({', '.join(sorted(PERMISSIVE))}). If it belongs there, amend ADR-017 "
                "and this script together."
            )

    licence_file = directory / "LICENSE"
    if not licence_file.is_file():
        # Upstreams vary; accept the common spellings before complaining.
        alternatives = [
            p for p in directory.iterdir()
            if p.is_file() and p.name.upper().startswith(("LICENSE", "LICENCE", "COPYING"))
        ]
        if alternatives:
            problems.append(
                f"{name}: licence is at '{alternatives[0].name}' — copy it to "
                "'LICENSE' as well, so the location is uniform across dependencies"
            )
        else:
            problems.append(f"{name}: no LICENSE file copied from upstream")
    elif not licence_file.read_text(encoding="utf-8", errors="replace").strip():
        problems.append(f"{name}: LICENSE is empty")

    if name not in notice_text:
        problems.append(
            f"{name}: not mentioned in NOTICE. ADR-017 item 3 — the inventory is updated "
            "in the same commit as the import, not afterwards."
        )

    patches = directory / "PATCHES"
    if patches.is_dir():
        stray = [p.name for p in sorted(patches.iterdir()) if p.suffix != ".patch"]
        if stray:
            problems.append(
                f"{name}: PATCHES/ contains non-patch files: {', '.join(stray)}"
            )

    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root", type=Path, default=THIRD_PARTY,
        help="directory to check (default: third_party/)",
    )
    parser.add_argument(
        "--notice", type=Path, default=NOTICE,
        help="NOTICE file to check against (default: NOTICE)",
    )
    args = parser.parse_args()

    if not args.root.is_dir():
        print(f"{args.root} does not exist — nothing to check.")
        return 0

    notice_text = args.notice.read_text(encoding="utf-8") if args.notice.is_file() else ""
    if not notice_text:
        print(f"warning: {args.notice} is missing or empty", file=sys.stderr)

    dependencies = sorted(
        d for d in args.root.iterdir() if d.is_dir() and not d.name.startswith(".")
    )

    if not dependencies:
        print("third_party/ holds no dependencies yet. Policy is in place; nothing to check.")
        return 0

    all_problems: list[str] = []
    for directory in dependencies:
        problems = check_dependency(directory, notice_text)
        status = "OK" if not problems else f"{len(problems)} problem(s)"
        print(f"── {directory.name}: {status}")
        for problem in problems:
            print(f"   {problem}")
        all_problems.extend(problems)

    print()
    if all_problems:
        print(f"FAIL: {len(all_problems)} problem(s) across {len(dependencies)} dependencies.")
        print("See third_party/README.md and ADR-017.")
        return 1

    print(f"OK: {len(dependencies)} dependencies conform to ADR-017.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
