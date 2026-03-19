# TASK-025: Real-Time Communication Libraries

**Status**: Planning
**Priority**: High
**Created**: 2026-03-16
**Target Version**: 0.16.0
**Depends On**: TASK-022 Phase 8 (HTTP client), TASK-022 Phase 9 (async/await)

---

## Overview

Real-time communication protocols for streaming, messaging, and service-to-service communication. These complement Simplex's actor model and cognitive hive architecture — actors need efficient ways to communicate beyond local message passing.

All implementations must be **pure Simplex** with C runtime extensions only for socket-level operations already provided by the runtime.

---

## Library 1: simplex-websocket

**Location**: `simplex-std/src/websocket.sx`
**Priority**: Critical — real-time streaming, SLM inference streaming, live dashboards

### Core API
```simplex
// Client
fn ws_connect(url: String, headers: Vec<String>) -> Result<WsConnection, WsError>
fn ws_send(conn: WsConnection, message: WsMessage) -> Result<bool, WsError>
fn ws_receive(conn: WsConnection) -> Result<WsMessage, WsError>
fn ws_close(conn: WsConnection, code: i64, reason: String)

// Server (actor-based)
fn ws_serve(addr: String, port: i64, handler: fn(WsConnection)) -> Result<WsServer, WsError>

enum WsMessage {
    Text(String),
    Binary(Vec<i64>),
    Ping(Vec<i64>),
    Pong(Vec<i64>),
    Close(i64, String)
}
```

### Wire Protocol
- RFC 6455 WebSocket protocol
- HTTP upgrade handshake (GET -> 101 Switching Protocols)
- Frame parsing (opcode, mask, payload length, payload data)
- Fragmentation support (continuation frames)
- Control frames (ping/pong/close)
- Masking (client -> server frames must be masked per spec)

### Features
- Client and server modes
- Text and binary message types
- Automatic ping/pong keepalive
- Per-connection actor integration (each WebSocket connection maps to an actor)
- TLS/WSS support
- Backpressure handling (slow consumers)

### Cognitive Hive Integration
- **SLM streaming**: Stream inference tokens to clients in real-time via WebSocket
- **Hive observation**: Live dashboard showing specialist activity, belief updates, memory changes
- **Inter-hive communication**: WebSocket as transport for Nexus Protocol over WAN
- **Agent streaming**: Stream agent thought/action to monitoring UI

### Success Criteria
- Client connects to standard WebSocket echo server
- Server accepts multiple concurrent connections
- Text and binary messages round-trip correctly
- Ping/pong keepalive maintains connection
- Clean close handshake with status codes
- New test: `tests/stdlib/spec_websocket.sx`

---

## Library 2: simplex-grpc

**Location**: `simplex-grpc/src/mod.sx`
**Priority**: Medium — microservice communication standard

### Core API
```simplex
// Service definition (parsed from .proto or defined in Simplex)
struct GrpcService {
    name: String,
    methods: Vec<GrpcMethod>
}

struct GrpcMethod {
    name: String,
    input_type: String,
    output_type: String,
    client_streaming: bool,
    server_streaming: bool
}

// Client
fn grpc_connect(host: String, port: i64) -> Result<GrpcChannel, GrpcError>
fn grpc_call(channel: GrpcChannel, method: String, request: Vec<i64>) -> Result<Vec<i64>, GrpcError>
fn grpc_stream(channel: GrpcChannel, method: String) -> Result<GrpcStream, GrpcError>

// Server
fn grpc_serve(addr: String, port: i64, service: GrpcService, handlers: Vec<GrpcHandler>) -> Result<GrpcServer, GrpcError>
```

### Wire Protocol
- HTTP/2 based transport (requires HTTP/2 framing implementation)
- Protocol Buffers serialization (see simplex-protobuf below)
- gRPC framing (length-prefixed messages over HTTP/2 streams)
- Status codes and error metadata

### Features
- Unary RPC (request-response)
- Server streaming RPC
- Client streaming RPC
- Bidirectional streaming RPC
- Metadata (headers/trailers)
- Deadline/timeout propagation
- TLS transport security

