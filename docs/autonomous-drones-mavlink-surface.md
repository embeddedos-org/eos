<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 EoS Project -->

# AD1 — MAVLink wire-protocol surface for EoS (scoping)

- **Status:** scoping only. No code ships in this slice. The codec, the link
  state machine, and the failsafe trigger are specified here so the
  implementation slice can be written against a fixed contract.
- **Backlog anchor:** growth-backlog AD1 (autonomous-drones, 2026-11-11).
  Domain register: `autonomous-drones`, pointer 36, reference product
  eVT-5000 + eSwarm-10, pipeline stage `selected`.
- **Home repo:** eos. The backlog entry names "eos + eNet"; this slice lives
  in eos because the surface it scopes is the eos net/socket surface. eNet
  owns the follow-up bearer work (radio link, GCS endpoint) — see §9.
- **Seam + journey stage:** building — no EoS device can talk to any GCS or
  autopilot today, so the eVT-5000 has no ground link. This doc removes the
  "what would the link even be" seam; the implementation slice removes the
  link itself.
- **Verified against live master:** eos `master` @
  `b0f44f95c25353e8448182102602675c6c4b2127` (2026-11-24). Every claim about
  the eos surface below was read from that head. MAVLink claims cite
  mavlink.io (first-party) or are labelled `UNVERIFIED`.

## 1. What exists at live master (the surface this scopes over)

### 1.1 The eos net/socket API — declared, split-brained

`net/include/eos/net.h` declares a transport-agnostic socket API:

| API | What it gives the MAVLink layer |
| --- | --- |
| `eos_net_socket(EOS_NET_TCP / EOS_NET_UDP)` | UDP datagram socket for the GCS link; TCP for a future wired GCS |
| `eos_net_bind`, `eos_net_listen`, `eos_net_accept`, `eos_net_connect` | Listener (vehicle waits for GCS) or connector (vehicle dials a known GCS) |
| `eos_net_sendto` / `eos_net_recvfrom` | UDP datagram send/receive with peer address — the GCS-link primitives |
| `eos_net_send` / `eos_net_recv(sock, buf, len, timeout_ms)` | Stream send; receive waits up to `timeout_ms`, `0` polls. Returns byte count, `0` on timeout **or** an orderly close by the peer, `-1` on error |
| `eos_net_close`, `eos_net_resolve` | Teardown; DNS (unused by this surface — the GCS is addressed by IP) |

What the API does **not** have, and the MAVLink layer must therefore not
assume: no `select`/`poll`/readiness notification, no non-blocking flag, no
datagram-size or MTU query, no socket options, no serial/UART transport in
`net.h` at all. The link loop is polling by construction:
`eos_net_recvfrom(..., timeout_ms)` with a bounded timeout is the only way
to wait for bytes.

### 1.2 The two implementations — only one of them moves bytes

- `net/src/net.c` (376 lines): the freestanding implementation. `connect`,
  `send`, `recv`, `sendto`, `recvfrom` all return `-1`; `bind`/`listen`
  return `0` without binding anything. On a target with no IP stack yet,
  failing is the honest answer — and it is the current answer.
- `net/src/net_posix.c` (225 lines): the Compute-profile mapping onto POSIX
  sockets (ADR-014 point 3). This is the only implementation that moves
  bytes today, and only on host/Compute — which is where EoSim runs.

Consequence, stated plainly: **the MAVLink surface is implementable and
testable on host/Compute today, and is a declared-but-dead API on Nano/Edge
until lwIP lands** (ADR-014 point 1, still the recorded decision). The
implementation slice must not claim target support it cannot exercise, and
its tests must run against the POSIX mapping plus a mock socket, the way
`tests/test_net.c` / `tests/test_net_mock.c` already do for the net layer.

### 1.3 The pattern to copy: the MQTT codec

