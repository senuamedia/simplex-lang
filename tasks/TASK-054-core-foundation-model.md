# TASK-054: Simplex Core Foundation Model

**Version:** 0.18.0
**Status:** Planned
**Priority:** P0 — Critical
**Depends on:** Training data pipeline (TASK-052), training framework (TASK-053)

## Why This Feature Is Needed

This is the model. Everything in v0.5.0 through v0.17.0 has been building toward this
moment: a foundation model that *is* Simplex — trained on the language, built with the
language, running in the language, and improving the language.

The Simplex Core Foundation Model is not a general-purpose LLM. It does not know about
cooking recipes or celebrity gossip. It knows one thing deeply: the Simplex programming
language. Every byte of its training data is Simplex source code, specifications, tests,
documentation, compiler internals, runtime behavior, and error patterns. It is the most
specialized model ever built for a single programming language — because it is built on
an architecture (the Cognitive Substrate) designed from the ground up for specialized
intelligence.

The model architecture uses every component from v0.17.0:
- **Liquid dynamics** for adaptive processing of different code constructs
- **Hyperbolic embeddings** for representing Simplex's type hierarchy and module tree
- **Neuro-symbolic engine** for formal reasoning about code correctness
- **Hopfield memory** for storing language spec, API references, and idiom patterns
- **Tensor network compression** for deployable model size
- **Equivariant layers** for variable-renaming invariance
- **SDM** for learning from each coding session

This model becomes the shared intelligence layer that all specialist models build upon.
It is the "brain" of the Simplex ecosystem.

## Why It Adds Value

1. **Validates the entire stack.** If this model works — if a Cognitive Substrate model
   trained in Simplex outperforms general code models on Simplex tasks — then v0.16.0
   and v0.17.0 are proven. Every architectural decision is validated.

2. **Shared foundation for all specialists.** Every specialist in the hive inherits
   from this base. The code generation specialist, test specialist, compiler specialist
   all share the core model's deep understanding of Simplex.

3. **Native language understanding.** This model doesn't treat Simplex as "some
   programming language." It understands neural gates, cognitive hives, dual numbers,
   belief systems, and contract logic as first-class concepts — because those concepts
   are in its training data and its architecture.

4. **Self-hosted AI.** Simplex is the first language whose primary AI model is trained
   by the language itself. The model is not an external dependency — it is part of the
   language ecosystem, versioned and distributed alongside the compiler.

## Why It Changes Systems Built With Simplex

The core model is the intelligence substrate that every Simplex tool and specialist
builds on:

- **sxlsp** uses the core model for intelligent code completion, hover documentation,
  and error explanation
- **sxc** uses the core model for better error messages and fix suggestions
- **sxlint** uses the core model for semantic lint rules that go beyond syntax
- Every specialist inherits the core model's understanding of Simplex

## Deliverables

### Phase 1: Model Architecture Definition (~500 lines)

Location: `simplex-training/src/models/`

- **CoreModelConfig** — define the Cognitive Substrate architecture for the core model:
  layer count, dimensions, liquid neuron count, hyperbolic embedding dim, Hopfield
  capacity, vocabulary size
- **CoreModel** — the actual model definition combining all substrate components
- **SimplexVocabulary** — vocabulary definition with special tokens for Simplex keywords,
  operators, and annotations
- **PositionEncoding** — position encoding suited for code (AST-aware positioning +
  standard positional)

```simplex
/// Configuration for the Simplex Core Foundation Model
struct CoreModelConfig {
    // Dimensions
    vocab_size: usize,        // Simplex token vocabulary (~8,000)
    hidden_dim: usize,        // model hidden dimension (512 for base, 1024 for large)
    num_layers: usize,        // number of substrate blocks (12 for base, 24 for large)
    max_seq_len: usize,       // maximum sequence length (2048)

    // Liquid dynamics
    liquid_neurons: usize,    // liquid neurons per layer (32)
    liquid_solver: SolverType, // CfC for speed, DormandPrince for accuracy

    // Hyperbolic embeddings
    hyp_dim: usize,           // hyperbolic embedding dimension (64)
    curvature: f64,           // Poincare ball curvature (-1.0)

    // Hopfield memory
    hopfield_patterns: usize, // max stored patterns (10,000)
    hopfield_dim: usize,      // pattern dimension (512)

    // SSM/Attention hybrid
    ssm_layers: usize,        // number of Mamba layers
    attn_layers: usize,       // number of attention layers
    attn_interval: usize,     // attention every N layers

    // Compression
    tt_rank: usize,           // tensor train rank for weight compression (32)
}

/// The Simplex Core Foundation Model
struct CoreModel {
    // Token processing
    embedding: HyperbolicEmbedding,  // tokens → hyperbolic space
    pos_encoding: PositionEncoding,

    // Core processing blocks
    blocks: Vec<SubstrateBlock>,

    // Knowledge
    hopfield: MemoryBank,            // language spec, API, idioms
    symbolic_kb: KnowledgeBase,      // formal rules from the spec

    // Output
    output_head: TTLinear,           // compressed output projection
    config: CoreModelConfig,
}

/// A single block of the Cognitive Substrate
struct SubstrateBlock {
    // Choose SSM or attention based on layer position
    sequence_layer: SequenceLayer,
    // Liquid feedforward (dynamic weights)
    feedforward: CfC,
    // Normalization
    norm1: RMSNorm,
    norm2: RMSNorm,
}

enum SequenceLayer {
    SSM(MambaBlock),
    Attention(HopfieldAttention),  // Hopfield-enhanced attention
}

impl CoreModel {
    fn forward(self: &Self, tokens: &[usize]) -> Tensor {
        // Embed in hyperbolic space
        let mut hidden = self.embedding.forward(tokens);
        hidden = self.pos_encoding.apply(hidden);

        // Process through substrate blocks
        for block in &self.blocks {
            // Sequence processing (SSM or attention)
            let seq_out = match &block.sequence_layer {
                SequenceLayer::SSM(mamba) => mamba.forward(block.norm1.forward(hidden.clone())),
                SequenceLayer::Attention(attn) => {
                    // Augment with Hopfield memory retrieval
                    let memory_context = self.hopfield.recall_batch(&hidden);
                    attn.forward_with_memory(block.norm1.forward(hidden.clone()), &memory_context)
                },
            };
            hidden = hidden + seq_out;

            // Liquid feedforward (dynamic weights per token)
            let ff_out = block.feedforward.forward(block.norm2.forward(hidden.clone()), &hidden);
            hidden = hidden + ff_out;
        }

        // Output projection
        self.output_head.forward(hidden)
    }
}
```

