# Simplex v0.5.0 Release Notes

**Release Date**: 2026-01-07

**Headline**: Native SLM Provisioning and Per-Hive Model Architecture

---

## Overview

Simplex v0.5.0 introduces the complete SLM (Small Language Model) provisioning system, enabling AI applications to run entirely locally with no external API dependencies. The headline feature is the **per-hive SLM architecture** where each hive provisions ONE shared model that all its specialists use, dramatically reducing memory requirements while maintaining cognitive coherence.

---

## Major Features

### 1. Per-Hive SLM Architecture

**The core architectural decision of v0.5.0**: Each hive provisions ONE shared SLM, not one per specialist.

```
┌─────────────────────────────────────────────────┐
│                  HIVE SLM                        │
│         simplex-cognitive-7b (4.1 GB)            │
│              (One model per hive)                │
└──────────────────┬──────────────────────────────┘
                   │
    ┌──────────────┼──────────────┐
    │              │              │
    ▼              ▼              ▼
┌────────┐   ┌────────┐   ┌────────┐
│Analyst │   │Coder   │   │Reviewer│
│ Anima  │   │ Anima  │   │ Anima  │
└────────┘   └────────┘   └────────┘
         All share ONE model
```

**Benefits**:
- 10 specialists use 1 model (not 10)
- 8-12 GB RAM vs 80+ GB for per-specialist
- Shared consciousness via HiveMnemonic

### 2. HiveMnemonic: Shared Consciousness

The HiveMnemonic creates collective memory across all specialists in a hive:

```simplex
hive AnalyticsHive {
    specialists: [Analyzer, Summarizer, Critic],
    slm: "simplex-cognitive-7b",

    mnemonic: {
        episodic: { capacity: 1000 },
        semantic: { capacity: 5000 },
        beliefs: { revision_threshold: 50 },
    },
}
```

**Three-tier memory hierarchy**:
- **Specialist Anima**: Personal memories (30% belief threshold)
- **Hive Mnemonic**: Shared consciousness (50% belief threshold)
- **Divine**: Solution-wide knowledge (70% belief threshold)

### 3. Model Provisioning via sxpm

New `sxpm model` commands for managing SLMs:

```bash
sxpm model list              # List available/installed models
sxpm model install <name>    # Download and install a model
sxpm model remove <name>     # Remove an installed model
sxpm model info <name>       # Show model details
```

### 4. Built-in Models

Three pre-configured models ship with Simplex:

| Model | Size | Use Case |
|-------|------|----------|
| `simplex-cognitive-7b` | 4.1 GB | Primary reasoning, `anima.think()` |
| `simplex-cognitive-1b` | 700 MB | Edge/mobile, resource-constrained |
| `simplex-mnemonic-embed` | 134 MB | Memory recall, semantic search |

### 5. Memory-Augmented Inference

When `infer()` is called, context flows automatically:

1. Specialist's Anima memories → formatted
2. Hive's Mnemonic → added
3. User prompt → appended
4. Combined → sent to Hive SLM

```simplex
specialist Analyst {
    receive Analyze(text: String) -> String {
        // Anima + Mnemonic context automatically included
        infer("Analyze: " + text)
    }
}
```

---

## Additional Features

### HTTP Client

Socket-based HTTP client with TLS support:

```simplex
let response = http_get("https://api.example.com/data")
let data = json_parse(response.body)
```

### Terminal Colors and Progress

Enhanced CLI output:

```simplex
use simplex_cli::{red, green, blue, bold, progress_bar, spinner}

println(green("Success!"))
println(red(bold("Error: ") + "Something went wrong"))

let bar = progress_bar(100)
bar.update(50)  // 50% complete
```

### Enhanced Test Framework

New assertions and helpers:

```simplex
#[test]
fn test_option_helpers() {
    let x = Some(42)
    assert_some(x)
    assert_eq(x.unwrap(), 42)

    let y: Option<i64> = None
    assert_none(y)
}

#[test]
fn test_result_helpers() {
    let ok: Result<i64, String> = Ok(42)
    assert_ok(ok)

    let err: Result<i64, String> = Err("failed")
    assert_err(err)
}
```

