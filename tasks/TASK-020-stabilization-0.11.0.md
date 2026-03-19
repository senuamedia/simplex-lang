# TASK-020: Stabilization & Optimization Release 0.11.0

**Status:** Partial (~40%)
**Priority:** High
**Target Release:** 0.11.0
**Created:** 2026-01-18
**Updated:** 2026-03-16 (audited against codebase)

> **Audit Note (2026-03-16):**
> Completed:
> - StringBuilder library: `simplex-core/src/strings.sx` — complete with 20+ utility functions
> - GGUF format spec: `simplex-core/src/llm.sx` — complete type definitions
> - SLM Simplex-side APIs: `runtime/slm.sx` — SLMConfig, SLMInstance structs
>
> Not started:
> - codegen.sx refactoring (still 10,654 lines monolithic, target: split into modules)
> - Shared lexer/AST libraries (tools still reimplement)
> - SLM native C bindings (stubs only, no GGML integration)
> - Arena allocator, parallel compilation, string interning
> - Model loading prewarm, KV cache persistence, model registry

---

## Overview

Version 0.11.0 is a **stabilization and optimization release** focused on:

1. **Code Quality** - Removing duplicate/poor quality code, consolidating patterns
2. **SLM Infrastructure** - Improving llama.cpp integration, bundling default SLM
3. **Performance** - Optimizing critical paths, reducing memory usage
4. **Developer Experience** - Polishing tools from 0.10.0

**Goal:** Make Simplex production-ready with a stable, optimized codebase and seamless SLM integration that "just works" out of the box.

---

# PART 1: CODE QUALITY AUDIT

## 1.1 Large File Analysis

Files exceeding 1000 lines that need refactoring:

| File | Lines | Issue | Action |
|------|-------|-------|--------|
| `compiler/bootstrap/codegen.sx` | 8,820 | Monolithic, mixed concerns | Split into modules |
| `compiler/bootstrap/parser.sx` | 3,656 | Large but acceptable | Extract helpers |
| `tools/sxpm.sx` | 3,550 | Package manager logic | Split registry/resolver |
| `tools/sxfmt.sx` | 3,219 | Formatter logic | OK - single purpose |
| `tools/sxlsp.sx` | 1,522 | LSP handlers | OK for now |
| `tools/sxlint.sx` | 1,500 | Lint rules | OK for now |

### 1.1.1 codegen.sx Refactoring Plan

Split the 8,820-line `codegen.sx` into focused modules:

```
compiler/bootstrap/
├── codegen/
│   ├── mod.sx           # Main entry, orchestration
│   ├── llvm_ir.sx       # LLVM IR emission primitives
│   ├── types.sx         # Type codegen (structs, enums, traits)
│   ├── functions.sx     # Function/method codegen
│   ├── expressions.sx   # Expression codegen
│   ├── statements.sx    # Statement codegen
│   ├── actors.sx        # Actor/receive codegen
│   ├── async.sx         # Async/await state machines
│   ├── hive.sx          # Hive/specialist codegen
│   └── debug.sx         # Debug info generation (DWARF)
```

**Estimated reduction:** 8,820 lines → 10 files averaging 900 lines each

---

## 1.2 Duplicate Code Analysis

### 1.2.1 String Building Patterns

**Issue:** Multiple tools repeat string concatenation patterns:

```simplex
// Found in: sxfmt.sx, sxdoc.sx, sxlsp.sx, sxpm.sx
let result: String = string_from("");
result = string_concat(result, x);
result = string_concat(result, y);
result = string_concat(result, z);
```

**Solution:** Create `StringBuilder` in stdlib:

```simplex
// simplex-core/src/strings.sx
struct StringBuilder {
    parts: Vec<String>,
    len: i64
}

fn sb_new() -> StringBuilder;
fn sb_append(sb: &mut StringBuilder, s: String);
fn sb_build(sb: StringBuilder) -> String;

// Usage
let sb = sb_new();
sb_append(&mut sb, x);
sb_append(&mut sb, y);
sb.build()  // Single allocation
```

### 1.2.2 Token Handling Patterns

**Issue:** Lexers in sxfmt.sx, sxlsp.sx, sxlint.sx, sxdoc.sx all reimplement:
- Token type enums (TK_*)
- Lexer state machines
- Parser combinators

**Solution:** Extract shared lexer library:

```simplex
// lib/lexer.sx - Shared lexer infrastructure
enum TokenKind { ... }  // All 100+ token types
struct Token { kind: TokenKind, text: String, line: i64, col: i64 }
struct Lexer { source: String, pos: i64, line: i64, col: i64 }

fn lexer_new(source: String) -> Lexer;
fn lexer_next(l: &mut Lexer) -> Token;
fn lexer_peek(l: &Lexer) -> Token;
```

**Estimated deduplication:** ~2,000 lines across 4 tools

### 1.2.3 AST Node Patterns

**Issue:** Each tool defines its own AST representation.

**Solution:** Shared AST library with visitor pattern:

```simplex
// lib/ast.sx
enum Expr { ... }
enum Stmt { ... }
enum Decl { ... }

trait AstVisitor {
    fn visit_expr(&mut self, e: &Expr);
    fn visit_stmt(&mut self, s: &Stmt);
    fn visit_decl(&mut self, d: &Decl);
}
```

---

## 1.3 Poor Quality Code Audit

### 1.3.1 Error Handling

