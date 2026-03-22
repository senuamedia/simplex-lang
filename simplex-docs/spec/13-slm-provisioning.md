# SLM Provisioning Architecture

**Version 0.5.0**

Simplex provisions Small Language Models (SLMs) natively, enabling AI applications to run entirely locally without external API dependencies. This document describes the three-tier SLM architecture and how models are provisioned, shared, and managed.

---

## Philosophy

### Why Native SLMs?

| External APIs | Native SLMs |
|---------------|-------------|
| $0.03-0.12 per 1K tokens | One-time download, free inference |
| 500-3000ms latency | 50-200ms local inference |
| Rate limits, outages | Always available |
| Data sent externally | Complete privacy |
| No customization | Fine-tune for your domain |

Simplex treats SLMs as first-class citizens - they ship with the runtime, are provisioned via `sxpm`, and integrate seamlessly with the Anima and Hive systems.

---

## Three-Tier SLM Architecture

Simplex implements a hierarchical SLM architecture with three distinct levels:

```
┌─────────────────────────────────────────────────────────────┐
│                    DIVINE SLM (Solution-wide)                │
│   - Aggregates all hive memories and beliefs                │
│   - Global reasoning and orchestration                       │
│   - 70% confidence threshold for global beliefs              │
└──────────────────────────┬──────────────────────────────────┘
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
         ▼                 ▼                 ▼
┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
│   HIVE SLM #1   │ │   HIVE SLM #2   │ │   HIVE SLM #3   │
│  (One per hive) │ │  (One per hive) │ │  (One per hive) │
│  + HiveMnemonic │ │  + HiveMnemonic │ │  + HiveMnemonic │
│  50% belief thr │ │  50% belief thr │ │  50% belief thr │
└───┬───┬───┬─────┘ └───┬───┬───┬─────┘ └───┬───┬───┬─────┘
    │   │   │           │   │   │           │   │   │
    ▼   ▼   ▼           ▼   ▼   ▼           ▼   ▼   ▼
   S1  S2  S3          S4  S5  S6          S7  S8  S9
   └────┴────┘         └────┴────┘         └────┴────┘
   Each specialist     Each specialist     Each specialist
   has its own         has its own         has its own
   Anima (30% thr)     Anima (30% thr)     Anima (30% thr)
```

### Level 1: Specialist Anima

Each specialist has an individual **Anima** (agent state manager) with:
- Personal episodic memories (experiences)
- Personal semantic memories (facts)
- Personal beliefs (30% revision threshold)
- Working memory for active context

The Anima feeds context into the Hive SLM but does NOT have its own model.

### Level 2: Hive SLM (Shared)

Each hive provisions **ONE shared SLM** that all its specialists use:
- Single model instance per hive
- All specialists in the hive call this model
- HiveMnemonic provides shared context (episodic, semantic, beliefs)
- 50% belief revision threshold for hive-level beliefs

**This is the core architectural decision: one model per hive, not per specialist.**

### Level 3: Divine SLM (Solution-wide)

For cross-hive reasoning and global coordination:
- Aggregates memories from all hives
- Maintains solution-wide beliefs (70% threshold)
- Used for orchestration decisions
- Optional - not all solutions need this level

---

## Model Configuration

### SLMConfig Structure

```simplex
struct SLMConfig {
    model_path: String,        // Path to GGUF model file
    quantization: Quantization, // Q4, Q5, Q8, F16, F32
    context_size: i64,         // Token context window (default: 4096)
    threads: i64,              // CPU threads (default: 4)
    gpu_layers: i64,           // Layers to offload to GPU (default: 0)
    batch_size: i64,           // Batch size (default: 512)
    temperature: i64,          // 0-100 (default: 70 = 0.7)
    top_p: i64,                // Nucleus sampling (0-100)
    top_k: i64,                // Top-k sampling
}
```

### Quantization Options

| Quantization | Bits | Size (7B) | Quality | Speed |
|--------------|------|-----------|---------|-------|
| Q4_K_M | 4-bit | ~4.1 GB | Good | Fast |
| Q5_K_M | 5-bit | ~5.1 GB | Better | Medium |
| Q8_0 | 8-bit | ~7.5 GB | Excellent | Slower |
| F16 | 16-bit | ~14 GB | Best | Slowest |