---

## Architecture Decisions

### Why Per-Hive, Not Per-Specialist?

| Approach | Models | RAM (10 specialists) | Verdict |
|----------|--------|---------------------|---------|
| Per-specialist | 10 | 80+ GB | Impractical |
| Per-hive | 1 | 8-12 GB | Efficient |

The per-hive approach enables cognitive AI on commodity hardware.

### Why Three Belief Thresholds?

| Level | Threshold | Rationale |
|-------|-----------|-----------|
| Specialist Anima | 30% | Individual beliefs should be flexible |
| Hive Mnemonic | 50% | Shared beliefs need more consensus |
| Divine | 70% | Global beliefs require high confidence |

### Model Size Trade-offs

| Model | Size | Quality | Use When |
|-------|------|---------|----------|
| 7B | 4.1 GB | Excellent | Server, desktop |
| 1B | 700 MB | Good | Edge, mobile, Raspberry Pi |
| Embed | 134 MB | N/A | Memory recall only |

---

## Breaking Changes

None. v0.5.0 is backwards compatible with v0.4.x code.

---

## Migration Guide

### From v0.4.x

No changes required. Existing code continues to work.

To take advantage of new features:

1. **Add mnemonic to hives**:
```simplex
hive MyHive {
    specialists: [A, B, C],
    mnemonic: {
        episodic: { capacity: 1000 },
    },
}
```

2. **Install models**:
```bash
sxpm model install simplex-cognitive-7b
```

3. **Use shared memory**:
```simplex
// Contribute to shared memory
hive.mnemonic.learn("Fact discovered by specialist")

// Access shared memory
let shared = hive.mnemonic.recall_for(query)
```

---

## Documentation

New and updated documentation:

| Document | Status |
|----------|--------|
| [SLM Provisioning](spec/13-slm-provisioning.md) | **New** |
| [Cognitive Hive AI](spec/09-cognitive-hive.md) | Updated with HiveMnemonic |
| [The Anima](spec/12-anima.md) | Updated with hive integration |
| [Tutorial: Cognitive Hives](tutorial/12-cognitive-hives.md) | Updated |

---

## Performance

### Memory Usage

| Configuration | RAM Required |
|---------------|--------------|
| 1 hive, 5 specialists, 7B model | 8-12 GB |
| 2 hives, 10 specialists, 7B models | 16-24 GB |
| 1 hive, 3 specialists, 1B model | 2-4 GB |

### Inference Latency

| Model | Hardware | Latency |
|-------|----------|---------|
| 7B Q4 | CPU (8 cores) | 100-300ms |
| 7B Q4 | GPU (Metal/CUDA) | 30-80ms |
| 1B Q4 | CPU (4 cores) | 20-50ms |

---

## Known Issues

1. **Model download progress**: Currently uses curl -#, may add native progress bar in v0.5.1
2. **GPU auto-detection**: CUDA path detection may need manual CUDA_PATH on some systems
3. **Divine SLM**: Cross-hive aggregation is synchronous; async planned for v0.6.0

---

## What's Next (v0.6.0)

- Auto-download models on first use
- Model lazy loading
- Async Divine SLM aggregation
- Package registry (sxpm publish/search)
- Parallel test execution

---

## Contributors

Thanks to all contributors who made v0.5.0 possible!

---

## Changelog Summary

| Category | Changes |
|----------|---------|
| SLM Provisioning | Model commands, built-in models, per-hive architecture |
| HiveMnemonic | Shared consciousness, three-tier memory hierarchy |
| HTTP Client | Socket-based with TLS |
| CLI | Terminal colors, progress bars, spinners |
| Testing | New assertions, Option/Result helpers |
| Toolchain | All components verified working |

---

*"Many specialists, one model, shared context."*

---

*Simplex v0.5.0 - Built for the AI era.*
