# Edge Hive

A lightweight, autonomous cognitive hive designed to run on user devices from smartwatches to desktops.

## Overview

The Edge Hive provides local intelligence, user advocacy, and offline capability. Unlike traditional "thin client" approaches where edge devices are merely dumb terminals, the Edge Hive is a living piece of the cognitive hive that operates with a local-first philosophy.

## Features

- **Local-First Intelligence**: All user data stored locally by default, processing on-device when possible
- **On-Device AI**: Local SLM inference with SmolLM, Qwen2, Phi-3, Llama 3.2, Gemma 2
- **User Advocacy**: Learns and represents user preferences, protects privacy by default
- **Adaptive Footprint**: Automatically adapts to device capabilities (watch to desktop)
- **Offline Operation**: First-class offline support with request queuing
- **Secure by Default**: Encryption at rest, TLS mandatory, user isolation

## Local Models (v0.9.0)

Edge Hive runs AI locally on your device - no cloud required for many queries:

### Supported Models

| Model | Size | Context | For Device |
|-------|------|---------|------------|
| SmolLM-135M | 135M | 2K | Watch |
| SmolLM-360M | 360M | 2K | Phone |
| SmolLM-1.7B | 1.7B | 2K | Tablet |
| Qwen2-0.5B | 500M | 32K | Phone |
| Phi-3-mini | 3.8B | 4K | Laptop |
| Llama-3.2-1B | 1B | 128K | Tablet |
| Llama-3.2-3B | 3B | 128K | Desktop |

### Automatic Selection

Models are automatically selected based on:
- Device class and available RAM
- Battery level (degrades to smaller models when low)
- Storage availability

### Quantization

All models use GGUF format with Q4_K_M quantization by default for optimal quality/size balance.

## Security (v0.9.0)

Edge Hive includes comprehensive security features:

### Encryption at Rest
- All persistent data encrypted with AES-256-GCM
- User passwords never stored; keys derived via PBKDF2 (100,000 iterations)
- Per-user data isolation with separate encrypted directories

