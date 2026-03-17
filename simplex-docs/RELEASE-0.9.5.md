# Simplex v0.9.5 Release Notes

**Release Date:** 2026-01-17
**Codename:** Consolidated Foundations

---

## Overview

Simplex v0.9.5 is a major release delivering **Edge Hive** for local AI inference on edge devices, the complete **Nexus Protocol** for high-frequency hive communication, and the **Research Module** for epistemic data validation. This release also includes Linux build support and comprehensive toolchain consolidation.

---

## Major Features

### Edge Hive (TASK-013)

Complete edge computing framework for IoT, mobile, and embedded devices:

#### Local AI Inference

On-device AI without cloud dependency:

| Model | Parameters | Context | Target Device |
|-------|------------|---------|---------------|
| SmolLM-135M | 135M | 2K | Watch/Wearable |
| SmolLM-360M | 360M | 2K | Phone |
| SmolLM-1.7B | 1.7B | 2K | Tablet |
| Qwen2-0.5B | 500M | 32K | Phone |
| Qwen2-1.5B | 1.5B | 32K | Desktop |
| Phi-3-mini | 3.8B | 4K | Laptop |
| Llama-3.2-1B | 1B | 128K | Tablet/Laptop |
| Llama-3.2-3B | 3B | 128K | Desktop |
| Gemma-2-2B | 2B | 8K | Tablet |

Features:
- Automatic model selection based on device class and resources
- GGUF format with quantization (Q4_K_M, Q8_0, Q3_K_M, Q2_K)
- Battery-aware degradation to smaller models
- Confidence-based routing between local and cloud

#### Security

Production-grade security for edge deployment:

| Feature | Implementation |
|---------|---------------|
| Encryption at Rest | AES-256-GCM for all persistent data |
| Authentication | PBKDF2 with 100,000 iterations |
| Network Security | TLS 1.3 mandatory, ws:// rejected |
| Message Signing | HMAC-SHA256 with timestamp validation |
| Session Management | 256-bit tokens, 24-hour expiry |
| Secure Shutdown | Memory wiping for keys and tokens |

```simplex
use edge_hive::{edge_hive_login, hive_infer_local, hive_shutdown_secure};

// Login with PBKDF2-derived credentials
let hive = edge_hive_login("user@example.com", "password", "wss://hub.example.com");

// Local inference (model auto-selected for device)
let response = hive_infer_local(hive, "Summarize my emails");
println(response);

// Secure shutdown clears all keys from memory
hive_shutdown_secure(hive);
```

See [simplex-edge-hive/RELEASE.md](../simplex-edge-hive/RELEASE.md) for full documentation.

---

### Nexus Protocol (TASK-012)

High-performance protocol for constant hive-to-hive communication achieving **237x compression**:

| Metric | Naive | Nexus | Improvement |
|--------|-------|-------|-------------|
| Bytes per belief | 90 | 0.38 | **237x** |
| 1000 beliefs sync | 90 KB | 378 bytes | **244x** |
| Encode time (1000) | ~500us | 7us | **71x** |
| Decode time (1000) | ~500us | 5us | **100x** |

#### Protocol Modules (28 total)

```
simplex-nexus/src/
├── bits.sx           # Bit manipulation primitives
├── frame.sx          # Wire format framing
├── sync.sx           # Belief synchronization
├── stf.sx            # State transfer format
├── dual.sx           # Dual number trajectories
├── neural.sx         # Neural compression codebook
├── prediction.sx     # Predictive encoding
├── flow_control.sx   # Backpressure management
├── multiplex.sx      # Channel multiplexing
├── conn_pool.sx      # Connection pooling
├── federation.sx     # Multi-hive federation
├── crdt.sx           # Conflict-free replicated data types
├── vector_clock.sx   # Causality tracking
├── secure_*.sx       # Security layer (TLS, auth)
└── *_transport.sx    # Transport bindings
```

#### Core Innovation: Bit-Packed Delta Streams

```
Update Stream (bit-packed, variable length):
  Op (2 bits)   Meaning              Additional Bits
  00 = SAME     No change            0 (just 2 bits)
  01 = DELTA_S  Small delta          +7 bits (9 total)
  10 = DELTA_L  Large delta          +16 bits (18 total)
  11 = FULL     Complete value       +32 bits (34 total)
```

```simplex
use nexus::{NexusSession, BitPackedBeliefSync};

let session = NexusSession::connect("wss://peer-hive.example.com");

// Add beliefs to sync
let sync = BitPackedBeliefSync::new();
sync.add_belief("quality", 0.95);
sync.add_belief("security", 0.88);

// Delta encoding transmits only changes
session.sync_beliefs(sync);  // ~0.76 bytes vs 180 naive
```

---

### Epistemic Data Refinement (TASK-015)

Research module for validating and improving training data quality:

#### Source Authority Hierarchy

1. **Language specifications** (ECMA, ISO, W3C, RFCs) - Credibility: 1.0
2. **Official API documentation** (docs.python.org, developer.mozilla.org) - Credibility: 0.95
3. **Peer-reviewed academic papers** (arxiv.org, ACM, IEEE) - Credibility: 0.9
4. **Trusted technical references** (cppreference.com, man pages) - Credibility: 0.85
5. **High-quality community knowledge** (StackOverflow 100+ votes) - Credibility: 0.7