**Issue:** Many functions silently fail or return 0/-1 on error.

**Improvement:** Consistent Result types:

```simplex
// Current
fn load_config(path: String) -> i64;  // Returns 0 on failure

// Improved
fn load_config(path: String) -> Result<Config, Error>;
```

### 1.3.2 Magic Numbers

**Issue:** Hard-coded values throughout:

```simplex
// Bad
if len > 4096 { ... }
let threads: i64 = 4;

// Good
const MAX_CONTEXT_SIZE: i64 = 4096;
const DEFAULT_THREADS: i64 = 4;
```

### 1.3.3 Missing Validation

**Issue:** Many public APIs lack input validation.

**Improvement:** Add guards at API boundaries:

```simplex
fn slm_config_new(model_path: String, ctx_size: i64) -> Result<SLMConfig, Error> {
    if string_len(model_path) == 0 {
        return Err(Error::InvalidPath("model_path cannot be empty"));
    }
    if ctx_size < 128 || ctx_size > 131072 {
        return Err(Error::InvalidConfig("ctx_size must be 128-131072"));
    }
    // ...
}
```

---

## 1.4 Code Consolidation Tasks

| Task | Files Affected | Lines Saved |
|------|---------------|-------------|
| Extract StringBuilder | 6 tools | ~400 |
| Shared Lexer library | 4 tools | ~2000 |
| Shared AST library | 4 tools | ~1500 |
| Error type consolidation | All | ~300 |
| Constants extraction | All | ~100 |
| **Total** | | **~4300** |

---

# PART 2: SLM INFRASTRUCTURE

## 2.1 Current State Analysis

### 2.1.1 llama.cpp Integration

**Current:** Native bindings declared but implementation is stub:

```simplex
// runtime/slm.sx - Current stubs
fn slm_native_load(model_path: String, ctx_size: i64, threads: i64) -> i64;
fn slm_native_unload(handle: i64);
fn slm_native_infer(handle: i64, prompt: String, temperature: i64) -> String;
```

**Actual C implementation needed in:** `runtime/native/slm_llama.c`

### 2.1.2 Missing llama.cpp Features

| Feature | Status | Priority |
|---------|--------|----------|
| Basic inference | Stub | Critical |
| Embeddings | Stub | Critical |
| Batch inference | Missing | High |
| KV cache management | Missing | High |
| Grammar-constrained output | Missing | Medium |
| LoRA adapter loading | Missing | Medium |
| Speculative decoding | Missing | Low |

---

## 2.2 llama.cpp Integration Plan

### 2.2.1 Native Binding Layer

Create comprehensive C bindings:

```c
// runtime/native/slm_llama.h

typedef struct {
    llama_model* model;
    llama_context* ctx;
    int n_ctx;
    int n_threads;
} simplex_slm_t;

// Core operations
simplex_slm_t* simplex_slm_load(const char* model_path, int ctx_size, int threads);
void simplex_slm_free(simplex_slm_t* slm);
char* simplex_slm_infer(simplex_slm_t* slm, const char* prompt, float temp, int max_tokens);

// Embeddings
float* simplex_slm_embed(simplex_slm_t* slm, const char* text, int* out_dim);
float simplex_slm_similarity(float* a, float* b, int dim);

// Batch operations
typedef struct {
    char** prompts;
    int count;
} simplex_batch_t;
char** simplex_slm_infer_batch(simplex_slm_t* slm, simplex_batch_t* batch, float temp);

// KV Cache management
void simplex_slm_cache_clear(simplex_slm_t* slm);
void simplex_slm_cache_save(simplex_slm_t* slm, const char* path);
void simplex_slm_cache_load(simplex_slm_t* slm, const char* path);
```

### 2.2.2 Build System Integration

**CMake configuration for llama.cpp:**

```cmake
# runtime/native/CMakeLists.txt

# Fetch llama.cpp
FetchContent_Declare(
    llama_cpp
    GIT_REPOSITORY https://github.com/ggerganov/llama.cpp
    GIT_TAG b4458   # Pin to stable release
)
FetchContent_MakeAvailable(llama_cpp)

# Build Simplex SLM native library
add_library(simplex_slm SHARED
    slm_llama.c
)
target_link_libraries(simplex_slm PRIVATE llama ggml)
```

### 2.2.3 Platform Support Matrix

| Platform | Backend | GPU Support | Status |
|----------|---------|-------------|--------|
| macOS ARM | Metal | Apple Silicon | Priority 1 |
| macOS x86 | CPU/Metal | AMD/Intel | Priority 2 |
| Linux x86_64 | CUDA/CPU | NVIDIA | Priority 1 |
| Linux ARM | CPU | - | Priority 3 |
| Windows | CUDA/CPU | NVIDIA | Priority 2 |
| WASM | CPU | - | Future |

---

## 2.3 Default SLM Selection

### 2.3.1 Requirements for Bundled SLM

The default SLM shipped with Simplex must:

1. **Work offline** - No network required after install
2. **Fit in memory** - Run on 8GB RAM machines
3. **Fast inference** - <500ms for typical prompts
4. **Good quality** - Comparable to GPT-3.5 for reasoning tasks
5. **Permissive license** - Apache 2.0 or similar
6. **Small download** - <5GB for initial install

### 2.3.2 Model Evaluation Matrix

