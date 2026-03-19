# TASK-029: Hive Intelligence Libraries

**Status**: Complete
**Priority**: Critical
**Created**: 2026-03-16
**Target Version**: 0.16.0
**Depends On**: TASK-022 (v0.13.0 complete), TASK-024 (Redis for vector storage option)

---

## Overview

These libraries **enhance the native AI capabilities** already built into Simplex. The language already has SLM inference, memory-augmented models, cognitive hives, belief systems, and specialist routing. These libraries provide the supporting infrastructure that makes those capabilities production-ready.

This is Simplex's **competitive moat** — no other language has this natively. These libraries should be the first things built after v0.13.0.

All implementations must be **pure Simplex**.

---

## Library 1: simplex-vectordb

**Location**: `simplex-vectordb/src/mod.sx`
**Priority**: Critical — persistent storage for the embeddings `slm_native_embed` already produces

### Core API
```simplex
struct VectorStore {
    name: String,
    dimension: i64,
    index: VectorIndex,
    storage: VectorStorage
}

fn vdb_create(name: String, dimension: i64) -> VectorStore
fn vdb_insert(store: VectorStore, id: String, embedding: Vec<f64>, metadata: JsonValue) -> Result<bool, VdbError>
fn vdb_search(store: VectorStore, query: Vec<f64>, top_k: i64) -> Result<Vec<VdbResult>, VdbError>
fn vdb_delete(store: VectorStore, id: String) -> Result<bool, VdbError>
fn vdb_save(store: VectorStore, path: String) -> Result<bool, VdbError>
fn vdb_load(path: String) -> Result<VectorStore, VdbError>

struct VdbResult {
    id: String,
    score: f64,            // Cosine similarity
    metadata: JsonValue
}
```

### Index Types
```simplex
enum VectorIndex {
    Flat,               // Brute-force exact search (small datasets)
    HNSW(HnswConfig),   // Hierarchical Navigable Small World (production)
    IVF(IvfConfig)      // Inverted file index (large datasets)
}

struct HnswConfig {
    m: i64,              // Max connections per node (default 16)
    ef_construction: i64, // Build-time search width (default 200)
    ef_search: i64        // Query-time search width (default 50)
}
```

### Why Native (Not Cloud API Wrappers)
Simplex runs SLM inference locally. The embeddings are produced locally. The vector store should be local too — this keeps the entire AI pipeline on-device, which is the Edge-Hive philosophy. No round-trips to Pinecone/Qdrant.

For cloud-scale workloads, the local vector store can sync to a persistent backend (Redis, PostgreSQL with pgvector, or a future cloud connector).

### Cognitive Hive Integration
- **Anima semantic memory**: Each specialist's semantic memory backed by vector search
- **Hive mnemonic search**: Shared hive knowledge searchable by semantic similarity
- **RAG pipeline**: `slm_native_embed(query)` -> `vdb_search()` -> inject results into `maslm_infer()` context
- **Belief evidence retrieval**: Find evidence supporting or contradicting beliefs

### Success Criteria
- Insert 10K vectors, search returns correct top-k by cosine similarity
- HNSW index is significantly faster than brute-force for 10K+ vectors
- Save/load persists index to disk
- Metadata filtering works alongside vector search
- New test: `tests/ai/spec_vectordb.sx`

---

## Library 2: simplex-rag

**Location**: `simplex-rag/src/mod.sx`
**Priority**: Critical — the #1 production AI pattern, and Simplex can do it natively

### Core API
```simplex
struct RagPipeline {
    chunker: ChunkConfig,
    store: VectorStore,
    slm: MemoryAugmentedSLM,
    reranker: Option<RerankConfig>
}

fn rag_create(slm: MemoryAugmentedSLM, store: VectorStore) -> RagPipeline
fn rag_ingest(pipeline: RagPipeline, document: String, source: String) -> Result<i64, RagError>
fn rag_query(pipeline: RagPipeline, question: String, top_k: i64) -> Result<RagResponse, RagError>

struct RagResponse {
    answer: String,
    sources: Vec<RagSource>,
    confidence: f64
}

struct RagSource {
    chunk: String,
    source: String,
    score: f64
}
```

### Pipeline Stages
```
Document --> Chunk --> Embed --> Store
                                  |
Query --> Embed --> Search --> Rerank --> Context Build --> SLM Infer --> Answer
```

### Chunking Strategies
```simplex
enum ChunkStrategy {
    FixedSize(i64, i64),           // chunk_size, overlap
    Sentence,                       // Split on sentence boundaries
    Paragraph,                      // Split on paragraph boundaries
    Semantic(f64)                   // Split when embedding similarity drops below threshold
}
```

### Features
- **Chunking**: Fixed-size, sentence, paragraph, semantic
- **Embedding**: Uses `slm_native_embed()` — no external API calls
- **Storage**: Uses `simplex-vectordb` — local, on-device
- **Retrieval**: Top-k similarity search with optional reranking
- **Context building**: Format retrieved chunks for SLM context injection
- **Source attribution**: Track which chunks contributed to the answer
- **Incremental ingestion**: Add documents without rebuilding index

