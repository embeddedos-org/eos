#!/usr/bin/env python3
import unittest
import sys
import os

if __name__ == '__main__':
    print("=========================================================")
    print("RUNNING ALL DOMAIN-SPECIFIC TESTS FOR THE REPOSITORY")
    print("=========================================================")
    
    try:
        import pytest
        sys.exit(pytest.main(["-v", "tests"]))
    except ImportError:
        loader = unittest.TestLoader()
        suite = loader.discover(start_dir=os.path.dirname(__file__) + '/tests', pattern='test_*.py')
        runner = unittest.TextTestRunner(verbosity=2)
        result = runner.run(suite)
        if not result.wasSuccessful():
            sys.exit(1)
        print("ALL TESTS PASSED SUCCESSFULLY! ✓")
