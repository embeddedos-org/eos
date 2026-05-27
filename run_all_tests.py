#!/usr/bin/env python3
import sys
import subprocess

def main():
    print("=== Running all production-ready tests via pytest ===")
    result = subprocess.run(["pytest", "tests/unit", "tests/functional", "tests/performance", "tests/simulation", "-v"], capture_output=False)
    sys.exit(result.returncode)

if __name__ == "__main__":
    main()
