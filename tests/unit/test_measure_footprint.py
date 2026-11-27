"""Tests for scripts/measure_footprint.py — compare() regression exit code."""

from __future__ import annotations

import json
import textwrap
import unittest
from pathlib import Path
from unittest.mock import patch

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
from scripts.measure_footprint import compare


class TestCompareExitCode(unittest.TestCase):
    """compare() must return 1 when flash or RAM grew."""

    def _write_baseline(self, tmp: Path, flash: int = 1000, ram: int = 500) -> Path:
        p = tmp / "baseline.json"
        p.write_text(json.dumps({"total": {"flash": flash, "ram": ram},
                                 "size_tool": "size"}),
                     encoding="utf-8")
        return p

    def _report(self, flash: int = 1000, ram: int = 500) -> dict:
        return {"total": {"flash": flash, "ram": ram},
                "size_tool": "size", "libraries": {}}

    def test_compare_returns_one_on_regression(self):
        """Flash grew → compare() must return 1."""
        import tempfile
        with tempfile.TemporaryDirectory() as td:
            baseline = self._write_baseline(Path(td), flash=1000, ram=500)
            now = self._report(flash=1200, ram=500)
            result = compare(now, baseline)
        self.assertEqual(result, 1)

    def test_compare_returns_zero_when_no_growth(self):
        """No growth → compare() must return 0."""
        import tempfile
        with tempfile.TemporaryDirectory() as td:
            baseline = self._write_baseline(Path(td), flash=1000, ram=500)
            now = self._report(flash=1000, ram=500)
            result = compare(now, baseline)
        self.assertEqual(result, 0)


class TestMainUsesCompareResult(unittest.TestCase):
    """main() must propagate compare()'s exit code into its return value."""

    def test_main_exits_one_on_regression(self):
        import tempfile
        with tempfile.TemporaryDirectory() as td:
            build_dir = Path(td) / "build"
            build_dir.mkdir()
            baseline = Path(td) / "baseline.json"
            baseline.write_text(
                json.dumps({"total": {"flash": 1000, "ram": 500},
                             "size_tool": "size"}),
                encoding="utf-8",
            )
            fake_report = {
                "total": {"flash": 2000, "ram": 500},
                "size_tool": "size",
                "libraries": {},
            }
            with patch("scripts.measure_footprint.collect", return_value=fake_report), \
                 patch("scripts.measure_footprint.find_size_tool", return_value="size"), \
                 patch("scripts.measure_footprint.render"), \
                 patch("sys.argv", ["measure_footprint.py",
                                    "--build-dir", str(build_dir),
                                    "--compare", str(baseline)]):
                from scripts.measure_footprint import main
                status = main()
            self.assertEqual(status, 1)


if __name__ == "__main__":
    unittest.main()