**Recommendation**: Q4_K_M for most use cases - best balance of quality/speed.

---

## Built-in Models

Simplex ships with three pre-configured models:

### simplex-cognitive-7b

The primary reasoning model for `anima.think()` operations.

| Property | Value |
|----------|-------|
| Base Model | Mistral-7B-Instruct v0.2 |
| Quantization | Q4_K_M |
| Size | 4.1 GB |
| Context Window | 8192 tokens |
| Use Case | General reasoning, code analysis, conversation |

```simplex
anima Assistant {
    slm: "simplex-cognitive-7b"  // Default for anima.think()
}
```

### simplex-cognitive-1b

Lightweight model for resource-constrained environments.

| Property | Value |
|----------|-------|
| Base Model | TinyLlama-1.1B-Chat v1.0 |
| Quantization | Q4_K_M |
| Size | 700 MB |
| Context Window | 2048 tokens |
| Use Case | Edge devices, quick classification, routing |

```simplex
hive LightweightHive {
    slm: "simplex-cognitive-1b"  // For edge deployment
}
```

### simplex-mnemonic-embed

Embedding model for memory recall and semantic search.

| Property | Value |
|----------|-------|
| Base Model | Nomic Embed Text v1.5 |
| Quantization | F16 |
| Size | 134 MB |
| Embedding Dimension | 768 |
| Use Case | Memory retrieval, semantic routing |

```simplex
// Used automatically by recall_for() operations
let memories = soul.recall_for(goal: "find similar code reviews")
```

---

## Model Provisioning via sxpm

### List Available Models

```bash
$ sxpm model list

Available Models:
  simplex-cognitive-7b    [installed]  4.1 GB  Primary reasoning model
  simplex-cognitive-1b    [installed]  700 MB  Lightweight model
  simplex-mnemonic-embed  [not installed]  134 MB  Embedding model

Installed models: ~/.simplex/models/
```

### Install a Model

```bash
$ sxpm model install simplex-cognitive-7b

Downloading simplex-cognitive-7b...
  [████████████████████████████████] 4.1 GB / 4.1 GB

Verifying checksum... OK
Installed to ~/.simplex/models/simplex-cognitive-7b.gguf
```

### Model Information

```bash
$ sxpm model info simplex-cognitive-7b

Model: simplex-cognitive-7b
  Source: Mistral-7B-Instruct-v0.2
  Quantization: Q4_K_M
  Size: 4.1 GB
  Context: 8192 tokens
  Category: cognitive
  Installed: ~/.simplex/models/simplex-cognitive-7b.gguf
```

### Remove a Model

```bash
$ sxpm model remove simplex-cognitive-7b

Removed simplex-cognitive-7b (4.1 GB freed)
```

### Model Storage Locations

| Location | Path | Priority |
|----------|------|----------|
| Project-local | `.simplex/models/` | 1 (highest) |
| User global | `~/.simplex/models/` | 2 |
| System | `/usr/share/simplex/models/` | 3 (lowest) |

---

## Hive SLM Configuration

### Defining a Hive with SLM

```simplex
hive AnalyticsHive {
    // SLM configuration for this hive
    slm: SLMConfig {
        model: "simplex-cognitive-7b",
        context_size: 8192,
        temperature: 70,  // 0.7
        gpu_layers: 32,   // Offload to GPU
    },

    // Specialists share this SLM
    specialists: [Analyzer, Summarizer, Critic],

    // Shared hive memory (mnemonic)
    mnemonic: {
        episodic: { capacity: 1000 },
        semantic: { capacity: 5000 },
        belief_threshold: 50,
    },

    strategy: OneForOne,
}
```

### How Specialists Use the Hive SLM

```simplex
specialist Analyzer {
    // This references the hive's SLM, not a separate model
    model: "test",
    temperature: 30,  // Lower for more deterministic output

    receive Analyze(text: String) -> String {
        // infer() uses the Hive SLM with this specialist's Anima context
        infer("Analyze the following text for key insights: " + text)
    }
}
```

When `infer()` is called:
1. Specialist's Anima memories are formatted as context
2. Hive's mnemonic (shared memories) is added
3. Combined context + prompt sent to Hive SLM
4. Response returned to specialist

---

## Memory-Augmented Inference

### Context Flow

