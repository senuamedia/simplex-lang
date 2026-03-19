# Real-Time Continuous Learning

**Version 0.15.0**

---

## Overview

The `simplex-learning` library enables AI specialists to learn and adapt during runtime without requiring offline batch training. This completes the vision of truly adaptive cognitive agents that improve through interaction.

---

## Architecture

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                              simplex-learning                                     │
├────────────┬────────────┬────────────┬──────────────┬────────────┬───────────────┤
│  tensor/   │  optim/    │  safety/   │ distributed/ │   dual/    │   belief/     │
├────────────┼────────────┼────────────┼──────────────┼────────────┼───────────────┤
│ Tensor     │StreamingSGD│SafetyBounds│FederatedLearn│ Dual       │ Confidence    │
│ DualTensor │StreamingAdam│Constraints│KnowledgeDistl│ Diff       │ Evidence      │
│ Autograd   │ AdamW      │SafeFallback│BeliefResolver│            │ Grounded      │
│ Ops        │ Scheduler  │ SafeLearner│HiveCoordinatr│            │ Provenance    │
│ Backends   │ Streaming  │ Zones      │ Sync         │            │ Scope         │
│ (CPU/GPU)  │            │            │              │            │ Timestamps    │
├────────────┼────────────┼────────────┼──────────────┼────────────┼───────────────┤
│calibration/│ epistemic/ │ feedback/  │   memory/    │  runtime/  │               │
├────────────┼────────────┼────────────┼──────────────┼────────────┤               │
│ Metrics    │Counterfact.│Attribution │ EWC          │OnlineLearner               │
│ Online     │ Dissent    │ Channel    │ Progressive  │ Checkpoint │               │
│ Temperature│ Integration│ Signals    │ ReplayBuffer │ Metrics    │               │
│            │MetaOptimizr│            │ RNG          │            │               │
│            │ Monitors   │            │              │            │               │
│            │ Schedule   │            │              │            │               │
│            │ Skeptic    │            │              │            │               │
└────────────┴────────────┴────────────┴──────────────┴────────────┘
```

---

## Core Components

### OnlineLearner

The central component for real-time learning:

```simplex
use simplex_learning::{OnlineLearner, StreamingAdam, SafeFallback};

// Create an online learner
let learner = OnlineLearner::new(model_params)
    .optimizer(StreamingAdam::new(0.001))
    .constraint(MaxLatency(10.0))
    .fallback(SafeFallback::with_default(default_output));

// Learn from each interaction
for (input, feedback) in interactions {
    let output = learner.forward(&input);
    learner.learn(&feedback);  // Adapts in real-time
}
```

---

## Tensor Operations

### Creating Tensors

```simplex
use simplex_learning::tensor::{Tensor, ops};

// Create tensors
let x = Tensor::zeros(&[32, 128]);
let y = Tensor::ones(&[128, 64]);
let z = Tensor::randn(&[32, 64]);

// Enable gradient tracking
let w = Tensor::randn(&[128, 64]).requires_grad_();
```

### Supported Operations

| Category | Operations |
|----------|------------|
| Creation | `zeros`, `ones`, `randn`, `from_slice` |
| Math | `add`, `sub`, `mul`, `div`, `matmul`, `batch_matmul` |
| Activations | `relu`, `sigmoid`, `tanh`, `softmax`, `gelu` |
| Loss | `mse_loss`, `cross_entropy`, `huber_loss` |
| Reductions | `sum`, `mean`, `max`, `min` |
| Shape | `reshape`, `transpose`, `squeeze`, `unsqueeze` |

### Automatic Differentiation

```simplex
let x = Tensor::randn(&[32, 128]).requires_grad_();
let w = Tensor::randn(&[128, 64]).requires_grad_();

let y = ops::matmul(&x, &w);
let loss = ops::mse_loss(&y, &target);

// Backpropagate
loss.backward();

// Access gradients
let grad_w = w.grad();  // Tensor of same shape as w
```

---

## Dual Numbers (v0.8.0)

Simplex provides native **dual numbers** for forward-mode automatic differentiation. Dual numbers extend real numbers with an infinitesimal component (ε), encoding the chain rule directly into arithmetic.

### The Dual Number Type

A dual number has the form:

```
a + bε    where ε² = 0
```

- **a** is the value (the function output)
- **b** is the derivative (the sensitivity to the input)

### Creating Dual Numbers

```simplex
use simplex::dual::dual;

