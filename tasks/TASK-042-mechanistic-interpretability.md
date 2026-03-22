# TASK-042: Mechanistic Interpretability Primitives

**Version:** 0.16.0
**Status:** Complete
**Priority:** P2 — Medium
**Depends on:** v0.15.0 release, SLM runtime (TASK-015), existing layer infrastructure

## Why This Feature Is Needed

The AI industry has a trust problem. Models are deployed in high-stakes settings —
medicine, finance, law, hiring — and nobody can explain what they learned or why they
produce specific outputs. Post-hoc explanations (SHAP, LIME) approximate feature
importance but don't reveal the actual algorithm the model is running. They can be
misleading: a model might use a spurious feature through a complex pathway that
SHAP attributes to a different, plausible-looking feature.

Mechanistic interpretability is different. It opens the model and identifies the actual
computational circuits — specific neurons, attention heads, and pathways that implement
specific behaviors. When Anthropic's interpretability team found that Claude has a
"Golden Gate Bridge neuron" or identified induction heads that implement in-context
learning, they were doing mechanistic interpretability. This is not approximation — it
is reverse-engineering the actual algorithm.

For Simplex's vision of governed AI — SLMs with contracts, beliefs, and safety
constraints — mechanistic interpretability is the verification layer. Contracts say
*what* the model should do. Interpretability verifies *that it actually does it*. Without
this, contracts are aspirational. With it, contracts are auditable.

No programming language provides mechanistic interpretability as built-in tooling.
Researchers use ad-hoc Python scripts with TransformerLens or nnsight. In Simplex,
interpretability is built into the model definition — `@interpretable` annotation
instruments a model automatically, and circuit extraction runs as a post-training
compiler pass.

## Why It Adds Value

1. **Trust through transparency.** Deployment teams can verify that a specialist SLM
   makes decisions based on the right features, not spurious correlations. This is not
   just desirable — it is becoming legally required (EU AI Act, FDA guidance for ML in
   medical devices).

2. **Bug detection.** Mechanistic analysis reveals when a model has learned a shortcut
   instead of the intended behavior. A sentiment classifier that learned to use sentence
   length as a proxy for sentiment — activation patching reveals this immediately.

3. **Circuit-level pruning.** Once you identify which circuits implement which behaviors,
   you can surgically remove unwanted behaviors or keep only desired ones. This is more
   precise than weight-magnitude pruning — you prune by *function*, not by *size*.

4. **Specialist verification.** Each cognitive hive specialist claims a domain. Mechanistic
   analysis can verify that the specialist's internal circuits actually implement domain-
   relevant computations, not generic patterns that happen to work on the training set.

5. **Safety auditing.** Before deploying an SLM, run interpretability analysis to check
   for known dangerous patterns: deceptive alignment circuits, sycophancy patterns,
   encoded biases. Flag these before they reach production.

6. **Integration with neural gates.** Neural gates have contracts (requires/ensures).
   Mechanistic analysis can verify that the gate's internal circuit actually enforces
   the contract, not just that the contract holds on the training distribution.

## Why It Changes Systems Built With Simplex

Mechanistic interpretability changes AI from "trust me" to "verify me":

- **Auditable hives.** Every specialist in a cognitive hive comes with a circuit map.
  Auditors can trace any decision from output back to the specific neurons and attention
  patterns that produced it. Regulatory compliance becomes tractable.

- **Self-healing systems.** When a specialist starts producing unexpected outputs, the
  system can run activation patching in real-time to identify *which circuit changed*
  and either revert that circuit or flag it for human review.

- **Targeted fine-tuning.** Instead of fine-tuning an entire model, identify the circuit
  responsible for the behavior you want to change, and fine-tune only that circuit. 100x
  more efficient, no side effects on other behaviors.

- **Proof of behavior.** A Simplex system can generate a certificate: "This specialist
  uses these circuits for these decisions, verified by activation patching on this test
  set." This is the beginning of formally verified AI — not verifying the math, but
  verifying the learned algorithm.

- **Knowledge extraction.** Identify what each attention head and MLP layer has learned.
  Extract this knowledge as structured data — the specialist "knows" these facts via
  these circuits. Feed this into the belief system for grounded, traceable beliefs.

## Deliverables

### Phase 1: Activation Analysis (~500 lines)

Location: `simplex-learning/src/interpret/`

- **ActivationCache** — record all intermediate activations during a forward pass,
  organized by layer and component