| Model | Size (Q4) | Quality | Speed | License | Recommendation |
|-------|-----------|---------|-------|---------|----------------|
| Phi-3-mini-4k | 2.2 GB | Good | Fast | MIT | **Default 1B** |
| Qwen2.5-3B | 2.0 GB | Very Good | Fast | Apache 2.0 | Alternative |
| Llama-3.2-3B | 2.0 GB | Very Good | Fast | Llama 3.2 | Consider |
| Mistral-7B-v0.3 | 4.1 GB | Excellent | Medium | Apache 2.0 | **Default 7B** |
| Qwen2.5-7B | 4.4 GB | Excellent | Medium | Apache 2.0 | Alternative |
| Phi-3-medium-14B | 8.0 GB | Outstanding | Slow | MIT | Optional |

### 2.3.3 Recommended Default Bundle

**Tier 1: Minimal Install (3GB)**
```
simplex-core-3b        Phi-3-mini-4k-instruct Q4_K_M    2.2 GB
simplex-embed-small    nomic-embed-text-v1.5 Q8        134 MB
```

**Tier 2: Standard Install (5GB)**
```
simplex-core-3b        Phi-3-mini-4k-instruct Q4_K_M    2.2 GB
simplex-core-7b        Mistral-7B-Instruct-v0.3 Q4_K_M  4.1 GB
simplex-embed-small    nomic-embed-text-v1.5 Q8        134 MB
```

**Tier 3: Full Install (10GB)**
```
simplex-core-3b        Phi-3-mini-4k-instruct Q4_K_M    2.2 GB
simplex-core-7b        Mistral-7B-Instruct-v0.3 Q4_K_M  4.1 GB
simplex-core-14b       Phi-3-medium-14B Q4_K_M          8.0 GB
simplex-embed-small    nomic-embed-text-v1.5 Q8        134 MB
simplex-embed-large    nomic-embed-text-v1.5 F16       274 MB
```

### 2.3.4 Model Provisioning Architecture

```
~/.simplex/
├── config.toml              # Global configuration
├── models/
│   ├── registry.json        # Installed models metadata
│   ├── simplex-core-3b.gguf # Default small model
│   ├── simplex-core-7b.gguf # Default large model
│   └── simplex-embed.gguf   # Embedding model
└── cache/
    ├── kv/                  # KV cache persistence
    └── embeddings/          # Cached embeddings
```

---

## 2.4 Cognitive SLM Architecture

> **Philosophy:** The SLM isn't a tool the Anima uses - the SLM inference IS the Anima's thinking process. Optimizations enhance cognitive fidelity, not just throughput.

### 2.4.1 Belief-Weighted Working Memory

**Current issue:** Context is treated as a FIFO buffer, discarding older content regardless of importance.

**Simplex Approach:** Context is the Anima's **active working memory** with belief-weighted attention:

```simplex
struct WorkingMemory {
    beliefs: Vec<Belief>,           // Active beliefs with confidence scores
    attention_weights: Vec<f64>,    // Learned relevance weights
    consolidation_queue: Vec<Belief> // Candidates for Mnemonic migration
}

struct Belief {
    content: String,
    confidence: f64,        // 0.0 to 1.0
    source: BeliefSource,   // Observed, Inferred, Retrieved
    timestamp: i64,
    coherence_score: f64    // Agreement with other beliefs
}

fn working_memory_update(wm: &mut WorkingMemory, new_input: String, anima: &Anima) {
    // 1. Parse new input into potential beliefs
    let candidates = extract_beliefs(new_input);

    // 2. Check coherence against existing beliefs
    for candidate in candidates {
        let coherence = belief_coherence_check(&candidate, &wm.beliefs);
        if coherence < CONTRADICTION_THRESHOLD {
            // Trigger belief revision - don't just append
            anima.trigger_belief_revision(candidate, wm.beliefs);
        } else {
            candidate.coherence_score = coherence;
            wm.beliefs.push(candidate);
        }
    }

    // 3. Anneal attention weights based on relevance to current goals
    anneal_attention_weights(wm, &anima.current_goals);

    // 4. Consolidate stable high-confidence beliefs to Mnemonic
    consolidate_to_mnemonic(wm, &anima.mnemonic);
}
```

**Key Differences from IT Caching:**
- Beliefs have confidence scores, not just recency
- Contradictions trigger revision, not silent overwrites
- Important beliefs migrate to long-term memory (Mnemonic)
- Attention weights are learned through annealing

### 2.4.2 Parallel Hypothesis Exploration

**Current issue:** Batch processing treats prompts as independent, missing ensemble reasoning.

**Simplex Approach:** Hives spawn Specialists to explore **divergent hypotheses** with consensus synthesis:

```simplex
struct HypothesisSpace {
    branches: Vec<ReasoningBranch>,
    consensus_threshold: f64,
    diversity_target: f64
}

struct ReasoningBranch {
    specialist: Specialist,
    hypothesis: String,
    confidence: f64,
    evidence: Vec<Belief>
}

fn hive_explore_hypotheses(hive: &Hive, question: String) -> ConsensusResult {
    // 1. Generate diverse hypotheses (not just parallel identical inference)
    let hypotheses = generate_diverse_hypotheses(question, hive.diversity_target);

    // 2. Spawn specialists to explore each branch
    let branches: Vec<ReasoningBranch> = hypotheses
        .par_iter()  // Parallel exploration
        .map(|h| {
            let specialist = hive.spawn_specialist(h);
            specialist.explore_with_evidence()
        })
        .collect();

    // 3. Synthesize through belief voting, not just majority
    let consensus = belief_weighted_consensus(&branches);

    // 4. Calibrate confidence based on agreement
    consensus.confidence = calibrate_confidence(&branches);
    // High agreement = high confidence
    // Divergent results = low confidence, flag for human review

    consensus
}

fn calibrate_confidence(branches: &Vec<ReasoningBranch>) -> f64 {
    let agreement = measure_semantic_agreement(branches);
    let evidence_strength = aggregate_evidence_strength(branches);

    // Annealed combination - learned weights
    agreement * AGREEMENT_WEIGHT + evidence_strength * EVIDENCE_WEIGHT
}
```

