# MQTT client design — implementing the `net.h` MQTT surface

Status: design doc (no code). Tracker: #199. Date: 2026-10-24.
This is the design input for the adoption decision ADR-014 point 5
reserved: MQTT is "a separate adoption decision on top of a working
IP stack". ADR-014 decided the stack (lwIP on Nano/Edge, POSIX
sockets on Compute); this doc designs the MQTT client that sits on
it, against the API `net/include/eos/net.h` already declares.

## 1. The declared surface (the contract)

`net/include/eos/net.h` declares the whole client API today:

```c
typedef struct {
    char     broker_host[128];
    uint16_t broker_port;
    char     client_id[64];
    char     username[64];
    char     password[64];
    uint16_t keepalive_sec;
} eos_mqtt_config_t;

typedef void (*eos_mqtt_msg_callback_t)(const char *topic,
                                        const uint8_t *payload,
                                        size_t payload_len, void *ctx);
typedef int eos_mqtt_handle_t;   /* EOS_MQTT_INVALID is -1 */

eos_mqtt_handle_t eos_mqtt_connect(const eos_mqtt_config_t *cfg);
int eos_mqtt_disconnect(eos_mqtt_handle_t handle);
int eos_mqtt_publish(eos_mqtt_handle_t handle, const char *topic,
                     const void *payload, size_t len, uint8_t qos);
int eos_mqtt_subscribe(eos_mqtt_handle_t handle, const char *topic,
                       uint8_t qos, eos_mqtt_msg_callback_t cb, void *ctx);
int eos_mqtt_unsubscribe(eos_mqtt_handle_t handle, const char *topic);
int eos_mqtt_loop(eos_mqtt_handle_t handle, uint32_t timeout_ms);
```

The shape is a polled client: no threads are promised, inbound
messages arrive when the application calls `eos_mqtt_loop`, and
delivery is by per-subscription callback. The design below
implements exactly this surface — no header change is proposed.

## 2. Current state (verified at HEAD, 2026-10-24)

- **`net/src/net.c` — every MQTT function is a stub.** Connect
  returns `EOS_MQTT_INVALID`; disconnect / publish / subscribe /
  unsubscribe / loop return `-1`. The file header says so:
  "Stub implementation".
- **`net/src/net_posix.c` — no MQTT at all, by decision.** The
  POSIX file implements the socket layer only; its header records
  that "ADR-014 point 5 keeps MQTT, CoAP and HTTP out: they are
  separate adoption decisions on top of a working IP stack, and
  they stay in net.c until those decisions are made." So there is
  no POSIX MQTT variant waiting to be switched on — the gap is
  the whole client, on every profile.
