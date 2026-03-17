# 16. Edge Hive Specification

## Overview

The Edge Hive is a lightweight, autonomous cognitive hive designed to run on user devices ranging from smartwatches to desktop computers. Unlike traditional "thin client" approaches where edge devices are merely dumb terminals executing cloud commands, the Edge Hive is a living piece of the cognitive hive that provides local intelligence, user advocacy, and offline capability.

## Design Principles

### 1. Local-First Intelligence

The Edge Hive operates with a "local-first" philosophy:
- All user data is stored locally by default
- Processing happens on-device when possible
- Cloud delegation is a fallback, not the primary mode
- Offline operation is a first-class feature

### 2. User Advocacy

Unlike current LLM applications that primarily serve the provider's interests, the Edge Hive acts as the user's advocate:
- Learns and represents user preferences
- Protects user privacy by default
- Filters and prioritizes based on user patterns
- Maintains user context across interactions

### 3. Adaptive Footprint

The Edge Hive automatically adapts to device capabilities:
- Watch: Minimal footprint, aggressive cloud delegation
- Phone: Balanced local/cloud processing
- Tablet: Enhanced local capability
- Laptop/Desktop: Maximum local intelligence

## Architecture

### Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                       Edge Hive                             │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │   Device    │  │   Belief    │  │     Specialist      │  │
│  │   Profile   │  │    Store    │  │      Registry       │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
│         │                │                    │              │
│         └────────────────┼────────────────────┘              │
│                          │                                   │
│                ┌─────────▼─────────┐                        │
│                │  Cognitive Loop   │                        │
│                └─────────┬─────────┘                        │
│                          │                                   │
│                ┌─────────▼─────────┐                        │
│                │    Federation     │                        │
│                │     Manager       │                        │
│                └─────────┬─────────┘                        │
└──────────────────────────┼──────────────────────────────────┘
                           │
               ┌───────────┴───────────┐
               │                       │
        ┌──────▼──────┐        ┌───────▼──────┐
        │ Cloud Hives │        │ Peer Devices │
        └─────────────┘        └──────────────┘