**Key Differences from IT Batching:**
- Hypotheses are intentionally diverse, not identical
- Results are synthesized through belief reconciliation
- Confidence is calibrated by agreement level
- Disagreement is informative, not averaged away

### 2.4.3 Semantic Memory (Mnemonic Integration)

**Current issue:** Embedding cache is content-addressed storage, missing memory semantics.

**Simplex Approach:** Embeddings are **semantic anchors** in the Mnemonic's episodic memory:

```simplex
struct SemanticMemory {
    mnemonic: Mnemonic,
    embedding_index: VectorIndex,    // HNSW or similar
    concept_clusters: Vec<Cluster>,  // Emergent groupings
    drift_detector: DriftDetector    // Tracks concept evolution
}

struct MemoryTrace {
    embedding: Vec<f64>,
    content: String,
    context: EpisodicContext,   // When, where, why stored
    access_count: i64,
    last_access: i64,
    belief_links: Vec<BeliefId> // Connected beliefs
}

fn semantic_store(mem: &mut SemanticMemory, content: String, context: EpisodicContext) {
    let embedding = slm_embed(content);

    // 1. Check for concept drift - same words, different meaning over time
    if let Some(existing) = mem.find_similar(embedding, SIMILARITY_THRESHOLD) {
        let drift = embedding_distance(embedding, existing.embedding);
        if drift > DRIFT_THRESHOLD {
            // Concept is evolving - don't overwrite, create new trace
            mem.drift_detector.record_drift(existing.content, content, drift);
            // This signals belief revision may be needed
        }
    }

    // 2. Store with episodic context
    let trace = MemoryTrace {
        embedding,
        content,
        context,
        access_count: 0,
        last_access: now(),
        belief_links: vec![]
    };
    mem.mnemonic.store(trace);

    // 3. Update concept clusters (emergent organization)
    mem.update_clusters(embedding);
}

fn semantic_recall(mem: &mut SemanticMemory, query: String, anima: &Anima) -> Vec<MemoryTrace> {
    let query_embedding = slm_embed(query);

    // 1. Find semantically similar traces
    let candidates = mem.embedding_index.search(query_embedding, K_NEIGHBORS);

    // 2. Filter by belief coherence with current Anima state
    let coherent = candidates.filter(|trace| {
        belief_coherent(&trace.belief_links, &anima.beliefs)
    });

    // 3. Boost traces that support current goals (relevance annealing)
    let ranked = rank_by_goal_relevance(coherent, &anima.current_goals);

    // 4. Update access patterns (strengthens frequently-used memories)
    for trace in &ranked {
        trace.access_count += 1;
        trace.last_access = now();
    }

    ranked
}
```

**Key Differences from IT Caching:**
- Embeddings have episodic context (when/why stored)
- Concept drift is detected and tracked
- Recall is filtered by belief coherence
- Access patterns strengthen memories (Hebbian learning)

### 2.4.4 Confidence-Gated Generation

**Current issue:** Streaming is about UI responsiveness, generation runs to completion.

**Simplex Approach:** Generation is **monitored in real-time** with belief-aware self-correction:

```simplex
struct GenerationState {
    tokens: Vec<String>,
    running_confidence: f64,
    belief_violations: Vec<Violation>,
    should_halt: bool
}

fn generate_with_monitoring(
    anima: &mut Anima,
    prompt: String,
    confidence_threshold: f64
) -> GenerationResult {
    let mut state = GenerationState::new();

    slm_infer_stream(anima.slm, prompt, |token| {
        state.tokens.push(token);

        // 1. Update running confidence estimate
        state.running_confidence = estimate_confidence(&state.tokens, anima);

        // 2. Check for belief violations in generated content
        if let Some(violation) = check_belief_coherence(&state.tokens, &anima.beliefs) {
            state.belief_violations.push(violation);

            if violation.severity > CRITICAL_THRESHOLD {
                // Self-interrupt: generated content contradicts core beliefs
                state.should_halt = true;
                anima.flag_for_revision(violation);
            }
        }

        // 3. Early termination if confidence threshold reached
        if state.running_confidence >= confidence_threshold {
            state.should_halt = true;
        }

        // 4. Adaptive temperature based on confidence
        let new_temp = anneal_temperature(state.running_confidence);
        slm_set_temperature(anima.slm, new_temp);
        // High confidence = lower temperature (more deterministic)
        // Low confidence = higher temperature (explore alternatives)

        !state.should_halt  // Continue if not halted
    });

    GenerationResult {
        content: state.tokens.join(""),
        confidence: state.running_confidence,
        violations: state.belief_violations,
        was_interrupted: state.should_halt
    }
}
```