`net/src/mqtt_codec.c` + `net/src/mqtt_codec.h` (167 lines) is the in-tree
precedent for exactly this kind of component (eos #199, slices 1–2): a pure
codec — buffer in, buffer out, no I/O, no allocation, no dependency beyond
`<stdint.h>`/`<stddef.h>`/`<stdbool.h>` — driven by a small polled state
machine in `net.c` over the `eos_net_*` socket API, with a static handle
table (2 clients), no heap, no threads. The MAVLink codec follows the same
shape: `mavlink_codec.{c,h}` (pure framing + CRC) plus a link driver that
owns one UDP socket and a heartbeat timer. Do not invent a second pattern.

### 1.4 What does not exist

- **No MAVLink code anywhere in eos** (0 hits at live master) and none in
  eNI. There is no dialect, no generated header, no CRC table to reuse.
- **No generic UART HAL.** The only UART code in-tree is
  `hal/src/hal_stm32f4/hal_stm32f4_uart.c` (vendor-specific, no public
  header in `hal/include/`). The eVT-5000's 900 MHz LoRa C2 radio is a
  serial bearer, and EoS currently has no API a MAVLink-over-serial driver
  could sit on. That bearer is therefore out of this surface's scope (§9);
  the first MAVLink transport is UDP over `eos_net_*`.
- **No time service the link can lean on.** MAVLink timestamps are
  `time_boot_ms` (`uint32_t`, wraps at ~49.7 days — the MAVLink wire
  format's own field, not EoS's to change) and `time_unix_usec`
  (`uint64_t`). The 64-bit-time backlog item T64 covers EoS's own
  contracts; this surface takes the wire format as given.

## 2. Codec scope

**MAVLink v2 only.** MAVLink 1 (magic `0xFE`, 8-byte header) is not
encoded and not decoded; a v1-framed peer is out of scope for the first
slice. Rationale: every current GCS/autopilot speaks v2, and supporting
both framings doubles the parser's state space for a peer class the
eVT-5000 will never meet in the field. (`UNVERIFIED`: "every current GCS
speaks v2 by default" — pin at implementation against the GCS actually
used for interop testing.)

Wire format (mavlink.io, "Packet Serialization", fetched 2026-11-24):

| Byte | Field | Value / rule |
| --- | --- | --- |
| 0 | `magic` | `0xFD` |
| 1 | `len` | payload length, 0–255 |
| 2 | `incompat_flags` | must-understand flags; unknown flag ⇒ discard packet. `0x01` = signed |
| 3 | `compat_flags` | ignorable if unknown |
| 4 | `seq` | per-component sequence, 0–255, used to detect loss |
| 5 | `sysid` | sender system id, 1–255 (0 is not a valid source) |
| 6 | `compid` | sender component id, 1–255 |
| 7–9 | `msgid` | 24-bit message id |
| 10… | `payload` | ≤ 255 bytes, little-endian, fields reordered by size, trailing zero bytes truncated |
| n+10… | `checksum` | CRC-16/MCRF4XX over everything except `magic`, **including the CRC_EXTRA byte** |
| n+12… | `signature` | optional 13 bytes, only when `incompat_flags & 0x01` |

- Minimum packet 12 bytes, maximum 280 bytes signed (mavlink.io).
- **CRC_EXTRA** is the per-message seed from the message definition: the
  codec carries a generated `MAVLINK_MESSAGE_CRCS`-style table for the
  minimum set (§4) only. A checksum mismatch means the peer's message
  definition differs from ours — the packet is dropped, never decoded
  on a best-effort basis.
- **Parser shape:** byte-at-a-time state machine over the receive buffer,
  `EOS_MAVLINK_OK / INCOMPLETE / MALFORMED` outcomes mirroring
  `eos_mqtt_status_t`. No allocation; the caller owns a fixed 280-byte
  frame buffer. A malformed byte resynchronises on the next `0xFD`, it
  never stalls the link.
- **Signing is NOT in this slice.** The 13-byte signature field is parsed
  (length accounted for) but never verified and never produced. Signed
  links are a follow-up gated on the eSec 7–9 work — an unsigned link must
  never be presented as authenticated. See §9.

## 3. GCS link contract

### 3.1 Identities

- The vehicle is one MAVLink system: `sysid` assigned by the deployment
  (default 1 for the first vehicle; the swarm case — eSwarm-10 as second
  system — takes a different sysid, assigned, not discovered).
- Components within the system are distinguished by `compid`: the
  flight-control component and the EoS link component are different
  components. `MAV_COMP_ID_AUTOPILOT1`-style values are taken from the
  MAVLink component enum at implementation time; this doc does not pin
  numbers it has not verified against the enum source.
- The GCS is a peer system, not a component of the vehicle. Its `sysid`
  is learned from its HEARTBEAT, never assumed.

### 3.2 Bearer

- **Transport:** one UDP socket (`EOS_NET_UDP`) via `eos_net_sendto` /
  `eos_net_recvfrom`, bound to the vehicle's link port. The GCS address
  is learned from the first received datagram (the vehicle listens; the
  GCS initiates) — no configuration of the GCS IP on the vehicle.
- **GCS port convention:** `UNVERIFIED` — the commonly used UDP port for
  a GCS endpoint is 14550, but this was not verified against a first-party
  source for this doc; pin the port at implementation and record the
  source then. Do not hard-code a port on the strength of this doc.
- **Datagram discipline:** one MAVLink frame per datagram on transmit.
  On receive, a datagram may contain partial frames (the parser is
  byte-streaming, so this is harmless) and datagrams may arrive out of
  order or duplicated — the `seq` field is the only loss detector, and
  loss is tolerated, never retransmitted at this layer.

### 3.3 Link state machine

Driven by HEARTBEAT, the only message the link layer itself owns:

| State | Entry | Exit |
| --- | --- | --- |
| `NO_LINK` | startup; no GCS heartbeat seen | first valid GCS HEARTBEAT ⇒ `LINKED` |
| `LINKED` | GCS heartbeat arriving | heartbeat gap exceeds the lost-link threshold (§6) ⇒ `LINK_LOST` |
| `LINK_LOST` | failsafe trigger fired (§6) | GCS heartbeat resumes ⇒ `LINKED` (recovery is explicit, §6) |

- The vehicle broadcasts its own HEARTBEAT at **1 Hz** — the rate
  mavlink.io's heartbeat service names as typical on RF telemetry links
  ("components typically publish their heartbeat at 1 Hz").
- **No other message is sent while in `NO_LINK`** (mavlink.io: a
  component "may choose not to send or broadcast information on a channel
  ... if it does not detect another system"). Telemetry starts when the
  GCS is known.
- A HEARTBEAT is never sent from a faulted state by a thread unaware of
  the rest of the component (mavlink.io heartbeat service). The link
  driver owns the heartbeat; the flight stack's health gates it.

## 4. Minimum message set

The smallest set that makes the vehicle visible to a GCS, reportable, and
commandable. All ids verified against mavlink.io, "MAVLink Common Message
Set (common.xml)", fetched 2026-11-24.

| # | Message | Id | Direction | Rate (proposed) | Purpose |
| --- | --- | --- | --- | --- | --- |
| 1 | `HEARTBEAT` | 0 | vehicle → GCS, GCS → vehicle | 1 Hz | presence, type, autopilot class, mode, system status |
| 2 | `SYS_STATUS` | 1 | vehicle → GCS | 1 Hz | sensor/subsystem health bitmaps, battery voltage / current / remaining |
| 3 | `GPS_RAW_INT` | 24 | vehicle → GCS | 1 Hz | raw GPS fix (not the fused estimate) |
| 4 | `ATTITUDE` | 30 | vehicle → GCS | 10 Hz | roll/pitch/yaw + rates, aeronautical frame |
| 5 | `GLOBAL_POSITION_INT` | 33 | vehicle → GCS | 5 Hz | fused global position (lat/lon degE7, alt mm, velocity cm/s, heading cdeg) |
| 6 | `COMMAND_LONG` | 76 | GCS → vehicle | on demand | command carriage (see §5) |
| 7 | `COMMAND_ACK` | 77 | vehicle → GCS | on ack | command result (see §5) |

Notes:

- Rates are **proposed values for the implementation slice**, not MAVLink
  requirements — MAVLink defines no telemetry rates. The only
  standards-grounded rate is HEARTBEAT at 1 Hz (§3.3).
- `SYSTEM_TIME` (2) is deliberately **not** in the minimum set: the
  vehicle has no trusted time source to report yet (§1.4). Add it when
  T64 lands, not before.
- `MISSION_*`, `PARAM_*`, `RC_CHANNELS_*`, `MANUAL_SETPOINT`,
  `SET_*_TARGET` and every other message are out of the minimum set.
  Mission upload and parameter sync are follow-up microservices, not
  part of "can the vehicle talk to a GCS".
- Payload fields are the common.xml definitions as fetched; the codec
  implements exactly the fields listed there for these seven messages,
  with v2 trailing-zero truncation and size-ordered field reordering.

## 5. Command–ack contract

- **Carriage:** `COMMAND_LONG` (76) inbound only. The vehicle never
  originates a command in this slice.
- **Acknowledgement:** every accepted `COMMAND_LONG` gets exactly one
  `COMMAND_ACK` (77) carrying `command` (the MAV_CMD id),
  `result` (MAV_RESULT), and, while in progress, `progress` percent.
  `result_param2` and the `target_system`/`target_component` extension
  fields are populated when the command definition gives them meaning;
  otherwise they are zero / absent (v2 truncation).
- **Retry rule (proposed):** the GCS is responsible for retransmission.
  The vehicle is stateless across retries: a duplicate `COMMAND_LONG`
  (same command, same target, received while its ack is outstanding) is
  answered with the same ack, not executed twice. The vehicle never
  retries a command itself.
- **Timeout (proposed):** a command that cannot complete within 1 s
  answers `MAV_RESULT_IN_PROGRESS` with progress, then a final ack.
  The 1 s figure is a proposal for the implementation slice, not a
  MAVLink requirement.
- **Commands in scope — exactly one:** `MAV_CMD_NAV_RETURN_TO_LAUNCH`
  (20), the failsafe's own command (§6). Every other `MAV_CMD` is
  answered `MAV_RESULT_UNSUPPORTED` until its owning slice lands. This
  is deliberate: a vehicle that accepts a command it cannot honour is
  worse than one that refuses it cleanly.
- **Mode changes are out of scope.** No `MAV_CMD_DO_SET_MODE` handling
  in this slice; the flight-mode contract belongs with the flight
  stack, not the link.

## 6. Lost-link failsafe

- **Detection (standards-grounded):** mavlink.io's heartbeat service
  says a system is considered disconnected "if four or five messages
  are not received" at the 1 Hz heartbeat rate. This surface adopts
  that: **link lost after 5 consecutive missed heartbeats (5 s at
  1 Hz)**. The threshold is a named constant in the contract, not a
  magic number in the code.
- **Trigger:** on entering `LINK_LOST`, the link driver raises the
  failsafe event to the flight stack. The event carries the last-known
  GCS heartbeat time and the link state — nothing more.
- **Safe state (per the domain register):** RTL on link loss. The
  register's proposed safe state is "RTL on link loss/low battery,
  motor-failure degrade". This surface scopes the **link-loss half
  only**: the trigger and the `MAV_CMD_NAV_RETURN_TO_LAUNCH` command
  that executes it. Low-battery RTL and motor-failure degrade are
  flight-stack concerns with their own triggers; they share the RTL
  execution path but are not this surface's contract.
- **Division of responsibility (the key scoping line):** EoS owns
  *detection and signalling* (heartbeat accounting, `LINK_LOST` event,
  the RTL command). The flight controller owns *execution* (climb,
  return, land). EoS never flies the vehicle; it tells the flight
  stack the link is gone.
- **Recovery:** when a GCS heartbeat resumes, the link returns to
  `LINKED` and reports recovery — but recovery **does not cancel an
  RTL already in progress**. Cancelling RTL is a GCS command
  (`MAV_CMD_NAV_RETURN_TO_LAUNCH` cannot be un-sent; the cancel path
  is a mode command, which is out of scope per §5). An RTL that
  started on link loss runs to completion unless the flight stack's
  own rules end it.
- **NOT decided:** what happens if RTL itself fails (no position fix,
  no home set, RTL timeout) — land-in-place, hover, or controlled
  descent are flight-stack decisions for a later slice, not this
  contract. The EoSim drone sim already covers motor-failure and
  low-battery RTL scenarios (`eosim/engine/native/simulators/drone.py`
  per backlog AD2 evidence); the sim is the place to exercise the
  trigger before hardware does.

## 7. What this slice enables next

- **Implementation slice:** `mavlink_codec.{c,h}` + link driver +
  tests, following the MQTT codec pattern (§1.3), on the POSIX mapping
  first, mocked-socket tests in CI.
- **AD2** (EoSim eVT-5000 VTOL airframe + mission surface) depends on
  this surface's message shapes for its MAVLink-shaped telemetry.
- **eSwarm-10 mesh peer** and the **eGCS-100 ground-station peer** both
  speak this same minimum set; the swarm case is a second system id on
  the same wire, not a second protocol.
- **MCP interrogation surface** wraps this API later, under eSec 7–9
  (per the AD1 backlog entry) — not in this slice.

## 8. Explicitly NOT decided

1. **MAVLink signing / authentication.** Parsed, never verified (§2).
   No claim of a secure link may be made on the strength of this surface.
2. **Dialect.** `common.xml` only. No PX4/ArduPilot dialect messages,
   no custom EoS dialect, no dialect negotiation.
3. **Serial/UART bearer.** No HAL API exists for it (§1.4). The 900 MHz
   LoRa C2 radio link is a follow-up owned with eNet once a UART HAL
   (or a radio driver surface) exists.
4. **4G LTE backup bearer.** The eVT-5000 register lists 4G LTE as a
   backup link; which bearer carries MAVLink when the primary fails, and
   how failover is decided, is not this contract.
5. **Parameter and mission microservices.** Out of the minimum set (§4).
6. **Telemetry rates beyond HEARTBEAT.** The §4 rates are proposals.
7. **Command set beyond RTL.** One command in scope (§5).
8. **RTL-failure escalation.** Not decided (§6).
9. **GCS-side implementation.** eGCS-100 is a peer; its software is not
   specified here.
10. **Certification claims.** Nothing here certifies anything. DO-178C,
    RTCA DO-396, ASTM F3411/F38, FAA Part 89/107/108 are AD3 / Specify-
    stage concerns, per the backlog's catalog note.
11. **GCS UDP port.** `UNVERIFIED` (§3.2) — pin at implementation.

## 9. Follow-ups and owners

| Follow-up | Owner | Depends on |
| --- | --- | --- |
| MAVLink codec + link driver implementation | eos (this surface's slice) | AD1 (this doc) |
| UART HAL / radio bearer for the LoRa C2 link | eos HAL + eNet | a UART HAL design |
| GCS endpoint + eGCS-100 peer | eNet | AD1 implementation |
| MAVLink signing under eSec 7–9 | eSec + eos | eSec controls landing |
| AD2 EoSim VTOL airframe + MAVLink-shaped telemetry | EoSim | AD1 |
| Parameter / mission microservices | eos | AD1 implementation |
| 4G backup-bearer failover | eNet | AD1 implementation |

## Sources

- eos live master `b0f44f95c25353e8448182102602675c6c4b2127`:
  `net/include/eos/net.h` (socket API), `net/src/net.c` (376 lines,
  freestanding stubs), `net/src/net_posix.c` (225 lines, POSIX mapping),
  `net/src/mqtt_codec.{c,h}` (167-line header; pure-codec pattern),
  `docs/eos-mqtt-client-design.md` (the #199 design the pattern comes
  from), ADR-014 (`docs/adr/ADR-014-tcpip-via-lwip.md`, lwIP decision).
  No MAVLink code in eos or eNI (tree search, 2026-11-24).
- MAVLink first-party, fetched 2026-11-24: mavlink.io "Packet
  Serialization" (v2 framing, CRC_EXTRA, signing field, 12/280-byte
  bounds); "MAVLink Common Message Set (common.xml)" (HEARTBEAT 0,
  SYS_STATUS 1, SYSTEM_TIME 2, GPS_RAW_INT 24, ATTITUDE 30,
  GLOBAL_POSITION_INT 33, COMMAND_LONG 76, COMMAND_ACK 77,
  MAV_CMD_NAV_RETURN_TO_LAUNCH 20); "Heartbeat/Connection Protocol"
  (1 Hz typical on RF links, 4–5 missed heartbeats = disconnected,
  no-heartbeat-from-faulted-state, no-telemetry-before-discovery).
- Growth backlog AD1 (2026-11-11 afternoon domain step) and the domains
  register (`autonomous-drones`, pointer 36): eVT-5000 use case, safe
  state, AD2–AD4 cross-links. PX4 v1.17.0 / ArduPilot 4.7.1 version
  claims in the backlog entry were not re-verified for this doc and are
  not relied on here.

— av_mando
