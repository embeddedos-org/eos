# CTestCustom.cmake — Valgrind/memcheck configuration for EoS
#
# CTest reads this file automatically from the build directory.
# Usage: ctest -T memcheck

set(CTEST_MEMORYCHECK_COMMAND "/usr/bin/valgrind")
set(CTEST_MEMORYCHECK_COMMAND_OPTIONS
    "--leak-check=full --show-reachable=yes --track-origins=yes --error-exitcode=1"
)

# Suppressions for known false positives
set(CTEST_MEMORYCHECK_SUPPRESSIONS_FILE "")

# Tests to exclude from memcheck (e.g., fuzz tests which run indefinitely)
set(CTEST_CUSTOM_MEMCHECK_IGNORE
    fuzz_sha256
    fuzz_aes
    fuzz_devicetree
    fuzz_ota_header
)