// Constant: value only, derivative = 0
let c = dual::constant(3.0);     // 3 + 0ε

// Variable: value with derivative seed = 1
let x = dual::variable(3.0);     // 3 + 1ε

// Explicit construction
let d = dual::new(3.0, 1.0);     // 3 + 1ε
```

### Arithmetic Operations

Dual numbers propagate derivatives automatically through the chain rule:

```simplex
// Addition: (a + bε) + (c + dε) = (a+c) + (b+d)ε
let sum = x + y;

// Multiplication: (a + bε)(c + dε) = ac + (ad + bc)ε
let product = x * y;

// Division: (a + bε)/(c + dε) = a/c + (bc - ad)/c²ε
let quotient = x / y;
```

### Transcendental Functions

All common mathematical functions are differentiable:

```simplex
let x = dual::variable(3.0);

x.sin()      // sin(x), derivative: cos(x)
x.cos()      // cos(x), derivative: -sin(x)
x.exp()      // e^x, derivative: e^x
x.ln()       // ln(x), derivative: 1/x
x.sqrt()     // √x, derivative: 1/(2√x)
x.tanh()     // tanh(x), derivative: 1 - tanh²(x)
x.sigmoid()  // σ(x), derivative: σ(x)(1-σ(x))
```

### Computing Derivatives

```simplex
use simplex::diff::derivative;

// Define a function using dual numbers
fn f(x: dual) -> dual {
    x * x + x.sin()
}

// Compute f'(3)
let df = derivative(f, 3.0);  // Exact derivative, not numerical approximation
```

### Multi-Dimensional Gradients

Use `multidual<N>` to compute all N partial derivatives in a single forward pass:

```simplex
use simplex::dual::multidual;

// Compute gradient of f(x,y,z) = x²y + sin(z)
fn gradient_example(x: f64, y: f64, z: f64) -> [f64; 3] {
    let dx = multidual::<3>::variable(x, 0);  // ∂/∂x
    let dy = multidual::<3>::variable(y, 1);  // ∂/∂y
    let dz = multidual::<3>::variable(z, 2);  // ∂/∂z

    let result = dx * dx * dy + dz.sin();
    result.gradient()  // Returns [2xy, x², cos(z)]
}
```

### Second-Order Derivatives (Hessians)

Use `dual2` for second derivatives:

```simplex
use simplex::dual::dual2;

let x = dual2::variable(3.0);
let y = x * x * x;  // x³

y.val   // 27 (function value)
y.d1    // 27 (first derivative: 3x²)
y.d2    // 18 (second derivative: 6x)
```

### Zero-Overhead Abstraction

Dual numbers compile to the same assembly as hand-written derivative code:

```simplex
// Source code
let x = dual::variable(3.0)
let y = x * x
print(y.der)

// Conceptual compilation (struct eliminated)
let x_val = 3.0
let x_der = 1.0
let y_val = x_val * x_val
let y_der = 2.0 * x_val * x_der  // = 6.0
print(y_der)
```

### Forward vs Reverse Mode

| Aspect | Forward-Mode (Dual Numbers) | Reverse-Mode (Backprop) |
|--------|----------------------------|------------------------|
| Computes | Jacobian-vector product (Jv) | Vector-Jacobian product (vᵀJ) |
| Complexity | O(n) for n inputs | O(m) for m outputs |
| Memory | Constant (no graph storage) | O(computation depth) |
| Best for | Few inputs, many outputs | Many inputs, few outputs |
| Use case | Sensitivity analysis, Jacobians | Neural network training |

Simplex supports both modes. The `@differentiable` annotation lets the compiler choose automatically.

---

## Streaming Optimizers

### StreamingSGD

```simplex
use simplex_learning::optim::StreamingSGD;

let sgd = StreamingSGD::new(0.01)
    .momentum(0.9)
    .weight_decay(0.0001)
    .max_grad_norm(1.0);  // Automatic gradient clipping

sgd.step(&mut params);
```

### StreamingAdam

```simplex
use simplex_learning::optim::StreamingAdam;

let adam = StreamingAdam::new(0.001)
    .betas(0.9, 0.999)
    .eps(1e-8)
    .accumulation_steps(4);  // Mini-batch accumulation

adam.step(&mut params);
```

### AdamW

```simplex
use simplex_learning::optim::AdamW;

let adamw = AdamW::new(0.001)
    .betas(0.9, 0.999)
    .weight_decay(0.01);  // Decoupled weight decay

