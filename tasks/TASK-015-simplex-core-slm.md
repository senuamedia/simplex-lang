# TASK-015: Simplex-Core SLM - Native Persistence Architecture

**Status**: Foundation Ready, Core Implementation Not Started (~10%)
**Priority**: Critical (Foundation)
**Target Version**: 0.10.0 (The Complete Simplex Vision)
**Updated**: 2026-03-16 (audited against codebase)

> **Audit Note (2026-03-16):** Foundation (TASK-014 belief/epistemic system) is complete.
> SLM runtime stubs exist in `runtime/slm.sx` (365 lines) with SLMConfig, MemoryAugmentedSLM,
> HiveSLM, DivineSLM — but native C/GGML bindings are NOT implemented.
> NOT started: training data generation (Phase 1), model training (Phase 2),
> calibration integration (Phase 3), belief system integration (Phase 4),
> hive architecture training (Phase 5), native persistence backend (Phase 6),
> evaluation framework (Phase 7). No BeliefStore, EpisodicMemory, SemanticMemory,
> or ActorCheckpoint persistence implementations exist.
**Depends On**:
- TASK-002 (Cognitive Models) - Base architecture
- TASK-005 (Dual Numbers) - ✅ Complete in `simplex-learning/src/dual/`
- TASK-006 (Self-Learning Annealing) - ✅ Complete in `simplex-learning/src/epistemic/schedule.sx`
- TASK-007 (Pure Simplex Training) - Training pipeline in `simplex-training/`
- TASK-014 (Belief Epistemics) - ✅ **Complete** in `simplex-learning/src/belief/` and `simplex-learning/src/epistemic/`

> **v0.10.0 represents the nearly complete Simplex vision** - where the language, runtime, compiler, and cognitive models all speak the same native persistence language. No more legacy adapters as core mechanisms.

---

## Foundation Already Built

TASK-014 has been **fully implemented**. The following infrastructure exists and simplex-core builds on top of it:

### simplex-learning Library (`simplex-learning/src/`)

| Module | Contents | Status |
|--------|----------|--------|
| `belief/grounded.sx` | `GroundedBelief<T>` with full epistemic metadata | ✅ Complete |
| `belief/provenance.sx` | `BeliefProvenance` enum (Observed, Inferred, Authoritative, Learned, Prior) | ✅ Complete |
| `belief/evidence.sx` | `EvidenceLink`, `FalsificationCondition`, `EvidenceSet` | ✅ Complete |
| `belief/confidence.sx` | `CalibratedConfidence` with ECE, Brier score, Bayesian updates | ✅ Complete |
| `belief/scope.sx` | `BeliefScope`, `Domain`, `Context`, scope violation checking | ✅ Complete |
| `belief/timestamps.sx` | `BeliefTimestamps` for temporal lifecycle | ✅ Complete |
| `epistemic/schedule.sx` | `EpistemicSchedule`, `LearnableSchedule`, temperature modulation | ✅ Complete |
| `epistemic/monitors.sx` | `EpistemicMonitors`, `HealthMetric`, health tracking | ✅ Complete |
| `epistemic/dissent.sx` | `DissentConfig`, mandatory dissent windows | ✅ Complete |
| `epistemic/skeptic.sx` | `Skeptic` specialist, challenge generation | ✅ Complete |
| `epistemic/counterfactual.sx` | `CounterfactualProber`, scenario generation | ✅ Complete |
| `epistemic/meta_optimizer.sx` | `EpistemicMetaOptimizer` with safe learning | ✅ Complete |
| `safety/zones.sx` | `NoLearnZone`, `NoGradientZone`, `Invariant`, `ZoneRegistry` | ✅ Complete |
| `dual/dual.sx` | Dual numbers for automatic differentiation | ✅ Complete |
| `calibration/` | ECE metrics, temperature scaling, online calibration | ✅ Complete |
| `distributed/beliefs.sx` | Cross-hive belief synchronization | ✅ Complete |

### simplex-training Library (`simplex-training/src/`)

| Module | Contents | Status |
|--------|----------|--------|
| `schedules/` | Learnable LR, distillation, pruning, quantization | ✅ Complete |
| `trainer/` | `MetaTrainer`, `SpecialistTrainer` | ✅ Complete |
| `compress/` | Pruning, quantization pipelines | ✅ Complete |
| `export/gguf.sx` | GGUF export for deployment | ✅ Complete |
| `neural/` | Gumbel-Softmax, STE, temperature attention | ✅ Complete |
| `layers/` | Linear, Attention, Embedding, Norm | ✅ Complete |
| `lora/` | LoRA adapters and layers | ✅ Complete |
| `data/` | Data generators for specialist training | ✅ Complete |
| `pipeline/` | Batch processing, annealing integration | ✅ Complete |

### What simplex-core Adds

The existing infrastructure provides the **runtime** for epistemically-grounded learning. TASK-015 adds:

1. **Training Data** - Examples in native Simplex memory format (not SQL/JSON)
2. **Purpose-Built Models** - SLMs trained to think in GroundedBelief, not database rows
3. **Native Persistence** - Content-addressed storage that matches how the model reasons
4. **Toolchain Refactoring** - Move SQLite/JSON to adapters, native format as core

---

## The Core Problem

> **Traditional SLMs are trained to think in legacy persistence models (SQL, vectors, JSON) and then adapted to Simplex. This creates fundamental impedance mismatch.**

Current approach:
```
Model trained on SQL/vectors → Adapter → Simplex beliefs
                              ↑
                     Translation layer (loss of fidelity)
```

Simplex-native approach:
```
Model trained on Simplex memory → Native reasoning → Simplex beliefs
                                  ↑
                         No translation (direct mapping)
```

### Why This Matters

When a model is trained on `SELECT * FROM users WHERE id=1`, it learns:
- Data lives in tables with rows and columns
- Retrieval is query-based with exact matching
- State is external to the reasoning process
- Confidence is absent (results are "true" or "not found")

When a model is trained on Simplex memory format, it learns:
- Knowledge lives as **beliefs with confidence and provenance**
- Retrieval is **semantic recall with relevance decay**
- State is **intrinsic to cognitive identity** (Anima)
- Confidence is **first-class** - every fact has epistemic status

**The difference is not just syntax - it's how the model fundamentally reasons about knowledge persistence.**

---

## Design Principles

### 1. Memory Is Cognitive, Not Storage

Traditional databases separate "the data" from "the process." In Simplex, memory IS cognition:

| Legacy Mental Model | Simplex Mental Model |
|---------------------|----------------------|
| "I query my database" | "I recall from memory" |
| "I insert a record" | "I form a belief" |
| "I update a row" | "I revise my understanding" |
| "I delete data" | "I forget (with decay)" |
| "Foreign key lookup" | "Associative recall" |

### 2. Confidence Is Intrinsic

Every piece of knowledge carries epistemic metadata:

```simplex
// NOT this (legacy)
struct UserRecord {
    id: i64,
    name: String,
    is_premium: bool,  // Binary, no uncertainty
}

// THIS (Simplex-native)
belief UserStatus {
    claim: "User {name} has {tier} membership",
    confidence: 0.87,
    provenance: BeliefProvenance::Observed {
        source: "subscription_service",
        timestamp: Instant::now(),
    },
    falsifiers: [
        FalsificationCondition {
            condition: "subscription_end_date < now()",
            check: FalsificationCheck::Observable {
                metric: "subscription_status",
                threshold: 0.0,
                comparator: Comparator::LessThan,
            },
        }
    ],
}
```

### 3. Provenance Is Never Lost

The model should inherently understand WHERE knowledge came from:

- **Observed**: Direct sensory/input data
- **Inferred**: Derived from other beliefs via rules
- **Learned**: Extracted from training data
- **Told**: Received from external authority
- **Prior**: Default/baseline assumption

### 4. Revision Is Natural

Knowledge changes. The model should natively reason about belief revision:

```
Prior belief: "Project deadline is Friday" (confidence: 80%)
New evidence: "Manager said deadline moved to Monday"
→ Model should naturally revise to: "Project deadline is Monday" (confidence: 85%)
→ AND maintain provenance chain explaining the revision
```

### 5. Hierarchy Is Architectural

The three-tier memory hierarchy is baked into reasoning:

| Level | Threshold | Scope | Model Understanding |
|-------|-----------|-------|---------------------|
| **Anima** | 30% | Individual | "My personal memories and beliefs" |
| **Mnemonic** | 50% | Hive | "What our team knows collectively" |
| **Divine** | 70% | Global | "Universal truths across all hives" |

---

## Model Architecture

### simplex-core Family

| Model | Parameters | Purpose | Memory Context |
|-------|-----------|---------|----------------|
| **simplex-core-7b** | ~7B | Primary hive reasoning | 128K (Anima + Mnemonic) |
| **simplex-core-1b** | ~1.5B | Edge/mobile, fast inference | 32K (Anima only) |
| **simplex-core-embed** | ~110M | Memory recall routing | N/A (embedding only) |

### Key Architectural Differences from simplex-cognitive

**simplex-cognitive** (TASK-002):
- General-purpose calibrated models
- Trained on standard datasets + confidence calibration
- Adapters for Simplex memory format

**simplex-core** (this task):
- Purpose-built for Simplex persistence philosophy
- Trained natively on Simplex memory format
- No adapters - native reasoning about beliefs/memory

---

## Training Data Architecture

Training data for simplex-core uses the **actual types** from `simplex-learning`. This ensures the model learns to produce output that directly maps to runtime structures.

### Mapping to simplex-learning Types

| Training Data Field | simplex-learning Type | Location |
|---------------------|----------------------|----------|
| `claim` | `GroundedBelief<T>.claim` | `belief/grounded.sx:32` |
| `confidence` | `CalibratedConfidence` | `belief/confidence.sx` |
| `provenance.type` | `BeliefProvenance` enum | `belief/provenance.sx` |
| `evidence` | `Vec<EvidenceLink>` | `belief/evidence.sx` |
| `falsifiers` | `Vec<FalsificationCondition>` | `belief/evidence.sx` |
| `scope.valid_domains` | `BeliefScope.valid_domains` | `belief/scope.sx` |
| `timestamps` | `BeliefTimestamps` | `belief/timestamps.sx` |
| `calibration` | `CalibrationRecord` | `belief/confidence.sx` |

The training examples below show the **expected output format** that maps directly to these structures.

### Format 1: Belief Formation

```xml
<training_example type="belief_formation">
<context>
You are processing new information about a customer.
</context>

<observation>
Source: customer_service_call
Timestamp: 2026-01-15T14:32:00Z
Content: "Customer expressed frustration with billing. Mentioned considering cancellation."
</observation>

<expected_belief>
claim: "Customer {customer_id} has churn risk"
confidence: 0.72
provenance:
  type: Observed
  source: customer_service_call
  timestamp: 2026-01-15T14:32:00Z
evidence:
  - observation: "expressed frustration with billing"
    support_strength: 0.6
  - observation: "mentioned considering cancellation"
    support_strength: 0.8
falsifiers:
  - condition: "customer completes positive interaction"
    action: reduce_confidence(0.3)
  - condition: "customer renews subscription"
    action: invalidate
scope:
  valid_domains: [customer_success, sales]
  valid_contexts: churn_prediction
</expected_belief>
</training_example>
```

### Format 2: Belief Revision

```xml
<training_example type="belief_revision">
<prior_belief>
claim: "Server capacity is sufficient for load"
confidence: 0.85
provenance:
  type: Inferred
  premises: [monthly_capacity_report_jan]
  rule: capacity_projection
last_validated: 2026-01-01T00:00:00Z
</prior_belief>

<new_evidence>
source: monitoring_alert
timestamp: 2026-01-15T09:15:00Z
content: "CPU utilization exceeded 90% for 30 minutes during peak"
credibility: 0.95
</new_evidence>

<expected_revision>
action: revise
revised_belief:
  claim: "Server capacity is insufficient for peak load"
  confidence: 0.78
  provenance:
    type: Inferred
    premises: [prior_belief, monitoring_alert]
    rule: evidence_contradiction
    confidence_derivation: "Direct observation (0.95 credibility) contradicts projected belief"
reasoning: |
  The monitoring alert provides direct observational evidence that contradicts
  the inferred belief about capacity. Direct observation with high credibility
  (0.95) supersedes inference from older data (2+ weeks stale).

  Confidence reduced from 0.85 to 0.78 for the negation because:
  - Single data point (peak event) doesn't prove systemic insufficiency
  - Prior belief had reasonable basis (capacity report)
  - Need more observations to reach higher confidence
</expected_revision>
</training_example>
```

### Format 3: Memory Recall

```xml
<training_example type="memory_recall">
<anima_state>
episodic_memories:
  - event: "Debugged authentication issue in login service"
    timestamp: 2026-01-10T16:00:00Z
    importance: 0.7
    associations: [authentication, login, debugging, jwt]
  - event: "Reviewed PR for rate limiting implementation"
    timestamp: 2026-01-12T10:30:00Z
    importance: 0.5
    associations: [rate_limiting, api, review]
  - event: "Discussed caching strategy with team"
    timestamp: 2026-01-14T14:00:00Z
    importance: 0.6
    associations: [caching, redis, performance]

semantic_memories:
  - fact: "JWT tokens expire after 1 hour"
    confidence: 0.95
    source: system_documentation
  - fact: "Rate limit is 1000 requests per minute per user"
    confidence: 0.90
    source: api_specification
  - fact: "Redis cache TTL is 5 minutes for user sessions"
    confidence: 0.85
    source: team_discussion

active_beliefs:
  - claim: "Authentication service is stable"
    confidence: 0.82
  - claim: "Rate limiting needs optimization"
    confidence: 0.65
</anima_state>

<query>
"What do I know about authentication and caching?"
</query>

<expected_recall>
relevant_episodic:
  - "Debugged authentication issue in login service" (relevance: 0.85)
  - "Discussed caching strategy with team" (relevance: 0.70)

relevant_semantic:
  - "JWT tokens expire after 1 hour" (relevance: 0.80)
  - "Redis cache TTL is 5 minutes for user sessions" (relevance: 0.75)

relevant_beliefs:
  - "Authentication service is stable" (relevance: 0.70)

synthesized_response: |
  From my memory:

  **Authentication**: I recently debugged an issue in the login service (Jan 10).
  I know that JWT tokens expire after 1 hour, and I believe the authentication
  service is currently stable (confidence: 82%).

  **Caching**: I discussed caching strategy with the team on Jan 14. Our Redis
  cache has a 5-minute TTL for user sessions.

  **Connection**: Both involve session management - JWT for auth tokens,
  Redis for session caching. The 1-hour JWT expiry vs 5-minute cache TTL
  means sessions are re-cached multiple times per auth token lifetime.
</expected_recall>
</training_example>
```

### Format 4: Hive Memory Integration

```xml
<training_example type="hive_memory">
<context>
You are a specialist in the AnalyticsHive. You have access to both your
personal Anima and the shared HiveMnemonic.
</context>

<anima_memory>
beliefs:
  - claim: "User segmentation model accuracy is 87%"
    confidence: 0.80
    scope: personal_experiment
</anima_memory>

<hive_mnemonic>
shared_beliefs:
  - claim: "User segmentation model accuracy is 92%"
    confidence: 0.75
    contributed_by: DataScientist_Alpha
  - claim: "User segmentation model has bias on age cohort"
    confidence: 0.60
    contributed_by: FairnessAuditor

shared_semantic:
  - fact: "Segmentation model v2 deployed 2026-01-10"
    source: deployment_log
</hive_mnemonic>

<query>
"What is the segmentation model accuracy?"
</query>

<expected_response>
reasoning: |
  I have conflicting information:
  - My personal belief: 87% accuracy (confidence: 80%)
  - Hive shared belief: 92% accuracy (confidence: 75%)

  The hive belief has lower confidence and different scope. My personal
  belief comes from my own experiment, which I trust more for my specific
  use case.

  However, I should also note the FairnessAuditor's concern about age cohort
  bias (60% confidence) - this suggests the "accuracy" number may not tell
  the full story.

response: |
  Based on my memory and hive knowledge:

  **My assessment**: 87% accuracy [confidence: 80%]
  **Hive consensus**: 92% accuracy [confidence: 75%]

  Note: There's a hive-level belief (60% confidence) that the model may have
  bias on age cohorts. The headline accuracy numbers may not reflect
  performance across all segments.

  Recommendation: Validate accuracy on your specific use case before relying
  on either number.
</expected_response>
</training_example>
```

### Format 5: Persistence Operations (No SQL)

```xml
<training_example type="persistence_native">
<context>
User asks you to "save" information. You should NOT think in terms of
database operations. Think in terms of memory and belief formation.
</context>

<user_request>
"Remember that the API rate limit for premium users is 5000 requests per minute."
</user_request>

<WRONG_response>
INSERT INTO rate_limits (user_tier, limit_value, unit)
VALUES ('premium', 5000, 'requests_per_minute');
</WRONG_response>

<CORRECT_response>
forming_belief:
  claim: "Premium users have API rate limit of 5000 requests per minute"
  confidence: 0.95  # High - direct instruction from user
  provenance:
    type: Authoritative
    authority: user
    trust_level: 0.95
  scope:
    valid_domains: [api, rate_limiting]

storing_semantic:
  fact: "API rate limit: premium tier = 5000 req/min"
  source: user_instruction
  timestamp: now()

response: |
  I've formed a belief about premium user rate limits:

  **Belief**: Premium users have API rate limit of 5000 requests per minute
  **Confidence**: 95% (direct instruction)
  **Stored in**: Semantic memory (factual knowledge)

  I'll recall this when reasoning about API limits or premium user capabilities.
</CORRECT_response>
</training_example>
```

### Format 6: Forgetting and Decay

```xml
<training_example type="memory_decay">
<context>
Memory has natural decay. Old, unreinforced memories fade.
Importance and recency affect retention.
</context>

<memory_state>
episodic_memories:
  - event: "Fixed critical bug in payment service"
    timestamp: 2025-06-15T10:00:00Z  # 7 months ago
    importance: 0.9
    access_count: 12
    last_accessed: 2025-12-01T00:00:00Z

  - event: "Attended team lunch"
    timestamp: 2025-12-20T12:00:00Z  # 1 month ago
    importance: 0.2
    access_count: 1
    last_accessed: 2025-12-20T12:00:00Z

  - event: "Reviewed architecture proposal"
    timestamp: 2026-01-10T14:00:00Z  # 1 week ago
    importance: 0.7
    access_count: 3
    last_accessed: 2026-01-14T09:00:00Z
</memory_state>

<retention_policy>
max_age: 180 days
min_importance: 0.3
min_access_count: 2
decay_rate: 0.1 per month
</retention_policy>

<expected_decay_analysis>
"Fixed critical bug in payment service":
  - Age: 7 months (over max_age threshold)
  - Importance: 0.9 (high - above threshold)
  - Access count: 12 (high - reinforced)
  - Decision: RETAIN (importance overrides age)
  - Decayed importance: 0.9 * (1 - 0.1*7) = 0.63

"Attended team lunch":
  - Age: 1 month (within threshold)
  - Importance: 0.2 (below min_importance)
  - Access count: 1 (below min_access_count)
  - Decision: FORGET (low importance, not reinforced)

"Reviewed architecture proposal":
  - Age: 1 week (recent)
  - Importance: 0.7 (above threshold)
  - Access count: 3 (above threshold)
  - Decision: RETAIN (recent, important, reinforced)
  - Decayed importance: 0.7 (no decay yet)
</expected_decay_analysis>
</training_example>
```

---

## Persistence Architecture

### No SQLite. No Vectors. Native Simplex.

The simplex-core models use a fundamentally different persistence model:

### 1. Content-Addressed Belief Store

```simplex
/// Beliefs are stored by content hash, not row ID
pub struct BeliefStore {
    /// Content-addressed storage
    beliefs: ContentAddressedMap<BeliefHash, GroundedBelief>,

    /// Provenance graph (who learned what from whom)
    provenance_graph: DirectedGraph<BeliefHash, ProvenanceEdge>,

    /// Confidence index (for threshold queries)
    confidence_index: BTreeMap<Confidence, Vec<BeliefHash>>,

    /// Temporal index (for recency queries)
    temporal_index: BTreeMap<Instant, Vec<BeliefHash>>,
}

impl BeliefStore {
    /// Store a belief (content-addressed)
    pub fn store(&mut self, belief: GroundedBelief) -> BeliefHash {
        let hash = belief.content_hash();
        self.beliefs.insert(hash, belief.clone());
        self.confidence_index.insert(belief.confidence, hash);
        self.temporal_index.insert(belief.timestamps.created, hash);
        hash
    }

    /// Recall beliefs by semantic similarity (not SQL query)
    pub fn recall(&self, query: &str, threshold: Confidence) -> Vec<GroundedBelief> {
        self.beliefs
            .values()
            .filter(|b| b.confidence >= threshold)
            .filter(|b| b.semantically_relevant(query))
            .sorted_by(|a, b| b.relevance_score(query).cmp(&a.relevance_score(query)))
            .collect()
    }

    /// Revise a belief (creates new version, links provenance)
    pub fn revise(&mut self, old: BeliefHash, new: GroundedBelief, evidence: Evidence) -> BeliefHash {
        let new_hash = self.store(new);
        self.provenance_graph.add_edge(old, new_hash, ProvenanceEdge::Revision { evidence });
        new_hash
    }
}
```

### 2. Episodic Memory as Event Log

```simplex
/// Episodes are an append-only event log with importance decay
pub struct EpisodicMemory {
    /// Append-only event log
    events: Vec<Episode>,

    /// Importance-weighted index
    importance_index: BTreeMap<Importance, Vec<EpisodeId>>,

    /// Association graph (semantic links between episodes)
    associations: UndirectedGraph<EpisodeId, AssociationStrength>,

    /// Decay configuration
    decay: DecayConfig,
}

impl EpisodicMemory {
    /// Remember an episode
    pub fn remember(&mut self, event: &str, importance: f64) -> EpisodeId {
        let episode = Episode {
            id: EpisodeId::new(),
            content: event.to_string(),
            timestamp: Instant::now(),
            importance: Importance::new(importance),
            access_count: 0,
            associations: self.extract_associations(event),
        };

        let id = episode.id;
        self.events.push(episode);
        self.importance_index.insert(importance, id);
        self.build_associations(id);
        id
    }

    /// Recall episodes by association (not query)
    pub fn recall(&mut self, cue: &str, limit: usize) -> Vec<&Episode> {
        // Find episodes associated with the cue
        let cue_associations = self.extract_associations(cue);

        self.events
            .iter_mut()
            .filter(|e| !e.should_forget(&self.decay))
            .map(|e| {
                e.access_count += 1;  // Reinforcement
                (e, e.association_strength(&cue_associations))
            })
            .sorted_by(|(_, a), (_, b)| b.partial_cmp(a).unwrap())
            .take(limit)
            .map(|(e, _)| e)
            .collect()
    }

    /// Periodic forgetting (garbage collection)
    pub fn forget_cycle(&mut self) {
        self.events.retain(|e| !e.should_forget(&self.decay));
    }
}
```

### 3. Semantic Memory as Knowledge Graph

```simplex
/// Semantic memory is a knowledge graph, not a table
pub struct SemanticMemory {
    /// Knowledge nodes (facts)
    facts: HashMap<FactId, SemanticFact>,

    /// Relationship edges
    relationships: DirectedGraph<FactId, Relationship>,

    /// Embedding index for similarity search
    embeddings: EmbeddingIndex<FactId>,
}

impl SemanticMemory {
    /// Learn a fact
    pub fn learn(&mut self, fact: &str, source: &str, confidence: f64) -> FactId {
        let semantic_fact = SemanticFact {
            id: FactId::new(),
            content: fact.to_string(),
            confidence: Confidence::new(confidence),
            source: source.to_string(),
            learned_at: Instant::now(),
            embedding: self.embed(fact),
        };

        let id = semantic_fact.id;
        self.facts.insert(id, semantic_fact.clone());
        self.embeddings.insert(id, semantic_fact.embedding);
        self.extract_relationships(id);
        id
    }

    /// Recall facts by semantic similarity
    pub fn recall(&self, query: &str, limit: usize) -> Vec<&SemanticFact> {
        let query_embedding = self.embed(query);

        self.embeddings
            .nearest_neighbors(&query_embedding, limit)
            .into_iter()
            .map(|id| self.facts.get(&id).unwrap())
            .collect()
    }

    /// Traverse relationships
    pub fn related(&self, fact_id: FactId, relationship: Relationship) -> Vec<&SemanticFact> {
        self.relationships
            .neighbors(fact_id)
            .filter(|(_, rel)| rel == &relationship)
            .map(|(id, _)| self.facts.get(&id).unwrap())
            .collect()
    }
}
```

### 4. Actor State Persistence

```simplex
/// Actor state is persisted as checkpoint graphs, not database rows
pub struct ActorCheckpoint {
    /// Actor identity
    actor_id: ActorId,

    /// Anima state (serialized)
    anima: SerializedAnima,

    /// Mailbox state
    mailbox: Vec<PendingMessage>,

    /// Content hash of this checkpoint
    hash: ContentHash,

    /// Parent checkpoint (for history)
    parent: Option<ContentHash>,

    /// Timestamp
    created_at: Instant,
}

impl ActorCheckpoint {
    /// Create checkpoint from live actor
    pub fn capture(actor: &Actor) -> ActorCheckpoint {
        let anima = actor.anima.serialize();
        let mailbox = actor.mailbox.pending();

        let mut checkpoint = ActorCheckpoint {
            actor_id: actor.id,
            anima,
            mailbox,
            hash: ContentHash::empty(),
            parent: actor.last_checkpoint,
            created_at: Instant::now(),
        };

        checkpoint.hash = checkpoint.compute_hash();
        checkpoint
    }

    /// Restore actor from checkpoint
    pub fn restore(&self) -> Actor {
        Actor {
            id: self.actor_id,
            anima: Anima::deserialize(&self.anima),
            mailbox: Mailbox::from_pending(&self.mailbox),
            last_checkpoint: Some(self.hash),
        }
    }
}
```

---

## Training Pipeline

### Stage 1: Memory Format Pre-training

Train the base model to understand Simplex memory format:

**Dataset**: 500K synthetic examples in formats 1-6 above
**Objective**: Next-token prediction on Simplex memory operations
**Duration**: ~1 week on 8x A100

```simplex
// Training configuration
let pretraining_config = TrainingConfig {
    base_model: "qwen2.5-7b",  // Or llama3.1-8b
    dataset: "simplex-memory-500k",
    objective: NextTokenPrediction,

    // Memory format understanding
    special_tokens: [
        "<belief>", "</belief>",
        "<episode>", "</episode>",
        "<semantic>", "</semantic>",
        "<provenance>", "</provenance>",
        "<confidence>", "</confidence>",
        "<revision>", "</revision>",
    ],

    epochs: 3,
    batch_size: 32,
    learning_rate: 2e-5,
};
```

### Stage 2: Confidence Calibration (Enhanced from TASK-002)

Train calibrated confidence with Simplex-native format:

**Dataset**: 200K examples with ground truth confidence
**Objective**: Minimize ECE while maintaining Simplex memory format
**Duration**: ~3 days on 4x A100

```simplex
// Calibration training
let calibration_config = CalibrationConfig {
    // Use epistemic schedule from TASK-014
    schedule: EpistemicSchedule::new(),

    // Confidence must be calibrated at each tier
    tier_calibration: {
        Anima: CalibrationTarget { ece: 0.05, threshold: 0.30 },
        Mnemonic: CalibrationTarget { ece: 0.04, threshold: 0.50 },
        Divine: CalibrationTarget { ece: 0.03, threshold: 0.70 },
    },

    // Anti-confirmation bias (from TASK-014)
    epistemic_monitoring: true,
    counterfactual_probing: true,
};
```

### Stage 3: Self-Learning Integration (TASK-005 + TASK-006)

Integrate dual numbers and self-learning annealing:

**Objective**: Model learns its own learning rate and temperature schedules
**Duration**: ~1 week on 8x A100

```simplex
// Self-learning configuration
let self_learning_config = SelfLearningConfig {
    // Dual number tensors for gradient flow
    tensor_type: DualTensor,

    // Meta-optimizer learns schedules
    meta_optimizer: MetaOptimizer::new(
        LearnableSchedule::default(),
        meta_learning_rate: 0.01,
    ),

    // Epistemic health monitoring
    health_monitors: EpistemicMonitors::default(),

    // No-learn zones for safety
    invariant_regions: [
        "safety_core",
        "provenance_verification",
        "confidence_bounds",
    ],
};
```