Files:
- `simplex-training/src/models/core_config.sx` — CoreModelConfig
- `simplex-training/src/models/core_model.sx` — CoreModel
- `simplex-training/src/models/vocabulary.sx` — SimplexVocabulary
- `simplex-training/src/models/position.sx` — PositionEncoding
- `simplex-training/src/models/substrate_block.sx` — SubstrateBlock

### Phase 2: Pre-Training Pipeline (~400 lines)

- **PreTrainer** — orchestrates core model pre-training: masked language modeling on
  Simplex source, next-token prediction, and Simplex-specific auxiliary objectives
- **SimplexMLM** — masked language modeling objective adapted for code (mask at statement
  boundaries, not random token positions)
- **ContractPrediction** — auxiliary objective: predict the requires/ensures contract
  for a given function body
- **TypePrediction** — auxiliary objective: predict return types from function bodies

```simplex
/// Pre-training objectives for the core model
struct PreTrainer {
    model: CoreModel,
    trainer: SubstrateTrainer,
    objectives: Vec<Box<dyn TrainingObjective>>,
}

trait TrainingObjective {
    fn compute_loss(self: &Self, model: &CoreModel, batch: &Batch) -> Dual;
    fn weight(self: &Self) -> f64;
}

/// Masked language modeling adapted for Simplex code
struct SimplexMLM {
    mask_ratio: f64,
}

impl TrainingObjective for SimplexMLM {
    fn compute_loss(self: &Self, model: &CoreModel, batch: &Batch) -> Dual {
        // Mask at statement boundaries for code-aware masking
        let (masked_input, mask_positions) =
            mask_at_boundaries(&batch.tokens, self.mask_ratio);
        let logits = model.forward(&masked_input);
        // Loss only on masked positions
        masked_cross_entropy(logits, &batch.tokens, &mask_positions)
    }
    fn weight(self: &Self) -> f64 { 1.0 }
}

/// Predict the contract for a function body
struct ContractPrediction;

impl TrainingObjective for ContractPrediction {
    fn compute_loss(self: &Self, model: &CoreModel, batch: &Batch) -> Dual {
        // Input: function body without contracts
        // Target: the requires/ensures annotations
        let predictions = model.forward(&batch.body_tokens);
        cross_entropy(predictions, &batch.contract_tokens)
    }
    fn weight(self: &Self) -> f64 { 0.3 }
}
```

Files:
- `simplex-training/src/models/pretrain.sx` — PreTrainer
- `simplex-training/src/models/objectives/mlm.sx` — SimplexMLM
- `simplex-training/src/models/objectives/contract.sx` — ContractPrediction
- `simplex-training/src/models/objectives/type_pred.sx` — TypePrediction

### Phase 3: Knowledge Base Population (~350 lines)

- **SpecLoader** — load the Simplex language specification into the Hopfield memory
  bank as retrievable patterns
- **APILoader** — load all API documentation into Hopfield memory
- **IdiomLoader** — extract and store common Simplex code patterns as memory patterns
- **RuleLoader** — load formal syntax and semantic rules into the symbolic knowledge
  base for neuro-symbolic reasoning

Files:
- `simplex-training/src/models/knowledge/spec_loader.sx` — spec → Hopfield
- `simplex-training/src/models/knowledge/api_loader.sx` — API docs → Hopfield
- `simplex-training/src/models/knowledge/idiom_loader.sx` — patterns → Hopfield
- `simplex-training/src/models/knowledge/rule_loader.sx` — rules → symbolic KB

### Phase 4: Model Sizes & Tests (~350 lines)

Define three model sizes:
- **simplex-core-micro** (50M params): for testing and development
- **simplex-core-base** (200M params): for standard development machines
- **simplex-core-large** (500M params): for maximum quality

Tests:
- CoreModel forward pass produces correct output shape
- All substrate components (liquid, hyperbolic, Hopfield, SSM) are exercised
- Pre-training loss decreases over 100 steps
- Hopfield memory retrieves relevant language spec for code queries
- Symbolic KB answers formal queries about Simplex syntax
- Model compresses via tensor networks with <5% quality loss

## Success Criteria

- [ ] simplex-core-micro trains end-to-end on extracted Simplex data
- [ ] Pre-training loss decreases consistently over training
- [ ] CompilationBench: >60% of generated Simplex code compiles
- [ ] Hopfield memory correctly retrieves relevant spec sections for code queries
- [ ] Symbolic KB correctly answers "is this valid Simplex?" for 90% of test cases
- [ ] Compressed model (tensor networks) maintains >95% of uncompressed quality

## Estimated Scope

~1,600 lines across library code and tests.