#### ResearchConfig

```simplex
use lib::simplex_training::research::{ResearchConfig, Researcher};

let config = ResearchConfig {
    min_credibility: 0.7,
    require_corroboration: true,
    min_corroborating_sources: 2,
    max_source_age_days: 365,
    search_domains: vec![
        "ecma-international.org",
        "docs.python.org",
        "developer.mozilla.org",
        "rust-lang.org",
    ],
    excluded_domains: vec!["w3schools.com"],
    rate_limit_rpm: 30,
};

let researcher = Researcher::new(config);
let fact = researcher.research("JavaScript async/await behavior");

// Returns GroundedBelief with full provenance
println(f"Confidence: {fact.confidence}");
for source in fact.supporting_sources {
    println(f"  - {source.url} (credibility: {source.credibility})");
}
```

---

### Cross-Platform Build

#### Linux Support

Simplex compiler successfully built and tested on Linux x86_64:

```bash
# Build output
$ file sxc-linux
sxc-linux: ELF 64-bit LSB executable, x86-64, version 1 (SYSV)

$ ./sxc-linux version
sxc v0.9.5 - Simplex Compiler
Platform: linux-x86_64
```

**Build Infrastructure:**
- AWS EC2 instance (Amazon Linux 2023)
- Automated build pipeline with S3 artifact storage
- SSM-based secure access

---

### Centralized Version Management

All binary versions managed from single source in `simplex-core/src/version.sx`:

```simplex
pub fn SIMPLEX_VERSION() -> i64 { string_from("0.9.5") }
pub fn TOOLCHAIN_VERSION() -> i64 { string_from("0.9.5") }
pub fn SXPM_VERSION() -> i64 { string_from("0.9.5") }
pub fn SXDOC_VERSION() -> i64 { string_from("0.9.5") }
pub fn SXLSP_VERSION() -> i64 { string_from("0.9.5") }
pub fn CURSUS_VERSION() -> i64 { string_from("0.9.5") }
```

**Version Utilities:**
```simplex
use simplex_core::version::{parse_version, version_meets_min, is_version_compatible};

if version_meets_min(SIMPLEX_VERSION(), "0.9.0") {
    // Use v0.9+ features
}
```

---

### Toolchain Audit (TASK-011)

Comprehensive audit of ~21,400 lines of pure Simplex toolchain code:

| Tool | Lines | Status |
|------|-------|--------|
| `sxc` | 1,148 | Audited |
| `sxpm` | 2,643 | Audited |
| `sxdoc` | 808 | Audited |
| `sxlsp` | 528 | Audited |
| `cursus` | 984 | Audited |
| `compiler/bootstrap/*` | 11,742 | Audited |
| Supporting modules | ~3,547 | Audited |

---

## New Libraries

### simplex-core/src/platform.sx

Cross-platform operations:

```simplex
use simplex_core::platform::{get_os_name, get_arch_name, get_path_separator};

let os = get_os_name();     // "windows", "linux", "macos"
let arch = get_arch_name(); // "x86_64", "aarch64"
let sep = get_path_separator(); // "/" or "\"
```

### simplex-core/src/safety.sx

Runtime safety primitives:

```simplex
use simplex_core::safety::{bounds_check, safe_div, null_check};

// Safe array access
if bounds_check(index, array_len) {
    let value = array[index];
}
```

---

## Tool Updates

All tools updated to version 0.9.5:

| Tool | Description |
|------|-------------|
| **sxc** | Simplex Compiler with unified versioning |
| **sxpm** | Package Manager with improved dependency resolution |
| **cursus** | Bytecode VM with safety improvements |
| **sxdoc** | Documentation Generator |
| **sxlsp** | Language Server Protocol |

---

## Migration Guide

### From v0.9.2

1. **No breaking changes** - existing code compiles unchanged

2. **Edge Hive**: New edge computing capability
   ```simplex
   use edge_hive::{edge_hive_login, hive_shutdown_secure};
   ```

3. **Nexus Protocol**: New hive-to-hive sync
   ```simplex
   use nexus::{NexusSession, BitPackedBeliefSync};
   ```

4. **Version utilities**: Use new centralized module
   ```simplex
   use simplex_core::version::{SIMPLEX_VERSION, version_meets_min};
   ```

---

## Compatibility

| Component | Minimum Version | Maximum Version |
|-----------|-----------------|-----------------|
| LLVM | 14.0.0 | - |
| Previous Simplex | 0.8.0 | 0.10.0 |
| macOS | 12.0 | - |
| Linux | Kernel 5.4+ | - |

---

## Statistics

- **New source files**: 50+
- **New test files**: 25+
- **Total lines added**: ~15,000
- **Nexus modules**: 28
- **Edge Hive modules**: 17
- **Supported edge models**: 9

---

## What's Next

- **v0.10.0**: GPU acceleration, HSM integration
- **v1.0.0**: Production-ready release

---

## Credits

Edge Hive, Nexus Protocol, and Research Module developed by Rod Higgins ([@senuamedia](https://github.com/senuamedia)).

---

*"Local intelligence, global consciousness."*