```
┌──────────────────────────────────────────────────────────────┐
│                    SPECIALIST CALLS infer()                   │
└──────────────────────────────┬───────────────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────────────┐
│                 FORMAT ANIMA MEMORIES                         │
│  <context>                                                    │
│  Recent experiences:                                          │
│  - Analyzed code review yesterday                             │
│  - Found null pointer bug in auth.sx                          │
│                                                               │
│  Known facts:                                                 │
│  - This codebase uses clean architecture                      │
│  - Team prefers explicit error handling                       │
│                                                               │
│  Current beliefs (confidence > 30%):                          │
│  - User prefers detailed explanations (85%)                   │
│  </context>                                                   │
└──────────────────────────────┬───────────────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────────────┐
│                  ADD HIVE MNEMONIC                            │
│  <hive name="AnalyticsHive">                                  │
│  Shared experiences:                                          │
│  - Team completed security audit last week                    │
│                                                               │
│  Shared knowledge:                                            │
│  - Production uses PostgreSQL 15                              │
│                                                               │
│  Hive beliefs (confidence > 50%):                             │
│  - Prefer async over sync operations (78%)                    │
│  </hive>                                                      │
└──────────────────────────────┬───────────────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────────────┐
│                  APPEND USER PROMPT                           │
│                                                               │
│  Analyze the following text for key insights: [user text]    │
└──────────────────────────────┬───────────────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────────────┐
│                    HIVE SLM INFERENCE                         │
│                                                               │
│  slm_native_infer(hive.slm.handle, full_context, temperature) │
└──────────────────────────────┬───────────────────────────────┘
                               │
                               ▼
                           Response
```

### Context Budget

Memory context is limited to preserve space for the prompt:

```simplex
struct MemoryAugmentedSLM {
    slm: SLMInstance,
    episodic_context: String,
    semantic_context: String,
    belief_context: String,
    context_budget: i64,  // Default: 2048 tokens for memory
}
```

If memories exceed the budget:
1. Lower-importance memories are dropped
2. Older memories are summarized
3. Beliefs below threshold are excluded

---

## HiveMnemonic: Shared Consciousness

The HiveMnemonic is the shared memory layer for all specialists in a hive.

### Structure

```simplex
struct HiveMnemonic {
    // Shared experiences across all specialists
    episodic: EpisodicStore {
        capacity: 1000,
        importance_threshold: 0.4,
    },

    // Shared knowledge
    semantic: SemanticStore {
        capacity: 5000,
    },

    // Collective beliefs (higher threshold than individual)
    beliefs: BeliefStore {
        revision_threshold: 0.5,  // 50% vs 30% for individual
        contradiction_resolution: ConsensusWithEvidence,
    },
}
```

### How Specialists Contribute

```simplex
specialist Researcher {
    receive Research(topic: String) -> Report {
        let findings = do_research(topic)

        // This goes to the specialist's personal Anima
        self.anima.remember("Researched: {topic}")

        // This goes to the shared HiveMnemonic
        hive.mnemonic.learn("Research finding: {findings.summary}")

        findings
    }
}
```

### Accessing Shared Memory

```simplex
specialist Synthesizer {
    receive Synthesize(query: String) -> Report {
        // Recall from shared hive memory
        let shared_knowledge = hive.mnemonic.recall_for(query)

        // Recall from personal anima
        let personal_context = self.anima.recall_for(query)

        // Both are available during inference
        infer("Synthesize report on: {query}")
    }
}
```

---

## Divine SLM: Solution-Wide Reasoning

For applications requiring cross-hive coordination:

### Structure

```simplex
struct DivineSLM {
    slm: MemoryAugmentedSLM,
    hive_contexts: Vec<String>,    // Aggregated from all hives
    global_beliefs: BeliefStore {
        revision_threshold: 0.7,   // High threshold for global beliefs
    },
}
```

### Use Cases

- Routing tasks between hives
- Resolving cross-hive conflicts
- Making solution-wide decisions
- Aggregating insights from all specialists

```simplex
// Divine level orchestration
let divine = DivineSLM::new("simplex-cognitive-7b")

// Aggregate beliefs from all hives
for hive in solution.hives {
    divine.observe(hive.mnemonic)
}

// Cross-hive reasoning
let decision = divine.think("Which hive should handle: {task}")
```

