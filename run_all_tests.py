#!/usr/bin/env python3
import sys
import subprocess

def run_tests():
    print("=== Running all tests via pytest ===")
    result = subprocess.run(["pytest", "tests/", "-v"], capture_output=False)
    sys.exit(result.returncode)

if __name__ == '__main__':
    run_tests()