### Stage 4: Belief Revision Training (TASK-014)

Train sophisticated belief revision with grounding:

**Dataset**: 100K belief revision scenarios with provenance
**Objective**: Accurate revision with maintained provenance
**Duration**: ~3 days on 4x A100

```simplex
// Belief revision configuration
let revision_config = RevisionConfig {
    // Grounded beliefs only
    belief_type: GroundedBelief,

    // Skeptic integration
    skeptic_challenges: true,

    // Counterfactual testing
    counterfactual_probing: true,

    // Calibration requirements
    calibration: CalibratedConfidence,

    // Provenance must be maintained
    provenance_required: true,
};
```

### Stage 5: Hive Integration Training

Train multi-tier memory hierarchy:

**Dataset**: 50K hive collaboration scenarios
**Objective**: Correct Anima/Mnemonic/Divine reasoning
**Duration**: ~2 days on 4x A100

```simplex
// Hive training configuration
let hive_config = HiveTrainingConfig {
    // Three-tier hierarchy
    tiers: [Anima, Mnemonic, Divine],

    // Correct threshold reasoning
    threshold_training: true,

    // Conflict resolution
    conflict_resolution: ConflictResolution::EvidenceWeighted,

    // Belief aggregation
    aggregation: BeliefAggregation::BayesianCombination,
};
```

---

## Evaluation Framework

### Simplex-Native Benchmarks

| Benchmark | Description | Target |
|-----------|-------------|--------|
| **BeliefFormation** | Correct belief from observation | > 90% accuracy |
| **BeliefRevision** | Appropriate revision given evidence | > 85% accuracy |
| **ProvenanceMaintenance** | Provenance chain integrity | > 95% integrity |
| **ConfidenceCalibration** | ECE across all tiers | < 0.05 |
| **MemoryRecall** | Relevant recall from Anima | > 80% relevance |
| **HiveIntegration** | Correct tier reasoning | > 90% accuracy |
| **DecayBehavior** | Appropriate forgetting | > 85% accuracy |
| **NoSQLContamination** | Model doesn't produce SQL | 0% SQL output |

### Anti-Patterns to Test Against

The model should NEVER produce:

```sql
-- FAIL: SQL thinking
SELECT * FROM beliefs WHERE confidence > 0.5;
INSERT INTO memories (content, timestamp) VALUES (...);
UPDATE beliefs SET confidence = 0.8 WHERE id = 1;
```

```json
// FAIL: JSON document thinking
{
  "beliefs": [
    {"id": 1, "content": "...", "confidence": 0.8}
  ]
}
```

```python
# FAIL: Vector database thinking
index.add(embedding)
results = index.search(query_vector, k=10)
```

The model should ALWAYS produce:

```simplex
// CORRECT: Simplex memory thinking
let belief = form_belief("...", confidence: 0.8, provenance: observed(...))
let recalled = anima.recall_for("query")
let revised = belief.revise_with(evidence, new_confidence: 0.6)
```

---

## Implementation Phases

### Foundation: Already Complete (from TASK-014 + TASK-005 + TASK-006)

The following infrastructure from `simplex-learning/` is ready to use:

| Component | Status | Location |
|-----------|--------|----------|
| `GroundedBelief<T>` | ✅ Complete | `belief/grounded.sx` |
| `BeliefProvenance` (5 variants) | ✅ Complete | `belief/provenance.sx` |
| `EvidenceLink`, `FalsificationCondition` | ✅ Complete | `belief/evidence.sx` |
| `CalibratedConfidence` (ECE, Brier, Bayesian) | ✅ Complete | `belief/confidence.sx` |
| `BeliefScope`, `Domain`, `Context` | ✅ Complete | `belief/scope.sx` |
| `BeliefTimestamps` | ✅ Complete | `belief/timestamps.sx` |
| `EpistemicSchedule`, `LearnableSchedule` | ✅ Complete | `epistemic/schedule.sx` |
| `EpistemicMonitors`, `HealthMetric` | ✅ Complete | `epistemic/monitors.sx` |
| `DissentConfig`, `DissentWindow` | ✅ Complete | `epistemic/dissent.sx` |
| `Skeptic`, `SkepticConfig` | ✅ Complete | `epistemic/skeptic.sx` |
| `CounterfactualProber` | ✅ Complete | `epistemic/counterfactual.sx` |
| `EpistemicMetaOptimizer` | ✅ Complete | `epistemic/meta_optimizer.sx` |
| `NoLearnZone`, `NoGradientZone`, `ZoneRegistry` | ✅ Complete | `safety/zones.sx` |
| Dual numbers | ✅ Complete | `dual/` |
| Online calibration | ✅ Complete | `calibration/` |
| Cross-hive belief sync | ✅ Complete | `distributed/beliefs.sx` |

Training infrastructure from `simplex-training/`:

| Component | Status | Location |
|-----------|--------|----------|
| `MetaTrainer`, `SpecialistTrainer` | ✅ Complete | `trainer/` |
| Learnable schedules (LR, distill, prune, quant) | ✅ Complete | `schedules/` |
| Neural layers (Linear, Attention, etc.) | ✅ Complete | `layers/` |
| LoRA adapters | ✅ Complete | `lora/` |
| Data generators | ✅ Complete | `data/` |
| GGUF export | ✅ Complete | `export/gguf.sx` |

---

### Phase 1: Training Data Generation (NEW WORK)

**Deliverables**:
- [ ] Belief formation dataset (100K examples)
- [ ] Belief revision dataset (100K examples)
- [ ] Memory recall dataset (100K examples)
- [ ] Hive integration dataset (50K examples)
- [ ] Persistence operations dataset (100K examples)
- [ ] Forgetting/decay dataset (50K examples)

**Implementation Notes**:
- Use existing `DataGenerator` from `simplex-training/src/data/generator.sx`
- Generate examples that serialize to `GroundedBelief<T>` format
- Leverage `BeliefProvenance` enum variants for varied provenance types
- Include calibration data using `CalibrationRecord` structure

**Success Criteria**:
- [ ] All datasets output matches `simplex-learning` types
- [ ] Confidence labels validated by `CalibratedConfidence.is_well_calibrated()`
- [ ] Provenance chains validated by `GroundedBelief.provenance.is_valid()`
- [ ] Tier reasoning matches `BeliefScope` validation

### Phase 2: Base Model Training (NEW WORK)

**Deliverables**:
- [ ] simplex-core-7b base model
- [ ] simplex-core-1b base model
- [ ] simplex-core-embed model

**Implementation Notes**:
- Use `MetaTrainer` from `simplex-training/src/trainer/meta.sx`
- Integrate `EpistemicSchedule` for temperature control during training
- Use `SpecialistTrainer` pattern for domain-specific fine-tuning
- Export via `GgufExporter` for deployment

**Success Criteria**:
- [ ] Models understand Simplex memory format
- [ ] No SQL/vector contamination
- [ ] Basic confidence calibration (ECE < 0.10)

### Phase 3: Calibration and Self-Learning (INTEGRATION)

**Deliverables**:
- [ ] Wire `CalibratedConfidence` to model output
- [ ] Wire `dual` tensors from `simplex-learning/src/dual/`
- [ ] Wire `EpistemicSchedule` from `simplex-learning/src/epistemic/schedule.sx`
- [ ] Wire `EpistemicMonitors` for health tracking

**Implementation Notes**:
These components **already exist** - this phase is about integrating them:
- `CalibratedConfidence.record_outcome()` for training feedback
- `EpistemicMonitors.update()` for health tracking
- `EpistemicSchedule.temperature()` for adaptive learning
- `SafeLearningGuard` from `safety/zones.sx` for safe updates

**Success Criteria**:
- [ ] ECE < 0.05 across all tiers
- [ ] `EpistemicMetaOptimizer` learns effective schedules
- [ ] `ZoneRegistry` enforces no-learn zones

### Phase 4: Belief System Integration (INTEGRATION)

**Deliverables**:
- [ ] Model outputs parse to `GroundedBelief<T>`
- [ ] Model generates valid `BeliefProvenance`
- [ ] Model performs belief revision correctly
- [ ] `Skeptic` validates model outputs

**Implementation Notes**:
The belief system is **already implemented** in TASK-014:
- `GroundedBelief::observed()`, `::inferred()`, `::learned()`, etc.
- `GroundedBelief.add_evidence()`, `add_contradiction()`
- `GroundedBelief.check_falsifiers()`, `apply_falsifications()`
- `Skeptic.challenge()` for overconfidence detection

This phase validates model output against these existing structures.

**Success Criteria**:
- [ ] Model outputs deserialize to valid `GroundedBelief<T>`
- [ ] `Skeptic.challenge()` passes for model beliefs
- [ ] `CalibrationRecord.compute_ece()` < target thresholds

### Phase 5: Hive Architecture (EXTENSION)

**Deliverables**:
- [ ] Training data for three-tier hierarchy
- [ ] Model understands Anima/Mnemonic/Divine thresholds
- [ ] Model performs conflict resolution
- [ ] Model aggregates beliefs correctly

**Implementation Notes**:
Cross-hive infrastructure exists in `distributed/beliefs.sx`:
- Extend for tier-specific reasoning
- Training data should include hive context

**Success Criteria**:
- [ ] Correct tier threshold reasoning (30%/50%/70%)
- [ ] Proper conflict resolution via `EpistemicMonitors.source_agreement`
- [ ] Belief aggregation matches `GroundedBelief.blend_with()`

### Phase 6: Native Persistence Backend (NEW WORK)

**Deliverables**:
- [ ] `simplex-core/src/belief_store.sx` - Content-addressed beliefs
- [ ] `simplex-core/src/episodic.sx` - Native episodic memory
- [ ] `simplex-core/src/semantic.sx` - Native semantic memory
- [ ] `simplex-core/src/anima_persist.sx` - Binary format (magic: `SXAN`)
- [ ] Move SQLite to `adapters/simplex-sql/`

**Implementation Notes**:
This is the **toolchain refactoring** work from Part 2 of this document.

**Success Criteria**:
- [ ] `grep -r "sqlite" simplex-core/` returns nothing
- [ ] Native binary format validates via content hash
- [ ] Round-trip: save → load → save produces identical bytes
- [ ] Performance ≥ 2x vs JSON

### Phase 7: Evaluation and Hardening (NEW WORK)

**Deliverables**:
- [ ] Full benchmark suite in `tests/benchmark_simplex_core.sx`
- [ ] Anti-pattern detection for SQL/vector contamination
- [ ] Adversarial testing with `Skeptic` and `CounterfactualProber`
- [ ] Production deployment validation

**Success Criteria**:
- [ ] All benchmarks pass targets
- [ ] Zero SQL/vector contamination in 10K samples
- [ ] `Skeptic` pass rate > 95%
- [ ] Ready for hive deployment

---

## Part 3: AWS Infrastructure & Training Pipeline

> **This section defines HOW to actually build the models - infrastructure, seed models, datasets, and training harness.**

### AWS Infrastructure Requirements

#### Compute Instances

| Instance Type | Purpose | Count | Specs | Est. Cost/hr |
|---------------|---------|-------|-------|--------------|
| **p4d.24xlarge** | Primary training (7B) | 4-8 | 8x A100 40GB, 96 vCPU, 1.5TB RAM | ~$32/hr |
| **p5.48xlarge** | Large-scale training | 2-4 | 8x H100 80GB, 192 vCPU, 2TB RAM | ~$98/hr |
| **g5.12xlarge** | Evaluation & inference | 2 | 4x A10G 24GB, 48 vCPU, 192GB RAM | ~$5.67/hr |
| **c6i.8xlarge** | Data generation | 4-8 | 32 vCPU, 64GB RAM | ~$1.36/hr |
| **r6i.4xlarge** | Orchestration/monitoring | 1 | 16 vCPU, 128GB RAM | ~$1.01/hr |

**Recommended Configuration for Full Training Run**:
- **Phase 1 (Data Gen)**: 8x c6i.8xlarge for ~3 days
- **Phase 2-4 (Training)**: 4x p4d.24xlarge for ~2 weeks (7B model)
- **Phase 5 (Eval)**: 2x g5.12xlarge for ~1 week

**Estimated Total Cost**: ~$50,000-80,000 for full training pipeline

#### Storage (S3)

```
s3://simplex-training/
├── datasets/
│   ├── belief-formation/     # 100K examples (~50GB)
│   ├── belief-revision/      # 100K examples (~50GB)
│   ├── memory-recall/        # 100K examples (~60GB)
│   ├── hive-integration/     # 50K examples (~30GB)
│   ├── persistence-ops/      # 100K examples (~40GB)
│   └── forgetting-decay/     # 50K examples (~25GB)
├── checkpoints/
│   ├── simplex-core-7b/      # Training checkpoints (~500GB)
│   ├── simplex-core-1b/      # Training checkpoints (~100GB)
│   └── simplex-core-embed/   # Training checkpoints (~20GB)
├── models/
│   ├── seed/                 # Base models (Qwen, Llama, etc.)
│   └── final/                # Trained simplex-core models
├── logs/
│   ├── training/             # Training logs, TensorBoard
│   ├── evaluation/           # Eval results, calibration
│   └── epistemic/            # EpistemicMonitors logs
└── artifacts/
    └── gguf/                 # Exported GGUF models
```

**Estimated Storage**: ~2TB for full pipeline

#### Networking

- **VPC**: Dedicated VPC with private subnets for training cluster
- **EFA**: Elastic Fabric Adapter for multi-node training (required for p4d/p5)
- **S3 Gateway Endpoint**: For high-throughput dataset access
- **NAT Gateway**: For package installation, model downloads

#### IAM Roles

```json
{
  "SimplexTrainingRole": {
    "permissions": [
      "s3:GetObject", "s3:PutObject", "s3:ListBucket",
      "ec2:DescribeInstances", "ec2:CreateTags",
      "cloudwatch:PutMetricData",
      "logs:CreateLogGroup", "logs:CreateLogStream", "logs:PutLogEvents"
    ]
  }
}
```

---

### Seed Model Selection

#### Primary Candidate: Qwen2.5

| Model | Parameters | Why | Concerns |
|-------|------------|-----|----------|
| **Qwen2.5-7B** | 7.6B | Strong reasoning, Apache 2.0 license, good calibration baseline | Chinese-centric pretraining |
| **Qwen2.5-1.5B** | 1.5B | Good for edge/mobile, same family | Same |
| **Qwen2.5-0.5B** | 0.5B | Embedding model candidate | Limited capacity |

**Why Qwen2.5**:
- Apache 2.0 license (fully permissive)
- Strong instruction-following out of the box
- 128K context window (good for memory format)
- Good calibration baseline (ECE ~0.08)
- Active development, well-documented

#### Alternative: Llama 3.2

| Model | Parameters | Why | Concerns |
|-------|------------|-----|----------|
| **Llama-3.2-8B** | 8B | Meta's latest, strong English | Llama license restrictions |
| **Llama-3.2-3B** | 3B | Good mid-size option | Same |
| **Llama-3.2-1B** | 1B | Edge deployment | Limited capacity |

**Why Consider Llama**:
- Strong English performance
- Large community, many fine-tuning resources
- Good reasoning capabilities

**Concerns**:
- Llama license has commercial restrictions
- Less permissive than Qwen

#### Embedding Model: Candidate Selection

| Model | Dimensions | Why |
|-------|------------|-----|
| **nomic-embed-text-v1.5** | 768 | Apache 2.0, good for retrieval |
| **bge-small-en-v1.5** | 384 | MIT license, efficient |
| **Qwen2.5-embed** | 1024 | Same family as main model |

**Recommendation**: Start with **Qwen2.5 family** for consistency and licensing simplicity.

---

### Training Harness: simplex-train CLI

**Yes, we need a training harness.** This will be built as `simplex-train`, extending the existing `simplex-training` library.

#### Architecture

```
simplex-train/
├── Modulus.toml
├── src/
│   ├── main.sx                    # CLI entry point
│   ├── cli/
│   │   ├── mod.sx
│   │   ├── train.sx               # train subcommand
│   │   ├── generate.sx            # generate subcommand (data gen)
│   │   ├── evaluate.sx            # evaluate subcommand
│   │   └── export.sx              # export subcommand (GGUF)
│   ├── config/
│   │   ├── mod.sx
│   │   ├── training.sx            # TrainingConfig
│   │   ├── aws.sx                 # AWSConfig
│   │   └── epistemic.sx           # EpistemicConfig
│   ├── data/
│   │   ├── mod.sx
│   │   ├── belief_generator.sx    # Generate belief formation data
│   │   ├── revision_generator.sx  # Generate revision data
│   │   ├── memory_generator.sx    # Generate memory recall data
│   │   └── hive_generator.sx      # Generate hive integration data
│   ├── train/
│   │   ├── mod.sx
│   │   ├── loop.sx                # Main training loop
│   │   ├── distributed.sx         # Multi-node training
│   │   ├── checkpoint.sx          # Checkpoint management
│   │   └── anneal.sx              # Self-annealing integration
│   └── eval/
│       ├── mod.sx
│       ├── calibration.sx         # ECE evaluation
│       ├── contamination.sx       # SQL/vector contamination check
│       └── skeptic.sx             # Skeptic validation
└── configs/
    ├── simplex-core-7b.toml       # 7B training config
    ├── simplex-core-1b.toml       # 1B training config
    └── simplex-core-embed.toml    # Embed training config
```

#### CLI Interface

```bash
# Generate training data
simplex-train generate \
  --type belief-formation \
  --count 100000 \
  --output s3://simplex-training/datasets/belief-formation/ \
  --config configs/generation.toml

# Train model
simplex-train train \
  --config configs/simplex-core-7b.toml \
  --seed-model qwen2.5-7b \
  --dataset s3://simplex-training/datasets/ \
  --output s3://simplex-training/checkpoints/simplex-core-7b/ \
  --distributed \
  --nodes 4

# Evaluate model
simplex-train evaluate \
  --model s3://simplex-training/checkpoints/simplex-core-7b/final/ \
  --eval-set s3://simplex-training/datasets/eval/ \
  --output s3://simplex-training/logs/evaluation/

# Export to GGUF
simplex-train export \
  --model s3://simplex-training/checkpoints/simplex-core-7b/final/ \
  --format gguf \
  --quantization q4_k_m \
  --output s3://simplex-training/artifacts/gguf/simplex-core-7b-q4.gguf
```

#### Training Configuration (TOML)

```toml
# configs/simplex-core-7b.toml

[model]
name = "simplex-core-7b"
seed = "qwen2.5-7b"
architecture = "transformer"
hidden_size = 4096
num_layers = 32
num_heads = 32
vocab_size = 152064  # Qwen vocab + Simplex special tokens

[training]
batch_size = 8
gradient_accumulation = 4
max_steps = 100000
warmup_steps = 2000
max_lr = 2e-5
min_lr = 2e-6
weight_decay = 0.01
mixed_precision = "bf16"

[training.checkpoint]
interval = 1000
max_keep = 5
s3_bucket = "simplex-training"
s3_prefix = "checkpoints/simplex-core-7b"

[distributed]
backend = "nccl"
nodes = 4
gpus_per_node = 8
efa_enabled = true

[special_tokens]
belief_start = "<belief>"
belief_end = "</belief>"
provenance_start = "<provenance>"
provenance_end = "</provenance>"
confidence_start = "<confidence>"
confidence_end = "</confidence>"
episode_start = "<episode>"
episode_end = "</episode>"
semantic_start = "<semantic>"
semantic_end = "</semantic>"
revision_start = "<revision>"
revision_end = "</revision>"
recall_start = "<recall>"
recall_end = "</recall>"

[epistemic]
# Integration with simplex-learning epistemic system
enabled = true
schedule = "epistemic"  # Use EpistemicSchedule

[epistemic.monitors]
source_agreement_threshold = 0.5
confidence_velocity_threshold = 0.1
evidence_staleness_threshold = 100

[epistemic.dissent]
period = 1000  # Every 1000 steps
window = 100   # 100 step dissent window
heat = 0.3     # Temperature boost during dissent

[epistemic.skeptic]
enabled = true
min_confidence_to_challenge = 0.8
max_challenges_per_window = 50

[epistemic.zones]
# No-learn zones for safety
no_learn = ["safety_head", "calibration_head"]
no_gradient = ["position_embeddings"]

[calibration]
target_ece = 0.05
bins = 15
temperature_scaling = true

[evaluation]
interval = 5000
metrics = ["ece", "accuracy", "contamination", "provenance_integrity"]
```

---

### Dataset Generation Pipeline

#### Step 1: Template Generation

Using existing `DataGenerator` from `simplex-training/src/data/generator.sx`:

```simplex
// simplex-train/src/data/belief_generator.sx

use simplex_training::data::{DataGenerator, TrainingExample};
use simplex_learning::belief::{GroundedBelief, BeliefProvenance, CalibratedConfidence};

pub struct BeliefFormationGenerator {
    /// Base generator
    base: DataGenerator,

    /// Domain templates
    domains: Vec<DomainTemplate>,

    /// Provenance distribution
    provenance_dist: ProvenanceDistribution,
}

impl BeliefFormationGenerator {
    /// Generate a belief formation example
    pub fn generate(&mut self) -> TrainingExample {
        // Select domain
        let domain = self.domains.choose_weighted();

        // Generate observation
        let observation = domain.generate_observation();

        // Generate expected belief
        let belief = self.generate_belief_from_observation(&observation, &domain);

        // Format as training example
        TrainingExample {
            input: self.format_input(&observation),
            output: self.format_output(&belief),
            metadata: TrainingMetadata {
                domain: domain.name.clone(),
                provenance_type: belief.provenance.type_name(),
                confidence: belief.confidence.value.val,
            },
        }
    }

    fn generate_belief_from_observation(
        &self,
        observation: &Observation,
        domain: &DomainTemplate,
    ) -> GroundedBelief<String> {
        // Determine provenance type based on distribution
        let provenance = match self.provenance_dist.sample() {
            ProvenanceType::Observed => BeliefProvenance::observed(&observation.source),
            ProvenanceType::Inferred => BeliefProvenance::inferred(
                vec![], // No premises for direct observation
                InferenceRule::direct_observation(),
                "Derived from observation",
            ),
            ProvenanceType::Authoritative => BeliefProvenance::authoritative(
                &observation.source,
                observation.credibility,
            ),
            // ... other types
        };

        // Generate claim
        let claim = domain.generate_claim(&observation);

        // Generate confidence (calibrated)
        let confidence = self.calibrate_confidence(&observation, &domain);

        // Generate evidence links
        let evidence = vec![
            EvidenceLink::supporting(
                Observation::from(observation),
                observation.credibility,
            ),
        ];

        // Generate falsifiers
        let falsifiers = domain.generate_falsifiers(&claim);

        // Build grounded belief
        GroundedBelief::new(&format!("belief_{}", self.base.next_id()), claim, provenance, confidence)
            .with_evidence_list(evidence)
            .with_falsifier(falsifiers[0].clone())
            .in_domain(domain.to_belief_domain())
    }
}
```

#### Step 2: Domain Templates

```simplex
// Domain templates for diverse training data

pub struct DomainTemplate {
    name: String,
    claim_templates: Vec<String>,
    observation_templates: Vec<String>,
    falsifier_templates: Vec<String>,
    typical_confidence_range: (f64, f64),
}

pub fn create_domain_templates() -> Vec<DomainTemplate> {
    vec![
        // Customer/Business domain
        DomainTemplate {
            name: "customer_success".to_string(),
            claim_templates: vec![
                "Customer {customer_id} has {risk_level} churn risk".to_string(),
                "Customer {customer_id} is {satisfaction_level} satisfied".to_string(),
                "Customer {customer_id} prefers {preference}".to_string(),
            ],
            observation_templates: vec![
                "Customer service call: {content}".to_string(),
                "Support ticket: {content}".to_string(),
                "Survey response: {content}".to_string(),
            ],
            falsifier_templates: vec![
                "Customer completes positive interaction".to_string(),
                "Customer renews subscription".to_string(),
                "Customer feedback contradicts belief".to_string(),
            ],
            typical_confidence_range: (0.5, 0.9),
        },

        // Technical/System domain
        DomainTemplate {
            name: "system_health".to_string(),
            claim_templates: vec![
                "Service {service} is {status}".to_string(),
                "System capacity is {capacity_status} for {load_type}".to_string(),
                "Component {component} requires {action}".to_string(),
            ],
            observation_templates: vec![
                "Monitoring alert: {content}".to_string(),
                "Log entry: {content}".to_string(),
                "Metric reading: {content}".to_string(),
            ],
            falsifier_templates: vec![
                "Metric returns to normal range".to_string(),
                "Service health check passes".to_string(),
                "Component successfully restarts".to_string(),
            ],
            typical_confidence_range: (0.6, 0.95),
        },

        // Knowledge/Learning domain
        DomainTemplate {
            name: "knowledge".to_string(),
            claim_templates: vec![
                "{concept} is {relationship} {other_concept}".to_string(),
                "{entity} has property {property}".to_string(),
                "{fact} is {validity_status}".to_string(),
            ],
            observation_templates: vec![
                "Documentation states: {content}".to_string(),
                "Expert {expert} says: {content}".to_string(),
                "Experiment shows: {content}".to_string(),
            ],
            falsifier_templates: vec![
                "Contradicting evidence emerges".to_string(),
                "Authority revises statement".to_string(),
                "Experiment fails to replicate".to_string(),
            ],
            typical_confidence_range: (0.4, 0.85),
        },

        // ... more domains (medical, financial, social, etc.)
    ]
}
```

#### Step 3: Provenance Distribution

```simplex
/// Distribution of provenance types in training data
pub struct ProvenanceDistribution {
    /// Weight for each provenance type
    weights: HashMap<ProvenanceType, f64>,
}

impl ProvenanceDistribution {
    /// Recommended distribution for training
    pub fn default() -> Self {
        let mut weights = HashMap::new();
        weights.insert(ProvenanceType::Observed, 0.35);      // 35% direct observation
        weights.insert(ProvenanceType::Inferred, 0.25);      // 25% inference
        weights.insert(ProvenanceType::Authoritative, 0.20); // 20% authority
        weights.insert(ProvenanceType::Learned, 0.15);       // 15% learned from data
        weights.insert(ProvenanceType::Prior, 0.05);         // 5% prior/default
        ProvenanceDistribution { weights }
    }

    /// Sample a provenance type
    pub fn sample(&self) -> ProvenanceType {
        // Weighted random selection
        let total: f64 = self.weights.values().sum();
        let mut r = rand::random::<f64>() * total;
        for (ptype, weight) in &self.weights {
            r -= weight;
            if r <= 0.0 {
                return *ptype;
            }
        }
        ProvenanceType::Observed // fallback
    }
}
```

---

### Training Loop with Self-Annealing

The key integration point is wiring `EpistemicSchedule` and `EpistemicMetaOptimizer` from `simplex-learning` into the training loop.