### Success Criteria
- Unary RPC call to a standard gRPC server
- Server streaming receives all messages
- Bidirectional streaming works correctly
- Deadline cancellation fires
- New tests: `tests/stdlib/spec_grpc.sx`

---

## Library 3: simplex-protobuf

**Location**: `simplex-protobuf/src/mod.sx`
**Priority**: Medium — serialization for gRPC and efficient wire formats

### Core API
```simplex
// Encode/decode Protocol Buffer messages
fn proto_encode(schema: ProtoSchema, values: Vec<ProtoField>) -> Vec<i64>
fn proto_decode(schema: ProtoSchema, data: Vec<i64>) -> Result<Vec<ProtoField>, ProtoError>

struct ProtoSchema {
    fields: Vec<ProtoFieldDef>
}

struct ProtoFieldDef {
    number: i64,
    name: String,
    field_type: ProtoType,
    repeated: bool
}

enum ProtoType {
    Int32, Int64, UInt32, UInt64,
    Float, Double,
    Bool, String, Bytes,
    Message(ProtoSchema)
}
```

### Features
- Proto3 wire format encoding/decoding
- Varint encoding for integers
- Length-delimited fields (strings, bytes, embedded messages)
- Repeated fields
- Schema-driven (no code generation required — schemas defined in Simplex)
- Optional: `.proto` file parser for interop with existing protobuf ecosystems

### Success Criteria
- Encode/decode round-trip for all primitive types
- Nested message encoding
- Repeated field handling
- Compatible with messages from other protobuf implementations
- New test: `tests/stdlib/spec_protobuf.sx`

---

## Library 4: simplex-nats

**Location**: `simplex-nats/src/mod.sx`
**Priority**: Medium — lightweight messaging, good fit for actor systems

### Core API
```simplex
fn nats_connect(url: String) -> Result<NatsConnection, NatsError>
fn nats_publish(conn: NatsConnection, subject: String, payload: String) -> Result<bool, NatsError>
fn nats_subscribe(conn: NatsConnection, subject: String) -> Result<NatsSubscription, NatsError>
fn nats_request(conn: NatsConnection, subject: String, payload: String, timeout_ms: i64) -> Result<NatsMessage, NatsError>
fn nats_close(conn: NatsConnection)
```

### Wire Protocol
- NATS text protocol (simple, line-based — ideal for pure Simplex)
- `CONNECT`, `PUB`, `SUB`, `UNSUB`, `MSG`, `PING`, `PONG`

### Features
- Publish/subscribe messaging
- Request/reply pattern
- Subject wildcards (`*`, `>`)
- Queue groups (load balancing across subscribers)
- JetStream support (persistent messages, replay) — stretch goal

### Cognitive Hive Integration
- **Inter-hive messaging**: Lightweight pub/sub for belief propagation between hives
- **Actor location transparency**: NATS subjects as actor addresses
- **Event sourcing**: JetStream for hive event persistence

### Success Criteria
- Pub/sub message delivery
- Request/reply with timeout
- Wildcard subjects route correctly
- Queue group distributes messages
- New test: `tests/stdlib/spec_nats.sx`

---

## Dependency Graph

```
TASK-022 Phase 8 (HTTP) + Phase 9 (Async)
    |
    +--> simplex-websocket (HTTP upgrade + TCP frames)
    |
    +--> simplex-protobuf (independent, binary format)
    |         |
    |         v
    +--> simplex-grpc (depends on protobuf + HTTP/2)
    |
    +--> simplex-nats (independent, text protocol over TCP)
```

Build order: websocket + protobuf + nats in parallel, then grpc after protobuf.

---

## Estimated Line Counts

| Library | Est. Lines |
|---------|-----------|
| simplex-websocket | ~1,200-1,600 |
| simplex-grpc | ~2,000-2,800 |
| simplex-protobuf | ~1,000-1,400 |
| simplex-nats | ~800-1,000 |
| **Total** | **~5,000-6,800** |