---

## Model Size Considerations

### Sizing Guidelines

| Specialists per Hive | Recommended Model | Memory Required |
|---------------------|-------------------|-----------------|
| 1-3 | cognitive-1b | 2-4 GB RAM |
| 3-8 | cognitive-7b | 8-12 GB RAM |
| 8+ | cognitive-7b + GPU | 12+ GB RAM + GPU |

### Cost Efficiency

The per-hive SLM architecture is designed for cost efficiency:

| Architecture | Model Instances | Memory (10 specialists) |
|--------------|-----------------|------------------------|
| Per-specialist SLM | 10 | 80+ GB |
| Per-hive SLM (Simplex) | 1-2 | 8-16 GB |

**Savings: 80-90% memory reduction with per-hive sharing.**

### Hardware Recommendations

| Deployment | CPU | RAM | GPU |
|------------|-----|-----|-----|
| Development | 4+ cores | 16 GB | Optional |
| Production (CPU) | 8+ cores | 32 GB | - |
| Production (GPU) | 4+ cores | 16 GB | 8+ GB VRAM |
| Edge | 2+ cores | 8 GB | - (use 1B model) |

---

## GPU Acceleration

### Metal (macOS)

```simplex
hive GPUHive {
    slm: SLMConfig {
        model: "simplex-cognitive-7b",
        gpu_layers: 32,  // Offload 32 layers to Metal
    }
}
```

### CUDA (NVIDIA)

```simplex
hive CUDAHive {
    slm: SLMConfig {
        model: "simplex-cognitive-7b",
        gpu_layers: 35,  // Most layers on GPU
        // CUDA detected automatically
    }
}
```

### Automatic Device Selection

```simplex
hive AutoHive {
    slm: SLMConfig {
        model: "simplex-cognitive-7b",
        device: Auto,  // Detect best available
    }
}
```

Device priority: CUDA > Metal > Vulkan > CPU

---

## Best Practices

### 1. One Model Per Hive

```simplex
// Good: All specialists share hive SLM
hive AnalyticsHive {
    slm: "simplex-cognitive-7b",
    specialists: [A, B, C, D, E],  // All share one model
}

// Avoid: Multiple hives for same function
hive HiveA { slm: "7b", specialists: [A, B] }
hive HiveB { slm: "7b", specialists: [C, D] }  // Wasteful
```

### 2. Right-Size Your Models

```simplex
// Edge/mobile: Use 1B model
hive EdgeHive {
    slm: "simplex-cognitive-1b"
}

// Server: Use 7B model
hive ServerHive {
    slm: "simplex-cognitive-7b"
}
```

### 3. Use Mnemonic for Shared Knowledge

```simplex
// Good: Shared via mnemonic
hive.mnemonic.learn("API uses JWT authentication")

// Avoid: Each specialist storing same fact
specialist1.anima.learn("API uses JWT")
specialist2.anima.learn("API uses JWT")  // Duplicate
```

### 4. Appropriate Belief Thresholds

```simplex
// Individual anima: Lower threshold (30%)
anima {
    beliefs: { revision_threshold: 30 }
}

// Hive mnemonic: Medium threshold (50%)
mnemonic {
    beliefs: { revision_threshold: 50 }
}

// Divine: High threshold (70%)
divine {
    global_beliefs: { revision_threshold: 70 }
}
```

---

## Summary

| Concept | Description |
|---------|-------------|
| Three-tier SLM | Divine → Hive → Specialist hierarchy |
| Per-hive SLM | ONE model shared by all specialists in a hive |
| HiveMnemonic | Shared memory/consciousness across specialists |
| Model provisioning | `sxpm model install/list/remove/info` |
| Built-in models | cognitive-7b, cognitive-1b, mnemonic-embed |
| Memory-augmented | Context from Anima + Mnemonic prepended to prompts |

The SLM provisioning architecture enables Simplex applications to:
- Run AI entirely locally with no external dependencies
- Share models efficiently across specialists
- Scale from edge (1B) to server (7B) deployments
- Maintain cost efficiency with per-hive model sharing

---

*"Many specialists, one model, shared context."*

---

*See also: [Cognitive Hive AI](09-cognitive-hive.md) | [The Anima](12-anima.md) | [AI Integration](07-ai-integration.md)*