**Key Differences from IT Streaming:**
- Generation monitors its own belief coherence
- Can self-interrupt on contradictions
- Confidence accumulates during generation
- Temperature anneals based on emerging confidence

---

### 2.4.5 Implementation Files

| File | Purpose |
|------|---------|
| `lib/belief.sx` | Belief representation with confidence, coherence |
| `lib/working_memory.sx` | Belief-weighted context management |
| `lib/hypothesis.sx` | Parallel exploration with consensus |
| `lib/semantic_memory.sx` | Mnemonic-integrated embedding store |
| `lib/generation.sx` | Confidence-monitored token generation |
| `lib/annealing.sx` | Self-learning weight optimization |

---

# PART 3: PERFORMANCE OPTIMIZATION

## 3.1 Compiler Performance

### 3.1.1 Parsing Performance

**Issue:** Parser creates many temporary allocations.

**Optimization:** Arena allocator for AST nodes:

```simplex
struct ParseArena {
    buffer: Vec<u8>,
    offset: i64
}

fn arena_alloc<T>(arena: &mut ParseArena) -> &mut T {
    // Bump allocator - O(1) allocation
    let ptr = arena.buffer.as_ptr() + arena.offset;
    arena.offset += size_of::<T>();
    ptr as &mut T
}
```

### 3.1.2 Code Generation Performance

**Issue:** String concatenation in IR generation is O(n^2).

**Optimization:** Use rope data structure or StringBuilder:

```simplex
// Current O(n^2)
let ir = "";
for instr in instructions {
    ir = string_concat(ir, instr);  // Copies entire string each time
}

// Optimized O(n)
let sb = sb_new();
for instr in instructions {
    sb_append(&mut sb, instr);  // Just stores reference
}
let ir = sb_build();  // Single allocation
```

### 3.1.3 Parallel Compilation

**Improvement:** Compile multiple modules concurrently:

```simplex
fn compile_project(project: &Project) -> Result<Binary, Error> {
    let modules = project.modules;

    // Compile in parallel using thread pool
    let compiled: Vec<CompiledModule> = parallel_map(modules, |m| {
        compile_module(m)
    });

    // Link sequentially
    link(compiled)
}
```

---

## 3.2 Runtime Performance

### 3.2.1 Vector Operations

**Issue:** `vec_get`/`vec_set` have bounds checks on every access.

**Optimization:** Unchecked variants for hot paths:

```simplex
// Safe (default) - with bounds check
fn vec_get<T>(v: &Vec<T>, idx: i64) -> T;

// Unsafe (opt-in) - no bounds check
fn vec_get_unchecked<T>(v: &Vec<T>, idx: i64) -> T;
```

### 3.2.2 String Interning

**Improvement:** Intern common strings to reduce allocations:

```simplex
struct StringInterner {
    strings: HashMap<String, i64>,
    table: Vec<String>
}

fn intern(interner: &mut StringInterner, s: String) -> i64 {
    match interner.strings.get(s) {
        Some(id) => id,
        None => {
            let id = interner.table.len();
            interner.table.push(s);
            interner.strings.insert(s, id);
            id
        }
    }
}
```

### 3.2.3 Memory Pool for Actors

**Improvement:** Reuse message buffers:

```simplex
struct MessagePool {
    free_list: Vec<*mut Message>,
    block_size: i64
}

fn pool_alloc(pool: &mut MessagePool) -> *mut Message {
    match pool.free_list.pop() {
        Some(ptr) => ptr,
        None => alloc_new_message()
    }
}

fn pool_free(pool: &mut MessagePool, msg: *mut Message) {
    pool.free_list.push(msg);  // Return to pool, don't deallocate
}
```

---

## 3.3 SLM Performance

### 3.3.1 Model Loading

**Issue:** Model loading takes 2-5 seconds on first use.

**Optimization:** Lazy loading with prewarming:

```simplex
// Background prewarm on solution start
fn solution_start(config: SolutionConfig) {
    // Start loading models in background
    spawn async {
        for hive in config.hives {
            slm_prewarm(hive.slm_config);
        }
    }
}
```

### 3.3.2 KV Cache Persistence

**Improvement:** Save/restore KV cache between sessions:

```simplex
fn save_session(hslm: &HiveSLM, path: String) {
    slm_native_cache_save(hslm.slm.handle, path);
}

fn restore_session(hslm: &mut HiveSLM, path: String) {
    slm_native_cache_load(hslm.slm.handle, path);
}
```

### 3.3.3 Quantization at Runtime

**Improvement:** Allow runtime quantization selection:

```simplex
fn slm_load_quantized(path: String, quant: Quantization) -> SLMInstance {
    match quant {
        Q4 => slm_native_load_q4(path),
        Q8 => slm_native_load_q8(path),
        F16 => slm_native_load_f16(path)
    }
}
```

---

# PART 4: IMPLEMENTATION PLAN

## Phase 1: Code Quality (Weeks 1-2)

| Task | Owner | Status |
|------|-------|--------|
| Extract StringBuilder to stdlib | - | Not Started |
| Extract shared Lexer library | - | Not Started |
| Split codegen.sx into modules | - | Not Started |
| Add input validation to public APIs | - | Not Started |
| Replace magic numbers with constants | - | Not Started |

## Phase 2: SLM Infrastructure (Weeks 3-5)