- **ActivationPatching** — replace activations at specific layers/positions with
  activations from a different input, measure effect on output (causal tracing)
- **AttentionAnalyzer** — extract and visualize attention patterns per head, identify
  induction heads, identify positional vs content-based attention
- **NeuronAnalyzer** — identify maximally activating inputs for each neuron, cluster
  neurons by activation pattern

```simplex
/// Cache all intermediate activations during forward pass
struct ActivationCache {
    activations: HashMap<String, Tensor>,  // "layer_3.attention.output" → tensor
    model: &dyn Model,
}

impl ActivationCache {
    /// Run forward pass and record all activations
    fn capture(model: &dyn Model, input: &Tensor) -> Self {
        let mut cache = HashMap::new();
        let hooks = model.layers().iter().map(|layer| {
            Hook::new(layer.name(), |name, activation| {
                cache.insert(name.to_string(), activation.clone());
            })
        }).collect();
        model.forward_with_hooks(input, &hooks);
        Self { activations: cache, model }
    }
}

/// Causal tracing via activation patching
struct ActivationPatching {
    clean_cache: ActivationCache,
    corrupted_cache: ActivationCache,
}

impl ActivationPatching {
    /// Patch activation at `layer` from corrupted run into clean run
    /// and measure effect on output — reveals causal importance of that layer
    fn patch(
        self: &Self,
        layer: &str,
        positions: &[usize],
    ) -> PatchEffect {
        let mut patched_activations = self.clean_cache.activations.clone();
        for pos in positions {
            patched_activations.get_mut(layer).unwrap()[*pos] =
                self.corrupted_cache.activations[layer][*pos].clone();
        }
        let patched_output = self.clean_cache.model
            .forward_from_cache(&patched_activations);
        let clean_output = self.clean_cache.model
            .forward_from_cache(&self.clean_cache.activations);

        PatchEffect {
            layer: layer.to_string(),
            positions: positions.to_vec(),
            effect_size: kl_divergence(&clean_output, &patched_output),
            output_change: (patched_output - clean_output).norm(),
        }
    }

    /// Scan all layers to find which ones causally matter for a specific output
    fn scan_all_layers(self: &Self) -> Vec<PatchEffect> {
        self.clean_cache.activations.keys()
            .map(|layer| self.patch(layer, &(0..self.clean_cache.activations[layer].len()).collect()))
            .sorted_by(|a, b| b.effect_size.partial_cmp(&a.effect_size).unwrap())
            .collect()
    }
}
```

Files:
- `simplex-learning/src/interpret/mod.sx` — module root
- `simplex-learning/src/interpret/cache.sx` — ActivationCache
- `simplex-learning/src/interpret/patching.sx` — ActivationPatching
- `simplex-learning/src/interpret/attention.sx` — AttentionAnalyzer
- `simplex-learning/src/interpret/neuron.sx` — NeuronAnalyzer

### Phase 2: Circuit Extraction (~400 lines)

- **CircuitExtractor** — given a model and a behavior (input→output pair), find the
  minimal subnetwork (circuit) that implements that behavior
- **CircuitGraph** — represent extracted circuits as a graph of components with edge
  weights (importance)
- **CircuitPruning** — remove all components not in an identified circuit (surgical
  behavior removal)
- **SuperpositionDetector** — identify when a single neuron encodes multiple unrelated
  features (polysemanticity), which indicates the model is harder to interpret

```simplex
/// Extract the minimal circuit implementing a specific behavior
struct CircuitExtractor {
    threshold: f64,    // minimum effect size to include in circuit
    method: CircuitMethod,
}

enum CircuitMethod {
    ActivationPatching,  // patch each component, keep those with large effect
    EdgePatching,         // patch edges between components
    ACDC,                 // Automatic Circuit DisCovery
}

/// A computational circuit: the subnetwork implementing a behavior
struct Circuit {
    nodes: Vec<CircuitNode>,
    edges: Vec<CircuitEdge>,
    behavior: String,
    fidelity: f64,  // how well this circuit reproduces the full model's behavior
}

struct CircuitNode {
    component: String,  // "layer_2.head_3" or "layer_5.mlp"
    importance: f64,
}

impl CircuitExtractor {
    /// Find the circuit for a specific behavior
    fn extract(
        self: &Self,
        model: &dyn Model,
        behavior_examples: &[(Tensor, Tensor)],  // input → expected output
    ) -> Circuit {
        let mut circuit = Circuit::full(model);
        // Iteratively remove components with low causal effect
        loop {
            let least_important = circuit.nodes.iter()
                .min_by_key(|n| n.importance)
                .unwrap();
            if least_important.importance > self.threshold {
                break;  // all remaining components are important
            }
            circuit.remove(least_important);
            circuit.recompute_importance(model, behavior_examples);
        }
        circuit
    }
}
```

