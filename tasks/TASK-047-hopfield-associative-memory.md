# TASK-047: Modern Hopfield Associative Memory

**Version:** 0.17.0
**Status:** Complete
**Priority:** P0 — Critical
**Depends on:** v0.16.0 release, existing tensor/attention infrastructure

## Why This Feature Is Needed

The biggest limitation of small models is knowledge capacity. A 7B model stores roughly
7 billion floating-point numbers — that is its entire world knowledge, compressed into
weights. A 500M model has 14x less capacity. This is why SLMs know less, hallucinate
more, and fail on knowledge-intensive tasks. The fix is not "make the model bigger."
The fix is **stop storing knowledge in weights.**

Modern Hopfield Networks (Ramsauer et al., 2020) are associative memories with
**exponential storage capacity**. A network with N neurons can store and perfectly
retrieve approximately `exp(N)` patterns. Classical Hopfield networks (1982) stored
only ~0.14N patterns. This exponential leap changes everything.

The mathematical connection to transformers is deep: the attention mechanism in
transformers is *mathematically equivalent* to a single Hopfield retrieval step. But
modern Hopfield networks go further — they can be used as explicit external memory
layers with guaranteed retrieval, not just implicit attention. Recent work (ICLR 2026,
AAAI 2026) shows that adding Hopfield memory to transformers improves attention quality,
fixes rank collapse in deep networks, and enables retrieval-augmented reasoning without
vector databases.

For Simplex specialist SLMs, this is the key to matching large-model knowledge with
small-model efficiency: **separate knowledge storage from computation.** The model is
small (fast inference). The memory is large (vast knowledge). Retrieval is guaranteed
(modern Hopfield convergence proofs).

## Why It Adds Value

1. **Knowledge without parameters.** Facts, rules, examples, and domain knowledge live
   in Hopfield memory, not in model weights. A 100M-parameter model with a 1M-pattern
   Hopfield memory has the knowledge capacity of a model orders of magnitude larger.

2. **Guaranteed retrieval.** Modern Hopfield networks have provable convergence — given
   a partial or noisy query, they converge to the nearest stored pattern in a fixed
   number of steps. No hallucinated retrievals. No "I think I remember something
   similar."

3. **Dynamic knowledge updates.** Adding new knowledge means adding patterns to the
   memory — no retraining. Removing knowledge means removing patterns. The specialist's
   knowledge base is a mutable data structure, not frozen weights.

4. **Attention enhancement.** Used as an attention layer, modern Hopfield networks
   improve attention quality in deep networks: they fix rank collapse (where all
   attention heads converge to the same pattern) and token uniformity (where all tokens
   get the same attention).

5. **One-shot learning.** Store a single example as a pattern. The model can retrieve
   and use it immediately without gradient updates. A specialist that sees one example
   of a new concept can use it correctly forever.

6. **Natural integration with hive memory.** The cognitive hive's three-tier memory
   (anima, mnemonic, divine) maps directly to Hopfield memory banks at different
   scopes. Content-addressable retrieval replaces the current linear-search memory.

## Why It Changes Systems Built With Simplex

Hopfield memory decouples knowledge from computation:

- **Specialists that know everything about their domain.** A medical specialist's
  Hopfield memory contains every relevant clinical guideline, drug interaction, and
  diagnostic criterion. The model itself is tiny — it just needs to retrieve and reason.

- **Instantly updatable knowledge.** When guidelines change, add new patterns to the
  Hopfield memory. The specialist is immediately updated — no retraining, no fine-tuning,
  no deployment cycle.

- **Hive-wide shared memory.** The HiveMnemonic becomes a shared Hopfield memory bank.
  Any specialist can store and retrieve patterns. Knowledge discovered by one specialist
  is immediately available to all others via associative retrieval.

- **Retrieval-augmented generation without infrastructure.** No vector database, no
  embedding pipeline, no separate retrieval service. The Hopfield memory IS the
  retrieval system — built into the model, differentiable, and trainable end-to-end.

## Deliverables

### Phase 1: Modern Hopfield Core (~500 lines)

Location: `simplex-learning/src/hopfield/`

- **ModernHopfieldLayer** — continuous modern Hopfield network with exponential storage,
  implementing the energy function E = -log(Σ exp(β·⟨ξ_i, x⟩)) and the update rule
- **HopfieldMemory** — explicit memory bank: store patterns, retrieve by query, add/
  remove patterns dynamically
- **HopfieldRetrieval** — single-step and multi-step retrieval with convergence
  guarantee
- **MemoryBank** — persistent storage of patterns with metadata (provenance, timestamp,
  confidence)

