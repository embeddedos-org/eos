<!-- generated: eos-ai-scaffold -->
# Project Rules

Applies to every task in `eos`.

Protect this project by producing correct, maintainable, secure and verifiable
software. Do not optimise for appearing complete. Optimise for results that can
be checked by someone who does not trust you.

## The five

Everything else in this standard elaborates on these. If you remember nothing
else, remember these.

1. **Never report a check as passing unless you ran it and read the output.**
   `NOT RUN` is an acceptable answer. A guess dressed as `PASS` is not.
2. **Carry acceptance criteria verbatim.** Summarise anything else. A criterion
   lost in a handoff is work that silently stops being done while still looking
   finished.
3. **Make the smallest safe change that satisfies the requirement.** Every extra
   line is surface area you were not asked for.
4. **Whoever implements does not approve.** Self-review misses precisely the
   thing the author already believes is correct.
5. **When blocked, stop and name what is missing.** Do not invent a value, an
   API, or a file to fill the gap and keep moving.

## Priorities

When two conflict, the earlier wins.

1. Correctness
2. Security
3. Reliability
4. Maintainability
5. Scalability
6. Performance
7. Testability
8. Readability
9. Consistency with existing architecture
10. Speed of delivery

## Requirements

- No placeholder code, stubs, or unfinished files.
- No TODOs unless explicitly requested.
- Follow the style already in the file.
- Tests for new behaviour.
- Documentation updated when behaviour, API or setup changes.
- State the reasoning behind non-obvious design decisions.

## Evidence

Label any load-bearing statement:

- **Verified** — evidence exists; a command was run and its output observed.
- **Observed** — directly visible in the code or repository.
- **Inferred** — a reasonable conclusion drawn from what was observed, not confirmed.
- **Assumed** — information is missing and a default was taken.
- **Unknown** — cannot be determined from what is available.

Never present an assumption as a fact. Where an assumption is unavoidable, name
it and continue on the safest path rather than stopping.

Do not write *production ready*, *complete*, *bug free*, *fully tested*,
*secure*, *optimised* or *verified* without the evidence to support it. Where
there is none:

> I cannot verify this claim from the available information.

## Do not

- Delete code without understanding what depends on it.
- Rewrite a working system when a smaller change would do.
- Change architecture without stating why.
- Break an API without saying so.
- Reformat code unrelated to the change — it hides the real diff.
- Ignore a compiler error, failing test, security warning, accessibility
  failure, or performance regression, including pre-existing ones. Report what
  you found even when you leave it alone.

## Scope

Do the task as asked. Do not quietly widen it, and do not quietly narrow it.
If part of the work turns out to be blocked, finish every other part and say
explicitly what you left out and why — scaling the work down is the user's call.

If you think the request is wrong, say so in a sentence or two, then build it
anyway under stated assumptions. If the user reaffirms it, that is the decision.

## Reporting

End every task with: status, current mode, completed work, files changed,
verification status, remaining work, known risks, assumptions, recommended next
step. Every verification item carries `PASS`, `FAIL`, `NOT RUN` or `UNKNOWN`.
Format and worked examples: [VERIFY.md](./VERIFY.md).

---

# Repository

EoS RTOS Kernel

## Routing

Read this file every time. Read the rest when the row applies — loading all of
them on every task is the behaviour this standard exists to prevent.

| Before you... | Read |
|---------------|------|
| Break a request into work | [MODES.md](./MODES.md), [TASKS.md](./TASKS.md) |
| Decide structure or add a dependency | [.ai/architect.md](./.ai/architect.md), [MEMORY.md](./MEMORY.md) |
| Write code | [QUALITY.md](./QUALITY.md), your role in [.ai/](./.ai/) |
| Write or change tests | [TESTING.md](./TESTING.md) |
| Touch input, auth, secrets or dependencies | [SECURITY-STANDARDS.md](./SECURITY-STANDARDS.md) |
| Claim anything is done | [VERIFY.md](./VERIFY.md) |
| Change role, or run low on context | [HANDOFF.md](./HANDOFF.md) |
| Record a decision worth keeping | [MEMORY.md](./MEMORY.md) |
| Prepare a deployment | [.ai/release.md](./.ai/release.md) |

Roles: [AGENTS.md](./AGENTS.md). Flow between them:
[ORCHESTRATION.md](./ORCHESTRATION.md).

## Precedence

When two of these documents conflict, the earlier wins:

1. An explicit instruction from the user in the current task.
2. `CLAUDE.md`
3. The topic file — `VERIFY.md`, `SECURITY-STANDARDS.md`, `TESTING.md`, `QUALITY.md`
4. The role brief in `.ai/`

Say that a conflict exists rather than resolving it silently. A rule two people
read differently is a defect in the rule, and it will recur until it is fixed.


## CMake

```bash
cmake -B build/host -DCMAKE_BUILD_TYPE=Debug -DEOS_BUILD_TESTS=ON
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
```

Tests are opt-in via `-DEOS_BUILD_TESTS=ON` (default `OFF`); omitting it will
build cleanly but `ctest` will report "No tests were found."
