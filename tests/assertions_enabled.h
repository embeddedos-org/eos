/* Test-only forced include: a passing suite with assert() compiled out is
 * not evidence. Keep this header in the tests CMake directory so it applies
 * to test executables, not to the libraries being exercised. */
#ifdef NDEBUG
#error "test targets must be built with assertions enabled"
#endif