```simplex
/// Modern Hopfield Network with exponential storage capacity
struct ModernHopfieldLayer {
    beta: Dual,              // inverse temperature (controls retrieval sharpness)
    stored_patterns: Tensor, // stored memory patterns [num_patterns x pattern_dim]
    pattern_dim: usize,
    num_patterns: usize,
}

impl ModernHopfieldLayer {
    /// Store a new pattern in memory
    fn store(mut self: &mut Self, pattern: Tensor) {
        self.stored_patterns = self.stored_patterns.append_row(pattern);
        self.num_patterns += 1;
    }

    /// Retrieve the nearest stored pattern to query
    fn retrieve(self: &Self, query: &Tensor) -> Tensor {
        // Modern Hopfield update: softmax attention over stored patterns
        // This converges to the nearest stored pattern in 1-3 steps
        let similarities = self.stored_patterns.matmul(&query.t()) * self.beta;
        let attention = softmax(similarities);
        self.stored_patterns.t().matmul(&attention)
    }

    /// Multi-step retrieval for higher precision
    fn retrieve_iterative(self: &Self, query: &Tensor, steps: usize) -> Tensor {
        let mut state = query.clone();
        for _ in 0..steps {
            state = self.retrieve(&state);
        }
        state
    }

    /// Energy function: lower energy = better match
    fn energy(self: &Self, state: &Tensor) -> Dual {
        let similarities = self.stored_patterns.matmul(&state.t()) * self.beta;
        -logsumexp(similarities) + 0.5 * state.dot(state) + self.log_normalization()
    }

    /// Batch retrieval — retrieve for multiple queries in parallel
    fn batch_retrieve(self: &Self, queries: &Tensor) -> Tensor {
        let similarities = queries.matmul(&self.stored_patterns.t()) * self.beta;
        let attention = softmax_rows(similarities);
        attention.matmul(&self.stored_patterns)
    }
}

/// Persistent memory bank with metadata
struct MemoryBank {
    hopfield: ModernHopfieldLayer,
    metadata: Vec<PatternMetadata>,
}

struct PatternMetadata {
    id: String,
    provenance: String,    // where this knowledge came from
    timestamp: u64,
    confidence: f64,
    domain: String,
}

impl MemoryBank {
    /// Store knowledge with metadata
    fn remember(mut self: &mut Self, pattern: Tensor, meta: PatternMetadata) {
        self.hopfield.store(pattern);
        self.metadata.push(meta);
    }

    /// Retrieve with metadata — know where the knowledge came from
    fn recall(self: &Self, query: &Tensor) -> (Tensor, PatternMetadata) {
        let similarities = self.hopfield.stored_patterns.matmul(&query.t()) * self.hopfield.beta;
        let best_idx = similarities.argmax();
        let retrieved = self.hopfield.retrieve(query);
        (retrieved, self.metadata[best_idx].clone())
    }

    /// Forget: remove a pattern by ID
    fn forget(mut self: &mut Self, id: &str) {
        if let Some(idx) = self.metadata.iter().position(|m| m.id == id) {
            self.hopfield.stored_patterns = self.hopfield.stored_patterns.remove_row(idx);
            self.metadata.remove(idx);
            self.hopfield.num_patterns -= 1;
        }
    }
}
```

Files:
- `simplex-learning/src/hopfield/mod.sx` — module root
- `simplex-learning/src/hopfield/layer.sx` — ModernHopfieldLayer
- `simplex-learning/src/hopfield/memory.sx` — HopfieldMemory
- `simplex-learning/src/hopfield/retrieval.sx` — retrieval algorithms
- `simplex-learning/src/hopfield/bank.sx` — MemoryBank with metadata

### Phase 2: Hopfield Attention Integration (~400 lines)

- **HopfieldAttention** — modern Hopfield attention mechanism that improves on standard
  attention by reducing rank collapse and token uniformity
- **HopfieldPooling** — aggregate variable-length sequences into fixed-size
  representations via Hopfield retrieval
- **HopfieldTransformerBlock** — transformer block with Hopfield attention (drop-in
  replacement)
- **CrossMemoryAttention** — attend across multiple memory banks (specialist attends
  to both its own memory and shared hive memory)

Files:
- `simplex-learning/src/hopfield/attention.sx` — HopfieldAttention
- `simplex-learning/src/hopfield/pooling.sx` — HopfieldPooling
- `simplex-learning/src/hopfield/transformer.sx` — HopfieldTransformerBlock
- `simplex-learning/src/hopfield/cross_memory.sx` — CrossMemoryAttention

### Phase 3: Hive Memory Integration (~400 lines)

- **AnimaHopfield** — specialist's personal memory as a Hopfield bank
- **MnemonicHopfield** — shared hive memory as a Hopfield bank with access control
- **DivineHopfield** — solution-wide memory with federated pattern aggregation
- **MemoryConsolidation** — periodic consolidation of episodic memories into semantic
  patterns (inspired by sleep consolidation in neuroscience)

Files:
- `simplex-learning/src/hopfield/anima.sx` — AnimaHopfield
- `simplex-learning/src/hopfield/mnemonic.sx` — MnemonicHopfield
- `simplex-learning/src/hopfield/divine.sx` — DivineHopfield
- `simplex-learning/src/hopfield/consolidation.sx` — MemoryConsolidation

### Phase 4: Tests (~350 lines)

Location: `tests/hopfield/`

- Store and perfectly retrieve 100 random patterns in 64-dim network
- Retrieval with noisy query (20% noise) recovers correct pattern
- Energy decreases monotonically during iterative retrieval
- MemoryBank add/remove/query lifecycle
- HopfieldAttention outperforms standard attention on synthetic rank-collapse test
- Memory consolidation merges similar episodic patterns into semantic clusters

## Success Criteria

- [ ] Store and retrieve 1000 patterns in 128-dim network with >99% accuracy
- [ ] Noisy retrieval (30% corruption) recovers correct pattern >95% of time
- [ ] Dynamic add/remove patterns without degrading retrieval of remaining patterns
- [ ] HopfieldAttention reduces attention entropy vs standard attention (less uniform)
- [ ] MemoryBank metadata correctly tracks provenance for all stored patterns
- [ ] Energy function decreases monotonically across retrieval steps (convergence proof)

## Estimated Scope

~1,650 lines across library code and tests.