adamw.step(&mut params);
```

### Gradient Clipping

```simplex
use simplex_learning::optim::{clip_grad_norm, clip_grad_value};

// Clip by norm (scales all gradients proportionally)
clip_grad_norm(&mut params, 1.0);

// Clip by value (clamps each gradient independently)
clip_grad_value(&mut params, 0.5);
```

---

## Safety Constraints

### Constraint Types

```simplex
use simplex_learning::safety::{ConstraintManager, MaxLatency, NoLossExplosion};

let constraints = ConstraintManager::new()
    .add_soft(MaxLatency("latency", 10.0, penalty_weight: 0.5))
    .add_hard(NoLossExplosion("loss", 100.0));
```

| Constraint Type | Behavior |
|-----------------|----------|
| Soft | Adds penalty to loss, doesn't block |
| Hard | Blocks update if violated |

### SafeFallback Strategies

```simplex
use simplex_learning::safety::SafeFallback;

// Return predefined safe output
let fallback = SafeFallback::with_default(safe_output);

// Return last successful output
let fallback = SafeFallback::last_good();

// Execute custom fallback logic
let fallback = SafeFallback::with_function(|input| {
    compute_safe_result(input)
});

// Restore from checkpoint
let fallback = SafeFallback::checkpoint("model_backup.ckpt");

// Skip the update, continue without learning
let fallback = SafeFallback::skip_update();
```

### SafeLearner

```simplex
use simplex_learning::safety::SafeLearner;

let safe_learner = SafeLearner::new(learner, SafeFallback::with_default(safe_output))
    .with_validator(|output| output.is_valid())
    .max_failures(3);

match safe_learner.try_process(&input, compute_fn) {
    Ok(output) => use_output(output),
    Err(SafetyError::NoFallbackAvailable { failures }) => {
        log_error(f"Failed after {failures} attempts");
    }
}
```

---

## Federated Learning

### FederatedLearner

```simplex
use simplex_learning::distributed::{FederatedLearner, FederatedConfig};

let config = FederatedConfig {
    aggregation: AggregationStrategy::FedAvg,
    min_nodes: 3,
    staleness_threshold: 5,
};

let federated = FederatedLearner::new(config, initial_params);

// Specialists submit updates
federated.submit_update(NodeUpdate {
    node_id: "specialist_1",
    params: local_params,
    sample_count: 100,
    validation_acc: 0.85,
});

// Aggregation happens when min_nodes reached
let global_params = federated.global_params();
```

### Aggregation Strategies

| Strategy | Description | Use Case |
|----------|-------------|----------|
| `FedAvg` | Simple averaging | Homogeneous data |
| `WeightedAvg` | Weighted by sample count | Varying dataset sizes |
| `PerformanceWeighted` | Weighted by validation accuracy | Quality-focused |
| `Median` | Byzantine-resilient median | Adversarial settings |
| `TrimmedMean` | Trimmed mean (top/bottom 10%) | Outlier robustness |
| `AttentionWeighted` | Similarity-weighted | Heterogeneous specialists |

---

## Knowledge Distillation

### Teacher-Student

```simplex
use simplex_learning::distributed::{KnowledgeDistiller, DistillationConfig};

let config = DistillationConfig {
    temperature: 2.0,
    alpha: 0.5,  // Balance hard vs soft labels
};

let distiller = KnowledgeDistiller::new(config);

// Compute combined loss
let loss = distiller.distillation_loss(
    &student_logits,
    &teacher_logits,
    &hard_labels,
);
```

### Self-Distillation

```simplex
use simplex_learning::distributed::SelfDistillation;

let config = SelfDistillConfig {
    ema_decay: 0.999,
    temperature: 1.0,
};

let self_distill = SelfDistillation::new(config, initial_params);

// Update EMA teacher after each step
self_distill.update_ema(&current_params);

// Get soft targets from EMA teacher
let soft_targets = self_distill.teacher_forward(&input);
```

---

## Belief Conflict Resolution

### HiveBeliefManager

```simplex
use simplex_learning::distributed::{HiveBeliefManager, ConflictResolution};

let manager = HiveBeliefManager::new(ConflictResolution::BayesianCombination);

// Specialists submit beliefs
manager.submit_belief(Belief::new("user_prefers_concise", 0.8, "specialist_1"));
manager.submit_belief(Belief::new("user_prefers_concise", 0.6, "specialist_2"));

