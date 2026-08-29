---
adr: 14
title: TCP/IP via lwIP
status: Proposed
date: 2026-08-28
deciders: Architecture Council (§33), Networking maintainers
source: EmbeddedOS Platform Architecture & Ecosystem Design Document v0.9, §12, §21, §25
---

# ADR-014 — TCP/IP via lwIP

## Context

§12 describes eNet as covering TCP/IP, UDP, DHCP, DNS, MQTT, CoAP, HTTP and WebSocket.

**Observed — what `net/src/net.c` actually contains:** 184 lines, in which
`eos_net_connect()` is

```c
int eos_net_connect(eos_socket_t sock, const eos_net_addr_t *addr)
{
    (void)sock; (void)addr;
    return -1;
}
```

and `eos_net_socket()` returns an incrementing integer. There is no protocol
implementation of any kind. `eos` PR #53 adds protocol-argument validation to this
facade — a correct fix to a function that connects nothing.

This is the clearest case in the tree where adoption costs nothing, because there is no
existing implementation to preserve.

## Decision

1. **Adopt lwIP** as the TCP/IP stack for the Nano and Edge profiles. BSD-3-Clause
   (**Inferred**; confirm at import).
2. `net/include/eos/net.h` stays as the EoS-facing API. The socket-shaped functions map
   onto lwIP's sockets or raw API depending on profile.
3. On the Compute profile, where a host OS is present, the same API maps to POSIX sockets.
4. **§25 status for eNet is corrected to `Planned` in the same PR that merges this ADR.**
   Documenting a stub as a networking subsystem is the failure mode §25 exists to prevent,
   and correcting the record does not wait for the implementation.
5. MQTT, CoAP and HTTP are **not** in scope here. They are separate adoption decisions on
   top of a working IP stack.

## Consequences

- lwIP is materially larger than 184 lines of stub. The Nano profile must be able to build
  without it; that is a Kconfig question and therefore depends on ADR-016.
- lwIP is already integrated by ST, Espressif, NXP and TI, so vendor porting layers exist
  for the reference boards named in the Phase 3 plan.
- PR #53's validation remains correct and harmless under the shim.

## Open

- Which lwIP integration mode — `NO_SYS=1` bare-metal, or the RTOS mode backed by EoS
  tasks. The RTOS mode is the better fit but requires the CMSIS-RTOS2 surface of ADR-015.
- Zero-copy buffer ownership across the EoS API boundary. **Unknown**; needs a design pass.