```

### Device Profile

Detects and manages device capabilities:
- **Device Class**: Watch, Wearable, Phone, Tablet, Laptop, Desktop
- **Hardware**: RAM, Storage, GPU presence
- **Sensors**: GPS, Accelerometer, Camera, Biometrics
- **Power**: Battery level and charging state
- **Network**: Connection type and bandwidth

### Belief Store

Persistent storage for user knowledge:
- **Preferences**: Explicit user settings
- **Patterns**: Learned behavioral patterns
- **Context**: Current situational awareness
- **Tasks**: Pending user tasks
- **Relationships**: Contact importance

Features:
- Confidence scores with decay
- Time-based eviction
- CRDT-based merge for sync
- Sensitivity-aware storage

### Specialist System

Local specialists handle domain-specific requests:

| Specialist | Description | Device Requirement |
|------------|-------------|-------------------|
| Context | Learns user patterns | All |
| UI | Device-specific interface | All |
| Task | Task management | All |
| Notification | Filters notifications | All |
| Calendar | Schedule management | Phone+ |
| Conversation | Full dialogue | Tablet+ |
| Offline | Offline operation | Phone+ |
| Quick | Fast responses | Watch/Wearable |
| Health | Health data | Devices with sensors |

Each specialist:
- Has a confidence score
- Tracks success rate
- Can handle specific request types
- Adapts to device capabilities

### Local Model System

The Edge Hive includes on-device Small Language Model (SLM) inference for privacy-preserving local intelligence:

#### Supported Models

| Model | Parameters | Context | Min RAM | Device Class |
|-------|------------|---------|---------|--------------|
| SmolLM-135M | 135M | 2K | 256 MB | Watch/Wearable |
| SmolLM-360M | 360M | 2K | 512 MB | Phone |
| SmolLM-1.7B | 1.7B | 2K | 2 GB | Tablet |
| Qwen2-0.5B | 500M | 32K | 768 MB | Phone |
| Qwen2-1.5B | 1.5B | 32K | 2 GB | Desktop |
| Phi-3-mini | 3.8B | 4K | 4 GB | Laptop |
| Llama-3.2-1B | 1B | 128K | 1.5 GB | Tablet/Laptop |
| Llama-3.2-3B | 3B | 128K | 4 GB | Desktop |
| Gemma-2-2B | 2B | 8K | 3 GB | Tablet |

#### Model Selection

Models are automatically selected based on:
- Device class (Watch → Desktop)
- Available RAM
- Available storage
- Battery level (degrades to smaller models when low)

#### Quantization

All models use GGUF format with quantization support:
- **Q4_K_M** (recommended): Good balance of quality and size
- **Q8_0**: Higher quality, larger size
- **Q3_K_M**: Aggressive compression for constrained devices
- **Q2_K**: Maximum compression (quality tradeoff)

### Cognitive Loop

The main processing cycle:

1. **Request Received**: User input or system event
2. **Cache Check**: Return cached response if valid
3. **Context Enrichment**: Update beliefs with current context
4. **Specialist Selection**: Find best local handler
5. **Confidence Evaluation**: Compare against delegation threshold
6. **Local Handling**: If confidence sufficient, process locally with specialist
7. **Local Model Inference**: If specialist insufficient, try local SLM
8. **Cloud Delegation**: If local model insufficient, delegate to cloud hive
9. **Response Caching**: Cache successful responses
10. **Pattern Learning**: Update patterns from interaction

### Federation Manager

Handles synchronization:
- **Cloud Sync**: Belief synchronization with cloud hives
- **Peer Sync**: Direct device-to-device sync
- **Request Delegation**: Sends complex requests to cloud
- **Queue Management**: Handles offline request queuing

Sync Strategies:
- **Aggressive**: Prefer cloud (low-resource devices)
- **Balanced**: Mix local and cloud
- **Local-First**: Prefer local (high-resource devices)

## Data Model

### i64 Encoding Pattern

The Edge Hive uses i64 handles for all complex types, enabling compatibility with Simplex:

```simplex
// Belief entry: [type, key_hash, value, confidence, timestamp]
fn belief_entry_new(belief_type: i64, key: i64, value: i64, confidence: i64) -> i64 {
    let entry: i64 = vec_new();
    vec_push(entry, belief_type);
    vec_push(entry, string_hash(key));
    vec_push(entry, value);
    vec_push(entry, confidence);
    vec_push(entry, time_now());
    entry
}
```

### Sensitivity Levels

Data is classified by sensitivity:

| Level | Description | Sync Behavior |
|-------|-------------|---------------|
| Public | Safe to share | Sync freely |
| Private | User's devices only | Encrypted sync |
| Sensitive | Minimal sync | End-to-end encrypted |
| Local | Never leaves device | No sync |

## Resource Limits

Device-specific limits ensure the Edge Hive adapts to available resources:

| Device | Max Model | Belief Store | Cache Size |
|--------|-----------|--------------|------------|
| Watch | 0 | 100 entries | 1 MB |
| Phone | 500M params | 10K entries | 50 MB |
| Tablet | 1B params | 50K entries | 100 MB |
| Laptop | 7B params | 100K entries | 500 MB |
| Desktop | 70B params | 1M entries | 1 GB |

## Privacy Model

### Local-First Privacy

- No data sent to cloud without explicit need
- All beliefs stored locally first
- Encryption at rest for sensitive data
- User controls sync preferences

### Sync Privacy

- End-to-end encryption for peer sync
- TLS for cloud sync
- No plain-text transmission of sensitive data
- User can disable cloud sync entirely

## Security Architecture

The Edge Hive implements a comprehensive security model to protect user data both at rest and in transit.

### Threat Model

The security architecture protects against:
- **Local attacker with file system access**: All stored data is encrypted
- **Network attacker (MITM)**: TLS 1.3 mandatory, certificate validation
- **Malicious peer devices**: Message signing and authentication
- **Device theft**: Encryption keys derived from user password
- **Replay attacks**: Timestamp validation with 5-minute window

### User Identity

Each user has a cryptographic identity:

```simplex
// UserIdentity structure:
// Slot 0: user_id (SHA-256 hash of username)
// Slot 1: user_id_hex (string representation for file paths)
// Slot 2: encryption_key (derived via PBKDF2)
// Slot 3: auth_token (for network requests)
// Slot 4: token_expiry (timestamp)
// Slot 5: device_key (unique per device)
// Slot 6: salt (for key derivation)
```

### Encryption at Rest

All persistent data is encrypted using AES-256-GCM:

| Component | Encryption | Key Source |
|-----------|------------|------------|
| Belief Store | AES-256-GCM | User password |
| Configuration | AES-256-GCM | User password |
| Cache | AES-256-GCM | User password |
| Salt | Unencrypted | N/A (needed for key derivation) |

Encrypted file format:
```
[version(1), nonce(12), ciphertext(N), auth_tag(16)]
```

### Key Derivation

User encryption keys are derived using PBKDF2:
- **Algorithm**: PBKDF2-HMAC-SHA256
- **Iterations**: 100,000 (OWASP recommended minimum)
- **Salt**: 128-bit random per user
- **Key Length**: 256 bits

### User Data Isolation

Each user's data is stored in an isolated directory:

| Platform | Path |
|----------|------|
| macOS | `~/Library/Application Support/EdgeHive/users/<user_id_hex>/` |
| Linux | `~/.local/share/edgehive/users/<user_id_hex>/` |
| Windows | `%APPDATA%\EdgeHive\users\<user_id_hex>\` |

Directory permissions are set to owner-only (0700).

### Network Security

All network communication requires:

1. **TLS 1.3**: Only `wss://` endpoints accepted; `ws://` rejected
2. **Certificate Validation**: TLS certificates verified before connection
3. **Token Authentication**: Session tokens with 24-hour expiry
4. **Message Signing**: HMAC-SHA256 on all messages
5. **Replay Protection**: Timestamp validation within 5-minute window

