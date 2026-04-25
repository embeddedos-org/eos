# Chapter 19: eIPC — Embedded Inter-Process Communication

*Author: Srikanth Patchava & EmbeddedOS Contributors*

---

## 19.1 Introduction

As embedded systems grow in complexity, the need for structured communication
between software components becomes critical. The **eIPC** (Embedded
Inter-Process Communication) framework provides secure, real-time IPC for
the EoS ecosystem — specifically designed as the communication bridge between
**eNI** (Neural Interface) and **eAI** (AI Layer).

```
ENI ══▶ EIPC ══▶ EAI
```

eIPC is a standalone, cross-platform, security-enhanced IPC framework written
in pure Go with zero external dependencies.

---

## 19.2 Architecture

eIPC follows a layered architecture with clean separation of concerns:

```
┌──────────────────────────────────────────────┐
│                 Application                   │
├─────────────┬─────────────┬──────────────────┤
│  core/      │  services/  │  security/       │
│  Message    │  Broker     │  Authenticator   │
│  Router     │  Registry   │  Capability      │
│  Endpoint   │  Policy     │  HMAC Integrity  │
│             │  Audit      │  ReplayTracker   │
│             │  Health     │  Keyring         │
├─────────────┴─────────────┴──────────────────┤
│               protocol/                       │
│  Frame encoding, versioning, compat           │
├──────────────────────────────────────────────┤
│               transport/                      │
│  TCP │ Unix Socket │ Named Pipe │ Shared Mem  │
└──────────────────────────────────────────────┘
```

### Core Components

| Component      | Responsibility                                         |
|----------------|--------------------------------------------------------|
| **Message**    | Typed message structure with headers and payload       |
| **Router**     | Routes messages between endpoints by topic/pattern     |
| **Endpoint**   | Named connection point for sending/receiving messages  |
| **Broker**     | Central message broker with pub/sub and request/reply  |

### Security Components

| Component           | Responsibility                                   |
|---------------------|--------------------------------------------------|
| **Authenticator**   | Peer authentication using shared secrets         |
| **Capability**      | Action-level authorization (safe/controlled/restricted) |
| **HMAC Integrity**  | Message integrity verification using HMAC-SHA256 |
| **ReplayTracker**   | Prevents message replay attacks with nonces      |
| **Keyring**         | Secure key storage and rotation                  |

---

## 19.3 Key Features

- **Real-time capable** — bounded queues, priority lanes, timeout-aware delivery
- **Security-enhanced** — peer auth, capability authorization, HMAC integrity, replay protection
- **Cross-platform** — Linux (amd64/arm64/armv7), macOS (amd64/arm64), Windows (amd64/arm64)
- **Pluggable transports** — TCP, Unix domain sockets, Windows named pipes, shared memory
- **Auditable** — JSON-line audit logging with full request tracing
- **Policy engine** — three-tier action classification
- **Zero external dependencies** — pure Go standard library
- **LTS-friendly** — versioned protocol with compatibility guarantees

---

## 19.4 Transport Layer

eIPC supports multiple transport backends, selected based on deployment
requirements:

| Transport          | Use Case                  | Latency | Security |
|--------------------|---------------------------|---------|----------|
| **TCP**            | Cross-machine, networked  | Medium  | TLS-ready|
| **Unix Socket**    | Same-machine, Linux/macOS | Low     | File ACL |
| **Named Pipe**     | Same-machine, Windows     | Low     | ACL      |
| **Shared Memory**  | Ultra-low-latency, local  | Lowest  | Process  |

Transport selection is transparent to application code — the same message
API works across all transports.

---

## 19.5 The Policy Engine

eIPC classifies all actions into three tiers:

```
┌─────────────────────────────────────────────┐
│  Tier 1: SAFE                               │
│  Read-only queries, health checks, status   │
│  → No authorization required                │
├─────────────────────────────────────────────┤
│  Tier 2: CONTROLLED                         │
│  Configuration changes, model switching     │
│  → Requires authenticated peer              │
├─────────────────────────────────────────────┤
│  Tier 3: RESTRICTED                         │
│  System shutdown, security config changes   │
│  → Requires admin capability + audit log    │
└─────────────────────────────────────────────┘
```

This classification is enforced at the broker level before message routing.

---

## 19.6 Server Implementation

The eIPC server (`cmd/eipc-server/main.go`) demonstrates the complete
framework in action. It initializes all components and serves as the IPC
bridge between eNI and eAI.

### Initialization Flow

```go
func main() {
    // 1. Load configuration
    cfg := config.Load()

    // 2. Initialize security services
    authenticator := auth.NewAuthenticator(cfg.Security)
    capabilities := capability.NewManager(cfg.Capabilities)

    // 3. Initialize transport
    listener := tcp.NewListener(cfg.ListenAddr)

    // 4. Initialize services
    broker := core.NewBroker()
    registry := registry.NewRegistry()
    auditor := audit.NewAuditor(cfg.AuditLog)
    healthSvc := health.NewService()

    // 5. Start serving
    server := NewServer(listener, broker, authenticator, capabilities, auditor)
    server.Serve(ctx)
}
```

