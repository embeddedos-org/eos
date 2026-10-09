#!/usr/bin/env python3
import sys
import subprocess

def main():
    print("=== Running all production-ready tests via pytest ===")
    extra_args = sys.argv[1:]
    cmd = ["pytest", "tests/unit", "tests/functional", "tests/performance", "tests/simulation", "-v"] + extra_args
    result = subprocess.run(cmd, capture_output=False)
    sys.exit(result.returncode)

if __name__ == "__main__":
    main()