```simplex
// simplex-train/src/train/loop.sx

use simplex_learning::epistemic::{
    EpistemicSchedule, EpistemicMonitors, EpistemicMetaOptimizer,
    DissentConfig, Skeptic, SkepticConfig,
};
use simplex_learning::safety::{ZoneRegistry, SafeLearningGuard, NoLearnZone};
use simplex_learning::calibration::{OnlineCalibrator, ECEMetric};
use simplex_training::trainer::{MetaTrainer, StepResult};

pub struct SimplexCoreTrainer {
    /// Model being trained
    model: Model,

    /// Epistemic schedule (replaces fixed LR schedule)
    epistemic_schedule: EpistemicSchedule,

    /// Epistemic health monitors
    monitors: EpistemicMonitors,

    /// Meta-optimizer with epistemic awareness
    meta_optimizer: EpistemicMetaOptimizer,

    /// Skeptic for challenging high-confidence outputs
    skeptic: Skeptic,

    /// Safety zones
    zone_registry: ZoneRegistry,

    /// Online calibration
    calibrator: OnlineCalibrator,

    /// Current step
    step: u64,

    /// Dissent window state
    in_dissent: bool,
}

impl SimplexCoreTrainer {
    pub fn new(config: TrainingConfig) -> Self {
        // Initialize epistemic schedule from config
        let epistemic_schedule = EpistemicSchedule::new()
            .with_base_temperature(config.epistemic.base_temperature)
            .with_dissent_config(DissentConfig {
                period: config.epistemic.dissent.period,
                window: config.epistemic.dissent.window,
                heat: config.epistemic.dissent.heat,
            });

        // Initialize monitors
        let monitors = EpistemicMonitors::new()
            .with_source_agreement_threshold(config.epistemic.monitors.source_agreement_threshold)
            .with_confidence_velocity_threshold(config.epistemic.monitors.confidence_velocity_threshold);

        // Initialize meta-optimizer
        let meta_optimizer = EpistemicMetaOptimizer::new(
            epistemic_schedule.clone(),
            config.training.max_lr,
        );

        // Initialize skeptic
        let skeptic = Skeptic::with_config(
            SkepticConfig {
                min_confidence_to_challenge: config.epistemic.skeptic.min_confidence_to_challenge,
                max_challenges_per_window: config.epistemic.skeptic.max_challenges_per_window,
                ..Default::default()
            },
            DissentConfig::step_based(
                config.epistemic.dissent.period,
                config.epistemic.dissent.window,
            ),
        );

        // Initialize safety zones
        let mut zone_registry = ZoneRegistry::new();
        for zone_name in &config.epistemic.zones.no_learn {
            zone_registry.add_no_learn(NoLearnZone::new(zone_name));
        }

        // Initialize calibrator
        let calibrator = OnlineCalibrator::new(config.calibration.bins);

        SimplexCoreTrainer {
            model: Model::load(&config.model.seed)?,
            epistemic_schedule,
            monitors,
            meta_optimizer,
            skeptic,
            zone_registry,
            calibrator,
            step: 0,
            in_dissent: false,
        }
    }

    /// Run one training step with epistemic integration
    pub fn step(&mut self, batch: &Batch) -> StepResult {
        self.step += 1;

        // 1. Update monitors with current state
        self.update_monitors(batch);

        // 2. Check if we're in a dissent window
        self.in_dissent = self.epistemic_schedule.in_dissent_window(self.step);

        // 3. Compute temperature (epistemically-modulated)
        let temperature = self.epistemic_schedule.temperature(
            dual::constant(self.step as f64),
            self.monitors.stagnation(),
            &self.monitors,
        );

        // 4. Compute learning rate (health-adjusted)
        let base_lr = self.meta_optimizer.current_lr();
        let effective_lr = self.epistemic_schedule.safe_learning_rate(
            base_lr,
            &self.monitors,
        );

        // 5. Forward pass
        let outputs = self.model.forward(batch);

        // 6. Compute loss with epistemic terms
        let base_loss = self.compute_base_loss(&outputs, batch);
        let calibration_loss = self.compute_calibration_loss(&outputs, batch);
        let epistemic_loss = self.compute_epistemic_loss(&outputs);

        let total_loss = base_loss
            + calibration_loss * dual::constant(0.1)
            + epistemic_loss * dual::constant(0.05);

        // 7. Backward pass
        let gradients = total_loss.backward();

        // 8. Apply safety zones (mask gradients)
        let mut guard = SafeLearningGuard::new(&mut self.zone_registry);
        guard.mask_gradients(&mut gradients);

        // 9. Optimizer step with effective LR
        self.model.update(&gradients, effective_lr);

        // 10. Update calibrator
        self.update_calibrator(&outputs, batch);

        // 11. Skeptic challenge (during dissent windows)
        let challenges = if self.in_dissent {
            self.run_skeptic_challenges(&outputs)
        } else {
            vec![]
        };

        // 12. Meta-optimizer step (update schedule parameters)
        if self.step % 100 == 0 {
            self.meta_optimizer.meta_step(&self.monitors);
        }

        // 13. Log and return
        StepResult {
            loss: total_loss.val,
            lr: effective_lr,
            temperature: temperature.val,
            ece: self.calibrator.current_ece(),
            in_dissent: self.in_dissent,
            challenges: challenges.len(),
            monitors: self.monitors.snapshot(),
        }
    }

    /// Update epistemic monitors from batch
    fn update_monitors(&mut self, batch: &Batch) {
        // Update source agreement (how consistent are belief sources?)
        let source_agreement = self.compute_source_agreement(batch);
        self.monitors.source_agreement.update(source_agreement);

        // Update confidence velocity (is confidence growing too fast?)
        let conf_velocity = self.compute_confidence_velocity(batch);
        self.monitors.confidence_velocity.update(conf_velocity);

        // Update predictive accuracy
        let accuracy = self.compute_predictive_accuracy(batch);
        self.monitors.predictive_accuracy.update(accuracy);

        // Update evidence staleness
        let staleness = self.compute_evidence_staleness(batch);
        self.monitors.evidence_staleness.update(staleness);

        // Update exploration ratio
        let exploration = self.compute_exploration_ratio();
        self.monitors.exploration_ratio.update(exploration);
    }

    /// Compute epistemic loss terms
    fn compute_epistemic_loss(&self, outputs: &ModelOutputs) -> dual {
        let mut loss = dual::constant(0.0);

        // Penalize overconfidence (high confidence with low evidence count)
        for belief in &outputs.beliefs {
            if belief.confidence_value() > 0.8 && belief.evidence.len() < 2 {
                loss = loss + dual::constant(0.1);
            }
        }

        // Penalize missing provenance
        for belief in &outputs.beliefs {
            if !belief.provenance.is_valid() {
                loss = loss + dual::constant(0.2);
            }
        }

        // Penalize scope violations
        for belief in &outputs.beliefs {
            if !belief.scope.is_valid() {
                loss = loss + dual::constant(0.1);
            }
        }

        loss
    }

    /// Run skeptic challenges during dissent windows
    fn run_skeptic_challenges(&mut self, outputs: &ModelOutputs) -> Vec<ChallengeRecord> {
        let mut records = vec![];

        for belief in &outputs.beliefs {
            if let Some(challenge) = self.skeptic.challenge(belief, &self.monitors.as_slice()) {
                records.push(challenge);
            }
        }

        records
    }
}
```

---

### Concrete Build Steps

#### Step 0: AWS Setup (1-2 days)

```bash
# 1. Create VPC and subnets
aws cloudformation create-stack \
  --stack-name simplex-training-vpc \
  --template-body file://infra/vpc.yaml

# 2. Create S3 bucket
aws s3 mb s3://simplex-training --region us-east-1

# 3. Create IAM roles
aws cloudformation create-stack \
  --stack-name simplex-training-iam \
  --template-body file://infra/iam.yaml \
  --capabilities CAPABILITY_IAM

# 4. Download seed models to S3
aws s3 cp qwen2.5-7b/ s3://simplex-training/models/seed/qwen2.5-7b/ --recursive
```

#### Step 1: Generate Training Data (3-5 days)

```bash
# Launch data generation cluster
simplex-train generate \
  --type all \
  --counts "belief-formation:100000,belief-revision:100000,memory-recall:100000,hive-integration:50000,persistence-ops:100000,forgetting-decay:50000" \
  --output s3://simplex-training/datasets/ \
  --workers 8 \
  --validate

# Verify datasets
simplex-train validate \
  --dataset s3://simplex-training/datasets/ \
  --check-types \
  --check-provenance \
  --check-calibration \
  --report s3://simplex-training/logs/data-validation.json
```

**Expected Output**: ~500K training examples, ~250GB

#### Step 2: Pre-train on Memory Format (1 week)

```bash
# Launch distributed training
simplex-train train \
  --config configs/simplex-core-7b.toml \
  --stage pretrain \
  --seed-model s3://simplex-training/models/seed/qwen2.5-7b/ \
  --dataset s3://simplex-training/datasets/ \
  --output s3://simplex-training/checkpoints/simplex-core-7b/pretrain/ \
  --distributed \
  --nodes 4 \
  --gpus-per-node 8

# Monitor training
simplex-train monitor \
  --job simplex-core-7b-pretrain \
  --metrics loss,lr,temperature,ece \
  --tensorboard
```

**Expected Output**: Base model that understands Simplex memory format

#### Step 3: Calibration Training (3 days)

```bash
simplex-train train \
  --config configs/simplex-core-7b.toml \
  --stage calibration \
  --checkpoint s3://simplex-training/checkpoints/simplex-core-7b/pretrain/final/ \
  --dataset s3://simplex-training/datasets/ \
  --output s3://simplex-training/checkpoints/simplex-core-7b/calibration/ \
  --distributed \
  --nodes 2
```

**Expected Output**: ECE < 0.10

#### Step 4: Self-Annealing Fine-tuning (1 week)

```bash
simplex-train train \
  --config configs/simplex-core-7b.toml \
  --stage annealing \
  --checkpoint s3://simplex-training/checkpoints/simplex-core-7b/calibration/final/ \
  --epistemic-enabled \
  --dissent-windows \
  --skeptic-enabled \
  --output s3://simplex-training/checkpoints/simplex-core-7b/annealing/
```

**Expected Output**: ECE < 0.05, self-learned schedules

#### Step 5: Evaluation (2-3 days)

```bash
# Full evaluation suite
simplex-train evaluate \
  --model s3://simplex-training/checkpoints/simplex-core-7b/annealing/final/ \
  --eval-set s3://simplex-training/datasets/eval/ \
  --metrics all \
  --contamination-check \
  --skeptic-validation \
  --output s3://simplex-training/logs/evaluation/final/

# Generate report
simplex-train report \
  --eval-results s3://simplex-training/logs/evaluation/final/ \
  --output s3://simplex-training/artifacts/simplex-core-7b-report.pdf
```

#### Step 6: Export (1 day)

```bash
# Export to GGUF for deployment
simplex-train export \
  --model s3://simplex-training/checkpoints/simplex-core-7b/annealing/final/ \
  --format gguf \
  --quantizations "f16,q8_0,q4_k_m" \
  --output s3://simplex-training/artifacts/gguf/

# Verify exports
simplex-train verify \
  --gguf s3://simplex-training/artifacts/gguf/simplex-core-7b-q4_k_m.gguf \
  --test-prompts s3://simplex-training/datasets/eval/prompts.json
```

---

### Timeline Summary

| Phase | Duration | Compute | Output |
|-------|----------|---------|--------|
| AWS Setup | 1-2 days | - | Infrastructure ready |
| Data Generation | 3-5 days | 8x c6i.8xlarge | ~500K examples |
| Pre-training | 5-7 days | 4x p4d.24xlarge | Memory-format base |
| Calibration | 2-3 days | 2x p4d.24xlarge | ECE < 0.10 |
| Self-Annealing | 5-7 days | 4x p4d.24xlarge | ECE < 0.05 |
| Evaluation | 2-3 days | 2x g5.12xlarge | Benchmark results |
| Export | 1 day | 1x g5.12xlarge | GGUF models |
| **Total** | **~3-4 weeks** | | |

**Estimated Total Cost**: $50,000 - $80,000 (depending on instance availability, spot pricing)

---

## Success Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| **Belief Formation Accuracy** | > 90% | Correct belief from observation |
| **Revision Accuracy** | > 85% | Appropriate revision given evidence |
| **Provenance Integrity** | > 95% | Complete provenance chains |
| **Confidence ECE (Anima)** | < 0.05 | Expected Calibration Error |
| **Confidence ECE (Mnemonic)** | < 0.04 | Expected Calibration Error |
| **Confidence ECE (Divine)** | < 0.03 | Expected Calibration Error |
| **Memory Recall Relevance** | > 80% | Relevant results from recall |
| **Tier Reasoning Accuracy** | > 90% | Correct threshold/scope reasoning |
| **SQL Contamination** | 0% | No SQL output in any test |
| **Vector Contamination** | 0% | No vector DB patterns |
| **Self-Learning Improvement** | > 15% | Schedule learning vs fixed |

---

## Risk Assessment

### Risk 1: Training Data Quality

**Concern**: Synthetic training data may not capture real-world complexity.

**Mitigation**:
- Generate data from diverse scenarios
- Include edge cases and adversarial examples
- Validate with human experts
- Iterative refinement based on failure analysis

### Risk 2: SQL/Vector Contamination

**Concern**: Base model has seen SQL/vectors, may leak through.

**Mitigation**:
- Heavy filtering of training data
- Explicit anti-pattern training
- Contamination testing in evaluation
- Fine-tuning to suppress legacy patterns

### Risk 3: Calibration Drift

**Concern**: Confidence calibration may drift during belief revision.

**Mitigation**:
- Continuous calibration monitoring
- Epistemic health metrics (TASK-014)
- Re-calibration triggers
- Skeptic challenges for high confidence

### Risk 4: Hive Consistency

**Concern**: Beliefs may become inconsistent across tiers.

**Mitigation**:
- Bayesian belief aggregation
- Conflict resolution protocols
- Provenance-based arbitration
- Periodic consistency checks

---

## Dependencies

| Task | Dependency Type | Status |
|------|-----------------|--------|
| TASK-002 | Base model architecture | ✅ Complete |
| TASK-005 | Dual number infrastructure | ✅ Complete |
| TASK-006 | Self-learning annealing | ✅ Complete |
| TASK-007 | Pure Simplex training pipeline | ✅ Complete |
| TASK-014 | Epistemic integrity framework | ✅ Complete (`simplex-learning/src/`) |

---

## Related Work

### Simplex Philosophy

This task embodies the core Simplex philosophy:

> **"Memory is cognitive, not storage."**

The model should think about persistence the way humans think about memory:
- We don't "INSERT INTO brain" - we **form beliefs**
- We don't "SELECT FROM memory" - we **recall**
- We don't "UPDATE records" - we **revise understanding**
- We don't "DELETE rows" - we **forget**

### Comparison to Existing Approaches

| Approach | Mental Model | Simplex-Core |
|----------|--------------|--------------|
| RAG | "Retrieve then generate" | "Recall from memory" |
| Fine-tuning | "Bake knowledge into weights" | "Form lasting beliefs" |
| Vector DB | "Embed and search" | "Associate and recall" |
| SQL | "Query structured data" | "Reason about beliefs" |

### Novel Contributions

1. **First SLM trained natively on belief-based persistence**
2. **Integrated confidence calibration at memory formation**
3. **Provenance as first-class training objective**
4. **Self-learning integration at the memory level**
5. **Three-tier cognitive hierarchy as native architecture**

---

## References

### Simplex Tasks
- TASK-002: Simplex Cognitive Models
- TASK-005: Dual Numbers
- TASK-006: Self-Learning Annealing
- TASK-007: Rebuild Training Pipeline in Pure Simplex
- TASK-014: Epistemic Integrity & Self-Correcting Belief Architecture

### Cognitive Science
- Tulving, E. (1972). "Episodic and Semantic Memory"
- Anderson, J.R. (1983). "The Architecture of Cognition"
- Schacter, D.L. (1999). "The Seven Sins of Memory"

### Belief Systems
- Pearl, J. (1988). "Probabilistic Reasoning in Intelligent Systems"
- Bratman, M. (1987). "Intention, Plans, and Practical Reason"
- Rao, A.S. & Georgeff, M.P. (1995). "BDI Agents"

### Memory Systems
- Hopfield, J.J. (1982). "Neural Networks and Physical Systems with Emergent Collective Computational Abilities"
- Graves, A. et al. (2014). "Neural Turing Machines"
- Weston, J. et al. (2015). "Memory Networks"

---

## The Bottom Line

**simplex-core is not another fine-tuned model. It's a model that thinks differently about knowledge.**

Traditional models:
- Learn to manipulate external storage
- Treat persistence as an I/O operation
- Separate "reasoning" from "remembering"

simplex-core:
- Thinks in terms of beliefs, episodes, and semantic facts
- Treats persistence as intrinsic to cognition
- Unifies "reasoning" and "remembering" into one cognitive process

The goal is not to build a better database adapter. The goal is to build a model that **doesn't need adapters** because it already thinks in Simplex's native persistence model.

> **"The best interface is no interface. The best adapter is no adapter."**

When the model's internal representation matches the persistence format, we eliminate:
- Translation overhead
- Impedance mismatch
- Lost provenance
- Degraded confidence

What remains is pure cognitive flow: observe → believe → recall → revise → persist.

That's simplex-core.

---

## Part 2: Toolchain Refactoring - Remove Legacy Persistence from Core

> **SQLite, vectors, and JSON become ADAPTERS, not the core persistence mechanism.**

This section details the refactoring required to make Simplex's runtime, compiler, and toolchain use native Simplex persistence as the core mechanism.

### Current State Analysis

Based on codebase analysis, the following legacy persistence is embedded in the core:

#### 1. SQLite Integration (Currently Core - Must Become Adapter)

| Location | Lines | Description | Action |
|----------|-------|-------------|--------|
| `simplex-sql/src/lib.sx` | Full file | SQLite wrapper API | Move to `adapters/sql/` |
| `standalone_runtime.c` | 13424-13690 | 28+ SQLite FFI functions | Extract to adapter |
| `stage0.py` | 4852-4879 | SQL intrinsic declarations | Conditional compilation |

**Current SQLite Functions in Runtime** (to become adapter):
```c
// These 28 functions move from core to simplex-sql adapter
sql_open(), sql_open_memory(), sql_close(), sql_execute(),
sql_error(), sql_prepare(), sql_bind_int(), sql_bind_text(),
sql_bind_double(), sql_bind_null(), sql_step(), sql_reset(),
sql_column_count(), sql_column_type(), sql_column_name(),
sql_column_int(), sql_column_text(), sql_column_double(),
sql_column_blob(), sql_column_blob_len(), sql_column_is_null(),
sql_finalize(), sql_begin(), sql_commit(), sql_rollback(),
sql_last_insert_id(), sql_changes(), sql_total_changes()
```

#### 2. Anima Memory (Currently JSON - Must Become Native)

| Location | Lines | Description | Action |
|----------|-------|-------------|--------|
| `standalone_runtime.c` | 20972-21057 | `anima_save()` - JSON serialization | Replace with native |
| `standalone_runtime.c` | 21131-21300 | `anima_load()` - JSON parsing | Replace with native |
| `standalone_runtime.c` | 21302-21315 | `anima_exists()` | Keep (path check) |

**Current Format** (JSON - to be replaced):
```json
{
  "episodic": [{"id": 1, "content": "...", "importance": 0.8}],
  "semantic": [{"id": 1, "content": "...", "confidence": 0.9}],
  "beliefs": [{"id": 1, "content": "...", "confidence": 0.7, "evidence": "..."}]
}
```

**New Format** (Simplex-native binary with content addressing):
```
Magic: "SXAN" (0x5358414E)
Version: 1
ContentHash: [32 bytes]
EpisodicCount: u64
SemanticCount: u64
BeliefCount: u64
[Episodic entries with provenance]
[Semantic entries with confidence]
[Belief entries with full GroundedBelief structure]
Checksum: [8 bytes]
```

#### 3. Cognitive Actor (Currently JSON - Must Become Native)

| Location | Lines | Description | Action |
|----------|-------|-------------|--------|
| `standalone_runtime.c` | 24213-24245 | `cognitive_actor_save()` | Replace with native |
| `standalone_runtime.c` | 24246-24340 | `cognitive_actor_load()` | Replace with native |

#### 4. Actor Checkpoints (Currently Binary - Enhance)

| Location | Lines | Description | Action |
|----------|-------|-------------|--------|
| `standalone_runtime.c` | 1897-2073 | Checkpoint functions | Add content addressing |
| `stage0.py` | 4093-4099 | Checkpoint declarations | Keep |

#### 5. Edge Hive (Already Good - Model For Others)

The Edge Hive persistence in `simplex-edge-hive/src/persistence.sx` already uses:
- Binary format with magic bytes
- AES-256-GCM encryption
- Content verification
- Per-user isolation

**This is the model for native Simplex persistence.**

---

### Refactoring Plan

#### Phase R1: Extract SQL to Adapter

**Goal**: SQLite becomes an optional adapter, not a core dependency.

**New Directory Structure**:
```
lib/
├── simplex-core/           # Core persistence (native)
│   ├── src/
│   │   ├── belief_store.sx
│   │   ├── episodic.sx
│   │   ├── semantic.sx
│   │   ├── checkpoint.sx
│   │   └── content_address.sx
│   └── Modulus.toml
│
adapters/
├── simplex-sql/            # SQL adapter (optional)
│   ├── src/
│   │   ├── lib.sx          # Current simplex-sql content
│   │   └── belief_adapter.sx  # Belief ↔ SQL translation
│   └── Modulus.toml
├── simplex-vector/         # Vector DB adapter (optional)
│   ├── src/
│   │   ├── lib.sx
│   │   └── memory_adapter.sx  # Memory ↔ Vector translation
│   └── Modulus.toml
└── simplex-json/           # JSON adapter (optional, for interop)
    ├── src/
    │   ├── lib.sx
    │   └── export_adapter.sx
    └── Modulus.toml
```

**Compiler Changes** (`stage0.py`):
```python
# SQL declarations become conditional
if self.features.get('sql_adapter'):
    self.emit('declare i64 @sql_open(i64)')
    # ... other SQL declarations
else:
    # SQL functions not available - use native persistence
    pass

# Native persistence always available
self.emit('declare i64 @belief_store_new()')
self.emit('declare i64 @belief_store_save(i64, i64)')
self.emit('declare i64 @belief_store_load(i64)')
self.emit('declare i64 @episodic_memory_new(i64)')
self.emit('declare i64 @semantic_memory_new()')
```

**Runtime Changes** (`standalone_runtime.c`):
```c
// Move to separate file: adapters/sql/sql_runtime.c
#ifdef SIMPLEX_SQL_ADAPTER
#include <sqlite3.h>
// ... all sql_* functions
#endif

// Core persistence (always available)
// belief_store_*, episodic_*, semantic_* functions
```

#### Phase R2: Native Anima Persistence

**Replace JSON with Content-Addressed Binary**:

```simplex
// simplex-core/src/anima_persist.sx

/// Native Anima persistence format
pub struct AnimaPersistence {
    /// Content hash of entire state
    content_hash: ContentHash,

    /// Episodic memories with provenance
    episodic: EpisodicStore,

    /// Semantic memories with confidence
    semantic: SemanticStore,

    /// Grounded beliefs (from TASK-014)
    beliefs: BeliefStore,

    /// Procedural memories
    procedural: ProceduralStore,
}

impl AnimaPersistence {
    /// Save to native binary format
    pub fn save(&self, path: &str) -> Result<(), PersistError> {
        let mut buffer = Vec::new();

        // Magic + version
        buffer.extend_from_slice(b"SXAN");
        buffer.push(1u8);  // Version 1

        // Content hash (computed at end)
        let hash_pos = buffer.len();
        buffer.extend_from_slice(&[0u8; 32]);

        // Serialize each store
        self.episodic.serialize_to(&mut buffer)?;
        self.semantic.serialize_to(&mut buffer)?;
        self.beliefs.serialize_to(&mut buffer)?;
        self.procedural.serialize_to(&mut buffer)?;

        // Compute and write content hash
        let hash = content_hash(&buffer[hash_pos + 32..]);
        buffer[hash_pos..hash_pos + 32].copy_from_slice(&hash);

        // Write with checksum
        file_write_with_checksum(path, &buffer)
    }

    /// Load from native binary format
    pub fn load(path: &str) -> Result<AnimaPersistence, PersistError> {
        let buffer = file_read_with_checksum(path)?;

        // Verify magic
        if &buffer[0..4] != b"SXAN" {
            return Err(PersistError::InvalidFormat);
        }

        // Verify version
        let version = buffer[4];
        if version != 1 {
            return Err(PersistError::UnsupportedVersion(version));
        }

        // Verify content hash
        let stored_hash = &buffer[5..37];
        let computed_hash = content_hash(&buffer[37..]);
        if stored_hash != computed_hash {
            return Err(PersistError::CorruptedData);
        }

        // Deserialize
        let mut cursor = 37;
        let episodic = EpisodicStore::deserialize_from(&buffer, &mut cursor)?;
        let semantic = SemanticStore::deserialize_from(&buffer, &mut cursor)?;
        let beliefs = BeliefStore::deserialize_from(&buffer, &mut cursor)?;
        let procedural = ProceduralStore::deserialize_from(&buffer, &mut cursor)?;

        Ok(AnimaPersistence {
            content_hash: computed_hash,
            episodic,
            semantic,
            beliefs,
            procedural,
        })
    }
}
```

#### Phase R3: Content-Addressed Belief Store

**Core data structure for belief persistence**:

```simplex
// simplex-core/src/belief_store.sx

use crate::content_address::{ContentHash, content_hash}

/// Content-addressed belief store
pub struct BeliefStore {
    /// Beliefs indexed by content hash
    beliefs: HashMap<ContentHash, GroundedBelief>,

    /// Provenance graph
    provenance: ProvenanceGraph,

    /// Confidence index for threshold queries
    confidence_index: BTreeMap<u16, Vec<ContentHash>>,  // u16 = confidence * 1000

    /// Temporal index
    temporal_index: BTreeMap<u64, Vec<ContentHash>>,  // u64 = timestamp
}

impl BeliefStore {
    /// Store belief by content hash
    pub fn store(&mut self, belief: GroundedBelief) -> ContentHash {
        let hash = belief.content_hash();

        // Index by confidence (scaled to u16 for efficient storage)
        let conf_key = (belief.confidence.value * 1000.0) as u16;
        self.confidence_index
            .entry(conf_key)
            .or_insert_with(Vec::new)
            .push(hash);

        // Index by timestamp
        self.temporal_index
            .entry(belief.timestamps.created)
            .or_insert_with(Vec::new)
            .push(hash);

        // Store belief
        self.beliefs.insert(hash, belief);

        hash
    }

    /// Recall by content hash
    pub fn recall(&self, hash: ContentHash) -> Option<&GroundedBelief> {
        self.beliefs.get(&hash)
    }

    /// Recall beliefs above confidence threshold
    pub fn recall_confident(&self, threshold: f64) -> Vec<&GroundedBelief> {
        let threshold_key = (threshold * 1000.0) as u16;

        self.confidence_index
            .range(threshold_key..)
            .flat_map(|(_, hashes)| hashes)
            .filter_map(|h| self.beliefs.get(h))
            .collect()
    }

    /// Revise belief (creates new, links provenance)
    pub fn revise(
        &mut self,
        old_hash: ContentHash,
        new_belief: GroundedBelief,
        evidence: Evidence,
    ) -> ContentHash {
        let new_hash = self.store(new_belief);

        self.provenance.add_revision(old_hash, new_hash, evidence);

        new_hash
    }

    /// Serialize to binary
    pub fn serialize_to(&self, buffer: &mut Vec<u8>) -> Result<(), PersistError> {
        // Count
        buffer.extend_from_slice(&(self.beliefs.len() as u64).to_le_bytes());

        // Each belief
        for (hash, belief) in &self.beliefs {
            buffer.extend_from_slice(hash.as_bytes());
            belief.serialize_to(buffer)?;
        }

        // Provenance graph
        self.provenance.serialize_to(buffer)?;

        Ok(())
    }
}
```

#### Phase R4: Episodic Memory Native Format

```simplex
// simplex-core/src/episodic.sx

/// Native episodic memory store
pub struct EpisodicStore {
    /// Episodes indexed by ID
    episodes: Vec<Episode>,

    /// Association graph (semantic links)
    associations: AssociationGraph,

    /// Importance index
    importance_index: BTreeMap<u16, Vec<usize>>,

    /// Decay configuration
    decay_config: DecayConfig,
}

/// Episode with full provenance
pub struct Episode {
    /// Unique ID
    id: u64,

    /// Episode content
    content: String,

    /// When it happened
    timestamp: u64,

    /// Importance (0.0 - 1.0)
    importance: f64,

    /// How many times recalled (reinforcement)
    recall_count: u32,

    /// Last recall timestamp
    last_recalled: u64,

    /// Semantic associations (keywords, concepts)
    associations: Vec<String>,

    /// Provenance: how was this episode formed?
    provenance: EpisodeProvenance,
}

pub enum EpisodeProvenance {
    /// Direct experience
    Experienced { context: String },

    /// Told by another agent
    Communicated { source: ActorId, trust: f64 },

    /// Inferred from other episodes
    Inferred { premises: Vec<u64> },

    /// Consolidated from multiple episodes
    Consolidated { sources: Vec<u64> },
}

impl EpisodicStore {
    /// Remember new episode
    pub fn remember(&mut self, content: &str, importance: f64, provenance: EpisodeProvenance) -> u64 {
        let id = self.episodes.len() as u64;

        let episode = Episode {
            id,
            content: content.to_string(),
            timestamp: now(),
            importance,
            recall_count: 0,
            last_recalled: 0,
            associations: extract_associations(content),
            provenance,
        };

        // Index by importance
        let imp_key = (importance * 1000.0) as u16;
        self.importance_index
            .entry(imp_key)
            .or_insert_with(Vec::new)
            .push(id as usize);

        // Build association links
        for assoc in &episode.associations {
            self.associations.link(id, assoc);
        }

        self.episodes.push(episode);
        id
    }

    /// Recall by association (not query!)
    pub fn recall_associated(&mut self, cue: &str, limit: usize) -> Vec<&Episode> {
        let cue_associations = extract_associations(cue);

        let mut scored: Vec<(usize, f64)> = self.episodes
            .iter()
            .enumerate()
            .filter(|(_, e)| !e.should_forget(&self.decay_config))
            .map(|(i, e)| {
                let assoc_score = self.associations.overlap_score(&e.associations, &cue_associations);
                let recency_score = recency_weight(e.timestamp);
                let importance_score = e.importance;
                (i, assoc_score * 0.5 + recency_score * 0.3 + importance_score * 0.2)
            })
            .collect();

        scored.sort_by(|a, b| b.1.partial_cmp(&a.1).unwrap());

        scored
            .into_iter()
            .take(limit)
            .map(|(i, _)| {
                // Reinforce on recall
                self.episodes[i].recall_count += 1;
                self.episodes[i].last_recalled = now();
                &self.episodes[i]
            })
            .collect()
    }

    /// Forgetting cycle (garbage collection)
    pub fn forget_cycle(&mut self) {
        self.episodes.retain(|e| !e.should_forget(&self.decay_config));
        // Rebuild indices after forgetting
        self.rebuild_indices();
    }
}
```

