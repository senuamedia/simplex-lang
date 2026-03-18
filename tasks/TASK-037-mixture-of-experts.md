# TASK-037: Mixture of Experts with Formal Routing

**Version:** 0.16.0
**Status:** Planned
**Priority:** P0 — Critical
**Depends on:** v0.15.0 release, neural gates (TASK-001), cognitive hive (TASK-009)

## Why This Feature Is Needed

The largest frontier models (GPT-4, Mixtral, DeepSeek-V2) all use Mixture of Experts.
The reason is simple arithmetic: a 70B-parameter MoE model that activates only 12B
parameters per token achieves 70B quality at 12B cost. This is the single most important
architectural technique for building models that are both intelligent and efficient.

Simplex already has the Cognitive Hive — specialists that divide labor across domains.
But the routing between specialists is currently rule-based or fixed. MoE makes the
routing *learnable*. The router becomes a neural gate (which Simplex already supports)
that learns which expert handles which input through gradient descent. Combined with
Gumbel-Softmax and temperature annealing (which Simplex already has), the router starts
soft (exploring all experts) and hardens to discrete routing (exploiting the best expert)
— exactly the program-as-model paradigm that defines Simplex.

No compiled language has MoE as a first-class architectural primitive. In Python, MoE
requires hundreds of lines of careful load-balancing code. In Simplex, `MoELayer` is a
drop-in replacement for `Linear` with built-in load balancing, capacity enforcement, and
expert collapse detection.

## Why It Adds Value

1. **10x parameter efficiency.** A hive with 10 specialists sharing a single MoE model
   activates only 1-2 experts per token. The total model can be large (capturing broad
   knowledge) while inference cost stays small (specialist-level).

2. **Natural fit with Cognitive Hive.** Each MoE expert maps to a specialist domain.
   The router learns the same boundaries the hive architect designed — but discovers
   finer-grained routing that humans would miss.

3. **Learnable routing via neural gates.** The MoE router is a `DualGate` — it trains
   with Gumbel-Softmax, anneals to hard routing, and carries contracts (requires/ensures).
   This means routing decisions are governable: you can require that certain inputs always
   route to the safety expert, for example.

4. **Load balancing as a compiled constraint.** Expert collapse (one expert gets all
   traffic) is the #1 failure mode of MoE. In Simplex, load balancing is an auxiliary
   loss enforced by the compiler — you cannot build an MoE layer without specifying a
   capacity factor.

5. **Expert-level pruning.** After training, entire experts with low utilization can be
   pruned away, reducing the model to only the experts that matter. This composes with
   the existing compression pipeline.

## Why It Changes Systems Built With Simplex

MoE transforms what is possible on constrained hardware:

- **Edge deployment.** A 7B MoE model activating 1.5B per token runs on a phone. The
  same quality previously required a server-class GPU.
- **Specialist hives become one model.** Instead of loading 10 separate specialist SLMs
  (even small ones), a single MoE model contains all 10 specialists with shared
  embeddings and expert-specific layers. Memory drops by 5-8x.
- **Dynamic specialization.** New domains are added by training new experts (via LoRA
  on expert layers) without retraining the router or other experts. The hive grows
  without catastrophic forgetting.
- **Governed routing.** The `@expert` annotation and capacity contracts mean deployment
  teams can audit which expert handles which input — regulatory compliance for AI
  decision-making.

## Deliverables

### Phase 1: Core MoE Architecture (~600 lines)

Location: `simplex-learning/src/moe/`

- **ExpertRouter** — top-k routing with Gumbel-Softmax noise during training, hard
  routing at inference
- **SparseExpert<T>** — wraps any layer, only activated when selected by router
- **MoELayer** — drop-in replacement for dense layers with configurable expert count,
  top-k, and capacity factor
- **LoadBalanceLoss** — auxiliary loss to prevent expert collapse (Switch Transformer
  style)

