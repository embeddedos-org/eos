<!-- generated: eos-ai-scaffold -->
# Testing Standards

A test exists to fail when the code is wrong. A test that passes regardless of
the implementation is worse than none: it consumes time and buys confidence
that is not real.

## What must be tested

- **Unit** — logic and its branches, including edge cases.
- **Integration** — real boundaries between components, not mocked ones.
- **Regression** — every fixed defect gets a test that would have caught it.
- **Failure cases** — the error paths, not only the happy path.

## The one check that matters

**Break the code and watch the test fail.** Then restore it and watch the test
pass.

A test that has never been observed failing has not been shown to test
anything. This takes seconds and catches the whole category of tests that
assert on the wrong thing, mock away the code under test, or pass because of a
default rather than the logic.

Do this for every test that backs an acceptance criterion. If a test still
passes with the implementation broken, the test is the defect.

## Native network tests on WSL

The native TCP tests use 127.0.0.1 by default. If WSL redirects loopback to the
Windows host, first verify a local IPv4 address that accepts connections to a
listener in the same WSL instance, then provide it as `EOS_NET_TEST_HOST`:

```sh
# HOST_BUILD_DIR is the configured native build with tests enabled.
# LOCAL_TEST_IPV4 must be a verified address of this WSL instance, not a remote host.
(cd "$HOST_BUILD_DIR" && EOS_NET_TEST_HOST="$LOCAL_TEST_IPV4" ctest --output-on-failure)
```

This override changes only the native TCP test destination; the echo listener,
real send/receive assertions, and connection-refused assertion still execute.
The localhost resolver assertions remain unchanged. An invalid IPv4 override
fails the test instead of falling back or skipping it. This is environment
qualification, not physical-board or network-service qualification.

## Coverage

Target 95% or higher on new and changed code where the toolchain measures it.

Coverage is a floor, not a goal. 100% coverage with assertion-free tests proves
nothing, and a repository with no coverage tooling is not exempt from testing —
it is exempt from the number. Where coverage cannot be measured here, say so
and report the check as `NOT RUN` rather than estimating a percentage.

Prefer a smaller number of tests that would actually fail.

## Rules

- Every acceptance criterion maps to at least one test that fails without the
  change, confirmed by breaking it.
- Tests are deterministic. No dependence on wall-clock time, network, ordering,
  filesystem state, or randomness without a fixed seed.
- Test behaviour through the public interface, not private internals. A test
  that breaks on every refactor of correct code is a maintenance cost, not a
  safety net.
- Mock at the boundary you own, not the code under test. A test that mocks the
  function it is testing verifies the mock.
- Never change the implementation to make a test pass without first
  establishing which of the two is wrong.
- Never delete or skip a failing test to get green. If a test must be skipped,
  say so in the report, with the reason.
- Fixtures contain no real credentials, tokens or personal data.

## Edge cases worth covering by default

Empty input, single element, maximum size, boundary values, duplicates,
unicode and non-ASCII, null and undefined, concurrent access where relevant,
and the failure of every external call.

## Reporting

Report results with actual command output, not a summary. State the number
passed, failed and skipped. A skipped test is not a passing test.

Any test that could not be run in this environment is named, with the reason,
and marked `NOT RUN` or `UNKNOWN` per [VERIFY.md](./VERIFY.md).
