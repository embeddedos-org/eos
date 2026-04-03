#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 EoS Contributors
#
# check_traceability.py — Verify traceability matrix references
#
# Parses docs/compliance/traceability_matrix.md and verifies that all
# referenced test files and source files exist on disk.
#
# Usage:
#   python3 scripts/check_traceability.py [--matrix <path>] [--root <path>]
#
# Exit codes:
#   0 — All references valid
#   1 — Broken references found

import argparse
import os
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class TraceabilityRow:
    req_id: str
    requirement: str
    design_doc: str
    source_file: str
    test_case: str
    release: str
    line_number: int


@dataclass
class ValidationResult:
    missing_source_files: list = field(default_factory=list)
    missing_test_files: list = field(default_factory=list)
    valid_source_files: list = field(default_factory=list)
    valid_test_files: list = field(default_factory=list)
    skipped_entries: list = field(default_factory=list)


def parse_matrix(matrix_path: str) -> list[TraceabilityRow]:
    """Parse the traceability matrix markdown table."""
    rows = []

    with open(matrix_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    in_table = False
    header_seen = False

    for line_num, line in enumerate(lines, start=1):
        line = line.strip()

        # Detect table start (header row)
        if re.match(r"^\|.*Req ID.*\|", line, re.IGNORECASE):
            in_table = True
            header_seen = True
            continue

        # Skip separator row
        if in_table and re.match(r"^\|[-\s|]+\|$", line):
            continue

        # Parse data rows
        if in_table and line.startswith("|") and header_seen:
            cells = [c.strip() for c in line.split("|")]
            # Remove empty first/last elements from split
            cells = [c for c in cells if c != ""]

            if len(cells) >= 6 and cells[0].startswith("REQ-"):
                rows.append(
                    TraceabilityRow(
                        req_id=cells[0],
                        requirement=cells[1],
                        design_doc=cells[2],
                        source_file=cells[3],
                        test_case=cells[4],
                        release=cells[5],
                        line_number=line_num,
                    )
                )
            elif len(cells) >= 1 and not cells[0].startswith("#"):
                # Non-empty row that doesn't match expected format
                pass

        # End of table
        if in_table and not line.startswith("|") and line != "":
            in_table = False

    return rows


def find_file(root: str, filename: str) -> bool:
    """Search for a file anywhere under the project root."""
    for dirpath, _, filenames in os.walk(root):
        if filename in filenames:
            return True
    return False


def extract_filenames(cell_value: str) -> list[str]:
    """Extract file names from a table cell (handles 'file1, file2' and 'file:function')."""
    if cell_value in ("—", "-", "N/A", ""):
        return []

    filenames = []
    # Split by comma for multiple files
    parts = [p.strip() for p in cell_value.split(",")]

    for part in parts:
        # Handle 'filename:function_name' format
        if ":" in part:
            filename = part.split(":")[0].strip()
        else:
            filename = part.strip()

        # Handle path references like 'services/crypto/src/aes.c'
        if "/" in filename:
            filename = os.path.basename(filename)

        # Only include actual filenames (with extensions)
        if "." in filename and not filename.startswith("#"):
            filenames.append(filename)

    return filenames


def validate_references(
    rows: list[TraceabilityRow], project_root: str
) -> ValidationResult:
    """Validate that all referenced files exist on disk."""
    result = ValidationResult()

    for row in rows:
        # Check source files
        source_files = extract_filenames(row.source_file)
        for sf in source_files:
            if find_file(project_root, sf):
                result.valid_source_files.append((row.req_id, sf, row.line_number))
            else:
                result.missing_source_files.append((row.req_id, sf, row.line_number))

        if not source_files and row.source_file not in ("—", "-", "N/A", ""):
            # Source references a directory or non-file pattern
            result.skipped_entries.append(
                (row.req_id, f"source: {row.source_file}", row.line_number)
            )

        # Check test files
        test_files = extract_filenames(row.test_case)
        for tf in test_files:
            if find_file(project_root, tf):
                result.valid_test_files.append((row.req_id, tf, row.line_number))
            else:
                result.missing_test_files.append((row.req_id, tf, row.line_number))

        if not test_files and row.test_case in ("—", "-"):
            result.skipped_entries.append(
                (row.req_id, "test: (none specified)", row.line_number)
            )

    return result


def print_report(result: ValidationResult) -> None:
    """Print the validation report."""
    print("=" * 70)
    print("EoS Traceability Matrix Validation Report")
    print("=" * 70)
    print()

    # Valid references
    total_valid = len(result.valid_source_files) + len(result.valid_test_files)
    print(f"✅ Valid references: {total_valid}")
    print(f"   Source files: {len(result.valid_source_files)}")
    print(f"   Test files:   {len(result.valid_test_files)}")
    print()

    # Missing source files
    if result.missing_source_files:
        print(f"❌ Missing source files: {len(result.missing_source_files)}")
        for req_id, filename, line_num in result.missing_source_files:
            print(f"   Line {line_num}: {req_id} → {filename}")
        print()

    # Missing test files
    if result.missing_test_files:
        print(f"❌ Missing test files: {len(result.missing_test_files)}")
        for req_id, filename, line_num in result.missing_test_files:
            print(f"   Line {line_num}: {req_id} → {filename}")
        print()

    # Skipped entries
    if result.skipped_entries:
        print(f"⚠️  Skipped (no file reference): {len(result.skipped_entries)}")
        for req_id, desc, line_num in result.skipped_entries:
            print(f"   Line {line_num}: {req_id} → {desc}")
        print()

    # Summary
    total_broken = len(result.missing_source_files) + len(result.missing_test_files)
    print("=" * 70)
    if total_broken == 0:
        print("RESULT: PASS — All file references are valid")
    else:
        print(f"RESULT: FAIL — {total_broken} broken reference(s) found")
    print("=" * 70)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify EoS traceability matrix references"
    )
    parser.add_argument(
        "--matrix",
        default=None,
        help="Path to traceability_matrix.md (default: auto-detect)",
    )
    parser.add_argument(
        "--root",
        default=None,
        help="Project root directory (default: auto-detect)",
    )
    args = parser.parse_args()

    # Auto-detect project root
    script_dir = Path(__file__).resolve().parent
    project_root = str(args.root or script_dir.parent)

    # Auto-detect matrix file
    matrix_path = args.matrix
    if matrix_path is None:
        candidates = [
            os.path.join(project_root, "docs", "compliance", "traceability_matrix.md"),
            os.path.join(project_root, "docs", "traceability_matrix.md"),
        ]
        for candidate in candidates:
            if os.path.isfile(candidate):
                matrix_path = candidate
                break

    if matrix_path is None or not os.path.isfile(matrix_path):
        print(f"ERROR: Traceability matrix not found. Searched:")
        for c in candidates:
            print(f"  - {c}")
        return 2

    print(f"Matrix: {matrix_path}")
    print(f"Root:   {project_root}")
    print()

    # Parse and validate
    rows = parse_matrix(matrix_path)
    print(f"Parsed {len(rows)} requirements from matrix.")
    print()

    result = validate_references(rows, project_root)
    print_report(result)

    total_broken = len(result.missing_source_files) + len(result.missing_test_files)
    return 1 if total_broken > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