Files:
- `simplex-learning/src/interpret/circuit.sx` — CircuitExtractor and Circuit
- `simplex-learning/src/interpret/circuit_graph.sx` — CircuitGraph representation
- `simplex-learning/src/interpret/circuit_pruning.sx` — circuit-based surgical pruning
- `simplex-learning/src/interpret/superposition.sx` — SuperpositionDetector

### Phase 3: Probing & Annotation Integration (~350 lines)

- **ProbeLayer** — linear probe trained on intermediate representations to read out
  specific features (e.g., "does this layer encode part-of-speech?")
- **@interpretable annotation** — compiler inserts activation caching hooks automatically
- **InterpretabilityReport** — structured output summarizing circuits, attention patterns,
  and probe results for a model
- **ContractVerifier** — verify that a neural gate's contract is enforced by its internal
  circuit, not just by training distribution

```simplex
/// Linear probe to read out features from intermediate representations
struct ProbeLayer<T> {
    linear: Linear,
    target_feature: String,
    layer_name: String,
    accuracy: f64,
}

impl<T> ProbeLayer<T> {
    /// Train a probe on cached activations
    fn train(
        layer_name: &str,
        feature: &str,
        activations: &[Tensor],
        labels: &[T],
    ) -> Self {
        let linear = Linear::new(activations[0].len(), num_classes::<T>());
        // Train simple linear classifier on activations → labels
        let accuracy = train_linear_probe(&linear, activations, labels);
        Self { linear, target_feature: feature.to_string(), layer_name: layer_name.to_string(), accuracy }
    }

    /// Does this layer encode this feature?
    fn is_encoded(self: &Self) -> bool {
        self.accuracy > 0.7  // above-chance linear readout = feature is encoded
    }
}

/// Verify a neural gate's contract is mechanistically enforced
struct ContractVerifier {
    gate: &dyn NeuralGate,
    circuit: Circuit,
}

impl ContractVerifier {
    /// Check if the gate's requires/ensures contracts are backed by circuits
    fn verify(self: &Self, test_inputs: &[Tensor]) -> VerificationResult {
        // For each contract clause, check if relevant circuit components
        // are active when the clause should fire
        let mut results = Vec::new();
        for input in test_inputs {
            let activations = ActivationCache::capture(self.gate.model(), input);
            let contract_satisfied = self.gate.check_contract(input);
            let circuit_active = self.circuit.is_active(&activations);
            results.push(VerificationResult {
                contract_satisfied,
                circuit_active,
                consistent: contract_satisfied == circuit_active,
            });
        }
        aggregate_verification(results)
    }
}
```

Files:
- `simplex-learning/src/interpret/probe.sx` — ProbeLayer
- `simplex-learning/src/interpret/annotation.sx` — @interpretable support
- `simplex-learning/src/interpret/report.sx` — InterpretabilityReport
- `simplex-learning/src/interpret/contract_verify.sx` — ContractVerifier

### Phase 4: Tests (~350 lines)

Location: `tests/interpret/`

- Activation patching identifies known-important layers on synthetic task
- Circuit extraction finds minimal circuit for modular arithmetic task
- Linear probes correctly detect feature encoding in intermediate layers
- SuperpositionDetector identifies polysemantic neurons in overparameterized model
- ContractVerifier catches contract violation (circuit doesn't enforce contract)
- @interpretable annotation produces correct activation cache

## Success Criteria

- [ ] Activation patching correctly ranks layer importance on synthetic copy task
- [ ] Circuit extraction recovers >90% of full model behavior with <30% of components
- [ ] Linear probes achieve >80% accuracy reading out known features from hidden layers
- [ ] SuperpositionDetector flags neurons with >2 unrelated activation patterns
- [ ] ContractVerifier detects planted contract violation in test neural gate
- [ ] Full interpretability pipeline runs in <10s for a 4-layer test model

## Estimated Scope

~1,600 lines across library code and tests.