Authenticated message frame format:
```
[msg_type(1), timestamp(8), payload_len(4), payload(N), signature(32)]
```

### Secure Device Identification

Device IDs are generated cryptographically:
```simplex
fn generate_secure_device_id() -> i64 {
    // Combines: hardware_id + random_bytes + timestamp + process_id
    // Hashed with SHA-256
}
```

### Session Management

- Session tokens: 256-bit cryptographically random
- Expiry: 24 hours
- Refresh: Automatic on activity
- Logout: Secure memory wiping

### Secure Shutdown

On shutdown, sensitive data is cleared:
1. Encryption keys overwritten with zeros
2. Auth tokens cleared
3. Device keys wiped
4. Memory barrier enforced

## API Reference

### Security API (Recommended)

```simplex
// Create user identity (new user registration)
fn user_identity_new(username: i64, password: i64) -> i64

// Login existing user
fn user_identity_login(username: i64, password: i64) -> i64

// Create secure Edge Hive with user identity
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

### Hive Lifecycle

```simplex
// Create a new Edge Hive (legacy - use edge_hive_new_secure)
fn edge_hive_new(device_id: i64, cloud_endpoint: i64) -> i64

// Handle a user request
fn hive_handle_request(hive: i64, request_type: i64, payload: i64) -> i64

// Run maintenance (call periodically)
fn hive_maintenance_cycle(hive: i64) -> i64

// Clean shutdown (legacy - use hive_shutdown_secure)
fn hive_shutdown(hive: i64) -> i64
```

### Request Types

```simplex
fn REQUEST_QUERY() -> i64 { 1 }       // Information query
fn REQUEST_COMMAND() -> i64 { 2 }     // Action command
fn REQUEST_SYNC() -> i64 { 3 }        // Sync request
fn REQUEST_NOTIFICATION() -> i64 { 4 } // Notification
```

### High-Level API

```simplex
// Query the hive
fn edge_hive_query(hive: i64, query: i64) -> i64

// Execute a command
fn edge_hive_command(hive: i64, command: i64) -> i64

// Process a notification
fn edge_hive_notify(hive: i64, notification: i64) -> i64

// Sync beliefs
fn edge_hive_sync(hive: i64) -> i64

// Connect to cloud
fn edge_hive_connect(hive: i64) -> i64

// Check online status
fn edge_hive_is_online(hive: i64) -> i64
```

### Preferences

```simplex
// Set a user preference
fn hive_set_preference(hive: i64, key: i64, value: i64) -> i64