| Task | Owner | Status |
|------|-------|--------|
| Implement slm_llama.c native bindings | - | Not Started |
| CMake build system for llama.cpp | - | Not Started |
| Test on macOS ARM (Metal) | - | Not Started |
| Test on Linux x86_64 (CUDA) | - | Not Started |
| Bundle Phi-3-mini as default 3B model | - | Not Started |
| Bundle Mistral-7B as default 7B model | - | Not Started |
| Implement embedding cache | - | Not Started |
| Add streaming inference | - | Not Started |

## Phase 3: Performance (Weeks 5-6)

| Task | Owner | Status |
|------|-------|--------|
| Implement StringBuilder for IR gen | - | Not Started |
| Add arena allocator for parser | - | Not Started |
| Implement parallel compilation | - | Not Started |
| Add KV cache persistence | - | Not Started |
| Profile and optimize hot paths | - | Not Started |

## Phase 4: Polish & Release (Week 7)

| Task | Owner | Status |
|------|-------|--------|
| Update documentation | - | Not Started |
| Performance benchmarks | - | Not Started |
| Release notes | - | Not Started |
| Version bump to 0.11.0 | - | Not Started |

---

# PART 5: SUCCESS CRITERIA

## 0.11.0 is ready when:

### Code Quality
- [ ] No source file exceeds 2000 lines
- [ ] Shared lexer library used by all tools
- [ ] StringBuilder used for all string building
- [ ] All public APIs have input validation
- [ ] No magic numbers in code

### SLM Infrastructure
- [ ] llama.cpp bindings fully implemented
- [ ] Default 3B model included in installer
- [ ] `sxpm model list` shows bundled models
- [ ] Inference works on macOS ARM with Metal
- [ ] Inference works on Linux x86_64 with CUDA
- [ ] Embedding cache reduces repeated computations by 90%

### Performance
- [ ] Compilation 2x faster than 0.10.0
- [ ] SLM inference <500ms for typical prompts
- [ ] Memory usage 30% lower than 0.10.0
- [ ] Model loading <3 seconds with prewarm

### Compatibility
- [ ] All 0.10.0 tests pass
- [ ] No breaking API changes
- [ ] Upgrade path documented

---

# PART 6: HARDWARE ACCELERATION ARCHITECTURE

## 6.1 Overview

Hardware acceleration is **critical** for Simplex's core mission - deep integration with evolving AI models requires extreme throughput. The architecture uses compile-time backend selection for zero-overhead runtime performance.

## 6.2 Supported Backends

| Backend | Hardware | Build Flag | Performance vs CPU |
|---------|----------|------------|-------------------|
| **Metal** | Apple Silicon (M1-M4) | `-DGGML_METAL=ON` | 10-20x |
| **CUDA** | NVIDIA GPUs | `-DGGML_CUDA=ON` | 15-30x |
| **Vulkan** | AMD/Intel/NVIDIA | `-DGGML_VULKAN=ON` | 8-15x |
| **HIP/ROCm** | AMD Instinct/RDNA | `-DGGML_HIP=ON` | 12-25x |
| **SYCL/oneAPI** | Intel Arc/Xe | `-DGGML_SYCL=ON` | 8-12x |
| **CPU SIMD** | All x86/ARM | Auto-detected | Baseline |

## 6.3 Auto-Detection Build System

The CMake build system (`runtime/native/CMakeLists.txt`) automatically detects available hardware:

```cmake
# Auto-detection priority:
# 1. Apple Silicon → Metal
# 2. NVIDIA GPU → CUDA
# 3. AMD GPU → HIP/ROCm
# 4. Vulkan available → Vulkan
# 5. Fallback → Optimized CPU (AVX512/AVX2/NEON)
```

**Build commands:**
```bash
# Auto-detect (recommended)
cmake -DSIMPLEX_AUTO_DETECT=ON ..

# Force specific backend
cmake -DSIMPLEX_CUDA=ON ..
cmake -DSIMPLEX_METAL=ON ..

# Portable CPU-only build
cmake -DSIMPLEX_CPU_ONLY=ON ..
```

## 6.4 Performance Predictions by Architecture

### 6.4.1 Token Throughput (7B Q4_K_M model)

| Platform | Architecture | Tokens/sec | First Token | Memory |
|----------|--------------|------------|-------------|--------|
| **M4 Max** | Metal | 120-150 | 0.2s | 16GB unified |
| **M3 Pro** | Metal | 80-100 | 0.3s | 18GB unified |
| **M2** | Metal | 50-70 | 0.5s | 8GB unified |
| **RTX 4090** | CUDA | 180-220 | 0.15s | 24GB VRAM |
| **RTX 4080** | CUDA | 130-160 | 0.2s | 16GB VRAM |
| **RTX 3090** | CUDA | 100-130 | 0.25s | 24GB VRAM |
| **RTX 3080** | CUDA | 80-100 | 0.3s | 10GB VRAM |
| **AMD 7900 XTX** | Vulkan/HIP | 90-120 | 0.3s | 24GB VRAM |
| **AMD 7800 XT** | Vulkan/HIP | 60-80 | 0.4s | 16GB VRAM |
| **Intel Arc A770** | SYCL | 40-60 | 0.5s | 16GB VRAM |
| **Ryzen 9 7950X** | AVX512 | 15-25 | 2.0s | System RAM |
| **Core i9-14900K** | AVX512 | 18-28 | 1.8s | System RAM |
| **Apple M4** | NEON (CPU) | 12-18 | 2.5s | System RAM |