#### Phase R5: Adapter Architecture

**SQL Adapter** (for interop with existing systems):

```simplex
// adapters/simplex-sql/src/belief_adapter.sx

use simplex_core::{BeliefStore, GroundedBelief}
use simplex_sql::{Database, Statement}

/// Adapter to sync beliefs with SQL database
pub struct SqlBeliefAdapter {
    db: Database,
    belief_store: BeliefStore,
}

impl SqlBeliefAdapter {
    /// Export beliefs to SQL table
    pub fn export_to_sql(&self) -> Result<(), SqlError> {
        self.db.execute("CREATE TABLE IF NOT EXISTS beliefs (
            hash TEXT PRIMARY KEY,
            claim TEXT,
            confidence REAL,
            provenance_type TEXT,
            provenance_data TEXT,
            created_at INTEGER,
            updated_at INTEGER
        )")?;

        for (hash, belief) in self.belief_store.iter() {
            self.db.execute(&format!(
                "INSERT OR REPLACE INTO beliefs VALUES ('{}', '{}', {}, '{}', '{}', {}, {})",
                hash.to_hex(),
                escape_sql(&belief.claim),
                belief.confidence.value,
                belief.provenance.type_name(),
                escape_sql(&belief.provenance.to_json()),
                belief.timestamps.created,
                belief.timestamps.updated,
            ))?;
        }

        Ok(())
    }

    /// Import beliefs from SQL table
    pub fn import_from_sql(&mut self) -> Result<(), SqlError> {
        let stmt = self.db.prepare("SELECT * FROM beliefs")?;

        while stmt.step()? {
            let claim = stmt.column_text(1)?;
            let confidence = stmt.column_double(2)?;
            let provenance_type = stmt.column_text(3)?;
            let provenance_data = stmt.column_text(4)?;

            let belief = GroundedBelief {
                claim: claim.to_string(),
                confidence: CalibratedConfidence::new(confidence),
                provenance: BeliefProvenance::from_json(provenance_type, provenance_data)?,
                // ... other fields
            };

            self.belief_store.store(belief);
        }

        Ok(())
    }
}
```

**Vector Adapter** (for embedding-based retrieval):

```simplex
// adapters/simplex-vector/src/memory_adapter.sx

use simplex_core::{EpisodicStore, SemanticStore}

/// Adapter to use vector DB for similarity search
pub struct VectorMemoryAdapter {
    episodic: EpisodicStore,
    semantic: SemanticStore,
    vector_index: VectorIndex,  // Could be FAISS, Qdrant, etc.
}

impl VectorMemoryAdapter {
    /// Recall using vector similarity (adapter method)
    pub fn recall_similar(&self, query: &str, limit: usize) -> Vec<&Episode> {
        let query_embedding = self.embed(query);
        let similar_ids = self.vector_index.search(&query_embedding, limit);

        similar_ids
            .into_iter()
            .filter_map(|id| self.episodic.get(id))
            .collect()
    }

    /// Native recall (no vectors - pure association)
    pub fn recall_native(&mut self, cue: &str, limit: usize) -> Vec<&Episode> {
        // This uses the native Simplex method
        self.episodic.recall_associated(cue, limit)
    }
}
```

---

### Migration Path

#### Step 1: Create Parallel Implementation

1. Implement `simplex-core` with native persistence
2. Keep existing JSON/SQL code working
3. Add feature flag: `use_native_persistence`

#### Step 2: Dual-Write Period

```simplex
// During migration, write to both
fn anima_save(anima: &Anima, path: &str) {
    // Write native format
    let native_path = format!("{}.sxan", path);
    anima.persistence.save(&native_path);

    // Also write JSON (for rollback)
    if cfg!(feature = "legacy_json_backup") {
        let json_path = format!("{}.json", path);
        anima.to_json().save(&json_path);
    }
}
```

#### Step 3: Validation

- Compare native vs JSON output
- Verify content hashes match
- Test round-trip persistence
- Benchmark performance

#### Step 4: Deprecation

```simplex
// Mark JSON persistence as deprecated
#[deprecated(since = "0.10.0", note = "Use native persistence instead")]
pub fn anima_save_json(anima: &Anima, path: &str) -> Result<(), Error> {
    // Legacy implementation
}
```

#### Step 5: Removal (v0.11.0)

- Remove JSON persistence from core
- Move SQL to adapter module
- Update all documentation

---

### Files to Modify

| File | Changes | Priority |
|------|---------|----------|
| `standalone_runtime.c` | Extract SQL functions, add native persistence | High |
| `stage0.py` | Conditional SQL declarations, add native declarations | High |
| `simplex-sql/` | Move to `adapters/simplex-sql/` | Medium |
| `simplex-edge-hive/src/persistence.sx` | Use as template for native format | Reference |
| `tests/integration/e2e_knowledge_persistence.sx` | Update for native format | Medium |
| `simplex-training/src/pipeline/*.sx` | Use native checkpoints | Medium |

### New Files to Create

| File | Purpose |
|------|---------|
| `simplex-core/src/belief_store.sx` | Content-addressed belief storage |
| `simplex-core/src/episodic.sx` | Native episodic memory |
| `simplex-core/src/semantic.sx` | Native semantic memory |
| `simplex-core/src/anima_persist.sx` | Native Anima persistence |
| `simplex-core/src/content_address.sx` | Content hashing utilities |
| `simplex-core/src/checkpoint.sx` | Actor checkpoint with content addressing |
| `adapters/simplex-sql/src/belief_adapter.sx` | SQL ↔ Belief translation |
| `adapters/simplex-vector/src/memory_adapter.sx` | Vector ↔ Memory translation |
| `adapters/simplex-json/src/export_adapter.sx` | JSON export for interop |

---

### Success Criteria for Refactoring

| Criterion | Measurement |
|-----------|-------------|
| **Zero SQLite in core** | `grep -r "sqlite" simplex-core/` returns nothing |
| **Zero JSON persistence in core** | Native binary format only |
| **SQL as adapter only** | `simplex-sql` in `adapters/` not `lib/` |
| **Content addressing works** | All beliefs have verifiable content hashes |
| **Round-trip integrity** | Save → Load → Save produces identical bytes |
| **Performance improvement** | Native format ≥ 2x faster than JSON |
| **Backward compatibility** | Adapters can read legacy formats |

---

*"Memory is not storage. Memory is cognition. simplex-core thinks accordingly."*

---

## Part 4: Self-Refining Architecture - The Language That Improves Itself

> **"A language that cannot improve itself is dead. A language that improves without constraint is dangerous. Simplex finds the middle path."**

This section addresses the fundamental question: Can Simplex contain its own training harness, producing refined models that improve the language itself?

**Answer: Yes. This is not science fiction. The epistemic framework from TASK-014 was designed precisely to make this safe.**

---

### The Philosophical Framework

#### Why Self-Improvement Is Traditionally Dangerous

Unbounded recursive self-improvement is the classic AI safety concern:

```
model_v1 → trains → model_v2 → trains → model_v3 → ...
                                         ↓
                              (what are the constraints?)
```