// Get consensus
let consensus = manager.consensus("user_prefers_concise");
// Returns combined confidence considering both sources
```

### Resolution Strategies

| Strategy | Description |
|----------|-------------|
| `HighestConfidence` | Use most confident belief |
| `MostRecent` | Use newest belief |
| `MostEvidence` | Use belief with most supporting evidence |
| `EvidenceWeighted` | Weighted average by evidence count |
| `BayesianCombination` | Log-odds combination |
| `SemanticWeighted` | Weighted by embedding similarity |
| `MajorityVote` | Democratic voting |
| `Custom(fn)` | User-defined resolution |

---

## Hive Learning Coordinator

Orchestrates all learning components across a hive:

```simplex
use simplex_learning::distributed::{HiveLearningCoordinator, HiveLearningConfig};

let config = HiveLearningConfigBuilder::new()
    .sync_interval(100)           // Sync every 100 steps
    .checkpoint_interval(1000)    // Checkpoint every 1000 steps
    .aggregation(AggregationStrategy::FedAvg)
    .belief_resolution(ConflictResolution::EvidenceWeighted)
    .enable_distillation(true)
    .build();

let coordinator = HiveLearningCoordinator::new(config, initial_params);

// Register specialists
coordinator.register_specialist("security", security_params);
coordinator.register_specialist("quality", quality_params);
coordinator.register_specialist("performance", perf_params);

// In training loop
coordinator.submit_gradients("security", &gradients);
coordinator.submit_belief("security", belief);

// Automatic sync and aggregation
coordinator.step();  // Handles sync, aggregation, checkpointing

// Get synchronized parameters
let params = coordinator.get_specialist_params("security");
```

---

## Checkpointing

### Manual Checkpointing

```simplex
use simplex_learning::runtime::Checkpoint;

// Save checkpoint
Checkpoint::save("model_v1.ckpt", &learner)?;

// Restore checkpoint
let learner = Checkpoint::load("model_v1.ckpt")?;
```

### Automatic Checkpointing

```simplex
let learner = OnlineLearner::new(params)
    .checkpoint_every(1000)
    .checkpoint_path("checkpoints/");
```

---

## Metrics and Monitoring

```simplex
use simplex_learning::runtime::Metrics

let metrics = learner.metrics()

print("Loss: {metrics.loss}")
print("Learning rate: {metrics.lr}")
print("Gradient norm: {metrics.grad_norm}")
print("Steps: {metrics.step_count}")
print("Throughput: {metrics.samples_per_sec} samples/sec")
```

---

## Integration with Specialists

```simplex
use simplex_learning::{OnlineLearner, SafeFallback};

specialist SecurityAnalyzer {
    model: "simplex-cognitive-7b";
    learner: OnlineLearner;

    fn init() {
        self.learner = OnlineLearner::new(self.params())
            .optimizer(StreamingAdam::new(0.001))
            .fallback(SafeFallback::with_default(Analysis::unknown()));
    }

    fn analyze(code: String) -> Analysis {
        let result = infer("Analyze: " + code);
        result
    }

    fn feedback(analysis: Analysis, correct: bool) {
        // Learn from user feedback
        let signal = FeedbackSignal::from_binary(correct);
        self.learner.learn(&signal);
    }
}
```

---

## Performance

| Operation | Throughput | Latency |
|-----------|------------|---------|
| Forward pass (128-dim) | 50K/sec | 20us |
| Backward pass | 25K/sec | 40us |
| Gradient clipping | 100K/sec | 10us |
| FedAvg aggregation | 10K params/ms | <1ms |
| Checkpoint save | 1M params/sec | <10ms |

---

## Best Practices

### When to Use Online Learning

**Good candidates:**
- User preference adaptation
- Dynamic environment response
- Continuous improvement from feedback
- Personalization

**Poor candidates:**
- Safety-critical decisions (use verified models)
- Stable, well-understood tasks
- Limited feedback signal

### Safety Guidelines

1. Always use SafeFallback for production
2. Set reasonable constraint bounds
3. Monitor gradient norms and loss values
4. Use checkpointing for recovery
5. Test fallback paths thoroughly

---

## See Also

- [Neural IR](14-neural-ir.md) - Differentiable execution
- [Cognitive Hive AI](09-cognitive-hive.md) - SLM integration
- [The Anima](12-anima.md) - Cognitive agents
- [RELEASE-0.7.0.md](../RELEASE-0.7.0.md) - Full release notes