### EAI Backend Integration

The server acts as a proxy between eNI clients and the eAI backend. When
a chat or completion request arrives via eIPC, the server forwards it to the
local eAI HTTP endpoint:

```go
const eaiBackendURL = "http://127.0.0.1:8090"

func forwardToEAI(ctx context.Context, endpoint string,
                  payload interface{}) (io.ReadCloser, string, error) {
    body, _ := json.Marshal(payload)
    req, _ := http.NewRequestWithContext(ctx, http.MethodPost,
        eaiBackendURL+endpoint, bytes.NewReader(body))
    req.Header.Set("Content-Type", "application/json")
    resp, err := http.DefaultClient.Do(req)
    // ...
    return resp.Body, resp.Header.Get("Content-Type"), nil
}
```

### Message Types

| Endpoint       | Direction    | Description                     |
|----------------|--------------|---------------------------------|
| `/v1/chat`     | eNI → eAI   | Conversational AI requests      |
| `/v1/complete` | eNI → eAI   | Text completion requests        |
| `/v1/health`   | Any → Server | Health check (safe tier)        |
| `/v1/status`   | Any → Server | System status and metrics       |

---

## 19.7 Protocol Format

eIPC uses a binary frame protocol for efficient wire encoding:

```
┌─────────┬─────────┬─────────┬──────────┬─────────┐
│ Magic   │ Version │ Length  │ Headers  │ Payload │
│ 4 bytes │ 2 bytes │ 4 bytes │ variable │ variable│
└─────────┴─────────┴─────────┴──────────┴─────────┘
```

### Protocol Versioning

The protocol includes a version field for forward compatibility. The server
negotiates the highest mutually supported version during the handshake:

- **v1.0** — Initial protocol with basic message routing
- **v1.1** — Added HMAC integrity and replay protection
- **v1.2** — Added priority lanes and bounded queues

---

## 19.8 Security Model

### Authentication

Peers authenticate using HMAC-based challenge-response:

1. Client connects and sends a hello message
2. Server responds with a random challenge nonce
3. Client computes HMAC-SHA256(challenge, shared_secret) and sends it
4. Server verifies and grants a session token

### Authorization

After authentication, every message is checked against the capability
manager:

```go
type CapabilityManager interface {
    Check(peer PeerID, action Action) (bool, error)
    Grant(peer PeerID, capability Capability) error
    Revoke(peer PeerID, capability Capability) error
}
```

### Audit Trail

All controlled and restricted actions are logged to a JSON-line audit file:

```json
{"ts":"2026-04-15T10:30:00Z","peer":"eni-001","action":"model.switch",
 "tier":"controlled","result":"allowed","latency_ms":2}
```

---

## 19.9 Health Monitoring

eIPC includes a built-in health service that tracks:

- **Transport health** — connection count, bytes transferred, errors
- **Broker health** — message queue depth, routing latency
- **Peer health** — connected peers, authentication status
- **Backend health** — eAI endpoint availability and response times

The health endpoint is always classified as "safe" tier — no authentication
required. This enables monitoring systems to check eIPC health without
credentials.

---

## 19.10 Deployment Patterns

### Pattern 1: Local Bridge (Most Common)

eNI and eAI run on the same device, communicating via Unix socket:

```
┌──────────┐  Unix Socket  ┌──────────┐  HTTP  ┌──────────┐
│   eNI    │──────────────►│  eIPC    │───────►│   eAI    │
│  Neural  │               │  Server  │        │  Inference│
└──────────┘               └──────────┘        └──────────┘
```

### Pattern 2: Distributed

eNI runs on an edge sensor, eAI runs on a gateway:

```
┌──────────┐     TCP      ┌──────────┐  Local  ┌──────────┐
│   eNI    │─────────────►│  eIPC    │────────►│   eAI    │
│  Sensor  │   Network    │  Gateway │         │  Server  │
└──────────┘              └──────────┘         └──────────┘
```

---

## 19.11 Building and Testing

```bash
git clone https://github.com/embeddedos-org/eipc.git
cd eipc

# Build
go build ./...

# Run tests
go test ./... -v -race

# Run the server
go run cmd/eipc-server/main.go --config config.yaml

# Cross-compile for ARM
GOOS=linux GOARCH=arm64 go build -o eipc-server-arm64 cmd/eipc-server/main.go
```

---

## 19.12 Summary

eIPC provides the secure communication backbone for the EoS AI stack. By
combining real-time message routing with layered security (authentication,
authorization, integrity, and audit), eIPC ensures that neural interface
data flows safely to the AI inference engine.

**Key takeaways:**

- Purpose-built IPC bridge between eNI and eAI
- Three-tier policy engine (safe / controlled / restricted)
- Four transport backends (TCP, Unix, Named Pipe, Shared Memory)
- HMAC-SHA256 integrity with replay protection
- Pure Go, zero external dependencies
- Cross-platform: Linux, macOS, Windows (amd64/arm64/armv7)

---

*Next: Chapter 20 — eAI Embedded AI Layer*