### Why This Is Unique to Simplex
In Python, RAG requires: LangChain + OpenAI API + Pinecone + text splitters + prompt templates. Five external services and libraries.

In Simplex: `rag_create(slm, store)` -> `rag_ingest(doc)` -> `rag_query(question)`. Three function calls. Everything runs locally. The SLM, embeddings, vector store, and inference are all native.

### Success Criteria
- Ingest a document, query it, get relevant answer with source attribution
- Semantic chunking produces better results than fixed-size on varied documents
- Top-k retrieval returns genuinely relevant chunks
- Confidence score correlates with answer quality
- New test: `tests/ai/spec_rag.sx`

---

## Library 3: simplex-guardrails

**Location**: `simplex-guardrails/src/mod.sx`
**Priority**: High — output validation, safety, belief consistency

### Core API
```simplex
struct GuardrailConfig {
    rules: Vec<GuardrailRule>,
    action_on_fail: GuardrailAction
}

enum GuardrailRule {
    MaxLength(i64),
    MinConfidence(f64),
    MustContain(Vec<String>),
    MustNotContain(Vec<String>),
    BeliefConsistent(BeliefStore),     // Output must not contradict known beliefs
    FactGrounded(VectorStore),         // Output must be grounded in retrieved evidence
    CustomValidator(fn(String) -> Result<bool, String>)
}

enum GuardrailAction {
    Reject,                  // Return error
    Retry(i64),             // Retry inference up to N times
    Fallback(String),       // Return fallback response
    Flag                    // Allow but flag for review
}

fn guardrail_check(output: String, config: GuardrailConfig) -> GuardrailResult
fn guardrail_wrap_infer(slm: MemoryAugmentedSLM, prompt: String, config: GuardrailConfig) -> Result<String, GuardrailError>
```

### Built-in Validators
- **Belief consistency**: Check that SLM output doesn't contradict the belief store
- **Fact grounding**: Verify claims against vector store evidence
- **Hallucination detection**: Compare output claims against retrieved context
- **Content safety**: Pattern-based content filtering
- **Format validation**: JSON output validation, structured output checking

### Cognitive Hive Integration
- **Specialist output validation**: Guardrails on specialist responses before queen aggregation
- **Belief update validation**: Validate new beliefs before adding to belief store
- **Cross-hive consistency**: Ensure beliefs don't diverge across federated hives

### Success Criteria
- Belief-inconsistent outputs are caught and rejected
- Retry mechanism produces compliant output on second attempt
- Fact grounding correctly identifies unsupported claims
- Custom validators integrate cleanly
- New test: `tests/ai/spec_guardrails.sx`

---

## Library 4: simplex-eval

**Location**: `simplex-eval/src/mod.sx`
**Priority**: Medium — benchmark and evaluate SLM and hive performance

### Core API
```simplex
struct EvalSuite {
    name: String,
    cases: Vec<EvalCase>
}

struct EvalCase {
    input: String,
    expected: String,       // Expected output (or reference answer)
    metadata: JsonValue
}

struct EvalResult {
    case_id: String,
    output: String,
    scores: Vec<EvalScore>,
    latency_ms: i64
}

struct EvalScore {
    metric: String,
    value: f64
}

fn eval_run(suite: EvalSuite, slm: MemoryAugmentedSLM) -> Vec<EvalResult>
fn eval_compare(results_a: Vec<EvalResult>, results_b: Vec<EvalResult>) -> EvalComparison
fn eval_report(results: Vec<EvalResult>) -> String
```

### Built-in Metrics
- **Exact match**: Output matches expected exactly
- **BLEU score**: N-gram overlap with reference
- **Semantic similarity**: Embedding cosine similarity between output and expected
- **Latency**: Inference time per case
- **Belief accuracy**: Did the SLM's belief updates match ground truth?
- **RAG relevance**: For RAG pipelines, did retrieved context contain the answer?

### Features
- Suite-based evaluation (define test cases, run, compare)
- A/B comparison between models or configurations
- Regression detection (alert when scores drop)
- Export results as JSON or CSV for analysis

### Success Criteria
- Run eval suite against SLM and get scored results
- A/B comparison correctly identifies which model is better
- Semantic similarity metric correlates with human judgment
- New test: `tests/ai/spec_eval.sx`

---

## Dependency Graph

```
TASK-022 (v0.13.0 complete) + SLM runtime (exists)
    |
    v
simplex-vectordb (foundational — everything else needs this)
    |
    +--> simplex-rag (depends on vectordb + SLM)
    |         |
    |         v
    +--> simplex-guardrails (depends on vectordb for grounding + belief store)
    |
    +--> simplex-eval (depends on SLM, optionally vectordb for similarity)
```

Build order: vectordb first, then rag + guardrails + eval in parallel.

---

## Estimated Line Counts

| Library | Est. Lines |
|---------|-----------|
| simplex-vectordb | ~1,800-2,400 |
| simplex-rag | ~1,200-1,600 |
| simplex-guardrails | ~800-1,200 |
| simplex-eval | ~600-800 |
| **Total** | **~4,400-6,000** |