### 6.4.2 Batch Throughput (prompts/minute)

| Platform | 128 tokens | 512 tokens | 2048 tokens |
|----------|------------|------------|-------------|
| **M4 Max (Metal)** | 450 | 180 | 45 |
| **RTX 4090 (CUDA)** | 600 | 250 | 65 |
| **RTX 3080 (CUDA)** | 320 | 130 | 35 |
| **CPU AVX512** | 60 | 25 | 6 |

### 6.4.3 Embedding Generation (vectors/second)

| Platform | 768-dim | 1536-dim | 4096-dim |
|----------|---------|----------|----------|
| **M4 Max** | 2,500 | 1,800 | 800 |
| **RTX 4090** | 4,000 | 2,800 | 1,200 |
| **CPU AVX512** | 400 | 280 | 120 |

## 6.5 Dynamic Architecture Selection

### 6.5.1 Runtime GPU Layer Control

```simplex
// Simplex runtime automatically selects optimal offload
let config = InferenceConfig {
    n_gpu_layers: AUTO,  // -1 = auto-detect optimal
    ..default()
};

// Manual control for fine-tuning
let config = InferenceConfig {
    n_gpu_layers: 35,    // Specific layer count
    offload_kqv: true,   // KV cache on GPU
    flash_attn: true,    // Flash attention optimization
    ..default()
};
```

### 6.5.2 Memory-Aware Offloading

The runtime calculates optimal layer distribution:

```
Available VRAM: 16 GB
Model size: 4.1 GB (7B Q4)
KV cache per layer: ~50 MB
Layers: 32

Optimal offload: 28 layers (leaves 2GB headroom)
```

### 6.5.3 Hybrid Inference (CPU + GPU)

For models larger than VRAM:

```simplex
// 70B model on 24GB GPU + CPU
let config = InferenceConfig {
    n_gpu_layers: 40,    // First 40 layers on GPU
    n_threads: 16,       // Remaining on CPU
    ..default()
};
```

## 6.6 Build Matrix for Distribution

| Distribution | Backends Included | Size | Target |
|--------------|------------------|------|--------|
| `simplex-macos-arm64` | Metal + CPU | 45 MB | macOS 12+ (Apple Silicon) |
| `simplex-macos-x64` | CPU only | 35 MB | macOS 12+ (Intel) |
| `simplex-linux-cuda` | CUDA + CPU | 120 MB | Linux + NVIDIA |
| `simplex-linux-rocm` | HIP + CPU | 110 MB | Linux + AMD |
| `simplex-linux-cpu` | CPU only | 35 MB | Linux (portable) |
| `simplex-windows-cuda` | CUDA + CPU | 125 MB | Windows + NVIDIA |
| `simplex-windows-cpu` | CPU only | 40 MB | Windows (portable) |

## 6.7 Future Hardware Support

| Hardware | Timeline | Notes |
|----------|----------|-------|
| **Apple Neural Engine** | 0.12.0 | CoreML integration for M-series |
| **Qualcomm Hexagon NPU** | 0.13.0 | Mobile/embedded deployment |
| **AMD XDNA (NPU)** | 0.13.0 | Ryzen AI acceleration |
| **Intel NPU** | 0.13.0 | Meteor Lake and beyond |
| **WASM SIMD** | 0.12.0 | Browser-based inference |

## 6.8 Implementation Files

| File | Purpose |
|------|---------|
| `runtime/native/CMakeLists.txt` | Auto-detection build system |
| `runtime/native/llm_native.c` | Native bindings with SIMD |
| `simplex-core/src/llm.sx` | Simplex LLM stdlib |
| `lib/slm.sx` | High-level SLM integration (existing) |

---

# PART 7: TOOL ISSUES FROM 0.10.0 AUDIT

This section tracks all issues discovered during the 0.10.0 tool testing phase.

## 7.1 sxfmt (Formatter)

| Issue | Severity | Status | Notes |
|-------|----------|--------|-------|
| `TK_KwInit()` split as `TK_KwIn()it` | Critical | Fixed | Line 292 |
| `TK_KwInfer()` split as `TK_KwIn()fer` | Critical | Fixed | Lines 294, 1288 |
| Missing comma in `token_new()` call | Critical | Fixed | Line 495 |
| Missing `)` in `parser_check()` calls | Critical | Fixed | Lines 926, 961 |
| DEBUG println statements left in code | High | Fixed | Removed from tokenize(), format_source(), main() |
| --check mode not calling format_source() | Medium | Fixed | Now correctly formats and compares |

## 7.2 sxc (Compiler Driver)

| Issue | Severity | Status | Notes |
|-------|----------|--------|-------|
| Hardcoded x86_64 architecture | High | Fixed | Added `get_native_arch()` |
| println output split across lines | Medium | Fixed | Line 838-839 |
| Windows .exe extension handling empty | Medium | Open | Line 996-998 |
| Missing .ll file cleanup | Low | Open | Line 1013-1016 |
| Hardcoded `standalone_runtime.c` path | Medium | Open | Line 973 |
| **GitHub #69**: Runtime not bundled in release | Critical | Fixed | CI updated to bundle runtime |
| Bootstrap compiler outputs LLVM IR only | Info | Expected | Need manual clang link for tools |