// Get a user preference
fn hive_get_preference(hive: i64, key: i64) -> i64
```

## Implementation Status

### Completed (v0.10.0)

- Core type system and constants
- Device profile detection with platform-specific hardware queries
- Belief store with CRDT merge
- Specialist system with routing
- Federation manager
- Cognitive loop with local model inference
- WebSocket I/O for cloud and peer federation
- File persistence for beliefs and configuration
- Platform-specific device detection (macOS, iOS, Linux, Windows, Android, watchOS)
- Unit tests
- **Security layer**:
  - User identity with password-based key derivation (PBKDF2)
  - AES-256-GCM encryption at rest for all persistent data
  - Per-user data isolation with separate directories
  - TLS 1.3 mandatory for all network connections
  - HMAC-SHA256 message signing
  - Replay attack protection
  - Cryptographically secure device ID generation
  - Session management with token expiry
  - Secure memory wiping on shutdown
- **Local model integration**:
  - Automatic model selection based on device class and resources
  - Support for SmolLM (135M-1.7B), Qwen2 (0.5B-1.5B), Phi-3-mini (3.8B), Llama 3.2 (1B-3B), Gemma 2 (2B)
  - GGUF format with quantization (Q4_K_M, Q8_0, Q3_K_M, Q2_K)
  - Prompt templates for each model family (ChatML, Phi-3, Llama 3, Gemma)
  - Confidence-based routing between local model and cloud
  - Inference statistics tracking (tokens/sec, cache hits)
  - Battery-aware model degradation

### Planned (v0.10.1+)

- Secure peer-to-peer sync protocol (end-to-end encrypted)
- Health sensor integration
- Cross-device migration with encrypted transfer
- Hardware security module (HSM) integration where available
- Biometric authentication support

## Example Usage

### Secure Usage (Recommended)

```simplex
fn main() -> i64 {
    let username: i64 = string_from("alice");
    let password: i64 = string_from("secure_password_123");
    let cloud_endpoint: i64 = string_from("wss://hive.example.com");

    // Register new user (first time) or login (returning user)
    let hive: i64 = edge_hive_login(username, password, cloud_endpoint);
    if hive == 0 {
        // User doesn't exist, register them
        hive = edge_hive_register(username, password, cloud_endpoint);
    }

    // Connect to cloud (TLS mandatory)
    edge_hive_connect(hive);

    // Set user preferences (encrypted at rest)
    hive_set_preference(hive, string_from("theme"), 1);  // Dark mode
    hive_set_preference(hive, string_from("notifications"), 1);  // Enabled

    // Handle a query
    let query: i64 = vec_new();
    vec_push(query, string_from("What's on my calendar?"));
    let response: i64 = edge_hive_query(hive, query);

    // Run maintenance loop (auto-saves encrypted state)
    hive_maintenance_cycle(hive);

    // Secure shutdown (saves encrypted state, clears keys from memory)
    hive_shutdown_secure(hive);

    0
}
```

### Legacy Usage (Not Recommended)

```simplex
fn main() -> i64 {
    // Create Edge Hive without security (legacy API)
    let hive: i64 = create_edge_hive(string_from("wss://hive.example.com"));

    // Connect to cloud
    edge_hive_connect(hive);

    // Set user preferences
    hive_set_preference(hive, string_from("theme"), 1);
    hive_set_preference(hive, string_from("notifications"), 1);

    // Shutdown cleanly
    hive_shutdown(hive);

    0
}
```

## File Structure

```
simplex-edge-hive/
├── src/
│   ├── runtime.sx      # Runtime function declarations
│   ├── types.sx        # Core type constants
│   ├── device.sx       # Device profile management (platform-specific)
│   ├── beliefs.sx      # Belief store with CRDT merge
│   ├── specialist.sx   # Specialist system with routing
│   ├── federation.sx   # Federation manager for cloud/peer sync
│   ├── network.sx      # WebSocket I/O for federation
│   ├── persistence.sx  # File-based storage for beliefs/config
│   ├── security.sx     # User identity, encryption, and authentication
│   ├── model.sx        # Local SLM inference (SmolLM, Qwen2, Phi-3, Llama 3.2)
│   ├── hive.sx         # Main cognitive loop
│   └── lib.sx          # Library entry point
└── tests/
    ├── test_types.sx     # Type tests
    ├── test_beliefs.sx   # Belief store tests
    ├── test_security.sx  # Security layer tests
    ├── test_model.sx     # Local model tests
    └── test_hive.sx      # Integration tests
```

## Version History

- **v0.10.0**: Complete Edge Hive implementation
  - Core architecture with device detection and adaptive footprint
  - Belief store with CRDT merge and confidence decay
  - Specialist system with confidence-based routing
  - Federation manager for cloud and peer sync
  - Security layer with encryption at rest (AES-256-GCM)
  - User identity with PBKDF2 key derivation (100K iterations)
  - TLS 1.3 mandatory for all network connections
  - HMAC-SHA256 message signing and replay protection
  - Local model integration with automatic device-appropriate selection
  - Support for SmolLM, Qwen2, Phi-3-mini, Llama 3.2, Gemma 2
  - Battery-aware model degradation and offline capability
