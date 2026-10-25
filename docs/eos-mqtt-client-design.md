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
3. **Subscribe / loop:** subscriptions + callback dispatch,
   `eos_mqtt_subscribe` / `eos_mqtt_unsubscribe` /
   `eos_mqtt_loop`, keep-alive. The remaining stub assertions
   turn over.
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