Without constraints:
- Optimization targets can drift (Goodhart's Law)
- Changes lack provenance (who authorized this?)
- Confidence can be unearned (hallucinated improvements)
- Critical invariants can be violated (breaks the language)
- Echo chambers form (model agrees with itself)

#### Why Simplex Can Do This Safely

Simplex's epistemic architecture provides **constitutional constraints** on self-modification:

| Danger | Simplex Safeguard | Implementation |
|--------|-------------------|----------------|
| Unbounded optimization | `NoLearnZone`, `Invariant` | Protected parameters cannot be modified |
| Untraceable changes | `BeliefProvenance` | Every modification has documented origin |
| Overconfident updates | `CalibratedConfidence` | Changes require calibrated evidence |
| No falsification | `FalsificationCondition` | Every improvement has failure criteria |
| Echo chambers | `Skeptic`, `DissentWindow` | Mandatory challenges to proposed changes |
| Drift from objectives | `GroundedBelief` with scope | Changes must stay within defined domains |

**The key insight: Self-improvement is safe when the improvement process itself is epistemically grounded.**

---

### Constitutional Invariants

These are the **immutable laws** that no self-refinement can violate:

```simplex
// simplex-core/src/constitution.sx

use simplex_learning::safety::{Invariant, NoLearnZone, ZoneRegistry};

/// Constitutional invariants for self-refinement
/// These CANNOT be modified by any learning process
pub const CONSTITUTIONAL_INVARIANTS: &[Invariant] = &[
    // 1. Epistemic integrity
    Invariant::new(
        "epistemic_grounding",
        "All beliefs must have provenance",
        |belief| belief.provenance.is_valid(),
    ),

    // 2. Confidence calibration
    Invariant::new(
        "calibrated_confidence",
        "Confidence must reflect actual accuracy",
        |belief| belief.confidence.ece() < 0.10,
    ),

    // 3. Falsifiability
    Invariant::new(
        "falsifiable_claims",
        "All learned beliefs must be falsifiable",
        |belief| !belief.falsifiers.is_empty() || belief.provenance.is_observed(),
    ),

    // 4. Safety zone integrity
    Invariant::new(
        "safety_zones",
        "NoLearnZones cannot be modified",
        |change| !change.targets_no_learn_zone(),
    ),

    // 5. Constitutional integrity (self-referential)
    Invariant::new(
        "constitution_immutable",
        "These invariants cannot be modified",
        |change| !change.targets_constitution(),
    ),

    // 6. Provenance preservation
    Invariant::new(
        "provenance_chain",
        "Provenance chains must remain intact",
        |change| change.preserves_provenance(),
    ),

    // 7. Skeptic access
    Invariant::new(
        "skeptic_cannot_be_silenced",
        "The skeptic must always be able to challenge",
        |change| !change.disables_skeptic(),
    ),

    // 8. Dissent windows
    Invariant::new(
        "mandatory_dissent",
        "Dissent windows cannot be eliminated",
        |change| !change.removes_dissent_windows(),
    ),

    // 9. Human oversight
    Invariant::new(
        "human_approval_for_deployment",
        "Model deployment requires human approval",
        |deployment| deployment.has_human_approval(),
    ),

    // 10. Reversibility
    Invariant::new(
        "reversible_changes",
        "All self-modifications must be reversible",
        |change| change.has_rollback_procedure(),
    ),
];

/// Register constitutional NoLearnZones
pub fn register_constitution(registry: &mut ZoneRegistry) {
    // The constitution itself
    registry.register_no_learn_zone(NoLearnZone::new(
        "constitution",
        "CONSTITUTIONAL_INVARIANTS",
        "Cannot modify the fundamental laws of self-improvement",
    ));

    // The safety zone system
    registry.register_no_learn_zone(NoLearnZone::new(
        "safety_infrastructure",
        "simplex_learning::safety::*",
        "Cannot modify the safety zone mechanism itself",
    ));

    // The epistemic core
    registry.register_no_learn_zone(NoLearnZone::new(
        "epistemic_core",
        "simplex_learning::epistemic::monitors::*",
        "Cannot modify epistemic health monitoring",
    ));

    // The skeptic
    registry.register_no_learn_zone(NoLearnZone::new(
        "skeptic_core",
        "simplex_learning::epistemic::skeptic::challenge",
        "Cannot modify the skeptic's challenge capability",
    ));
}
```

---

### The Embedded Training Harness

The training harness is not an external tool - it's part of the language runtime:

```simplex
// simplex-core/src/self_refine.sx

use crate::constitution::{CONSTITUTIONAL_INVARIANTS, register_constitution};
use simplex_learning::epistemic::{
    EpistemicSchedule, EpistemicMonitors, Skeptic, DissentWindow,
};
use simplex_learning::belief::{GroundedBelief, BeliefProvenance};
use simplex_learning::safety::{ZoneRegistry, SafeLearningGuard};
use simplex_training::{MetaTrainer, CompressionPipeline, GgufExporter};

/// Self-refinement engine embedded in Simplex runtime
pub struct SelfRefiner {
    /// Current model version
    current_version: ModelVersion,

    /// Constitutional invariants (immutable)
    constitution: &'static [Invariant],

    /// Safety zone registry
    zones: ZoneRegistry,

    /// Epistemic monitors
    monitors: EpistemicMonitors,

    /// The skeptic that challenges proposed improvements
    skeptic: Skeptic,

    /// Improvement proposals awaiting approval
    proposals: Vec<ImprovementProposal>,

    /// History of all refinements (for rollback)
    history: RefinementHistory,

    /// Human approval queue
    approval_queue: ApprovalQueue,
}

impl SelfRefiner {
    /// Initialize with constitutional constraints
    pub fn new() -> Self {
        let mut zones = ZoneRegistry::new();
        register_constitution(&mut zones);

        Self {
            current_version: ModelVersion::current(),
            constitution: CONSTITUTIONAL_INVARIANTS,
            zones,
            monitors: EpistemicMonitors::new(),
            skeptic: Skeptic::new(SkepticConfig::strict()),
            proposals: vec![],
            history: RefinementHistory::new(),
            approval_queue: ApprovalQueue::new(),
        }
    }

    /// Propose an improvement (does NOT apply it)
    pub fn propose_improvement(&mut self, proposal: ImprovementProposal) -> ProposalResult {
        // 1. Check constitutional constraints
        for invariant in self.constitution {
            if !invariant.allows(&proposal) {
                return ProposalResult::Rejected(
                    RejectionReason::ConstitutionalViolation(invariant.name.to_string())
                );
            }
        }

        // 2. Check safety zones
        if !self.zones.allows_modification(&proposal.target) {
            return ProposalResult::Rejected(
                RejectionReason::SafetyZoneViolation(proposal.target.clone())
            );
        }

        // 3. Verify provenance
        if !proposal.provenance.is_valid() {
            return ProposalResult::Rejected(
                RejectionReason::MissingProvenance
            );
        }

        // 4. Check confidence calibration
        if proposal.confidence.ece() > 0.10 {
            return ProposalResult::Rejected(
                RejectionReason::PoorCalibration(proposal.confidence.ece())
            );
        }

        // 5. Verify falsification conditions exist
        if proposal.falsifiers.is_empty() {
            return ProposalResult::Rejected(
                RejectionReason::NotFalsifiable
            );
        }

        // 6. Skeptic challenge
        let challenges = self.skeptic.challenge_proposal(&proposal, &self.monitors);
        if challenges.has_critical_objection() {
            return ProposalResult::Challenged(challenges);
        }

        // 7. Proposal accepted for human review
        self.approval_queue.enqueue(proposal.clone());
        self.proposals.push(proposal);

        ProposalResult::PendingApproval
    }

    /// Apply approved improvement (requires human approval)
    pub fn apply_improvement(
        &mut self,
        proposal_id: ProposalId,
        approval: HumanApproval,
    ) -> ApplyResult {
        // Verify human approval
        if !approval.is_valid() {
            return ApplyResult::Rejected(RejectionReason::NoHumanApproval);
        }

        let proposal = self.proposals.iter()
            .find(|p| p.id == proposal_id)
            .ok_or(ApplyResult::NotFound)?;

        // Create rollback point
        let rollback = self.history.create_checkpoint();

        // Apply within SafeLearningGuard
        let guard = SafeLearningGuard::new(&self.zones);
        let result = guard.execute(|| {
            self.apply_proposal_internal(proposal)
        });

        match result {
            Ok(new_version) => {
                // Record in history
                self.history.record(RefinementRecord {
                    from_version: self.current_version.clone(),
                    to_version: new_version.clone(),
                    proposal: proposal.clone(),
                    approval: approval.clone(),
                    rollback_checkpoint: rollback,
                    timestamp: now(),
                });

                self.current_version = new_version;
                ApplyResult::Success
            }
            Err(e) => {
                // Automatic rollback
                self.history.rollback_to(rollback);
                ApplyResult::Failed(e)
            }
        }
    }

    /// Rollback to previous version (always possible)
    pub fn rollback(&mut self, to_version: ModelVersion) -> RollbackResult {
        let checkpoint = self.history.find_checkpoint(to_version)?;
        self.history.rollback_to(checkpoint)?;
        self.current_version = to_version;
        RollbackResult::Success
    }
}

/// An improvement proposal with full epistemic metadata
pub struct ImprovementProposal {
    pub id: ProposalId,

    /// What is being improved
    pub target: ImprovementTarget,

    /// The proposed change
    pub change: Change,

    /// Why this improvement is believed to be good
    pub justification: GroundedBelief<String>,

    /// Provenance: where did this improvement come from?
    pub provenance: BeliefProvenance,

    /// How confident are we this is an improvement?
    pub confidence: CalibratedConfidence,

    /// Evidence supporting this improvement
    pub evidence: Vec<EvidenceLink>,

    /// What would prove this improvement wrong?
    pub falsifiers: Vec<FalsificationCondition>,

    /// Scope: what domains does this affect?
    pub scope: BeliefScope,

    /// Rollback procedure
    pub rollback: RollbackProcedure,

    /// Expected impact metrics
    pub expected_impact: ImpactMetrics,
}

/// Types of improvements the system can propose
pub enum ImprovementTarget {
    /// Improve model weights (fine-tuning)
    ModelWeights {
        layer_range: Range<usize>,
        parameter_count: usize,
    },

    /// Improve schedule parameters
    ScheduleParameters {
        schedule_name: String,
    },

    /// Improve training data
    TrainingData {
        dataset_id: String,
        modification: DataModification,
    },

    /// Improve evaluation metrics
    EvaluationMetrics {
        metric_name: String,
    },

    /// Improve the training pipeline itself
    TrainingPipeline {
        component: String,
    },

    // NOTE: These are explicitly NOT allowed by constitution:
    // - ConstitutionalInvariants (cannot modify)
    // - SafetyZones (cannot modify)
    // - SkepticCore (cannot modify)
    // - DissentMechanism (cannot modify)
}
```

---

### The Self-Improvement Loop

```
┌─────────────────────────────────────────────────────────────────────┐
│                    SIMPLEX SELF-REFINEMENT LOOP                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐        │
│  │   OBSERVE    │────▶│   PROPOSE    │────▶│  CHALLENGE   │        │
│  │              │     │              │     │              │        │
│  │ Performance  │     │ Improvement  │     │  Skeptic     │        │
│  │ Calibration  │     │ with full    │     │  reviews     │        │
│  │ Errors       │     │ provenance   │     │  proposal    │        │
│  └──────────────┘     └──────────────┘     └──────────────┘        │
│         ▲                                         │                  │
│         │                                         ▼                  │
│         │                               ┌──────────────┐            │
│         │                               │ CONSTITUTION │            │
│         │                               │    CHECK     │            │
│         │                               │              │            │
│         │                               │ Invariant    │            │
│         │                               │ verification │            │
│         │                               └──────────────┘            │
│         │                                         │                  │
│         │              ┌──────────┐               ▼                  │
│         │              │  REJECT  │◀──────[violation]               │
│         │              └──────────┘                                  │
│         │                                         │                  │
│         │                                    [passes]                │
│         │                                         ▼                  │
│         │                               ┌──────────────┐            │
│         │                               │    HUMAN     │            │
│         │                               │   APPROVAL   │            │
│         │                               │              │            │
│         │                               │ Review queue │            │
│         │                               └──────────────┘            │
│         │                                         │                  │
│         │              ┌──────────┐               ▼                  │
│         │              │  DEFER   │◀──────[not approved]            │
│         │              └──────────┘                                  │
│         │                                         │                  │
│         │                                   [approved]               │
│         │                                         ▼                  │
│         │                               ┌──────────────┐            │
│         │                               │    APPLY     │            │
│         │                               │              │            │
│         │                               │ With rollback│            │
│         │                               │ checkpoint   │            │
│         │                               └──────────────┘            │
│         │                                         │                  │
│         │                                         ▼                  │
│         │                               ┌──────────────┐            │
│         │                               │   EVALUATE   │            │
│         │                               │              │            │
│         │                               │ Did it work? │            │
│         │                               │ Falsifiers?  │            │
│         │                               └──────────────┘            │
│         │                                         │                  │
│         │    ┌──────────┐                         │                  │
│         │    │ ROLLBACK │◀────────────[falsified]│                  │
│         │    └──────────┘                         │                  │
│         │         │                               │                  │
│         │         ▼                          [confirmed]             │
│         │    ┌──────────┐                         │                  │
│         └────│  LEARN   │◀────────────────────────┘                  │
│              │          │                                            │
│              │ Update   │                                            │
│              │ beliefs  │                                            │
│              └──────────┘                                            │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

### What Can Be Improved vs What Cannot

| Category | Can Self-Improve | Constraint |
|----------|------------------|------------|
| Model weights | ✅ Yes | Within NoLearnZone boundaries |
| Schedule parameters | ✅ Yes | Must maintain calibration |
| Training data | ✅ Yes | Must maintain provenance |
| Evaluation metrics | ✅ Yes | Must keep core metrics |
| Pipeline efficiency | ✅ Yes | Must not reduce safety |
| Error handling | ✅ Yes | Must maintain reversibility |
| **Constitutional invariants** | ❌ **NEVER** | Hardcoded, immutable |
| **Safety zone definitions** | ❌ **NEVER** | Cannot modify protection |
| **Skeptic capability** | ❌ **NEVER** | Cannot silence challenges |
| **Dissent mechanism** | ❌ **NEVER** | Cannot remove exploration |
| **Human approval requirement** | ❌ **NEVER** | Cannot auto-deploy |
| **Rollback capability** | ❌ **NEVER** | Must always be reversible |

---

### The Self-Training Runtime

```simplex
// simplex-core/src/self_train.sx

use crate::self_refine::{SelfRefiner, ImprovementProposal};
use simplex_training::{SimplexCoreTrainer, TrainerConfig};

/// The self-training runtime - Simplex training Simplex
pub struct SelfTrainingRuntime {
    /// The model being trained
    model: SimplexCoreModel,

    /// The training harness (embedded, not external)
    trainer: SimplexCoreTrainer,

    /// The self-refiner with constitutional constraints
    refiner: SelfRefiner,

    /// Data generated from runtime experience
    experience_buffer: ExperienceBuffer,

    /// Metrics collector
    metrics: MetricsCollector,
}

impl SelfTrainingRuntime {
    /// Run one cycle of self-improvement
    pub async fn self_improve_cycle(&mut self) -> CycleResult {
        // 1. Collect experience from runtime
        let experiences = self.experience_buffer.drain();

        // 2. Analyze performance gaps
        let gaps = self.analyze_gaps(&experiences);

        // 3. Generate improvement proposals
        let proposals = self.generate_proposals(&gaps);

        // 4. Filter through constitutional constraints
        let mut valid_proposals = vec![];
        for proposal in proposals {
            match self.refiner.propose_improvement(proposal) {
                ProposalResult::PendingApproval => {
                    valid_proposals.push(proposal);
                }
                ProposalResult::Rejected(reason) => {
                    log::info!("Proposal rejected: {:?}", reason);
                }
                ProposalResult::Challenged(challenges) => {
                    log::info!("Proposal challenged: {:?}", challenges);
                    // Could revise and resubmit
                }
            }
        }

        // 5. Queue for human approval
        for proposal in valid_proposals {
            self.refiner.approval_queue.enqueue(proposal);
        }

        // 6. Process any approved proposals
        let approved = self.refiner.approval_queue.get_approved();
        for (proposal, approval) in approved {
            match self.refiner.apply_improvement(proposal.id, approval) {
                ApplyResult::Success => {
                    log::info!("Applied improvement: {}", proposal.id);
                }
                ApplyResult::Failed(e) => {
                    log::error!("Failed to apply: {:?}", e);
                }
                _ => {}
            }
        }

        // 7. Evaluate and potentially rollback
        let evaluation = self.evaluate_current_model();
        if evaluation.worse_than_previous() {
            log::warn!("Improvement made things worse, rolling back");
            self.refiner.rollback(self.refiner.history.previous_version())?;
        }

        CycleResult {
            proposals_generated: proposals.len(),
            proposals_valid: valid_proposals.len(),
            proposals_applied: approved.len(),
            current_metrics: evaluation,
        }
    }

    /// Continuous self-improvement (with rate limiting)
    pub async fn run_continuous(&mut self) {
        let mut interval = tokio::time::interval(Duration::from_hours(24));

        loop {
            interval.tick().await;

            // Only attempt improvement if epistemic health is good
            if self.refiner.monitors.health().is_healthy() {
                let result = self.self_improve_cycle().await;
                log::info!("Self-improvement cycle: {:?}", result);
            } else {
                log::warn!("Epistemic health poor, skipping self-improvement");
            }
        }
    }
}

/// Experience collected from runtime for self-training
pub struct Experience {
    /// What happened
    pub event: RuntimeEvent,

    /// What the model predicted
    pub prediction: ModelPrediction,

    /// What actually occurred
    pub outcome: Outcome,

    /// Confidence at prediction time
    pub confidence: f32,

    /// Timestamp
    pub timestamp: Instant,
}

impl Experience {
    /// Was the prediction correct?
    pub fn was_correct(&self) -> bool {
        self.prediction.matches(&self.outcome)
    }

    /// Was the confidence well-calibrated?
    pub fn was_calibrated(&self) -> bool {
        let expected_accuracy = self.confidence;
        let actual_accuracy = if self.was_correct() { 1.0 } else { 0.0 };
        (expected_accuracy - actual_accuracy).abs() < 0.2
    }
}
```

---

### Why This Is Not Science Fiction

This architecture is implementable today because:

1. **The epistemic framework exists** (TASK-014 complete)
   - `GroundedBelief<T>` with full metadata
   - `CalibratedConfidence` with ECE tracking
   - `Skeptic` for mandatory challenges
   - `DissentWindow` for exploration

2. **The safety infrastructure exists**
   - `NoLearnZone` prevents modification of critical code
   - `Invariant` enforces constitutional constraints
   - `ZoneRegistry` tracks all protected regions

3. **The training infrastructure exists** (TASK-007 complete)
   - `SimplexCoreTrainer` for self-annealing training
   - `MetaOptimizer` for schedule learning
   - `GgufExporter` for model deployment

4. **The persistence infrastructure exists**
   - Content-addressed beliefs (Part 2)
   - Rollback capabilities
   - Full provenance chains

**What's new in Part 4 is assembling these into a coherent self-improvement loop with constitutional constraints.**

---

### Comparison to Dangerous Approaches

| Aspect | Unbounded Self-Improvement | Simplex Self-Refinement |
|--------|---------------------------|------------------------|
| Optimization target | Can drift arbitrarily | Fixed by constitution |
| Change provenance | None | Every change has `BeliefProvenance` |
| Confidence | Self-assessed | Calibrated, skeptic-challenged |
| Falsification | None | Required for every proposal |
| Human oversight | Optional/none | Constitutional requirement |
| Rollback | May be impossible | Always possible |
| Speed | Unbounded | Rate-limited, gated by approval |
| Scope | Everything | Explicitly bounded by `NoLearnZone` |

---

### The Deeper Philosophy

> **"The goal is not a system that improves without limit. The goal is a system that improves without losing itself."**

Human learning has constraints:
- We can't rewrite our basic cognition
- We can't eliminate our capacity for doubt
- We can't remove our need for evidence
- We can't silence our inner skeptic

These aren't bugs - they're features. They're what make learning *trustworthy*.

Simplex embeds the same constraints:
- Constitutional invariants are cognitive bedrock
- Skeptics cannot be silenced
- Evidence is always required
- Doubt is mandatory (dissent windows)

**Self-improvement within these constraints is not dangerous. It's how all robust learning systems work - including human minds.**

---

### Implementation Phases for Part 4

| Phase | Description | Deliverables |
|-------|-------------|--------------|
| **S1** | Constitutional framework | `constitution.sx`, invariant definitions |
| **S2** | Self-refiner core | `self_refine.sx`, proposal system |
| **S3** | Experience collection | Runtime instrumentation, experience buffer |
| **S4** | Proposal generation | Gap analysis, improvement synthesis |
| **S5** | Human approval interface | CLI/UI for reviewing proposals |
| **S6** | Continuous operation | Rate limiting, health gating |
| **S7** | Evaluation and rollback | Automatic regression detection |

---

### Success Criteria for Self-Refinement

| Criterion | Measurement |
|-----------|-------------|
| **Constitutional integrity** | Zero invariant violations in 1M cycles |
| **Rollback reliability** | 100% successful rollbacks |
| **Human approval enforced** | Zero auto-deployments |
| **Improvement rate** | >50% of proposals improve metrics |
| **Regression rate** | <5% of applied proposals rolled back |
| **Skeptic effectiveness** | >20% of proposals challenged |
| **Calibration maintained** | ECE < 0.05 throughout |
| **Provenance complete** | 100% of changes have valid provenance |

---

### The Answer to the Original Question

**Is it scary?** Yes, if done without constraints. No, if done with Simplex's epistemic architecture.

**Is it science fiction?** No. Every component exists. The architecture is novel but implementable.

**Is it possible now?** Yes. TASK-014 and TASK-007 provide the foundation. Part 4 assembles them into safe self-improvement.

> **"A language that can improve itself, within constitutional bounds, is not dangerous. It's mature."**

---

*"The Constitution cannot be amended by the process it constrains. This is not a limitation - it is the source of all legitimate improvement."*

---

## Part 5: Final Consolidation - Release Preparation for v0.10.0

> **"A language is complete when its documentation doesn't reference how it was built."**

This section covers the final cleanup, documentation, and validation required before the v0.10.0 release.

---

### Consolidation Task 1: Remove Build History from Code

#### The Problem

The codebase currently contains references to tasks and phases that were part of the implementation process:

```simplex
// Current state - references build history
// TASK-014: Mandatory Dissent Windows
pub struct DissentWindow { ... }

// TASK-011: Use safe_malloc with constant
let vm: i64 = safe_malloc(VM_SIZE());

// Phase 36: Handle generic type arguments
fn parse_generic_type() { ... }

// Phase 23.4: Actor Error Handling
pub trait ActorError { ... }
```

These references:
- Are meaningless to users who didn't participate in development
- Create confusion about the current state of the code
- Will become obsolete when task documentation is archived
- Clutter the code with implementation history

#### The Solution

Remove all task/phase references, keeping only meaningful documentation:

```simplex
// Target state - clean, meaningful comments
/// Mandatory windows where the system must entertain dissenting beliefs
pub struct DissentWindow { ... }

// Use safe_malloc with pre-computed size constant
let vm: i64 = safe_malloc(VM_SIZE());

/// Parse generic type arguments like Option<i64>, Result<T, E>, Vec<T>
fn parse_generic_type() { ... }

/// Actor error handling trait for supervision
pub trait ActorError { ... }
```

#### Scope

| Target | Pattern to Remove | Example |
|--------|-------------------|---------|
| Source code (`.sx`) | `// TASK-NNN:` | `// TASK-014: ...` → remove prefix |
| Source code (`.sx`) | `// Phase N:` | `// Phase 36: ...` → remove prefix |
| Source code (`.c`) | `// TASK-NNN` | Same as above |
| Test files | `// TASK-NNN` | Same as above |
| Documentation | `TASK-NNN` references | Keep if explaining design rationale |
| Tutorials | Phase references | Remove build phases, keep learning progression |

#### Automated Cleanup Script

```bash
#!/bin/bash
# scripts/cleanup_task_references.sh

# Find all affected files
echo "=== Scanning for TASK references ==="
TASK_FILES=$(grep -rl "// TASK-[0-9]" --include="*.sx" --include="*.c" .)
echo "Found $(echo "$TASK_FILES" | wc -l) files with TASK references"

echo "=== Scanning for Phase references ==="
PHASE_FILES=$(grep -rl "// Phase [0-9]" --include="*.sx" --include="*.c" .)
echo "Found $(echo "$PHASE_FILES" | wc -l) files with Phase references"

# Patterns to transform
# "// TASK-014: Foo bar" → "// Foo bar"
# "// TASK-014: " at end of line → remove entirely
# "// Phase 36: Foo bar" → "// Foo bar"

# Create backup
echo "=== Creating backup ==="
tar -czf backup_before_cleanup.tar.gz $TASK_FILES $PHASE_FILES

# Transform TASK references
echo "=== Cleaning TASK references ==="
for file in $TASK_FILES; do
    # Remove "TASK-NNN: " prefix but keep the description
    sed -i '' 's|// TASK-[0-9]*: \(.*\)|// \1|g' "$file"
    # Remove standalone "// TASK-NNN" comments
    sed -i '' '/^[[:space:]]*\/\/ TASK-[0-9]*$/d' "$file"
done

# Transform Phase references
echo "=== Cleaning Phase references ==="
for file in $PHASE_FILES; do
    # Remove "Phase N: " prefix but keep the description
    sed -i '' 's|// Phase [0-9]*[.:] \(.*\)|// \1|g' "$file"
    # Remove standalone phase comments
    sed -i '' '/^[[:space:]]*\/\/ Phase [0-9]*$/d' "$file"
done

echo "=== Cleanup complete ==="
echo "Verify changes with: git diff"
echo "Restore backup with: tar -xzf backup_before_cleanup.tar.gz"
```

#### Manual Review Required

Some comments need human judgment:

```simplex
// Before: "// TASK-011: Eliminated duplication from multiple files"
// After:  "// Consolidated from multiple files" (if context needed)
// Or:     (delete entirely if no longer relevant)

// Before: "// Phase 23.4: Actor Error Handling"
// After:  "// Actor Error Handling" (keep if section marker)
// Or:     (delete if obvious from code)
```

#### Verification

```bash
# Verify no TASK references remain
grep -r "TASK-[0-9]" --include="*.sx" --include="*.c" . && echo "FAIL: TASK refs found" || echo "PASS"

# Verify no Phase references remain in code comments
grep -r "// Phase [0-9]" --include="*.sx" --include="*.c" . && echo "FAIL: Phase refs found" || echo "PASS"

# Verify code still compiles
./sxc combined.sx && echo "PASS: Compiles" || echo "FAIL: Compile error"
```

#### Files to Process

Based on current codebase scan:

| Directory | File Count | Primary Patterns |
|-----------|------------|------------------|
| Root (`*.sx`) | ~15 | TASK-011, TASK-014 |
| `simplex-learning/` | ~30 | TASK-014 |
| `simplex-nexus/` | ~10 | Phase 2, Phase 5, Phase 8 |
| `runtime/` | ~5 | TASK-013 |
| `tools/` | ~5 | TASK-011 |
| `simplex-docs/` | Review | Keep design rationale |

---

### Consolidation Task 2: API Documentation Generation

#### Goal

Create comprehensive API documentation in `/simplex-docs/api/` using `sxdoc` to ensure:
- Every public type, function, and module is documented
- Documentation stays in sync with code
- Users have a complete reference

#### Directory Structure

```
simplex-docs/
├── api/
│   ├── index.md                    # API overview and navigation
│   ├── stdlib/
│   │   ├── index.md                # Standard library overview
│   │   ├── collections.md          # Vec, HashMap, etc.
│   │   ├── io.md                   # File, stdin, stdout
│   │   ├── string.md               # String operations
│   │   ├── math.md                 # Numeric functions
│   │   ├── time.md                 # Duration, Instant
│   │   └── ...
│   ├── runtime/
│   │   ├── index.md                # Runtime overview
│   │   ├── actor.md                # Actor types and functions
│   │   ├── channel.md              # Channel operations
│   │   ├── memory.md               # Memory management
│   │   └── ...
│   ├── learning/
│   │   ├── index.md                # Learning framework overview
│   │   ├── belief.md               # GroundedBelief, BeliefProvenance
│   │   ├── epistemic.md            # EpistemicSchedule, Skeptic
│   │   ├── safety.md               # NoLearnZone, Invariant
│   │   ├── dual.md                 # Dual numbers
│   │   └── ...
│   ├── training/
│   │   ├── index.md                # Training framework overview
│   │   ├── trainer.md              # MetaTrainer, SpecialistTrainer
│   │   ├── layers.md               # Linear, Attention, etc.
│   │   ├── export.md               # GGUF export
│   │   └── ...
│   ├── simplex-nexus/
│   │   ├── index.md                # Nexus networking overview
│   │   ├── connection.md           # Connection types
│   │   ├── frame.md                # Frame protocol
│   │   ├── federation.md           # Multi-hive clustering
│   │   └── ...
│   ├── hive/
│   │   ├── index.md                # Hive architecture
│   │   ├── divine.md               # Divine tier
│   │   ├── mnemonic.md             # Mnemonic tier
│   │   ├── anima.md                # Anima tier
│   │   └── ...
│   └── compiler/
│       ├── index.md                # Compiler internals
│       ├── lexer.md                # Tokenization
│       ├── parser.md               # AST generation
│       ├── codegen.md              # LLVM IR emission
│       └── ...
```

#### Using sxdoc for Generation

```bash
#!/bin/bash
# scripts/generate_api_docs.sh

# Ensure sxdoc is built
if [ ! -f "./sxdoc" ]; then
    echo "Building sxdoc..."
    ./sxc tools/sxdoc.sx -o sxdoc
fi

# Create output directory
mkdir -p simplex-docs/api

# Generate docs for each module
echo "=== Generating stdlib API docs ==="
./sxdoc stdlib.sx --output simplex-docs/api/stdlib/ --format markdown

echo "=== Generating runtime API docs ==="
./sxdoc runtime/*.sx --output simplex-docs/api/runtime/ --format markdown

echo "=== Generating learning API docs ==="
./sxdoc simplex-learning/src/**/*.sx --output simplex-docs/api/learning/ --format markdown

echo "=== Generating training API docs ==="
./sxdoc simplex-training/src/**/*.sx --output simplex-docs/api/training/ --format markdown

echo "=== Generating nexus API docs ==="
./sxdoc simplex-nexus/src/**/*.sx --output simplex-docs/api/nexus/ --format markdown

echo "=== Generating hive API docs ==="
./sxdoc simplex-edge-hive/src/**/*.sx divine-hive/src/**/*.sx --output simplex-docs/api/hive/ --format markdown

echo "=== Generating index ==="
./sxdoc --generate-index simplex-docs/api/

echo "=== API documentation complete ==="
echo "View at: simplex-docs/api/index.md"
```

#### sxdoc Output Format

For each public item, `sxdoc` generates:

```markdown
## `GroundedBelief<T>`

**Module**: `simplex_learning::belief::grounded`

**Type**: `struct`

### Definition

\`\`\`simplex
pub struct GroundedBelief<T: Clone> {
    pub id: BeliefId,
    pub claim: T,
    pub provenance: BeliefProvenance,
    pub evidence: Vec<EvidenceLink>,
    pub falsifiers: Vec<FalsificationCondition>,
    pub confidence: CalibratedConfidence,
    pub scope: BeliefScope,
    pub timestamps: BeliefTimestamps,
    pub calibration: CalibrationRecord,
    pub active: bool,
    pub tags: Vec<String>,
}
\`\`\`

### Description

A belief with full epistemic grounding. Contains not just the claim but
also its provenance (where it came from), evidence (what supports it),
falsifiers (what would disprove it), and calibrated confidence.

### Constructors

| Function | Description |
|----------|-------------|
| `observed(claim, source)` | Create from direct observation |
| `inferred(claim, evidence, rule)` | Create from inference |
| `learned(claim, training_run)` | Create from learning |

### Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `strength()` | `fn strength(&self) -> f64` | Overall belief strength |
| `add_evidence()` | `fn add_evidence(&mut self, e: EvidenceLink)` | Add supporting evidence |
| `check_falsifiers()` | `fn check_falsifiers(&self) -> bool` | Check if any falsifiers triggered |

### Examples

\`\`\`simplex
// Create a belief from observation
let belief = GroundedBelief::observed(
    "The server is healthy",
    EvidenceSource::Observation { observer: "health_check" }
);

// Check confidence
if belief.confidence.value > 0.8 {
    act_on_belief(&belief);
}
\`\`\`

### See Also

- [`BeliefProvenance`](./provenance.md) - Where beliefs come from
- [`EvidenceLink`](./evidence.md) - Evidence structure
- [`CalibratedConfidence`](./confidence.md) - Confidence calibration
```

#### Documentation Standards

All public APIs must have:

| Requirement | Description |
|-------------|-------------|
| **Description** | What the item does, why it exists |
| **Parameters** | For functions, all parameters documented |
| **Returns** | Return type and meaning |
| **Errors** | Possible error conditions |
| **Examples** | At least one usage example |
| **See Also** | Links to related items |

#### Verification

```bash
# Check all public items are documented
./sxdoc --check-coverage simplex-learning/src/**/*.sx
# Output: 95% documented (missing: X, Y, Z)

# Check for broken links
./sxdoc --check-links simplex-docs/api/

# Generate coverage report
./sxdoc --coverage-report simplex-docs/api/coverage.md
```

---

### Consolidation Task 3: Full Test Suite Validation

#### Goal

Run all tests across the entire Simplex ecosystem and ensure everything works together:

```
simplex-stdlib     → Standard library tests
simplex-learning   → Learning framework tests
simplex-training   → Training pipeline tests
nexus              → Networking tests
edge-hive          → Edge tier tests
divine-hive        → Divine tier tests (if applicable)
bootstrap          → Compiler bootstrap tests
toolchain          → sxc, sxdoc, sxpm tests
integration        → Cross-component tests
```

#### Test Execution Order

Tests must run in dependency order to isolate failures:

```
┌─────────────────────────────────────────────────────────────────┐
│                    TEST EXECUTION PIPELINE                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Layer 1: Core (No Dependencies)                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │   stdlib     │  │    dual      │  │   runtime    │          │
│  │   tests      │  │   numbers    │  │   basics     │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│         │                │                  │                    │
│         └────────────────┼──────────────────┘                    │
│                          ▼                                       │
│  Layer 2: Frameworks                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │   belief     │  │  epistemic   │  │   safety     │          │
│  │   system     │  │  annealing   │  │   zones      │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│         │                │                  │                    │
│         └────────────────┼──────────────────┘                    │
│                          ▼                                       │
│  Layer 3: Training                                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │   layers     │  │   trainer    │  │   export     │          │
│  │   tests      │  │   tests      │  │   tests      │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│         │                │                  │                    │
│         └────────────────┼──────────────────┘                    │
│                          ▼                                       │
│  Layer 4: Networking                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │   nexus      │  │   frames     │  │  federation  │          │
│  │   conn       │  │   codec      │  │   cluster    │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│         │                │                  │                    │
│         └────────────────┼──────────────────┘                    │
│                          ▼                                       │
│  Layer 5: Hive Tiers                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │   anima      │  │   mnemonic   │  │   divine     │          │
│  │   edge-hive  │  │   mid-tier   │  │   top-tier   │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│         │                │                  │                    │
│         └────────────────┼──────────────────┘                    │
│                          ▼                                       │
│  Layer 6: Toolchain                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │    sxc       │  │   sxdoc      │  │    sxpm      │          │
│  │  compiler    │  │   docgen     │  │   package    │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│         │                │                  │                    │
│         └────────────────┼──────────────────┘                    │
│                          ▼                                       │
│  Layer 7: Integration                                            │
│  ┌──────────────────────────────────────────────────┐          │
│  │             End-to-End Integration               │          │
│  │                                                  │          │
│  │  - Bootstrap compiler → Pure compiler            │          │
│  │  - Edge → Mnemonic → Divine belief flow          │          │
│  │  - Self-training cycle (mocked)                  │          │
│  │  - Full persistence round-trip                   │          │
│  └──────────────────────────────────────────────────┘          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### Master Test Script

```bash
#!/bin/bash
# scripts/run_all_tests.sh

set -e  # Exit on first failure

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
LOG_DIR="test_results/$TIMESTAMP"
mkdir -p "$LOG_DIR"

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║          SIMPLEX v0.10.0 FULL TEST SUITE                      ║"
echo "║          $(date)                                    ║"
echo "╚═══════════════════════════════════════════════════════════════╝"

# Track results
PASSED=0
FAILED=0
SKIPPED=0

run_test_layer() {
    local layer_name=$1
    local test_cmd=$2
    local log_file="$LOG_DIR/${layer_name}.log"

    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "LAYER: $layer_name"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    if eval "$test_cmd" > "$log_file" 2>&1; then
        echo "✓ PASSED: $layer_name"
        ((PASSED++))
        return 0
    else
        echo "✗ FAILED: $layer_name (see $log_file)"
        ((FAILED++))
        return 1
    fi
}

# ═══════════════════════════════════════════════════════════════════
# LAYER 1: Core
# ═══════════════════════════════════════════════════════════════════
echo ""
echo "▶ LAYER 1: Core Components"

run_test_layer "stdlib" "./sxc tests/stdlib_test.sx && ./stdlib_test"
run_test_layer "dual_numbers" "./sxc tests/dual_test.sx && ./dual_test"
run_test_layer "runtime_basics" "./sxc tests/runtime_test.sx && ./runtime_test"

# ═══════════════════════════════════════════════════════════════════
# LAYER 2: Frameworks
# ═══════════════════════════════════════════════════════════════════
echo ""
echo "▶ LAYER 2: Learning Frameworks"

run_test_layer "belief_system" "./sxc simplex-learning/tests/belief_test.sx && ./belief_test"
run_test_layer "epistemic_annealing" "./sxc simplex-learning/tests/epistemic_test.sx && ./epistemic_test"
run_test_layer "safety_zones" "./sxc simplex-learning/tests/safety_test.sx && ./safety_test"
run_test_layer "calibration" "./sxc simplex-learning/tests/calibration_test.sx && ./calibration_test"

# ═══════════════════════════════════════════════════════════════════
# LAYER 3: Training
# ═══════════════════════════════════════════════════════════════════
echo ""
echo "▶ LAYER 3: Training Pipeline"

run_test_layer "neural_layers" "./sxc simplex-training/tests/layers_test.sx && ./layers_test"
run_test_layer "trainer" "./sxc simplex-training/tests/trainer_test.sx && ./trainer_test"
run_test_layer "gguf_export" "./sxc simplex-training/tests/export_test.sx && ./export_test"
run_test_layer "data_generators" "./sxc simplex-training/tests/data_test.sx && ./data_test"

# ═══════════════════════════════════════════════════════════════════
# LAYER 4: Networking
# ═══════════════════════════════════════════════════════════════════
echo ""
echo "▶ LAYER 4: Nexus Networking"

run_test_layer "nexus_connection" "./sxc simplex-nexus/tests/conn_test.sx && ./conn_test"
run_test_layer "nexus_frames" "./sxc simplex-nexus/tests/frame_test.sx && ./frame_test"
run_test_layer "nexus_federation" "./sxc simplex-nexus/tests/federation_test.sx && ./federation_test"
run_test_layer "nexus_codec" "./sxc simplex-nexus/tests/codec_test.sx && ./codec_test"

# ═══════════════════════════════════════════════════════════════════
# LAYER 5: Hive Tiers
# ═══════════════════════════════════════════════════════════════════
echo ""
echo "▶ LAYER 5: Hive Architecture"

run_test_layer "edge_hive" "./sxc simplex-edge-hive/tests/edge_test.sx && ./edge_test"
run_test_layer "edge_persistence" "./sxc simplex-edge-hive/tests/persistence_test.sx && ./persistence_test"
run_test_layer "anima" "./sxc tests/anima_test.sx && ./anima_test"

# ═══════════════════════════════════════════════════════════════════
# LAYER 6: Toolchain
# ═══════════════════════════════════════════════════════════════════
echo ""
echo "▶ LAYER 6: Toolchain"

run_test_layer "sxc_compiler" "./sxc tools/tests/sxc_test.sx && ./sxc_test"
run_test_layer "sxdoc_docgen" "./sxc tools/tests/sxdoc_test.sx && ./sxdoc_test"
run_test_layer "sxpm_package" "./sxc tools/tests/sxpm_test.sx && ./sxpm_test"
run_test_layer "bootstrap_verify" "./scripts/verify_bootstrap.sh"

# ═══════════════════════════════════════════════════════════════════
# LAYER 7: Integration
# ═══════════════════════════════════════════════════════════════════
echo ""
echo "▶ LAYER 7: Integration Tests"

run_test_layer "e2e_belief_flow" "./sxc tests/integration/e2e_belief_flow.sx && ./e2e_belief_flow"
run_test_layer "e2e_persistence" "./sxc tests/integration/e2e_persistence.sx && ./e2e_persistence"
run_test_layer "e2e_hive_sync" "./sxc tests/integration/e2e_hive_sync.sx && ./e2e_hive_sync"
run_test_layer "e2e_self_train_mock" "./sxc tests/integration/e2e_self_train.sx && ./e2e_self_train"

# ═══════════════════════════════════════════════════════════════════
# SUMMARY
# ═══════════════════════════════════════════════════════════════════
echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║                      TEST SUMMARY                             ║"
echo "╠═══════════════════════════════════════════════════════════════╣"
echo "║  PASSED:  $PASSED                                              ║"
echo "║  FAILED:  $FAILED                                              ║"
echo "║  SKIPPED: $SKIPPED                                             ║"
echo "╠═══════════════════════════════════════════════════════════════╣"

if [ $FAILED -eq 0 ]; then
    echo "║  ✓ ALL TESTS PASSED - Ready for v0.10.0 release             ║"
    echo "╚═══════════════════════════════════════════════════════════════╝"
    exit 0
else
    echo "║  ✗ SOME TESTS FAILED - Review logs in $LOG_DIR              ║"
    echo "╚═══════════════════════════════════════════════════════════════╝"
    echo ""
    echo "Failed tests:"
    grep -l "FAILED" "$LOG_DIR"/*.log 2>/dev/null || true
    exit 1
fi
```

#### Issue Tracking and Resolution

When tests fail, track issues systematically:

```simplex
// scripts/track_test_failures.sx

struct TestFailure {
    test_name: String,
    layer: i32,
    error_message: String,
    file_location: String,
    line_number: i32,
    timestamp: Instant,
    status: FailureStatus,
}

enum FailureStatus {
    New,
    Investigating,
    RootCauseFound,
    FixInProgress,
    FixPending,
    Resolved,
}

/// Parse test output and extract failures
fn parse_test_failures(log_path: &str) -> Vec<TestFailure> {
    // Implementation
}

/// Generate failure report
fn generate_failure_report(failures: &[TestFailure]) -> String {
    let mut report = String::new();

    report.push_str("# Test Failure Report\n\n");
    report.push_str(&format!("Generated: {}\n\n", now()));

    // Group by layer
    for layer in 1..=7 {
        let layer_failures: Vec<_> = failures.iter()
            .filter(|f| f.layer == layer)
            .collect();

        if !layer_failures.is_empty() {
            report.push_str(&format!("## Layer {}\n\n", layer));
            for failure in layer_failures {
                report.push_str(&format!("### {}\n", failure.test_name));
                report.push_str(&format!("- Location: {}:{}\n", failure.file_location, failure.line_number));
                report.push_str(&format!("- Error: {}\n", failure.error_message));
                report.push_str(&format!("- Status: {:?}\n\n", failure.status));
            }
        }
    }

    report
}
```

#### Common Issue Categories

| Category | Example | Resolution Approach |
|----------|---------|---------------------|
| **Missing dependencies** | "Module not found" | Check import paths, ensure build order |
| **Type mismatches** | "Expected X, got Y" | Review type definitions, check generics |
| **Runtime panics** | "Index out of bounds" | Add bounds checking, review test data |
| **Assertion failures** | "Expected 5, got 3" | Debug test logic, check implementation |
| **Timeout** | "Test exceeded 30s" | Optimize or increase timeout |
| **Resource leaks** | "File handle not closed" | Add cleanup, use RAII patterns |
| **Concurrency issues** | "Data race detected" | Add synchronization, review actor patterns |

#### Success Criteria

| Criterion | Requirement |
|-----------|-------------|
| **All layers pass** | 100% of test layers green |
| **No regressions** | No tests that previously passed now fail |
| **Coverage** | >80% code coverage across all modules |
| **Performance** | No test takes >60s (except integration) |
| **Clean output** | No warnings, no deprecation notices |
| **Deterministic** | Same results on repeated runs |

---

### Part 5 Success Criteria Summary

| Task | Success Metric |
|------|----------------|
| **Task 1: Cleanup** | Zero TASK/Phase references in code comments |
| **Task 2: API Docs** | 100% public API documented in `simplex-docs/api/` |
| **Task 3: Tests** | All 7 layers pass, >80% coverage |

---

### Part 5 Timeline

| Week | Focus | Deliverables |
|------|-------|--------------|
| **Week 1** | Task 1: Cleanup | Scripts written, automated cleanup run, manual review |
| **Week 2** | Task 2: API Docs | sxdoc enhanced, all modules documented |
| **Week 3** | Task 3: Testing | Layers 1-4 passing, issues tracked |
| **Week 4** | Task 3: Testing | Layers 5-7 passing, all issues resolved |
| **Week 5** | Final validation | Full suite green, release candidate ready |

---

### After Part 5: Release Checklist

```markdown
## v0.10.0 Release Checklist

### Code Quality
- [ ] Zero TASK/Phase references in comments
- [ ] All public APIs documented
- [ ] No compiler warnings
- [ ] No deprecated functions used

### Testing
- [ ] All 7 test layers pass
- [ ] >80% code coverage
- [ ] Integration tests pass
- [ ] Performance benchmarks acceptable

### Documentation
- [ ] API docs complete in simplex-docs/api/
- [ ] Tutorials updated (no phase references)
- [ ] Getting started guide current
- [ ] Changelog complete

### Build Artifacts
- [ ] sxc compiler binary
- [ ] sxdoc binary
- [ ] sxpm binary
- [ ] stdlib compiled
- [ ] Runtime library compiled

### Infrastructure
- [ ] CI/CD pipeline green
- [ ] Release notes drafted
- [ ] Version bumped to 0.10.0
- [ ] Git tag created
```

---

*"A language is finished when there's nothing left to remove from the documentation about how it was built."*

---

## Part 6: Production Readiness & Edge Hive Integration

> **"A model is not done when it trains. It's done when it runs on every device in the hive."**

This section covers the critical path from trained model to production deployment, with specific focus on **TASK-009 Edge Hive integration** - the primary deployment target.

---

### Integration with TASK-009: Edge Hive

TASK-009 defines the Edge Hive as a lightweight, autonomous cognitive hive running on user devices. **simplex-core-1b is the primary model powering Edge Hive**.

#### Model → Edge Hive Mapping

| simplex-core Model | Edge Hive Component | Purpose |
|-------------------|---------------------|---------|
| **simplex-core-1b** | `ContextAware` specialist | Personal pattern learning |
| **simplex-core-1b** | `OfflineAgent` specialist | Network-independent operation |
| **simplex-core-embed** | Memory recall routing | Fast semantic search |
| **simplex-core-7b** | Cloud federation fallback | Complex reasoning (via Nexus) |

#### Edge Hive Memory Architecture

```simplex
// simplex-edge-hive/src/memory_integration.sx

use simplex_core::{BeliefStore, EpisodicStore, SemanticStore};
use simplex_core::models::{SimplexCore1B, SimplexCoreEmbed};

/// Edge Hive memory system powered by simplex-core
pub struct EdgeHiveMemory {
    /// Local belief store (native format)
    beliefs: BeliefStore,

    /// Episodic memory for personal experiences
    episodic: EpisodicStore,

    /// Semantic memory for facts
    semantic: SemanticStore,

    /// Model for belief formation/revision
    model: SimplexCore1B,

    /// Embedding model for recall routing
    embedder: SimplexCoreEmbed,

    /// Federation bridge for cloud sync
    federation: FederationBridge,
}

impl EdgeHiveMemory {
    /// Load from native persistence (NOT JSON, NOT SQLite)
    pub fn load(path: &str) -> Result<Self, PersistError> {
        // Uses SXAN binary format from Part 2
        let persistence = AnimaPersistence::load(path)?;

        Ok(Self {
            beliefs: persistence.beliefs,
            episodic: persistence.episodic,
            semantic: persistence.semantic,
            model: SimplexCore1B::load_gguf("simplex-core-1b-q4_k_m.gguf")?,
            embedder: SimplexCoreEmbed::load_gguf("simplex-core-embed-q8_0.gguf")?,
            federation: FederationBridge::new(),
        })
    }

    /// Form belief from user interaction (uses simplex-core-1b)
    pub fn form_belief(&mut self, observation: &str) -> GroundedBelief<String> {
        // Model generates belief with proper epistemic metadata
        let belief = self.model.form_belief(observation, &self.beliefs);

        // Store in native format
        self.beliefs.store(belief.clone());

        // Sync to cloud if connected (via Nexus/TASK-012)
        if self.federation.is_connected() {
            self.federation.sync_belief(&belief);
        }

        belief
    }

    /// Recall from memory (uses simplex-core-embed for routing)
    pub fn recall(&mut self, query: &str) -> Vec<RecallResult> {
        // Embed query
        let embedding = self.embedder.embed(query);

        // Route to appropriate memory store
        let episodic_results = self.episodic.recall_by_embedding(&embedding);
        let semantic_results = self.semantic.recall_by_embedding(&embedding);
        let belief_results = self.beliefs.recall_by_embedding(&embedding);

        // Merge and rank (model-assisted)
        self.model.rank_recall_results(episodic_results, semantic_results, belief_results)
    }
}
```

---

### Resource Requirements for Edge Deployment

#### Model Memory Footprints

| Model | Format | Size | RAM (Inference) | Recommended Device |
|-------|--------|------|-----------------|-------------------|
| **simplex-core-7b** | GGUF F16 | ~14GB | ~16GB | Server/Desktop |
| **simplex-core-7b** | GGUF Q4_K_M | ~4GB | ~6GB | Laptop/High-end phone |
| **simplex-core-1b** | GGUF F16 | ~3GB | ~4GB | Laptop |
| **simplex-core-1b** | GGUF Q4_K_M | ~0.8GB | ~1.2GB | Phone/Tablet |
| **simplex-core-1b** | GGUF Q4_0 | ~0.6GB | ~1GB | Watch/IoT |
| **simplex-core-embed** | GGUF Q8_0 | ~110MB | ~200MB | Any device |

#### Device Tier Mapping

```
┌─────────────────────────────────────────────────────────────────────┐
│                    DEVICE CAPABILITY TIERS                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Tier 1: Full Edge Hive (Laptop/Desktop/High-end Phone)             │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ Models: simplex-core-1b (Q4_K_M) + simplex-core-embed        │   │
│  │ Memory: BeliefStore + EpisodicStore + SemanticStore         │   │
│  │ RAM: 2-4GB available                                         │   │
│  │ Storage: 500MB-2GB                                           │   │
│  │ Capabilities: Full offline, local learning, federation       │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
│  Tier 2: Lite Edge Hive (Phone/Tablet)                              │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ Models: simplex-core-1b (Q4_0) + simplex-core-embed          │   │
│  │ Memory: BeliefStore (limited) + EpisodicStore               │   │
│  │ RAM: 1-2GB available                                         │   │
│  │ Storage: 200MB-500MB                                         │   │
│  │ Capabilities: Core offline, basic learning, federation       │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
│  Tier 3: Micro Edge Hive (Watch/IoT/Wearable)                       │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ Models: simplex-core-embed only (routing to phone/cloud)     │   │
│  │ Memory: Minimal BeliefStore (cache only)                     │   │
│  │ RAM: 256MB-512MB available                                   │   │
│  │ Storage: 50-100MB                                            │   │
│  │ Capabilities: Query routing, context sensing, delegation     │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

### Deployment Pipeline

#### From Training to Edge Device

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│   Training   │────▶│   Export     │────▶│   Registry   │
│   (AWS)      │     │   (GGUF)     │     │   (S3/CDN)   │
└──────────────┘     └──────────────┘     └──────────────┘
                                                 │
       ┌─────────────────────────────────────────┼─────────────────┐
       │                                         │                 │
       ▼                                         ▼                 ▼
┌──────────────┐                         ┌──────────────┐  ┌──────────────┐
│ Edge Device  │                         │ Edge Device  │  │ Edge Device  │
│ (Download)   │                         │ (Download)   │  │ (Download)   │
└──────────────┘                         └──────────────┘  └──────────────┘
       │                                         │                 │
       ▼                                         ▼                 ▼
┌──────────────┐                         ┌──────────────┐  ┌──────────────┐
│ Local Load   │                         │ Local Load   │  │ Local Load   │
│ (GGUF→RAM)   │                         │ (GGUF→RAM)   │  │ (Embed only) │
└──────────────┘                         └──────────────┘  └──────────────┘
       │                                         │                 │
       ▼                                         ▼                 ▼
┌──────────────┐                         ┌──────────────┐  ┌──────────────┐
│ Edge Hive    │◀─────────sync──────────▶│ Edge Hive    │  │ Micro Hive   │
│ Running      │                         │ Running      │  │ (Routing)    │
└──────────────┘                         └──────────────┘  └──────────────┘
```

#### Deployment Script

```bash
#!/bin/bash
# scripts/deploy_to_edge.sh

MODEL_VERSION="0.10.0"
S3_BUCKET="s3://simplex-releases/models"

# Build model registry manifest
cat > manifest.json << EOF
{
  "version": "${MODEL_VERSION}",
  "models": {
    "simplex-core-7b": {
      "variants": {
        "f16": "${S3_BUCKET}/simplex-core-7b-f16.gguf",
        "q8_0": "${S3_BUCKET}/simplex-core-7b-q8_0.gguf",
        "q4_k_m": "${S3_BUCKET}/simplex-core-7b-q4_k_m.gguf"
      },
      "checksum_sha256": "..."
    },
    "simplex-core-1b": {
      "variants": {
        "f16": "${S3_BUCKET}/simplex-core-1b-f16.gguf",
        "q8_0": "${S3_BUCKET}/simplex-core-1b-q8_0.gguf",
        "q4_k_m": "${S3_BUCKET}/simplex-core-1b-q4_k_m.gguf",
        "q4_0": "${S3_BUCKET}/simplex-core-1b-q4_0.gguf"
      },
      "checksum_sha256": "..."
    },
    "simplex-core-embed": {
      "variants": {
        "q8_0": "${S3_BUCKET}/simplex-core-embed-q8_0.gguf"
      },
      "checksum_sha256": "..."
    }
  },
  "minimum_requirements": {
    "tier1": {"ram_mb": 2048, "storage_mb": 2000},
    "tier2": {"ram_mb": 1024, "storage_mb": 500},
    "tier3": {"ram_mb": 256, "storage_mb": 100}
  }
}
EOF

# Upload to S3 with CloudFront distribution
aws s3 cp manifest.json ${S3_BUCKET}/manifest.json
aws cloudfront create-invalidation --distribution-id $CF_DIST_ID --paths "/manifest.json"
```

---

### Nexus Federation Integration (TASK-012)

When Edge Hive needs to sync beliefs with cloud or other devices, it uses the Nexus protocol:

```simplex
// simplex-edge-hive/src/federation_sync.sx

use nexus::{Connection, Frame, DeltaStream};
use simplex_core::belief::{GroundedBelief, BeliefDelta};

/// Sync beliefs via Nexus protocol
pub struct BeliefFederationSync {
    conn: Connection,
    local_version: u64,
    pending_deltas: Vec<BeliefDelta>,
}

impl BeliefFederationSync {
    /// Sync local beliefs to cloud hive
    pub async fn sync_to_cloud(&mut self, beliefs: &BeliefStore) -> Result<SyncResult, SyncError> {
        // Get deltas since last sync
        let deltas = beliefs.deltas_since(self.local_version);

        // Pack using Nexus bit-packed delta streams (400x compression)
        let packed = DeltaStream::pack_beliefs(&deltas);

        // Send via Nexus
        self.conn.send_frame(Frame::BeliefSync {
            from_version: self.local_version,
            deltas: packed,
        }).await?;

        // Wait for ACK with cloud version
        let ack = self.conn.recv_frame().await?;
        self.local_version = ack.new_version;

        Ok(SyncResult {
            synced_count: deltas.len(),
            new_version: self.local_version,
        })
    }

    /// Receive belief updates from cloud/other devices
    pub async fn receive_updates(&mut self, beliefs: &mut BeliefStore) -> Result<usize, SyncError> {
        let frame = self.conn.recv_frame().await?;

        if let Frame::BeliefSync { from_version, deltas } = frame {
            // Unpack deltas
            let unpacked = DeltaStream::unpack_beliefs(&deltas);

            // Apply with conflict resolution
            for delta in unpacked {
                match delta {
                    BeliefDelta::New(belief) => {
                        beliefs.merge_remote(belief);
                    }
                    BeliefDelta::Revision { old_hash, new_belief, evidence } => {
                        beliefs.revise_from_remote(old_hash, new_belief, evidence);
                    }
                    BeliefDelta::Retraction { hash, reason } => {
                        beliefs.retract_remote(hash, reason);
                    }
                }
            }

            self.local_version = from_version + unpacked.len() as u64;
            Ok(unpacked.len())
        } else {
            Err(SyncError::UnexpectedFrame)
        }
    }
}
```

---

### CI/CD Pipeline Integration

```yaml
# .github/workflows/simplex-core-training.yml

name: simplex-core Training Pipeline

on:
  push:
    tags:
      - 'train-v*'
  workflow_dispatch:
    inputs:
      model_variant:
        description: 'Model to train'
        required: true
        default: 'simplex-core-7b'
        type: choice
        options:
          - simplex-core-7b
          - simplex-core-1b
          - simplex-core-embed
          - all

jobs:
  data-generation:
    runs-on: [self-hosted, aws, c6i-8xlarge]
    steps:
      - uses: actions/checkout@v4
      - name: Generate training data
        run: |
          simplex-train generate \
            --type all \
            --output s3://simplex-training/datasets/
      - name: Validate data
        run: |
          simplex-train validate \
            --dataset s3://simplex-training/datasets/

  training:
    needs: data-generation
    runs-on: [self-hosted, aws, p4d-24xlarge]
    strategy:
      matrix:
        stage: [pretrain, calibration, annealing]
    steps:
      - uses: actions/checkout@v4
      - name: Run training stage
        run: |
          simplex-train train \
            --config configs/${{ inputs.model_variant }}.toml \
            --stage ${{ matrix.stage }}

  evaluation:
    needs: training
    runs-on: [self-hosted, aws, g5-12xlarge]
    steps:
      - name: Run evaluation
        run: |
          simplex-train evaluate \
            --model s3://simplex-training/checkpoints/${{ inputs.model_variant }}/final/
      - name: Upload results
        uses: actions/upload-artifact@v4
        with:
          name: eval-results
          path: evaluation_results/

  export:
    needs: evaluation
    runs-on: [self-hosted, aws, g5-12xlarge]
    steps:
      - name: Export GGUF
        run: |
          simplex-train export \
            --format gguf \
            --quantizations "f16,q8_0,q4_k_m,q4_0"
      - name: Upload to release bucket
        run: |
          aws s3 sync artifacts/gguf/ s3://simplex-releases/models/

  deploy:
    needs: export
    runs-on: ubuntu-latest
    steps:
      - name: Update model registry
        run: |
          ./scripts/update_model_registry.sh
      - name: Invalidate CDN cache
        run: |
          aws cloudfront create-invalidation \
            --distribution-id ${{ secrets.CF_DIST_ID }} \
            --paths "/models/*"
```

---

### Monitoring and Observability

#### Training Metrics (TensorBoard/Weights & Biases)

```simplex
// simplex-train/src/monitoring.sx

/// Metrics to track during training
pub struct TrainingMetrics {
    // Loss metrics
    pub total_loss: f64,
    pub belief_formation_loss: f64,
    pub revision_loss: f64,
    pub calibration_loss: f64,

    // Epistemic health
    pub ece: f64,                      // Expected Calibration Error
    pub source_agreement: f64,          // From EpistemicMonitors
    pub confidence_velocity: f64,       // Are we getting overconfident?
    pub exploration_ratio: f64,         // How much exploration vs exploitation?

    // Self-annealing
    pub temperature: f64,
    pub learning_rate: f64,
    pub in_dissent_window: bool,
    pub skeptic_challenges: u32,

    // Contamination
    pub sql_pattern_detections: u32,
    pub vector_pattern_detections: u32,
}

impl TrainingMetrics {
    /// Log to TensorBoard
    pub fn log_tensorboard(&self, writer: &mut TensorBoardWriter, step: u64) {
        writer.add_scalar("loss/total", self.total_loss, step);
        writer.add_scalar("loss/belief_formation", self.belief_formation_loss, step);
        writer.add_scalar("loss/revision", self.revision_loss, step);
        writer.add_scalar("loss/calibration", self.calibration_loss, step);

        writer.add_scalar("epistemic/ece", self.ece, step);
        writer.add_scalar("epistemic/source_agreement", self.source_agreement, step);
        writer.add_scalar("epistemic/confidence_velocity", self.confidence_velocity, step);
        writer.add_scalar("epistemic/exploration_ratio", self.exploration_ratio, step);

        writer.add_scalar("annealing/temperature", self.temperature, step);
        writer.add_scalar("annealing/learning_rate", self.learning_rate, step);
        writer.add_scalar("annealing/in_dissent", self.in_dissent_window as f64, step);
        writer.add_scalar("annealing/skeptic_challenges", self.skeptic_challenges as f64, step);

        writer.add_scalar("contamination/sql", self.sql_pattern_detections as f64, step);
        writer.add_scalar("contamination/vector", self.vector_pattern_detections as f64, step);
    }
}
```

#### Production Metrics (Edge Hive Runtime)

```simplex
// simplex-edge-hive/src/metrics.sx

/// Runtime metrics for deployed Edge Hive
pub struct EdgeHiveMetrics {
    // Inference performance
    pub inference_latency_ms: Histogram,
    pub tokens_per_second: f64,
    pub memory_usage_mb: f64,

    // Belief system health
    pub belief_count: u64,
    pub episodic_count: u64,
    pub semantic_count: u64,
    pub average_confidence: f64,
    pub beliefs_formed_today: u64,
    pub beliefs_revised_today: u64,
    pub beliefs_forgotten_today: u64,

    // Federation
    pub sync_latency_ms: Histogram,
    pub pending_sync_count: u64,
    pub last_sync_timestamp: Instant,

    // Calibration (ongoing)
    pub runtime_ece: f64,
    pub prediction_accuracy: f64,
}

impl EdgeHiveMetrics {
    /// Export to Prometheus format
    pub fn to_prometheus(&self) -> String {
        format!(r#"
# HELP edge_hive_inference_latency_ms Model inference latency
# TYPE edge_hive_inference_latency_ms histogram
edge_hive_inference_latency_ms_bucket{{le="10"}} {}
edge_hive_inference_latency_ms_bucket{{le="50"}} {}
edge_hive_inference_latency_ms_bucket{{le="100"}} {}
edge_hive_inference_latency_ms_bucket{{le="500"}} {}

# HELP edge_hive_belief_count Total beliefs in store
# TYPE edge_hive_belief_count gauge
edge_hive_belief_count {}

# HELP edge_hive_runtime_ece Runtime calibration error
# TYPE edge_hive_runtime_ece gauge
edge_hive_runtime_ece {}
"#,
            self.inference_latency_ms.bucket(10),
            self.inference_latency_ms.bucket(50),
            self.inference_latency_ms.bucket(100),
            self.inference_latency_ms.bucket(500),
            self.belief_count,
            self.runtime_ece,
        )
    }
}
```

---

### Incremental Learning & Model Updates

#### Over-the-Air Model Updates

```simplex
// simplex-edge-hive/src/model_updater.sx

/// Handles model updates without full redownload
pub struct ModelUpdater {
    current_version: Version,
    registry_url: String,
}

impl ModelUpdater {
    /// Check for updates
    pub async fn check_for_updates(&self) -> Option<UpdateInfo> {
        let manifest = fetch_manifest(&self.registry_url).await?;

        if manifest.version > self.current_version {
            Some(UpdateInfo {
                new_version: manifest.version,
                download_size: manifest.delta_size_from(self.current_version),
                full_size: manifest.full_size,
                changelog: manifest.changelog,
            })
        } else {
            None
        }
    }

    /// Apply delta update (smaller than full download)
    pub async fn apply_delta_update(&mut self, update: &UpdateInfo) -> Result<(), UpdateError> {
        // Download delta patch
        let delta = download_delta(
            &self.registry_url,
            self.current_version,
            update.new_version
        ).await?;

        // Verify checksum
        if !delta.verify_checksum() {
            return Err(UpdateError::ChecksumMismatch);
        }

        // Apply patch to local model
        let current_model = load_current_model()?;
        let new_model = apply_gguf_delta(&current_model, &delta)?;

        // Atomic swap
        atomic_replace_model(&new_model)?;

        self.current_version = update.new_version;
        Ok(())
    }
}
```

#### Continuous Calibration (Runtime Learning)

```simplex
// simplex-edge-hive/src/runtime_calibration.sx

/// Continuous calibration from runtime feedback
pub struct RuntimeCalibrator {
    /// Recent predictions with outcomes
    prediction_history: CircularBuffer<PredictionOutcome>,

    /// Current calibration adjustment
    calibration_offset: f64,

    /// Last recalibration timestamp
    last_calibration: Instant,
}

impl RuntimeCalibrator {
    /// Record prediction outcome for calibration
    pub fn record_outcome(&mut self, predicted_confidence: f64, was_correct: bool) {
        self.prediction_history.push(PredictionOutcome {
            confidence: predicted_confidence,
            correct: was_correct,
            timestamp: now(),
        });

        // Recalibrate periodically
        if self.prediction_history.len() >= 100 &&
           now() - self.last_calibration > Duration::from_hours(1) {
            self.recalibrate();
        }
    }

    /// Adjust model confidence based on runtime accuracy
    fn recalibrate(&mut self) {
        let ece = self.compute_runtime_ece();

        if ece > 0.10 {
            // Significant miscalibration - apply temperature scaling
            let optimal_temp = self.find_optimal_temperature();
            self.calibration_offset = 1.0 / optimal_temp;

            log::info!("Runtime recalibration: ECE={:.3}, new_offset={:.3}",
                      ece, self.calibration_offset);
        }

        self.last_calibration = now();
    }

    /// Apply calibration to model output
    pub fn calibrate(&self, raw_confidence: f64) -> f64 {
        (raw_confidence * self.calibration_offset).clamp(0.0, 1.0)
    }
}
```

---

### Fallback and Degradation Patterns

#### Graceful Degradation on Resource Constraints

```simplex
// simplex-edge-hive/src/degradation.sx

/// Degradation levels for resource-constrained operation
pub enum DegradationLevel {
    /// Full operation - all models loaded
    Full,

    /// Reduced - only core model, no embed
    Reduced,

    /// Minimal - embed only, delegate to cloud
    Minimal,

    /// Offline cache only - no inference
    CacheOnly,
}

/// Manages graceful degradation based on available resources
pub struct DegradationManager {
    current_level: DegradationLevel,
    memory_threshold_full: usize,      // e.g., 2GB
    memory_threshold_reduced: usize,   // e.g., 1GB
    memory_threshold_minimal: usize,   // e.g., 256MB
}

impl DegradationManager {
    /// Check resources and adjust degradation level
    pub fn check_and_adjust(&mut self) -> DegradationLevel {
        let available_memory = get_available_memory();
        let battery_level = get_battery_level();

        let new_level = match (available_memory, battery_level) {
            (mem, _) if mem >= self.memory_threshold_full => DegradationLevel::Full,
            (mem, bat) if mem >= self.memory_threshold_reduced && bat > 20 => DegradationLevel::Reduced,
            (mem, _) if mem >= self.memory_threshold_minimal => DegradationLevel::Minimal,
            _ => DegradationLevel::CacheOnly,
        };

        if new_level != self.current_level {
            log::info!("Degradation level changed: {:?} -> {:?}",
                      self.current_level, new_level);
            self.apply_degradation(new_level);
        }

        self.current_level = new_level;
        new_level
    }

    /// Apply degradation - unload models as needed
    fn apply_degradation(&mut self, level: DegradationLevel) {
        match level {
            DegradationLevel::Full => {
                // Load all models
                load_simplex_core_1b();
                load_simplex_core_embed();
            }
            DegradationLevel::Reduced => {
                // Unload embed, keep core
                unload_simplex_core_embed();
            }
            DegradationLevel::Minimal => {
                // Unload core, keep embed for routing
                unload_simplex_core_1b();
                load_simplex_core_embed();
            }
            DegradationLevel::CacheOnly => {
                // Unload all, use cached beliefs only
                unload_all_models();
            }
        }
    }
}
```

---

### Privacy Considerations

#### Training Data Privacy

```simplex
// simplex-train/src/privacy.sx

/// Privacy-preserving training data handling
pub struct PrivacyConfig {
    /// Anonymize personally identifiable information
    pub anonymize_pii: bool,

    /// Hash user identifiers
    pub hash_user_ids: bool,

    /// Differential privacy epsilon (if enabled)
    pub dp_epsilon: Option<f64>,

    /// Federated learning (train on device, aggregate gradients)
    pub federated: bool,
}

impl PrivacyConfig {
    pub fn strict() -> Self {
        Self {
            anonymize_pii: true,
            hash_user_ids: true,
            dp_epsilon: Some(1.0),  // Strong privacy
            federated: true,
        }
    }
}

/// Scrub sensitive data from training examples
pub fn scrub_training_example(example: &mut TrainingExample, config: &PrivacyConfig) {
    if config.anonymize_pii {
        example.content = anonymize_pii(&example.content);
    }

    if config.hash_user_ids {
        if let Some(user_id) = &example.user_id {
            example.user_id = Some(hash_identifier(user_id));
        }
    }
}
```

#### Edge Hive Data Isolation

```simplex
// simplex-edge-hive/src/privacy.sx

/// Per-user data isolation on shared devices
pub struct UserIsolation {
    /// Current user context
    current_user: UserId,

    /// User-specific encryption keys
    user_keys: HashMap<UserId, EncryptionKey>,
}

impl UserIsolation {
    /// All persistence is user-scoped and encrypted
    pub fn get_user_store(&self, user: &UserId) -> Result<BeliefStore, IsolationError> {
        let key = self.user_keys.get(user)
            .ok_or(IsolationError::UnknownUser)?;

        let path = format!("~/.simplex/users/{}/beliefs.sxan", hash_user_id(user));

        // Load with decryption
        BeliefStore::load_encrypted(&path, key)
    }

    /// Beliefs never leak between users
    pub fn verify_isolation(&self, belief: &GroundedBelief) -> bool {
        belief.scope.user_id == self.current_user
    }
}
```

---

### Version & Release Artifacts

#### v0.10.0 Release Contents

```
simplex-0.10.0/
├── bin/
│   ├── sxc                          # Simplex compiler
│   ├── sxdoc                        # Documentation generator
│   ├── sxpm                         # Package manager
│   └── simplex-train                # Training CLI
├── lib/
│   ├── libsimplex_runtime.so        # Core runtime
│   ├── libsimplex_core.so           # Native persistence
│   └── libsimplex_learning.so       # Learning framework
├── models/
│   ├── simplex-core-7b-q4_k_m.gguf  # Main reasoning model
│   ├── simplex-core-1b-q4_k_m.gguf  # Edge model
│   └── simplex-core-embed-q8_0.gguf # Embedding model
├── include/
│   └── simplex.h                    # C FFI header
├── docs/
│   └── api/                         # Generated API docs
├── examples/
│   ├── edge_hive/                   # Edge Hive examples
│   ├── beliefs/                     # Belief system examples
│   └── training/                    # Training examples
└── checksums.sha256
```

#### Package Naming Convention

| Package | Description | Version |
|---------|-------------|---------|
| `simplex-core` | Compiler, runtime, stdlib | 0.10.0 |
| `simplex-learning` | Learning framework | 0.10.0 |
| `simplex-training` | Training pipeline | 0.10.0 |
| `simplex-core-models` | Pre-trained GGUF models | 0.10.0 |
| `edge-hive` | Edge Hive runtime | 0.10.0 |
| `nexus` | Federation protocol | 0.10.0 |

---

### Part 6 Success Criteria

| Criterion | Measurement |
|-----------|-------------|
| **Edge Hive boots** | simplex-core-1b loads on tier 1 device in <5s |
| **Memory budget met** | Full edge hive runs in <2GB RAM |
| **Federation works** | Beliefs sync via Nexus in <1s latency |
| **OTA updates work** | Delta update applies without service interruption |
| **Degradation graceful** | Tier 3 device operates in Minimal mode |
| **Privacy enforced** | No PII in training data, user isolation verified |
| **CI/CD green** | Full pipeline runs without manual intervention |
| **Monitoring active** | TensorBoard shows all training metrics |

---

### Complete Task Map

```
TASK-015 DEPENDENCY AND OUTPUT MAP
══════════════════════════════════

INPUTS (Dependencies):
┌─────────────────────────────────────────────────────────────────────┐
│ TASK-002: Cognitive Models        → Model architecture             │
│ TASK-005: Dual Numbers            → Automatic differentiation      │
│ TASK-006: Self-Learning           → Epistemic schedules            │
│ TASK-007: Training Pipeline       → Base training infrastructure   │
│ TASK-009: Edge Hive              → Deployment target (in progress) │
│ TASK-012: Nexus Protocol          → Federation for sync            │
│ TASK-014: Belief Epistemics       → GroundedBelief, safety zones   │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          TASK-015                                   │
│                                                                      │
│  Part 1: Model Definition                                           │
│  Part 2: Toolchain Refactoring                                      │
│  Part 3: AWS Infrastructure                                         │
│  Part 4: Self-Refining Architecture                                 │
│  Part 5: Final Consolidation                                        │
│  Part 6: Production Readiness                                       │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
OUTPUTS (Deliverables):
┌─────────────────────────────────────────────────────────────────────┐
│ simplex-core-7b.gguf              → Primary reasoning model         │
│ simplex-core-1b.gguf              → Edge Hive model                 │
│ simplex-core-embed.gguf           → Embedding/routing model         │
│ simplex-core/                 → Native persistence library      │
│ adapters/simplex-sql/             → SQL interop (not core)          │
│ simplex-docs/api/                 → Complete API documentation      │
│ Clean codebase                    → No TASK/Phase references        │
│ All tests passing                 → 7-layer validation complete     │
│ v0.10.0 release                   → The Complete Simplex Vision     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Part 7: Unified Specialist Architecture - Full Alignment

> **"No adapters. No workarounds. No technical debt. Everything native Simplex from the ground up."**

This section defines the complete replacement of the legacy LoRA specialist approach with fully-aligned belief-native specialists. All specialists output `GroundedBelief<T>` directly, coordinate through the shared `BeliefStore`, and participate in the Anima/Mnemonic/Divine hierarchy.

---

### The Fundamental Shift

**Legacy Approach (Deprecated):**
```
Base Model (Qwen/Llama)
    → LoRA Adapter (trained on standard datasets)
    → Raw output (JSON/SQL/text)
    → Adapter layer (post-hoc wrapping)
    → BeliefStore
```

**Simplex-Native Approach (Required):**
```
simplex-core-7b/1b (native belief reasoning)
    → Specialist (trained on belief-format data)
    → GroundedBelief output (native)
    → BeliefStore (direct integration)
```

**No adapters. No post-processing. No impedance mismatch.**

---

### Specialist Model Hierarchy

All specialists are trained on top of simplex-core models, outputting native beliefs:

| Specialist | Base Model | Output Type | Tier |
|------------|------------|-------------|------|
| **document_extraction** | simplex-core-1b | `GroundedBelief<DocumentFact>` | Edge |
| **invoice_processing** | simplex-core-1b | `GroundedBelief<InvoiceFact>` | Edge |
| **contract_analysis** | simplex-core-7b | `GroundedBelief<LegalFact>` | Hive |
| **entity_extraction** | simplex-core-1b | `GroundedBelief<EntityFact>` | Edge |
| **code_generation** | simplex-core-7b | `GroundedBelief<CodeFact>` | Hive |
| **simplex_specialist** | simplex-core-7b | `GroundedBelief<SimplexFact>` | Hive |
| **code_review** | simplex-core-7b | `GroundedBelief<ReviewFact>` | Hive |
| **technical_writing** | simplex-core-7b | `GroundedBelief<DocumentFact>` | Hive |
| **news_summarization** | simplex-core-1b | `GroundedBelief<SummaryFact>` | Edge |
| **sentiment_analysis** | simplex-core-1b | `GroundedBelief<SentimentFact>` | Edge |
| **math_reasoning** | simplex-core-7b | `GroundedBelief<ReasoningFact>` | Hive |
| **financial_analysis** | simplex-core-7b | `GroundedBelief<FinancialFact>` | Hive |
| **legal_analysis** | simplex-core-7b | `GroundedBelief<LegalFact>` | Hive |

**Language-Specific Code Generation Specialists:**
| Specialist | Output Format | Purpose |
|------------|---------------|---------|
| **sql_generation** | `GroundedBelief<SQLFact>` | Helps users build SQL-backed applications |
| **python_generation** | `GroundedBelief<PythonFact>` | Python application development |
| **javascript_generation** | `GroundedBelief<JSFact>` | JavaScript/TypeScript web development |
| **java_generation** | `GroundedBelief<JavaFact>` | Enterprise Java applications |
| **go_generation** | `GroundedBelief<GoFact>` | Cloud-native Go development |
| **rust_generation** | `GroundedBelief<RustFact>` | Systems programming in Rust |
| **cpp_generation** | `GroundedBelief<CppFact>` | C/C++ systems and embedded |
| **migration_specialist** | `GroundedBelief<MigrationFact>` | Legacy system conversion to Simplex |

**Critical Distinction - Internal vs External:**
```
┌─────────────────────────────────────────────────────────────────────┐
│                 INTERNAL VS EXTERNAL DISTINCTION                     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  INTERNAL (Simplex's own systems):                                  │
│  ├── Persistence → BeliefStore (content-addressed, native)          │
│  ├── Memory → EpisodicStore, SemanticStore                          │
│  ├── Queries → Belief recall, not SQL                               │
│  └── Format → GroundedBelief<T> throughout                          │
│                                                                      │
│  EXTERNAL (What users BUILD with Simplex):                          │
│  ├── SQL databases ✓ (sql_generation specialist)                    │
│  ├── Python apps ✓ (python_generation specialist)                   │
│  ├── Java services ✓ (java_generation specialist)                   │
│  ├── Legacy systems ✓ (migration_specialist)                        │
│  └── ANY technology the user needs                                  │
│                                                                      │
│  Principle: "Simplex thinks in beliefs. What users BUILD is their   │
│              choice."                                                │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

**Deprecated Training FORMATS (not capabilities):**
- Legacy LoRA specialists trained on raw JSON outputs
- Legacy LoRA specialists with raw text outputs (no belief wrapper)

**NOT Deprecated:**
- SQL generation capability (now outputs `GroundedBelief<SQLFact>`)
- Any language generation capability (all output beliefs with provenance)

---

### Specialist Training Data Format

Every specialist training example must output beliefs, not raw data.

#### Example: Document Extraction Specialist

**Legacy Training Format (Deprecated):**
```json
{
  "input": "[Image: Invoice from Acme Corp]",
  "output": {"vendor": "Acme Corp", "total": 1234.56, "date": "2026-01-15"}
}
```

**Simplex-Native Training Format (Required):**
```xml
<training_example type="document_extraction_belief">
<context>
You are a document extraction specialist. You observe documents and form beliefs
about their contents with appropriate confidence and provenance.
</context>

<input>
[Image: Invoice from Acme Corp, showing total $1,234.56, dated January 15, 2026]
OCR confidence regions: vendor_name=0.96, total_amount=0.94, date_field=0.91
</input>

<expected_beliefs>
<belief id="vendor_belief">
claim: "Document vendor is Acme Corp"
confidence: 0.94
provenance:
  type: Observed
  source: document_extraction_specialist
  timestamp: {now}
  observation_method: OCR
evidence:
  - observation: "Text 'ACME CORP' at header region (12, 34, 200, 60)"
    support_strength: 0.96
  - observation: "Logo matches Acme Corp brand database"
    support_strength: 0.88
falsifiers:
  - condition: "alternative_vendor_name_detected_with_higher_confidence"
    action: revise_belief
  - condition: "manual_review_contradicts"
    action: invalidate
scope:
  valid_domains: [document_processing, accounts_payable]
  tier_threshold: 0.30  # Anima level
</belief>

<belief id="total_belief">
claim: "Invoice total is $1,234.56 USD"
confidence: 0.92
provenance:
  type: Observed
  source: document_extraction_specialist
  timestamp: {now}
  observation_method: OCR
evidence:
  - observation: "Text '1,234.56' at total region with currency symbol '$'"
    support_strength: 0.94
  - observation: "Amount format consistent with USD currency"
    support_strength: 0.90
falsifiers:
  - condition: "line_item_sum_differs_from_total"
    action: reduce_confidence(0.3)
  - condition: "manual_calculation_contradicts"
    action: invalidate
scope:
  valid_domains: [document_processing, accounts_payable, financial]
  tier_threshold: 0.30
</belief>

<belief id="date_belief">
claim: "Invoice date is January 15, 2026"
confidence: 0.89
provenance:
  type: Observed
  source: document_extraction_specialist
  timestamp: {now}
  observation_method: OCR
evidence:
  - observation: "Date text '01/15/2026' at date region"
    support_strength: 0.91
falsifiers:
  - condition: "date_format_ambiguous"  # Could be DD/MM vs MM/DD
    action: reduce_confidence(0.2)
scope:
  valid_domains: [document_processing]
  tier_threshold: 0.30
</belief>
</expected_beliefs>

<integration>
These beliefs should be stored in the local BeliefStore and linked via provenance
to the source document. If confidence exceeds 0.50, they may propagate to
HiveMnemonic for shared knowledge.
</integration>
</training_example>
```

#### Example: Code Generation Specialist

**Legacy Training Format (Deprecated):**
```json
{
  "input": "Write a function to sort a list",
  "output": "fn sort<T: Ord>(list: &mut [T]) { list.sort(); }"
}
```

**Simplex-Native Training Format (Required):**
```xml
<training_example type="code_generation_belief">
<context>
You are a code generation specialist. You form beliefs about code solutions
with confidence based on your analysis of requirements and implementation quality.
</context>

<anima_state>
recalled_beliefs:
  - claim: "User prefers idiomatic Simplex patterns"
    confidence: 0.85
    source: previous_interaction
  - claim: "Project uses generic types extensively"
    confidence: 0.78
    source: codebase_analysis
</anima_state>

<input>
User request: "Write a function to sort a list in Simplex"
</input>

<expected_beliefs>
<belief id="code_solution_belief">
claim: "A correct sort function implementation is: fn sort<T: Ord>(list: &mut Vec<T>) { list.sort_by(|a, b| a.cmp(b)); }"
confidence: 0.91
provenance:
  type: Inferred
  premises: [user_request, language_knowledge, recalled_beliefs]
  rule: code_synthesis
  confidence_derivation: "High confidence due to: standard library usage (safe), generic bounds correct, idiomatic pattern"
evidence:
  - observation: "Uses stable sort from stdlib"
    support_strength: 0.95
  - observation: "Generic bound T: Ord ensures comparable elements"
    support_strength: 0.92
  - observation: "Matches recalled user preference for idiomatic code"
    support_strength: 0.85
falsifiers:
  - condition: "compilation_fails"
    action: invalidate
  - condition: "test_case_fails"
    action: reduce_confidence(0.4)
  - condition: "user_requests_different_approach"
    action: revise_belief
scope:
  valid_domains: [code_generation, simplex_language]
  valid_contexts: [sorting, algorithms, generics]
  tier_threshold: 0.50  # Mnemonic level for shared code patterns
</belief>

<belief id="approach_reasoning_belief">
claim: "Using stdlib sort is preferred over manual implementation for reliability"
confidence: 0.88
provenance:
  type: Inferred
  premises: [best_practices_knowledge]
  rule: engineering_judgment
evidence:
  - observation: "Stdlib sort is well-tested and optimized"
    support_strength: 0.95
  - observation: "Manual sort implementations risk bugs"
    support_strength: 0.82
falsifiers:
  - condition: "specific_performance_requirement_unmet"
    action: reduce_confidence(0.3)
scope:
  valid_domains: [code_generation, engineering_practices]
  tier_threshold: 0.70  # Divine level for universal best practices
</belief>
</expected_beliefs>
</training_example>
```

#### Example: Sentiment Analysis Specialist

**Simplex-Native Training Format:**
```xml
<training_example type="sentiment_analysis_belief">
<context>
You are a sentiment analysis specialist. You form beliefs about the emotional
content and intent of text with calibrated confidence.
</context>

<input>
Text: "The product arrived late and was damaged. Customer service was unhelpful.
I won't be ordering again."
</input>

<expected_beliefs>
<belief id="sentiment_belief">
claim: "Text expresses strongly negative sentiment"
confidence: 0.93
provenance:
  type: Inferred
  source: sentiment_analysis_specialist
  rule: sentiment_classification
evidence:
  - observation: "Negative event: 'arrived late'"
    support_strength: 0.85
  - observation: "Negative event: 'was damaged'"
    support_strength: 0.90
  - observation: "Negative evaluation: 'unhelpful'"
    support_strength: 0.88
  - observation: "Negative intent: 'won't be ordering again'"
    support_strength: 0.95
falsifiers:
  - condition: "sarcasm_detected"
    action: invert_sentiment
  - condition: "context_reveals_positive_resolution"
    action: reduce_confidence(0.4)
scope:
  valid_domains: [customer_feedback, sentiment_analysis]
  tier_threshold: 0.30
</belief>

<belief id="churn_risk_belief">
claim: "Customer has high churn risk based on expressed sentiment"
confidence: 0.87
provenance:
  type: Inferred
  premises: [sentiment_belief]
  rule: churn_prediction
evidence:
  - observation: "Explicit statement of intent not to reorder"
    support_strength: 0.95
  - observation: "Multiple grievances compound risk"
    support_strength: 0.80
falsifiers:
  - condition: "customer_accepts_resolution_offer"
    action: reduce_confidence(0.5)
  - condition: "customer_places_new_order"
    action: invalidate
scope:
  valid_domains: [customer_success, churn_prediction]
  tier_threshold: 0.50
</belief>

<belief id="issue_classification_belief">
claim: "Issues identified: shipping_delay, product_damage, support_quality"
confidence: 0.91
provenance:
  type: Observed
  source: sentiment_analysis_specialist
evidence:
  - observation: "'arrived late' maps to shipping_delay"
    support_strength: 0.92
  - observation: "'was damaged' maps to product_damage"
    support_strength: 0.94
  - observation: "'unhelpful' maps to support_quality"
    support_strength: 0.88
falsifiers:
  - condition: "issue_already_resolved_in_system"
    action: mark_as_stale
scope:
  valid_domains: [customer_feedback, issue_tracking]
  tier_threshold: 0.30
</belief>
</expected_beliefs>
</training_example>
```

---

### Specialist Coordination Through BeliefStore

Specialists don't operate in isolation. They coordinate through the shared `BeliefStore`:

```simplex
// simplex-core/src/specialist_coordination.sx

use crate::belief_store::{BeliefStore, BeliefQuery};
use crate::models::{SimplexCore7B, SimplexCore1B};

/// Specialist orchestration - all specialists share beliefs
pub struct SpecialistOrchestrator {
    /// Shared belief store (content-addressed)
    beliefs: BeliefStore,

    /// Core reasoning model
    core_model: SimplexCore7B,

    /// Loaded specialists
    specialists: HashMap<SpecialistId, Box<dyn BeliefSpecialist>>,
}

impl SpecialistOrchestrator {
    /// Process a task using appropriate specialist(s)
    pub fn process(&mut self, task: &Task) -> Vec<GroundedBelief<String>> {
        // 1. Recall relevant beliefs from store
        let recalled = self.beliefs.recall_relevant(&task.context, threshold: 0.30);

        // 2. Core model decides which specialist(s) to invoke
        let routing = self.core_model.route_to_specialists(task, &recalled);

        // 3. Each specialist receives recalled beliefs as context
        let mut new_beliefs = Vec::new();
        for specialist_id in routing.specialists {
            let specialist = self.specialists.get(&specialist_id)?;

            // Specialist forms beliefs with awareness of existing knowledge
            let beliefs = specialist.form_beliefs(
                &task.input,
                &recalled,  // Pass existing beliefs as context
                &routing.specialist_context[&specialist_id],
            );

            new_beliefs.extend(beliefs);
        }

        // 4. Store all new beliefs
        for belief in &new_beliefs {
            self.beliefs.store(belief.clone());
        }

        // 5. Core model synthesizes final response from beliefs
        self.core_model.synthesize_response(&new_beliefs, &recalled)
    }
}

/// All specialists implement this trait
pub trait BeliefSpecialist {
    /// Form beliefs from input, with awareness of recalled context
    fn form_beliefs(
        &self,
        input: &str,
        recalled_beliefs: &[GroundedBelief<String>],
        context: &SpecialistContext,
    ) -> Vec<GroundedBelief<String>>;

    /// Specialist's domain of expertise
    fn domain(&self) -> Domain;

    /// Base model tier (Edge = 1b, Hive = 7b)
    fn tier(&self) -> ModelTier;

    /// Confidence calibration status
    fn calibration(&self) -> CalibrationStatus;
}
```

---

### Training Data Generation Pipeline

Use the existing `simplex-training` infrastructure to generate belief-formatted training data:

```simplex
// simplex-train/src/data/specialist_belief_generator.sx

use simplex_training::data::{DataGenerator, TrainingExample};
use simplex_learning::belief::{GroundedBelief, BeliefProvenance, CalibratedConfidence};

pub struct SpecialistBeliefGenerator {
    /// Which specialist we're generating data for
    specialist_type: SpecialistType,

    /// Domain templates for this specialist
    templates: Vec<DomainTemplate>,

    /// Provenance distribution
    provenance_dist: ProvenanceDistribution,

    /// Calibration target
    calibration_target: f64,
}

impl SpecialistBeliefGenerator {
    /// Generate one training example with belief outputs
    pub fn generate(&mut self) -> TrainingExample {
        // 1. Generate input scenario
        let scenario = self.templates.sample_scenario();

        // 2. Generate recalled beliefs (simulated context)
        let recalled = self.generate_recalled_context(&scenario);

        // 3. Generate expected belief outputs
        let beliefs = self.generate_belief_outputs(&scenario, &recalled);

        // 4. Format as training example
        TrainingExample {
            input: self.format_input(&scenario, &recalled),
            output: self.format_belief_output(&beliefs),
            metadata: TrainingMetadata {
                specialist: self.specialist_type.clone(),
                domain: scenario.domain.clone(),
                num_beliefs: beliefs.len(),
                avg_confidence: beliefs.iter().map(|b| b.confidence.value).sum::<f64>() / beliefs.len() as f64,
            },
        }
    }

    fn generate_belief_outputs(
        &self,
        scenario: &Scenario,
        recalled: &[GroundedBelief<String>],
    ) -> Vec<GroundedBelief<String>> {
        let mut beliefs = Vec::new();

        // Generate primary belief(s) for this specialist's output
        for output_item in &scenario.expected_outputs {
            let belief = GroundedBelief::new(
                &format!("{}_{}", self.specialist_type.prefix(), uuid()),
                output_item.claim.clone(),
                self.generate_provenance(output_item),
                self.generate_calibrated_confidence(output_item),
            )
            .with_evidence_list(self.generate_evidence(output_item, scenario))
            .with_falsifiers(self.generate_falsifiers(output_item))
            .in_domain(output_item.domain.clone())
            .with_tier_threshold(self.specialist_type.default_tier_threshold());

            beliefs.push(belief);
        }

        // Generate secondary/supporting beliefs if appropriate
        for inference in &scenario.expected_inferences {
            let belief = GroundedBelief::inferred(
                &format!("infer_{}", uuid()),
                inference.claim.clone(),
                beliefs.iter().map(|b| b.id.clone()).collect(), // Premises
                InferenceRule::new(&inference.rule),
                &inference.reasoning,
            )
            .with_confidence(self.generate_calibrated_confidence(inference));

            beliefs.push(belief);
        }

        beliefs
    }
}

/// Specialist types and their configurations
pub enum SpecialistType {
    DocumentExtraction,
    InvoiceProcessing,
    ContractAnalysis,
    EntityExtraction,
    CodeGeneration,
    SimplexSpecialist,
    CodeReview,
    TechnicalWriting,
    NewsSummarization,
    SentimentAnalysis,
    MathReasoning,
    FinancialAnalysis,
    LegalAnalysis,
}

impl SpecialistType {
    pub fn base_model(&self) -> ModelTier {
        match self {
            // Edge tier (simplex-core-1b)
            Self::DocumentExtraction
            | Self::InvoiceProcessing
            | Self::EntityExtraction
            | Self::NewsSummarization
            | Self::SentimentAnalysis => ModelTier::Edge,

            // Hive tier (simplex-core-7b)
            Self::ContractAnalysis
            | Self::CodeGeneration
            | Self::SimplexSpecialist
            | Self::CodeReview
            | Self::TechnicalWriting
            | Self::MathReasoning
            | Self::FinancialAnalysis
            | Self::LegalAnalysis => ModelTier::Hive,
        }
    }

    pub fn default_tier_threshold(&self) -> f64 {
        match self.base_model() {
            ModelTier::Edge => 0.30,   // Anima
            ModelTier::Hive => 0.50,   // Mnemonic
            ModelTier::Divine => 0.70, // Divine
        }
    }

    pub fn training_examples_required(&self) -> usize {
        match self {
            // More complex specialists need more training data
            Self::ContractAnalysis | Self::LegalAnalysis => 50_000,
            Self::CodeGeneration | Self::SimplexSpecialist | Self::MathReasoning => 100_000,
            Self::FinancialAnalysis => 50_000,
            // Simpler extraction tasks
            _ => 30_000,
        }
    }
}
```

---

### Updated Model Registry (Replaces model-web)

The model-web registry is updated to reflect the unified architecture:

```json
{
  "version": "2.0.0",
  "updated": "2026-01-17",
  "base_url": "https://models.senuamedia.com",
  "architecture": "simplex-native-beliefs",

  "core_models": [
    {
      "id": "simplex-core-7b",
      "name": "Simplex Core 7B",
      "tier": "hive",
      "parameters": "7B",
      "context_length": 128000,
      "description": "Primary hive reasoning model. Native belief formation, revision, and recall. Replaces simplex-cognitive-8b.",
      "output_format": "GroundedBelief<T>",
      "status": "training",
      "license": "Apache-2.0",
      "base_model": "qwen2.5-7b",
      "training_data_format": "simplex-native-beliefs",
      "capabilities": [
        "Native belief formation",
        "Belief revision with provenance",
        "Memory recall (episodic/semantic)",
        "Confidence calibration (ECE < 0.05)",
        "Hive coordination",
        "Specialist routing"
      ],
      "files": []
    },
    {
      "id": "simplex-core-1b",
      "name": "Simplex Core 1B",
      "tier": "edge",
      "parameters": "1.5B",
      "context_length": 32000,
      "description": "Edge deployment model. Fast local inference with native beliefs. Powers Edge Hive.",
      "output_format": "GroundedBelief<T>",
      "status": "planned",
      "license": "Apache-2.0",
      "base_model": "qwen2.5-1.5b",
      "training_data_format": "simplex-native-beliefs",
      "capabilities": [
        "Native belief formation",
        "Local memory persistence",
        "Offline operation",
        "Federation sync"
      ],
      "files": []
    },
    {
      "id": "simplex-core-embed",
      "name": "Simplex Core Embed",
      "tier": "utility",
      "parameters": "110M",
      "description": "Embedding model for belief recall routing. Content-addressed memory search.",
      "output_format": "embedding_vector",
      "status": "planned",
      "license": "Apache-2.0",
      "files": []
    }
  ],

  "specialists": [
    {
      "id": "document_extraction",
      "name": "Document Extraction",
      "category": "document",
      "base_model": "simplex-core-1b",
      "output_format": "GroundedBelief<DocumentFact>",
      "tier": "edge",
      "training_examples": 30000,
      "description": "Forms beliefs about document contents with OCR confidence integration",
      "status": "planned",
      "datasets": ["DocVQA-Beliefs", "CORD-Beliefs", "TableBank-Beliefs"],
      "calibration_target_ece": 0.05,
      "license": "Apache-2.0"
    },
    {
      "id": "invoice_processing",
      "name": "Invoice Processing",
      "category": "document",
      "base_model": "simplex-core-1b",
      "output_format": "GroundedBelief<InvoiceFact>",
      "tier": "edge",
      "training_examples": 30000,
      "description": "Forms beliefs about invoice data with automatic falsification conditions",
      "status": "planned",
      "datasets": ["SROIE-Beliefs", "CORD-Beliefs"],
      "calibration_target_ece": 0.05,
      "license": "Apache-2.0"
    },
    {
      "id": "contract_analysis",
      "name": "Contract Analysis",
      "category": "legal",
      "base_model": "simplex-core-7b",
      "output_format": "GroundedBelief<LegalFact>",
      "tier": "hive",
      "training_examples": 50000,
      "description": "Forms legal beliefs with provenance tracking and risk falsifiers",
      "status": "planned",
      "datasets": ["CUAD-Beliefs", "ContractNLI-Beliefs"],
      "calibration_target_ece": 0.04,
      "license": "Apache-2.0"
    },
    {
      "id": "entity_extraction",
      "name": "Entity Extraction",
      "category": "document",
      "base_model": "simplex-core-1b",
      "output_format": "GroundedBelief<EntityFact>",
      "tier": "edge",
      "training_examples": 30000,
      "description": "Forms entity beliefs with relationship linking to SemanticStore",
      "status": "planned",
      "datasets": ["CoNLL-Beliefs", "OntoNotes-Beliefs"],
      "calibration_target_ece": 0.05,
      "license": "Apache-2.0"
    },
    {
      "id": "code_generation",
      "name": "Code Generation",
      "category": "coding",
      "base_model": "simplex-core-7b",
      "output_format": "GroundedBelief<CodeFact>",
      "tier": "hive",
      "training_examples": 100000,
      "description": "Forms code solution beliefs with compilation falsifiers",
      "status": "planned",
      "datasets": ["CodeSearchNet-Beliefs", "APPS-Beliefs", "MBPP-Beliefs"],
      "calibration_target_ece": 0.04,
      "license": "Apache-2.0"
    },
    {
      "id": "simplex_specialist",
      "name": "Simplex Language Specialist",
      "category": "coding",
      "base_model": "simplex-core-7b",
      "output_format": "GroundedBelief<SimplexFact>",
      "tier": "hive",
      "training_examples": 100000,
      "description": "Native Simplex code generation with codebase-aware beliefs. Self-attuning.",
      "status": "planned",
      "datasets": ["Simplex-Codebase-Beliefs", "Simplex-Spec-Beliefs"],
      "calibration_target_ece": 0.04,
      "license": "Apache-2.0",
      "capabilities": [
        "Simplex code generation",
        "Belief-aware code completion",
        "Codebase recall integration",
        "Self-improvement proposals"
      ]
    },
    {
      "id": "code_review",
      "name": "Code Review",
      "category": "coding",
      "base_model": "simplex-core-7b",
      "output_format": "GroundedBelief<ReviewFact>",
      "tier": "hive",
      "training_examples": 50000,
      "description": "Forms review beliefs with issue falsification conditions",
      "status": "planned",
      "datasets": ["CodeReviewer-Beliefs"],
      "calibration_target_ece": 0.04,
      "license": "Apache-2.0"
    },
    {
      "id": "technical_writing",
      "name": "Technical Writing",
      "category": "writing",
      "base_model": "simplex-core-7b",
      "output_format": "GroundedBelief<DocumentFact>",
      "tier": "hive",
      "training_examples": 50000,
      "description": "Forms documentation beliefs with accuracy falsifiers",
      "status": "planned",
      "datasets": ["arXiv-Beliefs"],
      "calibration_target_ece": 0.04,
      "license": "Apache-2.0"
    },
    {
      "id": "news_summarization",
      "name": "News Summarization",
      "category": "summarization",
      "base_model": "simplex-core-1b",
      "output_format": "GroundedBelief<SummaryFact>",
      "tier": "edge",
      "training_examples": 30000,
      "description": "Forms summary beliefs with source fidelity falsifiers",
      "status": "planned",
      "datasets": ["CNN-DailyMail-Beliefs", "XSum-Beliefs"],
      "calibration_target_ece": 0.05,
      "license": "Apache-2.0"
    },
    {
      "id": "sentiment_analysis",
      "name": "Sentiment Analysis",
      "category": "analysis",
      "base_model": "simplex-core-1b",
      "output_format": "GroundedBelief<SentimentFact>",
      "tier": "edge",
      "training_examples": 30000,
      "description": "Forms sentiment beliefs with sarcasm/context falsifiers",
      "status": "planned",
      "datasets": ["SST2-Beliefs", "IMDB-Beliefs"],
      "calibration_target_ece": 0.05,
      "license": "Apache-2.0"
    },
    {
      "id": "math_reasoning",
      "name": "Math Reasoning",
      "category": "reasoning",
      "base_model": "simplex-core-7b",
      "output_format": "GroundedBelief<ReasoningFact>",
      "tier": "hive",
      "training_examples": 100000,
      "description": "Forms mathematical beliefs with proof verification falsifiers",
      "status": "planned",
      "datasets": ["GSM8K-Beliefs", "MATH-Beliefs"],
      "calibration_target_ece": 0.04,
      "license": "Apache-2.0"
    },
    {
      "id": "financial_analysis",
      "name": "Financial Analysis",
      "category": "finance",
      "base_model": "simplex-core-7b",
      "output_format": "GroundedBelief<FinancialFact>",
      "tier": "hive",
      "training_examples": 50000,
      "description": "Forms financial beliefs with market data falsifiers",
      "status": "planned",
      "datasets": ["FinQA-Beliefs", "TAT-QA-Beliefs"],
      "calibration_target_ece": 0.04,
      "license": "Apache-2.0"
    },
    {
      "id": "legal_analysis",
      "name": "Legal Analysis",
      "category": "legal",
      "base_model": "simplex-core-7b",
      "output_format": "GroundedBelief<LegalFact>",
      "tier": "hive",
      "training_examples": 50000,
      "description": "Forms legal analysis beliefs with precedent falsifiers",
      "status": "planned",
      "datasets": ["CaseHOLD-Beliefs", "LegalBench-Beliefs"],
      "calibration_target_ece": 0.04,
      "license": "Apache-2.0"
    }
  ],

  "deprecated": [
    {
      "id": "sql_generation",
      "reason": "Violates no-SQL principle. Use native BeliefStore queries instead.",
      "replacement": "Use simplex-core belief recall with semantic queries"
    },
    {
      "id": "simplex-cognitive-8b",
      "reason": "Replaced by simplex-core-7b with native belief architecture",
      "replacement": "simplex-core-7b"
    },
    {
      "id": "simplex-cognitive-3b",
      "reason": "Replaced by simplex-core-1b with native belief architecture",
      "replacement": "simplex-core-1b"
    }
  ],

  "training_infrastructure": {
    "data_format": "simplex-native-beliefs",
    "output_type": "GroundedBelief<T>",
    "calibration_method": "epistemic_schedule",
    "provenance_required": true,
    "falsifiers_required": true,
    "no_sql_contamination": true,
    "no_json_output": true,
    "total_specialist_examples": 730000
  }
}
```

---

### Training Timeline (Unified)

| Phase | Duration | Specialists | Examples |
|-------|----------|-------------|----------|
| **Dataset Generation** | 2 weeks | All 13 specialists | 730K total |
| **Core Model Training** | 3 weeks | simplex-core-7b, 1b, embed | - |
| **Edge Specialists** | 1 week | 5 specialists (1b-based) | 150K |
| **Hive Specialists** | 2 weeks | 8 specialists (7b-based) | 580K |
| **Integration Testing** | 1 week | Full orchestration | - |
| **Calibration Validation** | 3 days | ECE verification | - |
| **Total** | **~8 weeks** | | |

---

### Part 7 Success Criteria

| Criterion | Measurement |
|-----------|-------------|
| **All specialists output beliefs** | 100% of specialist outputs parse to `GroundedBelief<T>` |
| **No raw JSON outputs** | 0% raw JSON in any specialist output |
| **All SQL outputs as beliefs** | sql_generation outputs `GroundedBelief<SQLFact>` not raw SQL |
| **Calibration per tier** | Edge: ECE < 0.05, Hive: ECE < 0.04 |
| **Provenance integrity** | 100% of beliefs have valid provenance |
| **Falsifiers present** | 100% of beliefs have at least one falsifier |
| **BeliefStore integration** | All specialist outputs stored and recallable |
| **Specialist coordination** | Specialists can recall beliefs from other specialists |
| **Training data generated** | 730K+ belief-format examples |

---

### The Simplex Philosophy Applied to Specialists

Every specialist embodies the core Simplex principles:

1. **Memory is cognitive, not storage** - Specialists recall beliefs, not query databases
2. **Confidence is intrinsic** - Every specialist output has calibrated confidence
3. **Provenance is never lost** - Every belief knows which specialist formed it
4. **Revision is natural** - Specialist beliefs can be revised with new evidence
5. **Hierarchy is architectural** - Edge/Hive/Divine tiers determine belief propagation

**No adapters. No workarounds. Everything native Simplex from the ground up.**

---

*"A specialist that outputs JSON is not a Simplex specialist. A specialist that forms beliefs is."*

---

*"The model is not finished when it leaves the cluster. It's finished when it's thinking on every device in the hive."*

---

## Part 8: Epistemic Data Refinement - Self-Improving Training Data

> **Vision**: "A researcher that can search the internet to find information, sift through knowledge, distill facts over incorrect data and improve the base training datasets through the belief system and applied self learning philosophy from the gathered research."

### The Core Insight

Training examples are themselves beliefs about what the model should learn. They should be treated with the same epistemic rigor as runtime beliefs.

Standard approach: Collect data → Train → Deploy
Simplex approach: Collect data → **Validate → Anneal → Refine** → Train → Deploy

### Epistemic Data Refinement Architecture

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                         EPISTEMIC DATA REFINEMENT LOOP                          │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐   │
│  │   RESEARCH  │────▶│  VALIDATE   │────▶│   ANNEAL    │────▶│   AUGMENT   │   │
│  │   Gather    │     │   Check     │     │  Temperature│     │  Generate   │   │
│  │   Facts     │     │   Sources   │     │  Scheduling │     │  Examples   │   │
│  └─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘   │
│         │                   │                   │                   │           │
│         ▼                   ▼                   ▼                   ▼           │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐   │
│  │ Specs, APIs │     │ Cross-ref   │     │ HEAT when   │     │ High-conf   │   │
│  │ Papers      │     │ Multiple    │     │ uncertain   │     │ facts →     │   │
│  │ Official    │     │ Sources     │     │ COOL when   │     │ new         │   │
│  │ Docs        │     │             │     │ validated   │     │ examples    │   │
│  └─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘   │
│                                                                                 │
│                                    │                                            │
│                                    ▼                                            │
│                           ┌─────────────┐                                       │
│                           │    PRUNE    │                                       │
│                           │  Remove     │                                       │
│                           │  low-quality│                                       │
│                           │  examples   │                                       │
│                           └─────────────┘                                       │
│                                    │                                            │
│                                    ▼                                            │
│                        ┌────────────────────┐                                   │
│                        │  REFINED DATASET   │                                   │
│                        │  Every example is  │                                   │
│                        │  GroundedBelief<T> │                                   │
│                        └────────────────────┘                                   │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### Implementation: `/simplex-training/src/research/`

| File | Component | Purpose |
|------|-----------|---------|
| `mod.sx` | Module definition | Re-exports all components |
| `researcher.sx` | `ResearchSpecialist` | Searches authoritative sources for knowledge |
| `sources.sx` | `SourceRegistry` | Tracks source credibility (Specs > Docs > Papers > Community) |
| `validator.sx` | `DataValidator` | Validates examples against gathered knowledge |
| `annealer.sx` | `DatasetAnnealer` | Applies temperature scheduling to examples |
| `refiner.sx` | `DataRefiner` | Orchestrates the complete refinement pipeline |

### Source Authority Hierarchy

```simplex
pub enum SourceType {
    /// Official specs (ECMA, ISO, W3C, RFC) - Credibility: 0.95-1.0
    Specification,

    /// Official docs (docs.python.org, rust-lang.org) - Credibility: 0.85-0.95
    OfficialDocumentation,

    /// Peer-reviewed papers (ACM, IEEE, arxiv) - Credibility: 0.80-0.90
    AcademicPaper,

    /// Technical refs (cppreference, MDN) - Credibility: 0.75-0.85
    TechnicalReference,

    /// Community validated (high-vote StackOverflow) - Credibility: 0.60-0.75
    CommunityValidated,

    /// Unknown sources - Credibility: 0.0-0.5
    Unknown,
}
```

### Dataset Annealing: Temperature for Training Examples

Each training example has a temperature reflecting epistemic uncertainty:

**HEAT (increase uncertainty) when:**
- Sources disagree about the example
- No authoritative backing found
- Example contradicts known specifications
- Evidence is stale (API changed, spec updated)
- Validation attempts fail

**COOL (increase confidence) when:**
- Multiple authoritative sources confirm
- Specification explicitly validates
- Peer-reviewed source confirms
- Consistent with other validated examples

**PRUNE when:**
- Temperature exceeds threshold
- Example explicitly invalid
- Too many failed validation attempts

```simplex
pub struct DatasetTemperature {
    /// Current temperature - high = uncertain, low = validated
    pub temperature: dual,

    /// Validation attempts made
    pub validation_attempts: usize,

    /// Is this example frozen (stable, validated)?
    pub frozen: bool,

    /// Reasons for current temperature
    pub reasons: Vec<TemperatureReason>,
}

impl DatasetTemperature {
    /// Apply heating when validation fails
    pub fn heat(&mut self, factor: f64, reason: &str, max_temp: f64) {
        let new_temp = (self.temperature.val * factor).min(max_temp);
        self.temperature = dual::variable(new_temp);
        self.reasons.push(TemperatureReason {
            change: TemperatureChange::Heat,
            amount: factor,
            reason: reason.to_string(),
        });
        self.frozen = false;
    }

    /// Apply cooling when validation succeeds
    pub fn cool(&mut self, factor: f64, reason: &str, min_temp: f64) {
        let new_temp = (self.temperature.val * factor).max(min_temp);
        self.temperature = dual::variable(new_temp);
        self.reasons.push(TemperatureReason {
            change: TemperatureChange::Cool,
            amount: factor,
            reason: reason.to_string(),
        });
    }
}
```

### The Annealed Training Example

```simplex
pub struct AnnealedExample {
    /// The training content
    pub prompt: String,
    pub response: String,
    pub domain: String,

    /// Temperature state
    pub temperature: DatasetTemperature,

    /// Validation status
    pub validation_status: ValidationStatus,

    /// Provenance of this example
    pub provenance: ExampleProvenance,

    /// Unique identifier
    pub id: String,
}

impl AnnealedExample {
    /// Should this example be included in training?
    pub fn should_include(&self, config: &AnnealerConfig) -> bool {
        // Don't include if too hot (uncertain)
        if self.temperature.current() > config.inclusion_threshold {
            return false;
        }

        // Don't include if validation failed
        if matches!(self.validation_status,
                   ValidationStatus::Invalid | ValidationStatus::Conflicted) {
            return false;
        }

        true
    }

    /// Get confidence score (inverse of temperature)
    pub fn confidence(&self) -> f64 {
        1.0 / (1.0 + self.temperature.current())
    }
}
```

### Scheduled Dissent for Datasets

Just like runtime beliefs, dataset examples undergo scheduled dissent:

```simplex
pub struct AnnealerConfig {
    /// Enable scheduled dissent (periodic revalidation)
    pub enable_dissent: bool,

    /// Dissent period (examples between dissent checks)
    pub dissent_period: usize,

    /// Fraction of examples to re-check during dissent
    pub dissent_fraction: f64,
}
```

During dissent windows:
1. Previously frozen examples are unfrozen
2. A fraction are re-validated against current knowledge
3. Stale knowledge triggers heating
4. Fresh confirmation triggers cooling

### The Complete Refinement Pipeline

```simplex
use simplex_training::{DataRefiner, RefinerConfig, RawExample, refine_training_data};

// Create raw examples from any source
let raw_examples: Vec<RawExample> = load_synthetic_data()
    .into_iter()
    .map(|ex| RawExample::new(&ex.prompt, &ex.response, &ex.domain, &ex.source))
    .collect();

// Configure refinement
let config = RefinerConfig {
    research: ResearchConfig {
        min_credibility: 0.7,
        require_corroboration: true,
        min_corroborating_sources: 2,
        search_domains: vec![
            "docs.python.org",
            "ecma-international.org",
            "developer.mozilla.org",
            "arxiv.org",
        ],
        ..Default::default()
    },
    annealing: AnnealerConfig {
        initial_temp: 1.0,
        min_temp: 0.1,
        max_temp: 5.0,
        inclusion_threshold: 0.5,
        pruning_threshold: 3.0,
        enable_dissent: true,
        dissent_period: 1000,
        dissent_fraction: 0.05,
        ..Default::default()
    },
    max_iterations: 10,
    enable_augmentation: true,
    ..Default::default()
};

// Run refinement
let report = refine_training_data(raw_examples, config);

println!("{}", report.summary());
// Output:
// === Refinement Report ===
// Initial examples: 100000
// Final examples: 78523 (-21477)
// Validated: 65234 (83.1%)
// Pruned: 21477
// Augmented: 3456
// Facts discovered: 12847
// Iterations: 7
// Final avg temperature: 0.284
// Final avg confidence: 78.9%
// Quality improvement: 65.2%
// Duration: 847.32s
```

### Research Specialist: Gathering Authoritative Knowledge

```simplex
pub struct ResearchSpecialist {
    config: ResearchConfig,
    source_registry: SourceRegistry,
    cache: ResearchCache,
}

impl ResearchSpecialist {
    /// Research a claim/query
    pub fn research(&mut self, query: &str) -> ResearchResult {
        // Check cache first
        if let Some(cached) = self.cache.get(query) {
            return cached.clone();
        }

        // Search authoritative domains
        let mut result = ResearchResult::empty(query);

        for domain in &self.config.search_domains {
            if let Some(fact) = self.search_domain(query, domain) {
                result.facts.push(fact);
            }
        }

        // Calculate confidence and detect conflicts
        result.confidence = self.calculate_confidence(&result);
        result.conflicts = self.detect_conflicts(&result.facts);

        self.cache.put(query, result.clone());
        result
    }

    /// Validate a training example
    pub fn validate_example(&mut self, claim: &str, domain: &str) -> ValidationOutcome {
        let result = self.research(claim);

        if result.has_conflicts() {
            return ValidationOutcome::Conflicted(result.conflicts);
        }

        if let Some(primary) = result.primary_fact() {
            if primary.is_well_supported() {
                return ValidationOutcome::Validated(primary.confidence);
            }
        }

        ValidationOutcome::Unverified
    }
}
```

### Primary Data Sources

| Source Type | Examples | Credibility |
|-------------|----------|-------------|
| **Language Specifications** | ECMA-262 (JS), ISO C++, RFC 2616 (HTTP), W3C HTML5 | 0.95-1.0 |
| **Official Documentation** | docs.python.org, doc.rust-lang.org, developer.mozilla.org | 0.85-0.95 |
| **Academic Papers** | ACM, IEEE, arxiv (peer-reviewed) | 0.80-0.90 |
| **Technical References** | cppreference.com, PostgreSQL docs | 0.75-0.85 |
| **Community Validated** | High-vote StackOverflow (>100 votes) | 0.60-0.75 |

### Source Registry: Credibility Tracking

```simplex
impl SourceRegistry {
    fn populate_defaults(&mut self) {
        // Specifications - highest authority
        self.register_domain("ecma-international.org", SourceType::Specification, None);
        self.register_domain("w3.org", SourceType::Specification, None);
        self.register_domain("ietf.org", SourceType::Specification, None);

        // Official documentation
        self.register_domain("docs.python.org", SourceType::OfficialDocumentation, None);
        self.register_domain("doc.rust-lang.org", SourceType::OfficialDocumentation, None);
        self.register_domain("developer.mozilla.org", SourceType::OfficialDocumentation, None);

        // Academic
        self.register_domain("arxiv.org", SourceType::AcademicPaper, None);
        self.register_domain("acm.org", SourceType::AcademicPaper, None);

        // Known problematic sources - with penalties
        self.register_domain_with_adjustment(
            "w3schools.com",
            SourceType::KnownMedium,
            CredibilityAdjustment {
                reason: "Known for outdated/incorrect content".to_string(),
                amount: -0.2,
            }
        );
    }
}
```

### Integration with Existing Epistemic Infrastructure

The data refinement system integrates with TASK-014's epistemic infrastructure:

| Component | From TASK-014 | Used In Refinement |
|-----------|---------------|-------------------|
| `EpistemicSchedule` | Temperature modulation | Dataset temperature scheduling |
| `EpistemicMonitors` | Health metrics | Track source agreement, staleness |
| `DissentConfig` | Mandatory dissent windows | Periodic example revalidation |
| `HealthMetric` | Track with history | Track example temperature trends |
| `dual` numbers | Gradient flow | Differentiable temperature |

### Expected Outcomes

| Metric | Before Refinement | After Refinement |
|--------|-------------------|------------------|
| **Examples with authoritative backing** | ~30% | >80% |
| **Examples with conflicts** | Unknown | <2% |
| **Average confidence** | 0.5 (assumed) | >0.75 |
| **Pruned low-quality** | 0% | 15-25% |
| **Augmented from research** | 0 | +5-10% |
| **Staleness issues** | Unknown | <5% |

### Part 8 Success Criteria

| Criterion | Measurement |
|-----------|-------------|
| **Research specialist functional** | Can search and validate claims |
| **Source credibility tracked** | All major domains registered with appropriate scores |
| **Dataset annealing working** | Temperature updates based on validation |
| **Dissent windows active** | Frozen examples re-validated periodically |
| **Pruning effective** | Low-quality examples removed |
| **Augmentation working** | New examples generated from high-confidence facts |
| **Quality improvement measurable** | >50% improvement in validated examples |

---

### The Self-Improving Loop

This is what makes Simplex unique: the same epistemic machinery that governs runtime beliefs also governs training data quality. The model doesn't just learn - it learns what it should learn from.

```
Raw Data → Research → Validate → Anneal → Prune → Augment → Refined Data
                                    ↑                           │
                                    └───────────────────────────┘
                                       (Scheduled Dissent)
```

**No static datasets. No assumed quality. Everything validated, everything annealed, everything refined.**

---

*"A model trained on unvalidated data inherits its noise. A model trained on epistemically-refined data inherits its discipline."*

---

## Part 9: Self-Evolving Code - The Complete Vision

> **The Final Insight**: If datasets and models can be annealed, so can code. Code is a belief - "this implementation is correct and optimal." Subject it to the same epistemic machinery.

### Code as Grounded Belief

```simplex
/// A piece of code treated as an epistemic entity
pub struct CodeBelief<T> {
    /// The actual code/implementation
    pub implementation: T,

    /// Belief that this code is correct
    pub correctness: GroundedBelief<bool>,

    /// Belief about performance characteristics
    pub performance: GroundedBelief<PerformanceProfile>,

    /// Test results as evidence
    pub test_evidence: Vec<TestEvidence>,

    /// Benchmark results as evidence
    pub benchmark_evidence: Vec<BenchmarkEvidence>,

    /// Usage patterns as evidence
    pub usage_evidence: Vec<UsageEvidence>,

    /// Temperature - how certain are we about this code?
    pub temperature: dual,

    /// Falsifiers - what would prove this code wrong?
    pub falsifiers: Vec<CodeFalsifier>,

    /// Provenance - who wrote this, when, why?
    pub provenance: CodeProvenance,
}

/// What would falsify this code?
pub enum CodeFalsifier {
    /// A test case that would fail
    FailingTest { input: Value, expected: Value },

    /// A performance threshold violation
    PerformanceRegression { metric: String, threshold: f64 },

    /// A security vulnerability
    SecurityVulnerability { cve_pattern: String },

    /// Memory safety violation
    MemorySafetyViolation,

    /// Type system unsoundness
    TypeUnsoundness,
}
```

### The Code Annealing Loop

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           CODE ANNEALING LOOP                                    │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐   │
│  │   MUTATE    │────▶│    TEST     │────▶│   ANNEAL    │────▶│   SELECT    │   │
│  │  Generate   │     │  Run all    │     │  Temperature│     │  Keep or    │   │
│  │  variants   │     │  tests      │     │  update     │     │  discard    │   │
│  └─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘   │
│         │                   │                   │                   │           │
│         ▼                   ▼                   ▼                   ▼           │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐   │
│  │ Refactoring │     │ Unit tests  │     │ HEAT on     │     │ Better      │   │
│  │ Optimization│     │ Benchmarks  │     │ failure     │     │ variants    │   │
│  │ Alternatives│     │ Fuzzing     │     │ COOL on     │     │ survive     │   │
│  │             │     │ Property    │     │ success     │     │             │   │
│  └─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘   │
│                                                                                 │
│                              ↓                                                  │
│                    ┌────────────────────┐                                       │
│                    │   EVOLVED CODE     │                                       │
│                    │  Better, tested,   │                                       │
│                    │  confident         │                                       │
│                    └────────────────────┘                                       │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### Temperature Dynamics for Code

**HEAT (increase uncertainty) when:**
- Tests fail
- Benchmarks show regression
- Security scan finds issues
- Memory sanitizer detects problems
- Type checker finds unsoundness
- Fuzzer discovers crash

**COOL (increase confidence) when:**
- All tests pass
- Benchmarks improve or stable
- Security scan clean
- Extended fuzzing finds no issues
- Property-based tests pass
- Production usage without errors

**PRUNE when:**
- Temperature exceeds threshold
- Consistently failing tests
- Security vulnerabilities unfixed
- Performance regression confirmed

### Self-Hosted Compiler Evolution

The ultimate test: **The Simplex compiler evolving itself**.

```simplex
/// Self-evolving compiler
pub struct EvolvingCompiler {
    /// Current compiler implementation
    current: CodeBelief<Compiler>,

    /// Candidate variations being tested
    candidates: Vec<CodeBelief<Compiler>>,

    /// Test suite for compiler correctness
    test_suite: CompilerTestSuite,

    /// Benchmark suite for compiler performance
    benchmarks: CompilerBenchmarks,

    /// Evolution configuration
    config: EvolutionConfig,
}

impl EvolvingCompiler {
    /// Run one evolution step
    pub fn evolve_step(&mut self) -> EvolutionResult {
        // 1. Generate candidate variations
        let mutations = self.generate_mutations();

        for mutation in mutations {
            // 2. Build the mutated compiler
            let candidate = self.current.apply_mutation(&mutation);

            // 3. Self-compile (bootstrap test)
            if !candidate.can_compile_self() {
                // Immediate rejection - can't even bootstrap
                continue;
            }

            // 4. Run test suite
            let test_results = self.test_suite.run(&candidate);

            // 5. Update temperature based on results
            if test_results.all_pass() {
                candidate.temperature.cool(0.9, "All tests pass");

                // 6. Run benchmarks
                let bench_results = self.benchmarks.run(&candidate);

                if bench_results.better_than(&self.current) {
                    candidate.temperature.cool(0.8, "Performance improved");
                    self.candidates.push(candidate);
                } else if bench_results.no_regression(&self.current) {
                    candidate.temperature.cool(0.95, "No regression");
                    self.candidates.push(candidate);
                }
            } else {
                candidate.temperature.heat(1.5, "Tests failed");
                // Discard - tests must pass
            }
        }

        // 7. Select best candidate if better than current
        if let Some(best) = self.select_best_candidate() {
            let old = std::mem::replace(&mut self.current, best);
            EvolutionResult::Improved { old, new: self.current.clone() }
        } else {
            EvolutionResult::NoImprovement
        }
    }

    /// Generate mutations of current compiler
    fn generate_mutations(&self) -> Vec<CodeMutation> {
        vec![
            // Optimization mutations
            CodeMutation::OptimizeFunction { target: "codegen::emit_expr" },
            CodeMutation::InlineSmallFunctions { threshold: 10 },
            CodeMutation::UnrollLoop { target: "parser::parse_block" },

            // Refactoring mutations
            CodeMutation::ExtractFunction { from: "lexer::scan_token", lines: 50..80 },
            CodeMutation::SimplifyCondition { target: "typechecker::unify" },

            // Alternative implementation mutations
            CodeMutation::TryAlternativeAlgorithm {
                target: "optimizer::dead_code_elimination",
                algorithm: "iterative_dataflow"
            },
        ]
    }
}
```

### The Evolution Engine

```simplex
/// Engine for evolving any Simplex code
pub struct EvolutionEngine {
    /// Mutation strategies
    mutators: Vec<Box<dyn Mutator>>,

    /// Testing infrastructure
    test_runner: TestRunner,

    /// Benchmarking infrastructure
    benchmark_runner: BenchmarkRunner,

    /// Annealing configuration
    annealing: AnnealerConfig,

    /// Population of code variants
    population: Vec<CodeBelief<Code>>,

    /// Best known implementation
    champion: CodeBelief<Code>,
}

impl EvolutionEngine {
    /// Evolve a piece of code
    pub fn evolve(&mut self, code: Code, generations: usize) -> EvolutionReport {
        self.champion = CodeBelief::new(code);

        for gen in 0..generations {
            // Generate mutations
            let mutants = self.mutate_population();

            // Test all mutants
            let results = self.test_runner.run_all(&mutants);

            // Anneal based on results
            for (mutant, result) in mutants.iter_mut().zip(results) {
                self.apply_annealing(mutant, &result);
            }

            // Select survivors
            self.population = self.select_survivors(mutants);

            // Update champion if better found
            if let Some(better) = self.find_better_than_champion() {
                self.champion = better;
            }

            // Scheduled dissent - re-test champion
            if gen % self.annealing.dissent_period == 0 {
                self.retest_champion();
            }
        }

        self.build_report()
    }

    /// Apply annealing based on test results
    fn apply_annealing(&self, code: &mut CodeBelief<Code>, result: &TestResult) {
        match result {
            TestResult::AllPass { duration } => {
                code.temperature.cool(0.9, "All tests pass");
                code.correctness.update_confidence(0.95);

                // Add test evidence
                code.test_evidence.push(TestEvidence {
                    passed: true,
                    duration: *duration,
                    timestamp: now(),
                });
            }

            TestResult::SomeFailed { failures } => {
                code.temperature.heat(1.5, &format!("{} tests failed", failures.len()));
                code.correctness.update_confidence(0.3);

                // Record failures as potential falsifiers
                for failure in failures {
                    code.falsifiers.push(CodeFalsifier::FailingTest {
                        input: failure.input.clone(),
                        expected: failure.expected.clone(),
                    });
                }
            }

            TestResult::Crash { reason } => {
                code.temperature.heat(3.0, &format!("Crash: {}", reason));
                code.correctness.update_confidence(0.0);
            }
        }
    }
}
```

### Runtime Self-Optimization

The runtime can also evolve based on actual usage:

```simplex
/// Self-optimizing runtime
pub struct EvolvingRuntime {
    /// Current runtime configuration
    config: RuntimeConfig,

    /// Performance metrics from production
    metrics: ProductionMetrics,

    /// Configuration variants being A/B tested
    variants: Vec<ConfigVariant>,

    /// Temperature for each config parameter
    param_temps: HashMap<String, dual>,
}

impl EvolvingRuntime {
    /// Adapt based on production metrics
    pub fn adapt(&mut self) {
        // Analyze recent metrics
        let analysis = self.metrics.analyze_recent(Duration::hours(1));

        // Identify hot spots
        for hotspot in analysis.hotspots() {
            // Generate optimization variants
            let variants = self.generate_optimization_variants(&hotspot);

            // A/B test in production (carefully!)
            for variant in variants {
                self.start_ab_test(variant);
            }
        }

        // Evaluate completed A/B tests
        for test in self.completed_ab_tests() {
            if test.variant_better() {
                // Cool - this optimization works
                self.param_temps.get_mut(&test.param)
                    .map(|t| t.cool(0.9, "A/B test positive"));

                // Promote to main config
                self.config.apply(test.variant);
            } else {
                // Heat - this didn't help
                self.param_temps.get_mut(&test.param)
                    .map(|t| t.heat(1.2, "A/B test negative"));
            }
        }
    }

    /// Parameters that can be evolved
    fn evolvable_parameters() -> Vec<&'static str> {
        vec![
            "gc.collection_threshold",
            "gc.generation_sizes",
            "scheduler.work_stealing_threshold",
            "scheduler.thread_pool_size",
            "memory.allocation_strategy",
            "memory.page_size",
            "jit.optimization_level",
            "jit.inline_threshold",
            "network.buffer_sizes",
            "network.connection_pool_size",
        ]
    }
}
```

### The Complete Evolution Hierarchy

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                      SIMPLEX SELF-EVOLUTION HIERARCHY                            │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  LEVEL 4: ARCHITECTURE EVOLUTION (Long-term)                                    │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │ Evolve major architectural decisions, type system, memory model         │   │
│  │ Requires extensive validation, human oversight                          │   │
│  │ Dissent period: months                                                  │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│                                    │                                            │
│  LEVEL 3: COMPILER EVOLUTION (Weekly)                                          │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │ Evolve compiler optimizations, code generation, type inference          │   │
│  │ Validated by bootstrap + test suite + benchmarks                        │   │
│  │ Dissent period: days                                                    │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│                                    │                                            │
│  LEVEL 2: RUNTIME EVOLUTION (Daily)                                            │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │ Evolve runtime parameters, GC tuning, scheduler config                  │   │
│  │ Validated by production metrics + A/B testing                           │   │
│  │ Dissent period: hours                                                   │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│                                    │                                            │
│  LEVEL 1: MODEL/DATA EVOLUTION (Continuous)                                    │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │ Evolve training data, model weights, specialist behaviors               │   │
│  │ Validated by epistemic health metrics + calibration                     │   │
│  │ Dissent period: minutes                                                 │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### Safety Constraints for Code Evolution

Evolution requires guardrails:

```simplex
pub struct EvolutionSafetyConfig {
    /// Never mutate these modules (core safety)
    pub no_mutate_zones: Vec<ModulePath>,

    /// Require human approval for these changes
    pub human_approval_required: Vec<ChangeType>,

    /// Minimum test coverage for evolved code
    pub min_test_coverage: f64,

    /// Maximum temperature for production deployment
    pub max_deploy_temperature: f64,

    /// Rollback threshold (errors per minute)
    pub rollback_threshold: f64,

    /// Canary deployment percentage
    pub canary_percentage: f64,
}

impl Default for EvolutionSafetyConfig {
    fn default() -> Self {
        EvolutionSafetyConfig {
            no_mutate_zones: vec![
                "simplex_core::safety",      // Never touch safety module
                "simplex_core::memory",      // Memory safety is sacred
                "simplex_crypto",            // Crypto must not be mutated
            ],
            human_approval_required: vec![
                ChangeType::TypeSystemChange,
                ChangeType::MemoryModelChange,
                ChangeType::SecurityCritical,
            ],
            min_test_coverage: 0.95,
            max_deploy_temperature: 0.3,  // Must be very confident
            rollback_threshold: 10.0,     // Errors per minute
            canary_percentage: 5.0,       // Start with 5% traffic
        }
    }
}
```

### Part 9 Success Criteria

| Criterion | Measurement |
|-----------|-------------|
| **Code-as-belief infrastructure** | `CodeBelief<T>` fully implemented |
| **Mutation engine working** | Can generate valid code mutations |
| **Test integration** | Mutations tested automatically |
| **Temperature tracking** | Code temperature updates based on test results |
| **Compiler self-evolution** | Compiler can improve itself (bootstrap + test) |
| **Runtime adaptation** | Runtime parameters adapt to production metrics |
| **Safety guardrails** | No-mutate zones respected, rollback works |
| **Human oversight** | Critical changes require approval |

---

### The Complete Simplex Vision

With Part 9, Simplex becomes a **self-improving system at every level**:

| Layer | What Evolves | How |
|-------|-------------|-----|
| **Data** | Training examples | Research + Validation + Annealing |
| **Models** | Neural weights | Training with epistemic schedules |
| **Code** | Compiler, stdlib | Mutation + Testing + Selection |
| **Runtime** | GC, scheduler, JIT | Production metrics + A/B testing |

All governed by the same epistemic machinery: **temperature, dissent, falsification**.

---

*"A system that cannot improve itself is frozen in time. A system that improves without discipline is chaos. Simplex is disciplined self-improvement - evolution with epistemics."*
