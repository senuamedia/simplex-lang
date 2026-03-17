# Nexus Protocol

**High-Performance Hive-to-Hive Communication**

Nexus is a protocol optimized for constant belief synchronization between cognitive hives, achieving **237x compression** over naive approaches through bit-packed delta streams.

## Performance

| Metric | Naive | Nexus | Improvement |
|--------|-------|-------|-------------|
| Bytes per belief | 90 | 0.38 | **237x** |
| 1000 beliefs sync | 90 KB | 378 bytes | **244x** |
| Encode time (1000) | ~500us | 7us | **71x** |
| Decode time (1000) | ~500us | 5us | **100x** |

## Quick Start

```simplex
use nexus::{NexusSession, BitPackedBeliefSync};

fn main() {
    // Connect to peer hive
    let session = NexusSession::connect("wss://peer-hive.example.com");

    // Create belief sync with delta encoding
    let sync = BitPackedBeliefSync::new();
    sync.add_belief("code_quality", 0.92);
    sync.add_belief("security_verified", 0.88);
    sync.add_belief("performance_ok", 0.95);

    // Transmit - only changes are sent
    session.sync_beliefs(sync);  // ~1.14 bytes for 3 beliefs

    // Receive updates from peer
    let updates = session.receive_updates();
    for belief in updates {
        println(f"Peer update: {belief.key} = {belief.confidence}");
    }
}
```

## Core Innovation: Bit-Packed Delta Streams

Instead of sending full belief states, Nexus uses variable-length encoding:

```
Update Stream (bit-packed):
  Op (2 bits)   Meaning              Additional Bits
  00 = SAME     No change            0 (just 2 bits)
  01 = DELTA_S  Small delta (±64)    +7 bits (9 total)
  10 = DELTA_L  Large delta (±32K)   +16 bits (18 total)
  11 = FULL     Complete value       +32 bits (34 total)
```

For typical belief updates (small confidence changes), most beliefs use SAME (2 bits) or DELTA_S (9 bits), achieving massive compression.

## Architecture

### Protocol Modules (28 total)

```
simplex-nexus/src/
├── Core Protocol
│   ├── bits.sx           # Bit manipulation primitives
│   ├── frame.sx          # Wire format framing
│   ├── message.sx        # Message types and encoding
│   └── types.sx          # Core type definitions
│
├── Synchronization
│   ├── sync.sx           # Belief synchronization
│   ├── stf.sx            # State transfer format
│   ├── crdt.sx           # Conflict-free replicated data types
│   └── vector_clock.sx   # Causality tracking
│
├── Compression
│   ├── dual.sx           # Dual number trajectories
│   ├── neural.sx         # Neural compression codebook
│   └── prediction.sx     # Predictive encoding
│
├── Transport
│   ├── transport.sx      # Transport abstraction
│   ├── tcp_transport.sx  # TCP with optional TLS
│   ├── conn.sx           # Connection management
│   └── conn_pool.sx      # Connection pooling
│
├── Flow Control
│   ├── flow_control.sx   # Backpressure management
│   ├── multiplex.sx      # Channel multiplexing
│   └── reconnect.sx      # Automatic reconnection
│
├── Security
│   ├── security.sx       # Authentication and signing
│   ├── secure_conn.sx    # Secure connection wrapper
│   └── secure_frame.sx   # Encrypted frame format
│
├── Coordination
│   ├── session.sx        # Session management
│   ├── federation.sx     # Multi-hive federation
│   ├── coordinator.sx    # Cluster coordination
│   └── address.sx        # Hive addressing
│
└── Advanced
    ├── trajectory.sx     # Belief trajectory prediction
    ├── request_response.sx # RPC pattern
    └── lib.sx            # Public API
```

## Transport Bindings

| Transport | Use Case | Latency | Reliability |
|-----------|----------|---------|-------------|
| TCP | General purpose | Medium | High |
| UDP | Low-latency sync | Low | Best-effort |
| WebSocket | Browser/edge hives | Medium | High |
| Shared Memory | Same-machine | Ultra-low | Guaranteed |

```simplex
use nexus::transport::{TcpTransport, UdpTransport, WsTransport};

// TCP for reliable sync
let tcp = TcpTransport::connect("hive1.example.com:9000");

// UDP for real-time belief streaming
let udp = UdpTransport::bind("0.0.0.0:9001");

// WebSocket for edge hives
let ws = WsTransport::connect("wss://edge.example.com/nexus");
```

## Features

### Dual Number Trajectories

Track not just beliefs, but their rate of change for predictive sync:

```simplex
use nexus::dual::BeliefTrajectory;

let trajectory = BeliefTrajectory::new();
trajectory.observe(0.80, timestamp_0);
trajectory.observe(0.85, timestamp_1);
trajectory.observe(0.88, timestamp_2);

// Predict future value
let predicted = trajectory.predict(timestamp_3);  // ~0.91
```

### Neural Compression

For cognitive payloads (embeddings, activations), use learned codebooks:

```simplex
use nexus::neural::NeuralCodebook;

let codebook = NeuralCodebook::load("cognitive-v1");
let compressed = codebook.encode(embedding);  // 768 floats -> 32 bytes
```

### CRDT-Based Conflict Resolution

Automatic conflict resolution for concurrent updates:

```simplex
use nexus::crdt::{LWWRegister, GCounter, ORSet};

// Last-writer-wins for beliefs
let belief = LWWRegister::new("confidence", 0.85);

// Grow-only counter for event counts
let events = GCounter::new();
events.increment();

// Observed-remove set for tags
let tags = ORSet::new();
tags.add("verified");
```

### Federation

Connect multiple hives through a federation layer:

```simplex
use nexus::federation::{Federation, HiveId};

let federation = Federation::new();
federation.join("wss://hub.example.com");

// Broadcast to all federated hives
federation.broadcast(sync);

// Target specific hive
federation.send_to(HiveId::from("hive-alpha"), sync);
```

## Security

- **TLS 1.3** mandatory for network connections
- **HMAC-SHA256** message authentication
- **Replay protection** via timestamp validation
- **Secure handshake** with hive identity verification

```simplex
use nexus::security::{SecureSession, HiveCredentials};

let creds = HiveCredentials::load("./hive-key.pem");
let session = SecureSession::connect("wss://peer.example.com", creds);
```

## Configuration

```toml
# nexus.toml
[connection]
max_connections = 100
connection_timeout_ms = 5000
keepalive_interval_ms = 30000

[sync]
batch_size = 1000
max_delta_age_ms = 60000
compression_level = "adaptive"

[security]
require_tls = true
verify_peer = true
message_ttl_secs = 300
```

## Tests

```bash
# Run all nexus tests
./simplex-nexus/run_tests.sh

# Run specific test
sxc run simplex-nexus/tests/test_bits.sx
sxc run simplex-nexus/tests/test_sync.sx
```

## License

AGPL-3.0-or-later WITH Simplex-Runtime-Exception

## Credits

Nexus Protocol designed and implemented by Rod Higgins ([@senuamedia](https://github.com/senuamedia)).

---

*"Thoughts synchronized at the speed of light."*