```simplex
/// A Mixture of Experts layer with top-k routing
struct MoELayer<E> {
    experts: Vec<E>,
    router: ExpertRouter,
    top_k: usize,
    capacity_factor: f64,
    load_balance_weight: f64,
}

/// The router is a neural gate — learnable, annealable, contractable
struct ExpertRouter {
    gate: DualGate,
    num_experts: usize,
    temperature: Dual,  // anneals from soft to hard routing
}

impl ExpertRouter {
    /// Route input to top-k experts with load-balanced selection
    fn route(self: &Self, input: &Tensor) -> Vec<(usize, f64)> {
        let logits = self.gate.forward(input);
        let probs = match self.gate.mode() {
            Mode::Training => gumbel_softmax(logits, self.temperature),
            Mode::Inference => hard_topk(logits, self.top_k),
        };
        top_k_with_capacity(probs, self.top_k, self.capacity_factor)
    }
}

impl<E: Layer> Layer for MoELayer<E> {
    fn forward(self: &Self, input: Tensor) -> Tensor {
        let routes = self.router.route(&input);
        let mut output = Tensor::zeros(input.shape());
        for (expert_idx, weight) in routes {
            output = output + weight * self.experts[expert_idx].forward(input.clone());
        }
        output
    }
}
```

Files:
- `simplex-learning/src/moe/mod.sx` — module root
- `simplex-learning/src/moe/router.sx` — ExpertRouter with DualGate
- `simplex-learning/src/moe/layer.sx` — MoELayer implementation
- `simplex-learning/src/moe/expert.sx` — SparseExpert wrapper
- `simplex-learning/src/moe/balance.sx` — LoadBalanceLoss and capacity enforcement

### Phase 2: Hive Integration (~400 lines)

- **HiveRouter** — maps MoE experts to cognitive hive specialists by domain
- **SpecialistExpert** — wraps a hive specialist as an MoE expert with shared embeddings
- **CrossExpertAttention** — optional attention across expert outputs for multi-domain
  reasoning
- **ExpertMemory** — each expert maintains its own episodic/semantic memory partition

```simplex
/// Routes to hive specialists via learned MoE routing
struct HiveRouter {
    moe: MoELayer<SpecialistExpert>,
    domain_hints: HashMap<String, usize>,  // optional hard routing overrides
}

impl HiveRouter {
    fn route_query(self: &Self, query: &str, context: &HiveContext) -> SpecialistResponse {
        // Check for hard-coded domain routing first (governance)
        if let Some(expert_id) = self.domain_hints.get(query.domain()) {
            return self.moe.experts[*expert_id].handle(query, context);
        }
        // Otherwise, learned routing via MoE
        let input = self.encode(query, context);
        self.moe.forward(input)
    }
}
```

Files:
- `simplex-learning/src/moe/hive.sx` — HiveRouter and SpecialistExpert
- `simplex-learning/src/moe/memory.sx` — per-expert memory partitioning
- `simplex-learning/src/moe/cross_expert.sx` — cross-expert attention

### Phase 3: Training & Optimization (~400 lines)

- **ExpertParallelism** — distribute experts across devices for parallel execution
- **DropTokens** — overflow handling when expert capacity is exceeded
- **ExpertPruning** — remove underutilized experts post-training
- **LoRAExpert** — add new experts via LoRA without retraining existing ones
- **MoESchedule** — anneal number of active experts during training (start with all,
  narrow to fewer)

Files:
- `simplex-learning/src/moe/parallel.sx` — expert parallelism
- `simplex-learning/src/moe/pruning.sx` — expert utilization analysis and pruning
- `simplex-learning/src/moe/lora_expert.sx` — LoRA-based expert addition

### Phase 4: Tests (~400 lines)

Location: `tests/moe/`

- Router learns to separate synthetic multi-domain data
- Load balance loss prevents expert collapse
- Capacity overflow handled correctly (tokens dropped/redirected)
- Expert pruning removes unused experts without quality loss
- HiveRouter correctly maps to specialists
- Temperature annealing transitions from soft to hard routing

## Success Criteria

- [ ] MoELayer is a drop-in replacement for Linear in existing models
- [ ] Router trains via Gumbel-Softmax and anneals to hard top-k at inference
- [ ] Load balance auxiliary loss keeps expert utilization within 2x of uniform
- [ ] Expert pruning reduces model size without >1% quality degradation on test set
- [ ] HiveRouter composes with existing Cognitive Hive specialist system
- [ ] MoE model with 8 experts, top-2 routing achieves ~4x speedup over dense equivalent

## Estimated Scope

~1,800 lines across library code and tests.