### Network Security
- TLS 1.3 mandatory for all connections (ws:// rejected)
- HMAC-SHA256 message signing and authentication
- Replay attack protection with 5-minute timestamp window
- Session tokens with 24-hour expiry

### Additional Security
- Cryptographically secure device ID generation
- Secure memory wiping on shutdown
- User-controlled sync preferences

## Quick Start

### Secure Usage (Recommended)

```simplex
fn main() -> i64 {
    let username: i64 = string_from("alice");
    let password: i64 = string_from("secure_password_123");
    let cloud_endpoint: i64 = string_from("wss://hive.example.com");

    // Register new user or login returning user
    let hive: i64 = edge_hive_login(username, password, cloud_endpoint);
    if hive == 0 {
        hive = edge_hive_register(username, password, cloud_endpoint);
    }

    // Connect to cloud (TLS mandatory)
    edge_hive_connect(hive);

    // Set preferences (encrypted at rest)
    hive_set_preference(hive, string_from("theme"), 1);

    // Handle queries
    let query: i64 = vec_new();
    vec_push(query, string_from("What's on my calendar?"));
    let response: i64 = edge_hive_query(hive, query);

    // Secure shutdown (saves encrypted state, clears keys)
    hive_shutdown_secure(hive);

    0
}
```

### Device-Specific Creation

```simplex
// Automatically configures for device capabilities
let hive: i64 = edge_hive_for_phone_secure(username, password, endpoint);
let hive: i64 = edge_hive_for_tablet_secure(username, password, endpoint);
let hive: i64 = edge_hive_for_laptop_secure(username, password, endpoint);
let hive: i64 = edge_hive_for_desktop_secure(username, password, endpoint);
```

## API Reference

### Security API

```simplex
// Create user identity (new user)
fn user_identity_new(username: i64, password: i64) -> i64

// Login existing user
fn user_identity_login(username: i64, password: i64) -> i64

// Create secure hive with identity
fn edge_hive_new_secure(identity: i64, device_id: i64, cloud_endpoint: i64) -> i64

// Convenience: register and create hive
fn edge_hive_register(username: i64, password: i64, cloud_endpoint: i64) -> i64

// Convenience: login and create hive
fn edge_hive_login(username: i64, password: i64, cloud_endpoint: i64) -> i64

// Secure shutdown (saves encrypted state, clears keys)
fn hive_shutdown_secure(hive: i64) -> i64

// Generate cryptographically secure device ID
fn generate_secure_device_id() -> i64
```

### Hive Operations

```simplex
// Query the hive
fn edge_hive_query(hive: i64, query: i64) -> i64

// Execute a command
fn edge_hive_command(hive: i64, command: i64) -> i64

// Process a notification
fn edge_hive_notify(hive: i64, notification: i64) -> i64

// Sync beliefs with cloud
fn edge_hive_sync(hive: i64) -> i64

// Connect to cloud
fn edge_hive_connect(hive: i64) -> i64

// Check online status
fn edge_hive_is_online(hive: i64) -> i64

// Run maintenance cycle
fn hive_maintenance_cycle(hive: i64) -> i64
```

### Preferences

```simplex
fn hive_set_preference(hive: i64, key: i64, value: i64) -> i64
fn hive_get_preference(hive: i64, key: i64) -> i64
```

## Architecture

```
Edge Hive
├── Device Profile     - Detects and manages device capabilities
├── Belief Store       - Persistent storage with CRDT merge
├── Specialist System  - Domain-specific request handlers
├── Cognitive Loop     - Main processing cycle
├── Federation Manager - Cloud and peer synchronization
├── Security Layer     - Encryption, authentication, isolation
└── Network Layer      - WebSocket I/O with TLS
```

## Device Resource Limits

| Device  | Max Model    | Belief Store  | Cache Size |
|---------|--------------|---------------|------------|
| Watch   | 0            | 100 entries   | 1 MB       |
| Phone   | 500M params  | 10K entries   | 50 MB      |
| Tablet  | 1B params    | 50K entries   | 100 MB     |
| Laptop  | 7B params    | 100K entries  | 500 MB     |
| Desktop | 70B params   | 1M entries    | 1 GB       |

## Data Sensitivity Levels

| Level     | Description          | Sync Behavior          |
|-----------|----------------------|------------------------|
| Public    | Safe to share        | Sync freely            |
| Private   | User's devices only  | Encrypted sync         |
| Sensitive | Minimal sync         | End-to-end encrypted   |
| Local     | Never leaves device  | No sync                |

## File Structure

```
simplex-edge-hive/
├── src/
│   ├── runtime.sx      # Runtime function declarations
│   ├── types.sx        # Core type constants
│   ├── device.sx       # Device profile management
│   ├── beliefs.sx      # Belief store with CRDT
│   ├── specialist.sx   # Specialist system
│   ├── federation.sx   # Federation manager
│   ├── network.sx      # WebSocket I/O
│   ├── persistence.sx  # File-based storage
│   ├── security.sx     # User identity, encryption
│   ├── hive.sx         # Main cognitive loop
│   └── lib.sx          # Library entry point
└── tests/
    ├── test_types.sx     # Type tests
    ├── test_beliefs.sx   # Belief store tests
    ├── test_security.sx  # Security layer tests
    └── test_hive.sx      # Integration tests
```

## Running Tests

```bash
# Run all tests
spx test simplex-edge-hive/tests/

# Run specific test
spx run simplex-edge-hive/tests/test_security.sx
spx run simplex-edge-hive/tests/test_hive.sx
```

## Version History

- **v0.9.0**: Security layer implementation
  - User identity with PBKDF2 key derivation
  - AES-256-GCM encryption at rest
  - Per-user data isolation
  - TLS 1.3 mandatory
  - HMAC-SHA256 message signing
  - Replay attack protection
  - Secure device ID generation
  - Session management
  - Secure memory wiping

- **v0.9.0**: Initial implementation with core architecture

## License

Part of the Simplex project.