- **The tests pin the stub behaviour.** `tests/test_net.c` checks
  only that connect does not crash (it tolerates
  `EOS_MQTT_INVALID`). `tests/test_net_integration.c` asserts the
  stub outright: connect returns `EOS_MQTT_INVALID` ("No network
  in stub"), and the other calls return `-1` on an invalid handle.
  Those assertions are the spec of the *stub*, and the
  implementation slices below replace them deliberately.
- **The only wire knowledge in the tree is in a test.**
  `tests/test_net_mock.c` hand-builds an MQTT 3.1.1 CONNECT
  packet (10 checks: fixed header, protocol name "MQTT", level 4,
  clean-session flag, keep-alive 60) and a QoS 0 PUBLISH packet
  (5 checks) against a mock socket, and injects a CONNACK to read
  back. The framing is therefore already pinned byte-for-byte in
  test form — the codec in §5 formalises what this test knows.
- **ebuild holds no transport to port.** Its
  `layers/eai/framework/src/connector_mqtt.c` is a real
  connector-framework integration with a stub transport:
  `mqtt_connect` sets state and logs (no socket), `mqtt_write`
  logs the publish (in-code comment: "stub: real implementation
  would use an MQTT client library"), `mqtt_read` returns a
  canned `{"status":"ok"}`. What transfers is the parameter shape
  (broker / port / client_id, defaults localhost:1883 /
  "eai-client") and the connector state model
  (CONNECTED / DISCONNECTED on the ops struct) — not wire code.
  **No working MQTT transport exists anywhere in the org.**
- HTTP and mDNS in `net.c` are stubs of the same kind. They are
  separate ADR-014 point 5 decisions and are out of scope here;
  this doc is MQTT only.

## 3. Transport options

**(a) Minimal in-tree MQTT 3.1.1 codec over the `eos_net` socket
API — RECOMMENDED.** A small codec (packet encode/decode plus a
per-handle state machine) written against `eos_net_socket` /
`eos_net_connect` / `eos_net_send` / `eos_net_recv`, so the same
code runs on the POSIX profile today and on the lwIP profile when
it lands (both sit behind the same `net.h` socket API).
Reasoning: the modular-adoption rule — eos must stay liftable on
its own, and an in-tree codec adds no external dependency, no
licence question, and no size surprise; the protocol subset a
QoS 0/1 client needs is small (§5); and the framing is already
pinned by the in-tree mock tests.

**(b) Vendored client library — not recommended as the first
move.** Any candidate would need a licence + size audit before
adoption (none was performed for this doc, and no library's
licence or footprint is claimed here). Vendoring also couples
eos's release to an upstream the org does not control, for a
protocol subset the tree can hold itself. Revisit only if the
codec's maintenance cost proves larger than the audit predicted
— that is a future decision with its own evidence.

**(c) POSIX-first via `net_posix.c` — rejected as the shape,
accepted as the first target.** Implementing MQTT only inside
the POSIX file would contradict ADR-014 point 5's placement
(MQTT stays in `net.c`'s layer, above the socket API) and would
strand the Nano/Edge profiles, which are the product. But the
first *runnable* implementation target is the host build,
because the POSIX socket mapping is the one working transport
today; the codec is written profile-agnostic from the first
slice and merely exercised first where sockets work.

## 4. Design shape

- **One codec module** beside the stubs it replaces
  (`net/src/`, exact file split is the first slice's call):
  pure packet encode/decode functions (buffer in, buffer out,
  no I/O) separated from the handle state machine that does I/O
  through the `eos_net` API. The pure half is unit-testable
  without any socket, real or mock.
- **Handles are indices into a small static table** of client
  contexts (config copy, socket, connection state, packet-id
  counter, subscription list with callbacks, one pending QoS 1
  retransmit slot per in-flight publish). No heap requirement is
  introduced beyond what the socket layer already has; the table
  size is a config constant, named in the slice that sets it.
- **`eos_mqtt_loop` is the engine.** It recv's with the caller's
  timeout, decodes at most one packet per call (bounded work —
  an MCU caller must be able to budget it), dispatches inbound
  PUBLISH to the matching subscription callback, answers
  inbound QoS 1 PUBLISH with PUBACK, completes pending PUBACK /
  SUBACK / UNSUBACK bookkeeping, and drives the keep-alive
  timer (PINGREQ when the line has been idle past
  `keepalive_sec`, disconnect on a missed PINGRESP window).
- **Errors stay in the declared vocabulary:** `-1` / 
  `EOS_MQTT_INVALID`, with CONNACK refusal codes mapped to a
  failed connect (handle never becomes valid). No new public
  error type is added to `net.h`.

## 5. Codec scope — MQTT 3.1.1, QoS 0/1 first

Packet set (OASIS MQTT 3.1.1 control-packet table; values are
the packet-type nibble):

| Packet | Value | Direction | In scope |
| --- | ---: | --- | --- |
| CONNECT | 1 | C → S | encode |
| CONNACK | 2 | S → C | decode |
| PUBLISH | 3 | both | encode + decode (QoS 0/1) |
| PUBACK | 4 | both | encode + decode |
| SUBSCRIBE / SUBACK | 8 / 9 | C → S / S → C | encode / decode |
| UNSUBSCRIBE / UNSUBACK | 10 / 11 | C → S / S → C | encode / decode |
| PINGREQ / PINGRESP | 12 / 13 | C → S / S → C | encode / decode |
| DISCONNECT | 14 | C → S | encode |
| PUBREC / PUBREL / PUBCOMP | 5 / 6 / 7 | QoS 2 only | **out of scope** |

TLS / MQTTS transport security is explicitly deferred to secure-socket
adoption (ADR-014 / crypto and security services), and is out of scope for
Issue #259 and Slice 3.

QoS semantics: QoS 0 is fire-and-forget (no packet identifier,
no retry). QoS 1 adds a packet identifier, a PUBACK, storage
until acknowledged, and retransmission with DUP=1 — duplicates
are possible and the callback contract tolerates them.
Shared framing every packet needs: fixed header, Remaining
Length variable-byte integer (1–4 bytes), UTF-8 length-prefixed
strings, a packet-identifier allocator, and the keep-alive
timer. An inbound QoS 2 packet is a protocol error for this
client: `eos_mqtt_loop` fails the connection rather than
silently parsing it as QoS 1 — the behaviour is named here so
the slice implements it deliberately. MQTT 5 is out of scope;
the declared API has no 5.0 surface (no properties, no reason
codes) and adding one is a header decision for later.

## 6. Test strategy — no real broker

The sandbox and CI have no MQTT broker, and the design must not
need one (the eNet mcp2mqtt trial is blocked on exactly that
absence; this track is designed around it):

- **Fixture tests for the pure codec:** one byte-exact fixture
  per packet in §5, seeded from the packets `test_net_mock.c`
  already pins (CONNECT, PUBLISH QoS 0, CONNACK), extended with
  malformed cases: bad Remaining Length encodings, truncated
  strings, CONNACK refusal codes, an unexpected QoS 2 packet.
- **Fake-broker tests for the state machine:** an in-process
  peer over the mock-socket idiom the tree already uses (or a
  loopback socket pair on the host build) that plays back
  CONNACK / PUBACK / SUBACK / PINGRESP and scripted inbound
  PUBLISH packets, asserting the client's bytes and callback
  deliveries. This exercises connect → publish → subscribe →
  loop → disconnect end to end with no external service.
- **Stub-assertion turnover:** as each slice lands, the
  `test_net_integration.c` assertions that pin stub behaviour
  for the implemented functions are replaced by the fake-broker
  expectations in the same commit — never left asserting the
  old failure as if it were the contract.

## 7. Slice sequence (each a day-sized commit, Refs #199)

1. **Codec core + fixtures:** pure encode/decode for the §5
   packet set with byte-exact fixture tests. No behaviour
   change to the stubs yet.
2. **Connect / publish on the host path:** state machine +
   handle table; `eos_mqtt_connect`, `eos_mqtt_disconnect`,
   `eos_mqtt_publish` live over the POSIX profile; fake-broker
   tests; the integration assertions for these three turn over.
   *As landed:* slice 2 shipped connect and disconnect only. The
   handle table, the DISCONNECTED → CONNECTING → CONNECTED →
   DISCONNECTING states, and fake-broker tests are in
   `tests/test_net.c`, which CI runs. `test_net_integration.c`
   is still unregistered (see `tests/CMakeLists.txt`). Publish
   moved to slice 3. Until slice 3 lands, nothing sends PINGREQ,
   so a broker drops an idle connection after 1.5 ×
   `keepalive_sec` (MQTT-3.1.2-24).
3. **Publish / subscribe / loop:** `eos_mqtt_publish` (moved
   here from slice 2), subscriptions + callback dispatch,
   `eos_mqtt_subscribe` / `eos_mqtt_unsubscribe` /
   `eos_mqtt_loop`, keep-alive. The remaining stub assertions
   turn over.
   *As landed (Issue #259):* slice 3 implements `eos_mqtt_publish`,
   `eos_mqtt_subscribe`, `eos_mqtt_unsubscribe`, and
   `eos_mqtt_loop`, along with keep-alive, callback dispatch,
   QoS 0 and QoS 1 with DUP=1 retry, and fake-broker verification
   in `tests/test_net.c`.
4. **Freestanding-profile verification:** the same codec built
   and fixture-tested in a no-POSIX configuration (lwIP profile
   when it exists; until then, a build-level verification that
   the codec depends only on the `net.h` socket API), plus the
   ebuild connector re-pointed at the real client or explicitly
   scheduled — the connector's parameter shape already matches
   the config struct, so this is integration, not redesign.

#199 closes when slice 3 lands and the declared surface behaves
as documented on the host path; slice 4 may Refs it after close
if it slips.

## 8. Layer statement

This is **API-layer** work: the C contract in `net.h` is the
surface ROS 2 bridges, MQTT brokers and third-party
applications integrate against without an agent. No MCP tool
surface is designed here — per the org rule, MCP wraps the API
only after the API works, and a network client on a constrained
device may never need one. The next API-layer scoping after
this one is micro-ROS (an EoS device as a native ROS 2 node,
building on eNI's existing rclc package) — sequenced after, not
bundled into, this track.

## 9. Modular-adoption check

Could someone lift just this client into their own
implementation? By construction, yes: the codec depends on the
`net.h` socket API and the declared config struct, nothing else
in eos — no kernel, scheduler or filesystem coupling. The one
honest coupling to name: it inherits the socket API's own
profile split (POSIX today, lwIP pending), so a lifter takes
`net.h`'s socket contract with it. That contract is the API
layer working as intended, not hidden coupling.

## 10. Slice 3 Protocol Specification & Implementation Decisions

With the completion of Slice 3 (Issue #259), the following protocol behaviors and implementation decisions are codified:

1. **Packet Identifier (PID) Lifecycle and Wraparound:**
   - Packet identifiers for QoS 1 PUBLISH, SUBSCRIBE, and UNSUBSCRIBE are allocated monotonically starting at 1.
   - PIDs wrap from `0xFFFF` back to 1 (PID 0 is reserved and invalid per MQTT 3.1.1 §2.3.1).
   - Only one QoS 1 publication or acknowledgement wait is in flight per client instance at any given time, avoiding PID collisions.

2. **Keep-Alive Configuration and Disabled State (`keepalive_sec == 0`):**
   - When `keepalive_sec == 0`, keep-alive monitoring is disabled completely; no PINGREQ packets are transmitted, and no PINGRESP deadlines are enforced.

3. **Idle Definition and PINGREQ Generation:**
   - Outbound traffic activity (`last_send_sec`) is refreshed on every transmitted control packet (PUBLISH, SUBSCRIBE, UNSUBSCRIBE, and PINGREQ).
   - If no control packet has been sent for `keepalive_sec` seconds, `eos_mqtt_loop()` or synchronous wait loops emit a PINGREQ.

4. **PINGRESP Deadline Policy and Window:**
   - Upon sending PINGREQ, the client sets `ping_pending = true` and records `ping_sent_ms`.
   - The broker must respond with PINGRESP within `keepalive_sec * 1000` milliseconds.
   - To prevent false disconnects from network jitter or packet buffering, input is drained and processed before evaluating deadline expiration. If `keepalive_sec` elapses with no PINGRESP, the session is invalidated fail-closed and released.

5. **TCP Framing, Fragmentation, and RX Buffer Sizing:**
   - Inbound stream data is framed incrementally using MQTT 3.1.1 fixed-header parsing: the packet type byte is read first, followed by the variable-length Remaining Length multiplier (up to 4 bytes).
   - Bounded static buffers (1024 bytes per client) hold inbound packets without heap allocation.
   - Chunked / fragmented TCP data is accumulated using deadline-bounded reads (`mqtt_recv_exact_deadline`), where the caller's timeout budget is tracked monotonically so partial packets cannot stall indefinitely.

6. **Maximum Inbound Limits:**
   - Inbound control packets larger than the 1024-byte static buffer are rejected and trigger connection teardown.
   - Inbound PUBLISH topics must not exceed 127 bytes (`EOS_MQTT_MAX_TOPIC_LEN - 1` with null terminator; buffer size is 128 bytes). If an inbound topic exceeds 127 bytes, the packet is rejected and no PUBACK is returned.

7. **Callback Lifetime, Immutability, and Reentrancy Contract:**
   - `eos_mqtt_msg_callback_t` receives pointers to `topic` (null-terminated string) and `payload` (byte buffer of length `payload_len`).
   - The memory backed by these pointers is valid **only** during the synchronous execution of the callback. Callbacks needing data asynchronously must copy topic and payload.
   - **Reentrancy and Call Restrictions:**
     - *Same-Client Reentrancy:* While an application callback is executing (`c->in_callback == true`), all synchronous operations on that *same* client instance (`eos_mqtt_publish`, `eos_mqtt_subscribe`, `eos_mqtt_unsubscribe`, `eos_mqtt_disconnect`, `eos_mqtt_loop`) are strictly prohibited and return -1 immediately.
     - *Cross-Client Invocations:* Operations on an independent client handle (e.g. bridging/gatewaying between client A and client B) while client A is executing a callback are permitted.
     - *Connect Restrictions:* `eos_mqtt_connect()` mutates the global client handle pool and is rejected with `EOS_MQTT_INVALID` if any client is currently dispatching a callback.

8. **QoS 1 Retransmission and Acknowledgement Policy:**
   - Outgoing QoS 1 PUBLISH packets wait for PUBACK up to the specified timeout.
   - If PUBACK is delayed beyond `timeout_ms / 2`, a single retry is attempted with `DUP = 1` using the identical packet identifier.
   - If PUBACK does not arrive before the total timeout expires, the session is invalidated fail-closed and released.
   - If a queued PUBACK, SUBACK, or UNSUBACK arrives while keep-alive input is draining before a PINGRESP, it is matched and routed directly to the active waiter rather than being lost.

9. **Inbound Retained Messages Arriving Before SUBACK:**
   - When `eos_mqtt_subscribe()` is called, the subscription entry and its callback are staged in the client subscription table *before* transmitting SUBSCRIBE.
   - Inbound PUBLISH packets (e.g. broker-retained messages) received while awaiting SUBACK are dispatched immediately to matching callbacks.
   - If the SUBACK returns an error code (0x80) or an invalid granted QoS, or if the wait times out, the staged subscription is rolled back and the session is invalidated.

10. **Session State Transitions, Reconnect, and Last Will Contract:**
    - Any unrecoverable network error, peer disconnect (`EOS_NET_EOF`), acknowledgement timeout, or malformed packet causes an immediate transition to `MQTT_DISCONNECTED`.
    - Sockets are closed via `eos_net_close()`, in-flight state and subscriptions are cleared, and the client handle slot is released back to the pool.
    - **Reconnect Policy:** Automatic reconnect is not provided. When an error or disconnect occurs, the application must explicitly call `eos_mqtt_connect()` to establish a fresh session.
    - **Clean Session:** `CleanSession = 1` is enforced on all connections; persistent sessions across disconnects are out of scope.
    - **Last Will and Testament (LWT):** LWT is unsupported in Slice 3; Will flags in CONNECT fixed/variable headers are set to 0.

11. **Static Resource Constraints:**
    - Concurrency and resource allocations are strictly bounded at compile-time:
      - `EOS_MQTT_MAX_CLIENTS`: 2 simultaneous client handles.
      - `EOS_MQTT_MAX_SUBSCRIPTIONS`: 8 active subscriptions per client.
      - Static buffers for RX (1024 B), TX (1024 B), and topic matching.
      - Zero dynamic memory allocation (`malloc`/`free`) throughout the entire MQTT implementation.
