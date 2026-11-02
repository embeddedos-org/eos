<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# Robot control + safety contract (EoS)

Status: **contract slice, not code.** Every section is labelled
**READ** (verified from the sources listed in §1 at eos master
`ce2f110e`) or **PROPOSED** (needs implementation and/or a
maintainer decision). Nothing in the PROPOSED sections is
implemented by this document, and no value in them is a
measurement. This document takes **no position** on eos #202
(the compat-API timeout question — the maintainer's decision);
§2 records only the kernel convention as read.

Companion: the EoSim side of this story — driving the simulated
rover from `cmd_vel` / reading `odom` — is
`EoSim/eosim/integrations/rover_ros2.py` (EoSim #52). This
document is the on-device contract that a real rover build
would have to satisfy; the sim adapter is the place the
contract is first exercised.

## 1. Sources read for this slice

- `kernel/src/task.c` — task creation, priority model,
  preemption, tick/wake behaviour, stack canary handling.
- `kernel/include/eos/kernel.h` — public task/sync/IPC API,
  wait constants, return codes, task limits.
- `kernel/include/eos/kernel_internal.h` — tick counter,
  tick comparison, internal task helpers.
- `kernel/src/sync.c` — mutex/semaphore wait semantics.
- `docs/safety/work-in-progress/iso26262/safety_plan.md` —
  the SEooC safety lifecycle (governance, not code).
- `docs/book/part7-advanced/ch35-safety.md` — standards
  survey (IEC 61508, ISO 26262, DO-178C).
- `services/os/include/eos/os_services.h`,
  `services/init/src/service.c` — watchdog surfaces in eos.
- `eNI/common/src/stim_safety.c` — read only to confirm it
  is BCI stimulation safety (max amplitude / duration /
  interval limits) and therefore NOT part of this contract's
  lineage.
- Code search over eos at `ce2f110e`: `cmd_vel` — 0 hits.

## 2. READ — scheduler facts as found

- **Priority model (lower number wins).**
  `find_next_task()` (`kernel/src/task.c`) scans from the
  slot after the current task and keeps the READY task with
  the smallest `priority` value; `eos_task_create()` yields
  immediately when a new task's priority is numerically
  below the running task's. The idle task is slot 0, is
  permanent (delete/suspend refuse handle 0), and runs at
  priority 255 — chosen only when nothing else is READY.
  Usable application priorities are therefore 0–254, with
  smaller meaning more urgent. At most `EOS_MAX_TASKS`
  (16, `kernel.h`) tasks exist, idle included.
- **Tick = 1 ms.** `eos_task_delay_ms(ms)` sets
  `wake_tick = g_tick + ms` (`task.c`), i.e. ticks are
  milliseconds; `kernel_internal.h` states the same
  ("about 24.9 days at 1 ms per tick") for the 32-bit
  wrap-safe deadline comparison. `g_tick` increments in
  `eos_task_tick()`, driven by the port's SysTick handler,
  which also runs the blocked-task wake check
  (`eos_task_wake_check()`) and software timers.
- **Preemption points observed in `task.c`.** A task is
  preempted when it blocks, yields, or when a higher
  priority (lower-numbered) task is created. **No tick
  time-slice statement was found in `task.c`:** nothing
  there says a running task is preempted merely because
  a tick fired. Equal-priority tasks rotate only through
  `find_next_task()` starting after the current task.
  Any control loop that needs periodic execution must
  therefore be written as a periodic task (delay loop),
  not assumed to be time-sliced.
- **Wait semantics.** `kernel.h` defines
  `EOS_NO_WAIT = 0` and `EOS_WAIT_FOREVER = 0xFFFFFFFF`.
  `sync.c` honours them literally: a mutex/semaphore wait
  with `EOS_NO_WAIT` returns `EOS_KERN_TIMEOUT`
  immediately; a wait with `EOS_WAIT_FOREVER` arms no
  wake deadline (`wake_armed` stays clear in
  `eos_task_block_with_timeout()`). Finite waits arm
  `wake_tick = g_tick + timeout_ms` with the overflow-safe
  comparison above. (This is the kernel convention as
  read. The #202 question concerns the compat layer's
  receive-timeout reading and is NOT decided here.)
- **Stack overflow handling (current behaviour, read).**
  `eos_schedule()` checks the outgoing task's canary; on
  mismatch it marks the task `EOS_TASK_DELETED`. The
  comment there says "In production: trigger fault
  handler or reboot" — i.e. the current code deletes the
  task and continues, it does not fault. A control task
  that overflows its stack therefore disappears silently
  today; §4's watchdog/safe-stop proposal exists partly
  because of this.
- **No control-period or jitter statement was found**
  in `task.c`, `kernel.h`, `kernel_internal.h`, `sync.c`,
  the safety plan, or ch35. The kernel gives the
  mechanism (priorities, 1 ms ticks, waits); it does
  not state what period or jitter a control loop must
  meet. §3 proposes those numbers; they are proposals,
  not readings.

## 3. READ — what already exists for safety

- **SEooC safety lifecycle (governance).** The safety plan
  defines EoS as a Safety Element out of Context per
  ISO 26262-10, target ASIL levels QM to ASIL D by
  component, with concept / HARA / functional-safety-concept
  phases, confirmation reviews, and ASIL decomposition.
  It commits to a lifecycle and review structure, not to
  any robot behaviour. This contract's PROPOSED sections
  map to its concept phase (item definition, HARA,
  functional safety concept) — the plan section that owns
  each requirement is named in §4.
- **Standards background.** ch35 surveys IEC 61508 (SIL),
  ISO 26262 (ASIL), and DO-178C (DAL) plus practices
  (memory safety, deterministic behaviour, defensive
  programming, redundancy). It is background for a
  safety case, not a robot contract.
- **Watchdog surfaces in eos (found, not assumed).**
  `services/os` declares a software watchdog
  (`EosWatchdog`, `eos_watchdog_init()` in
  `os_services.h`); `services/init` runs a per-service
  health-check watchdog (`watchdog_timeout_ms`, default
  30000 ms, set via `eos_svc_set_watchdog()`). The
  VxWorks-compat `wd.h` maps VxWorks watchdog calls to
  kernel software timers and is a compatibility layer,
  not an eos watchdog API. None of these is a robot
  velocity watchdog: **no `cmd_vel` consumer, no
  command-timeout → safe-stop rule exists anywhere in
  eos** (code search: `cmd_vel` 0 hits at `ce2f110e`).
  The velocity watchdog in §4 is therefore new contract,
  composed from — not duplicating — the existing software
  watchdog primitives.
- **Explicitly excluded lineage.** `eNI`'s stimulation
  safety (`stim_safety.c`: amplitude / duration /
  interval / daily limits) guards neural stimulation
  hardware. It is BCI safety, not robot safety, and
  nothing in §4 depends on it.

## 4. PROPOSED — control contract

All values below are PROPOSED targets with a verification
method, not measured constants. Each needs a maintainer
decision or an implementation before it can be claimed.

- **Control task class.** A robot control loop runs as a
  periodic task: delay-loop on the 1 ms tick (§2), at the
  highest application priority band. PROPOSED: priority
  **1** for the control task (0 left for a future fault
  handler; idle sits at 255 by construction), telemetry
  and logging at 10 and above, background work at 100
  and above. Rationale: lower number wins (§2), so the
  control task preempts everything except a fault
  handler; the band leaves room above (0) and below
  (telemetry) without renumbering. To confirm at
  integration: no existing application task already
  claims priority 1 on the target build.
- **Control period: 10 ms. Jitter budget: ±1 ms.**
  Rationale: 10 ms matches the EoSim rover's sim tick
  (the same loop the contract is first exercised
  against), so sim and device share a period; ±1 ms is
  one tick quantum, the smallest budget the 1 ms tick
  can express. Verification method (not a claim of
  measurement): EoSim tick-count tests driving the
  rover adapter, plus on-target logging of the control
  task's wake-to-wake intervals against `g_tick`
  (per-task `run_count` statistics in `kernel.h` give
  the counting hook). If measurement shows the budget
  unmet, the budget does not move silently — the
  period or the priority assignment changes, in this
  document, with the measurement attached.
- **No period/jitter claim for other loops.** Perception,
  planning, and telemetry loops get their own periods
  when they exist; this contract covers the control
  loop only, so a fast perception loop cannot be
  mistaken for control-loop compliance.

## 5. PROPOSED — safety contract

- **E-stop: latched, explicitly cleared.** PROPOSED:
  an e-stop source (dedicated input or topic — the
  source is a maintainer decision, flagged) drives the
  rover to safe-stop and LATCHES: motion stays stopped
  until an explicit clear command, never resumes on
  e-stop release alone. Rationale: momentary-release
  resumption is the classic rover runaway shape; the
  latched rule is the conservative default. Decision
  owed: the e-stop source and the clear authority.
  SEooC owner: functional safety concept (plan §3).
- **Velocity watchdog: no `cmd_vel` within T → safe-stop.**
  PROPOSED: **T = 500 ms.** Rationale: 50 control
  periods — tolerant of transient command drops, tight
  enough to matter: at 0.5 m/s a rover travels 0.25 m
  inside T, and the EoSim rover's traverse speed is
  0.04 m/s (0.02 m inside T). The watchdog composes
  from the existing software-watchdog primitives (§3);
  it does not need a new hardware timer. Verification
  method: fault injection — kill the `cmd_vel` stream
  in sim and assert safe-stop within T + one period.
  Decision owed: whether 500 ms is the right T for
  faster rovers (the arithmetic above is the decision
  aid, not the decision).
- **Safe-stop behaviour.** PROPOSED: safe-stop means
  zero velocity command applied (the rover's
  `set_cmd_vel(0, 0)` equivalent on-device) AND the
  latched stop state above; a safe-stopped rover
  reports its state (telemetry) rather than going
  silent, so "stopped" is distinguishable from "dead".
  SEooC owner: functional safety concept + HARA
  (plan §3) — the HARA must name the hazard
  (uncommanded motion) this rule mitigates.
- **Stack-overflow interaction (gap, not a rule).**
  §2's READ finding — an overflowed task is deleted,
  not faulted — means a control-task overflow today
  looks exactly like a silent stop unless something
  watches for it. PROPOSED: the velocity watchdog and
  the service health-check watchdog (§3) are the
  interim tripwires, and a fault-handler-or-reboot
  policy for canary failure is owed as follow-up
  work, not assumed here.

## 6. Explicitly out of this slice

- Any code change (no `task.c`, `sync.c`, `net.c`, or
  driver edits). The contract is paper until a later
  slice implements it.
- Any #202 position (see §2 — kernel convention only).
- Any Isaac / GR00T / Isaac Sim interface claim: no
  first-party interface surface for those products has
  been verified against (the GTC Berlin announcements
  are announcement-level, not interface sources).
- Any signed-model / on-device-inference completeness
  claim: the signing step has not been verified against
  the eSec control set in this slice.
- Any certification claim (ASIL/SIL/DAL): the SEooC
  plan is governance; this contract does not assert
  a level for any component.

## 7. Follow-ups (each a future slice, not this one)

1. Confirm no application task claims priority 1 on the
   rover target build (integration check for §4).
2. Decide the e-stop source and clear authority (§5).
3. Decide T for faster rovers, or scope T by rover
   class (§5).
4. Fault-handler-or-reboot policy for canary failure
   (§5 gap).
5. Measure period/jitter against §4's method; attach
   the measurement here when it exists.
6. When the micro-ROS scoping lands (eos N1, gated on
   #199 slice 3), revisit §4's period against the
   transport's latency — the contract must survive
   the transport it runs over.