**Note:** The bootstrap compiler (`build/sxc`) only emits LLVM IR. To compile tools:
```bash
./build/sxc tool.sx -o tool.ll                    # Emit LLVM IR
clang -O2 tool.ll runtime/standalone_runtime.c \
      -lssl -lcrypto -lsqlite3 -o tool            # Link to binary
```

## 7.3 sxlsp (Language Server)

| Issue | Severity | Status | Notes |
|-------|----------|--------|-------|
| `read_body()` only reads one line | High | Open | Multi-line JSON-RPC fails |
| JSON escaping issues in output | Medium | Open | Line 1240 |
| Missing: workspace symbols | Low | Open | LSP feature gap |
| Missing: document outline | Low | Open | LSP feature gap |
| Missing: rename support | Medium | Open | LSP feature gap |
| Missing: find references | Medium | Open | LSP feature gap |
| Missing: code actions | Low | Open | LSP feature gap |
| Missing: formatting integration | Low | Open | LSP feature gap |

## 7.4 playground

| Issue | Severity | Status | Notes |
|-------|----------|--------|-------|
| `__file__` not defined in exec() | High | Fixed | Wrapped in try/except |
| Dependencies not installed | Medium | Fixed | Local deps/ directory |
| sxc path priority wrong | Medium | Fixed | Reordered search |

## 7.5 sxdoc (Documentation Generator)

| Issue | Severity | Status | Notes |
|-------|----------|--------|-------|
| Orphan code in `get_os_name()` | High | Fixed | Lines 40-56, leftover from refactoring |

## 7.6 sxlint (Linter)

| Issue | Severity | Status | Notes |
|-------|----------|--------|-------|
| False positive: string literals `"{ }"` detected as empty blocks | Medium | Open | Line 904-915, needs string literal context |
| File path handling: "Could not read file" for some paths | Low | Open | May be relative path issue |
| Compiles and runs successfully | - | Verified | Help, check modes work |

## 7.7 sxpm (Package Manager)

| Issue | Severity | Status | Notes |
|-------|----------|--------|-------|
| Infinite recursion in `SXPM_VERSION()` | Critical | Fixed | Line 15, called self instead of version module |
| Missing `get_arch_name()` in platform.sx | High | Fixed | Added to simplex-core/src/platform.sx |
| Inconsistent naming: file_read vs read_file | Low | Open | Standardize for 0.11.0 |

## 7.8 cursus (Bytecode VM)

| Issue | Severity | Status | Notes |
|-------|----------|--------|-------|
| Compiles and runs successfully | - | Verified | Help output works |
| No critical issues found | - | - | Ready for 0.10.0 release |

## 7.9 tree-sitter-simplex (Grammar)

| Issue | Severity | Status | Notes |
|-------|----------|--------|-------|
| tree-sitter CLI not installed | Info | Blocked | Requires: `npm i -g tree-sitter-cli` |
| grammar.js exists (28KB) | - | Verified | Grammar definition present |
| Test corpus present | - | Verified | definitions.txt, expressions.txt |
| Node-gyp build fails | Medium | Open | Native binding compilation issue |

**To test manually:**
```bash
npm install -g tree-sitter-cli
cd tree-sitter-simplex
tree-sitter generate
tree-sitter test
```

## 7.10 lib/bench.sx (Benchmarking)

| Issue | Severity | Status | Notes |
|-------|----------|--------|-------|
| Library module, no main() | Info | Expected | Cannot run standalone, use as import |
| Compiles successfully | - | Verified | No syntax/semantic errors |

## 7.11 spec_actor_basic.sx

| Issue | Severity | Status | Notes |
|-------|----------|--------|-------|
| Undefined @Add symbol in LLVM IR | Critical | Open | Actor message send not compiling |
| Actor codegen incomplete | High | Open | Need to fix actor message dispatch |

## 7.12 spec_async_basic.sx

| Issue | Severity | Status | Notes |
|-------|----------|--------|-------|
| Exit code 128 instead of 42 | Critical | Open | async/await machinery not working |
| Expected: `await compute(21)` returns 42 | - | - | Test file works syntactically |

---

# PART 8: RISKS & MITIGATIONS

| Risk | Impact | Mitigation |
|------|--------|------------|
| llama.cpp API changes | High | Pin to specific release tag |
| Model licensing issues | High | Only use Apache 2.0/MIT models |
| macOS Metal bugs | Medium | Fallback to CPU inference |
| CUDA compatibility | Medium | Support CUDA 11.8+ only |
| Model size bloat | Medium | Offer minimal/standard/full tiers |

---

# PART 9: REFERENCES

## llama.cpp Resources
- [llama.cpp GitHub](https://github.com/ggerganov/llama.cpp)
- [GGUF Format Spec](https://github.com/ggerganov/ggml/blob/master/docs/gguf.md)
- [llama.cpp Server API](https://github.com/ggerganov/llama.cpp/blob/master/examples/server/README.md)

## Model Sources
- [Hugging Face GGUF Models](https://huggingface.co/models?library=gguf)
- [Phi-3 Models](https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-gguf)
- [Mistral Models](https://huggingface.co/mistralai)
- [Qwen2.5 Models](https://huggingface.co/Qwen)

## Related Tasks
- TASK-015: Simplex Core SLM (original SLM design)
- TASK-019: Compiler Bugs 0.10.0
- TASK-017: API Docs Tooling

---

*"Stability through simplicity, performance through focus."*
