# TASK-007: Rebuild Training Pipeline in Pure Simplex

**Status:** Complete (verified)
**Priority:** High
**Target Release:** v0.9.2
**Dependencies:** TASK-005 (Dual Numbers - Complete), TASK-006 (Self-Learning Annealing - Complete)
**Estimated Complexity:** Large
**Updated**: 2026-03-16 (audited against codebase)

> **Audit Note (2026-03-16):** All 7 phases verified complete with 4,225 lines of tests.
> Two simplex-training libraries exist:
> 1. `simplex-training/` (48 files) — full pure Simplex training pipeline
> 2. `simplex-training/` (17 files) — minimal AWS Inferentia deployment variant
>
> Key locations: tensor ops, DualTensor, dual-mode neural gates, soft logic,
> temp-aware attention, 6 specialist data generators, LoRA (layer/config/adapter),
> neural layers (linear/attention/embedding/norm), specialist/batch/anneal pipelines.
> Tests: unit_tensor_ops, unit_lora, unit_attention, unit_generators, unit_annealing,
> unit_neural_gates, integration_pipeline (all in `tests/training/`).

---

## Overview

Convert the Python-based specialist training pipeline (`/Users/rod/code/simplex/training/`) to a pure Simplex implementation using self-learning annealing and dual-mode neural gates. This eliminates the Python/PyTorch dependency and enables fully native training within the Simplex ecosystem.

---

## Current State Analysis

### Python Training Code (`training/`)

The Python implementation includes:

| Component | Files | Description |
|-----------|-------|-------------|
| **Specialist Catalog** | `configs/specialists_catalog.yaml` | 52+ specialist definitions across 15 categories |
| **Data Generation** | `scripts/generate_*.py` | Synthetic data generators using Faker |
| **Training Scripts** | `scripts/train_*.py` | LoRA fine-tuning with HuggingFace |
| **Neural IR Training** | `scripts/train_neural_ir_gates.py` | Temperature-aware, soft logic training |
| **Evaluation** | `scripts/evaluate_model.py` | Metrics computation |
| **Export** | `scripts/export_to_gguf.py` | GGUF conversion |
| **AWS Infrastructure** | `aws/`, `infrastructure/` | EC2 GPU provisioning |

**Key Python Dependencies:**
- PyTorch
- HuggingFace Transformers
- PEFT (LoRA adapters)
- Datasets
- Faker (synthetic data)
- bitsandbytes (quantization)

### simplex-learning Library (`simplex-learning/`)

Implemented in v0.9.0:

| Component | Files | Status |
|-----------|-------|--------|
| **Tensor Type** | `tensor/tensor.sx` | Complete - shape tracking, requires_grad, grad storage |
| **Tensor Ops** | `tensor/ops.sx` | Complete - add, sub, mul, div, matmul (with batch), scale |
| **Activations** | `tensor/ops.sx` | Complete - relu, sigmoid, tanh, gelu, softmax, log_softmax |
| **Normalization** | `tensor/ops.sx` | Complete - layer_norm |
| **Loss Functions** | `tensor/ops.sx` | Complete - mse_loss, cross_entropy_loss, binary_cross_entropy |
| **Autograd** | `tensor/autograd.sx` | Complete - backward pass, gradient accumulation |
| **Dual Numbers** | `dual/dual.sx`, `dual/diff.sx` | Complete - forward-mode AD |
| **Optimizers** | `optim/adam.sx`, `optim/sgd.sx` | Complete - Adam, SGD with schedulers |
| **Memory** | `memory/` | Complete - replay buffer, EWC, progressive learning |
| **Distributed** | `distributed/` | Complete - federated, distillation, beliefs |
| **Calibration** | `calibration/` | Complete - temperature scaling, metrics |
| **Safety** | `safety/` | Complete - constraints, bounds, fallback |

### simplex-training Library (`simplex-training/`)

Implemented in v0.9.0:

| Component | Files | Status |
|-----------|-------|--------|
| **LR Schedule** | `schedules/lr.sx` | Complete - learnable with dual numbers |
| **Distillation** | `schedules/distill.sx` | Complete - temperature schedules |
| **Pruning** | `schedules/prune.sx`, `compress/pruning.sx` | Complete - structured pruning |
| **Quantization** | `schedules/quant.sx`, `compress/quantization.sx` | Complete - mixed-precision |
| **Curriculum** | `schedules/curriculum.sx` | Complete - learnable curriculum |
| **Meta-Trainer** | `trainer/meta.sx` | Complete - meta-gradient optimization |
| **Specialist Trainer** | `trainer/specialist.sx` | Partial - needs model architectures |
| **Data Loader** | `data/loader.sx` | Complete - batch loading |
| **Data Generators** | `data/generators.sx` | Partial - generic only |
| **GGUF Export** | `export/gguf.sx` | Complete |

### Self-Learning Annealing (`simplex::optimize::anneal`)

Implemented in v0.9.0:

| Component | Status |
|-----------|--------|
| **LearnableSchedule** | Complete - all parameters as dual numbers |
| **MetaOptimizer** | Complete - learns schedule via meta-gradients |
| **Soft Acceptance** | Complete - differentiable accept/reject |
| **self_learn_anneal()** | Complete - convenience API |

**All Components Complete for v0.9.2:**
- ~~DualTensor type (tensor with dual number elements, not f64)~~ IMPLEMENTED
- ~~Neural IR gate training integration~~ IMPLEMENTED
- ~~Model architecture definitions (Linear, MultiHeadAttention, Embedding layers)~~ IMPLEMENTED
- ~~LoRA implementation~~ IMPLEMENTED
- ~~Specialist-specific data generators (document, coding, sentiment, etc.)~~ IMPLEMENTED
- ~~Full end-to-end training pipeline integration~~ IMPLEMENTED

---

## Target Architecture

### Pure Simplex Training Stack

```
┌─────────────────────────────────────────────────────────────────┐
│                    simplex-training                              │
├─────────────────────────────────────────────────────────────────┤
│  Training Pipeline                                               │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐    │
│  │ DataGen   │→│ Training  │→│ Compress  │→│ Export    │    │
│  │ (Native)  │  │ (Dual)    │  │ (Prune/   │  │ (GGUF)   │    │
│  └───────────┘  │           │  │  Quant)   │  └───────────┘    │
│                 └───────────┘  └───────────┘                    │
├─────────────────────────────────────────────────────────────────┤
│  Self-Learning Schedules (TASK-006)                             │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ LearnableLR │ LearnableDistill │ LearnablePrune │ Quant    ││
│  │ (meta-grad) │ (temp schedule)  │ (importance)   │ (bits)   ││
│  └─────────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────────┤
│  Neural IR Integration (Neural Gates)                           │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ soft_gate() │ dual_mode()     │ @differentiable           ││
│  │ (Gumbel)    │ (inference/     │ (forward-mode AD)         ││
│  │             │  training)      │                            ││
│  └─────────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────────┤
│  Tensor Operations (simplex-learning)                           │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ Tensor     │ matmul()        │ backward()    │ optim      ││
│  │ (dual)     │ softmax()       │ (autograd)    │ (Adam)     ││
│  └─────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
```

---

## Implementation Plan

### Phase 1: Core Tensor Operations COMPLETE (v0.9.0)

**Status:** Complete in simplex-learning library

The core Tensor type and operations are implemented in `simplex-learning/src/tensor/`:
- Tensor construction (zeros, ones, rand, randn, scalar)
- Shape management and broadcasting
- Element-wise operations (add, sub, mul, div)
- Matrix multiplication with batch support
- Activation functions (relu, sigmoid, tanh, gelu, softmax)
- Layer normalization
- Loss functions (MSE, cross-entropy, binary cross-entropy)
- Autograd support (requires_grad, backward pass)

### Phase 1.5: DualTensor Type COMPLETE (v0.9.2)

**Status:** Complete in `simplex-learning/src/tensor/dual_tensor.sx`

DualTensor provides tensors where each element is a dual number, enabling forward-mode AD:
- Construction: zeros, ones, variable, variable_at, from_tensor, from_values
- Element-wise: add, sub, mul, div, neg, mul_scalar, add_scalar
- Reductions: sum, mean, max, min, sum_axis, mean_axis
- Activations: relu, sigmoid, tanh, gelu, exp, ln, sqrt, pow, softmax, log_softmax
- Matrix ops: matmul (2D and batched), transpose
- Shape ops: reshape, flatten, unsqueeze, squeeze
- Loss functions: mse_loss, cross_entropy_loss, binary_cross_entropy
- Layer norm: layer_norm with epsilon
- Conversion: to_tensor, derivative_tensor, values, derivatives

Unit tests: `tests/learning/unit_dual_tensor.sx` (30+ tests)

**Objective (for reference):** Implement tensor operations with dual number elements for forward-mode automatic differentiation.

#### 1.1 Tensor Type with Dual Numbers

```simplex
// simplex-training/src/tensor/mod.sx

/// Tensor with dual number elements for forward-mode AD
pub struct DualTensor {
    /// Shape of tensor
    shape: Vec<usize>,
    /// Data as dual numbers
    data: Vec<dual>,
    /// Gradient storage (for backward compatibility)
    grad: Option<Vec<f64>>,
}

impl DualTensor {
    pub fn zeros(shape: &[usize]) -> DualTensor;
    pub fn ones(shape: &[usize]) -> DualTensor;
    pub fn rand(shape: &[usize]) -> DualTensor;

    /// Create from f64 values (constants)
    pub fn from_values(shape: &[usize], data: &[f64]) -> DualTensor;

    /// Create variable tensor (seed = 1 for each element)
    pub fn variable(shape: &[usize], data: &[f64]) -> DualTensor;

    /// Element-wise operations
    pub fn add(&self, other: &DualTensor) -> DualTensor;
    pub fn mul(&self, other: &DualTensor) -> DualTensor;
    pub fn div(&self, other: &DualTensor) -> DualTensor;

    /// Matrix operations
    pub fn matmul(&self, other: &DualTensor) -> DualTensor;
    pub fn transpose(&self) -> DualTensor;

    /// Activations
    pub fn relu(&self) -> DualTensor;
    pub fn sigmoid(&self) -> DualTensor;
    pub fn tanh(&self) -> DualTensor;
    pub fn softmax(&self, dim: i64) -> DualTensor;
    pub fn gelu(&self) -> DualTensor;

    /// Reductions
    pub fn sum(&self) -> dual;
    pub fn mean(&self) -> dual;
    pub fn norm(&self) -> dual;

    /// Extract gradients
    pub fn gradient(&self) -> Vec<f64>;
}
```

#### 1.2 Linear Layer

```simplex
// simplex-training/src/layers/linear.sx

pub struct Linear {
    weight: DualTensor,  // [out_features, in_features]
    bias: Option<DualTensor>,  // [out_features]
    in_features: usize,
    out_features: usize,
}

impl Linear {
    pub fn new(in_features: usize, out_features: usize, bias: bool) -> Linear;

    pub fn forward(&self, input: &DualTensor) -> DualTensor {
        let output = input.matmul(&self.weight.transpose());
        if let Some(ref b) = self.bias {
            output.add(b)
        } else {
            output
        }
    }

    pub fn parameters(&self) -> Vec<&DualTensor>;
    pub fn gradients(&self) -> Vec<Vec<f64>>;
}
```

#### 1.3 Attention Mechanism

```simplex
// simplex-training/src/layers/attention.sx

pub struct MultiHeadAttention {
    num_heads: usize,
    head_dim: usize,
    q_proj: Linear,
    k_proj: Linear,
    v_proj: Linear,
    out_proj: Linear,
    scale: dual,
}

impl MultiHeadAttention {
    pub fn new(embed_dim: usize, num_heads: usize) -> MultiHeadAttention;

    pub fn forward(&self, query: &DualTensor, key: &DualTensor, value: &DualTensor,
                   mask: Option<&DualTensor>) -> DualTensor {
        let q = self.q_proj.forward(query);
        let k = self.k_proj.forward(key);
        let v = self.v_proj.forward(value);

        // Scaled dot-product attention
        let scores = q.matmul(&k.transpose()) * self.scale;
        let weights = if let Some(m) = mask {
            scores.add(m).softmax(-1)
        } else {
            scores.softmax(-1)
        };

        let attn_output = weights.matmul(&v);
        self.out_proj.forward(&attn_output)
    }
}
```

### Phase 2: Self-Learning Annealing Integration COMPLETE (v0.9.0)

**Status:** Complete in `simplex::optimize::anneal` module

Self-learning annealing with meta-gradients is fully implemented:
- LearnableSchedule with all parameters as dual numbers
- MetaOptimizer for schedule learning
- Soft acceptance function (differentiable)
- Convenience API: `self_learn_anneal()`

**Objective (for reference):** Integrate self-learning annealing for hyperparameter optimization.

#### 2.1 Training with Annealing

```simplex
// simplex-training/src/anneal/training.sx

use simplex::optimize::anneal::{LearnableSchedule, MetaOptimizer};

/// Training state for annealing-based optimization
pub struct AnnealTrainingState<M> {
    model: M,
    schedule: LearnableSchedule,
    meta_optimizer: MetaOptimizer,
    step: u64,
    epoch: u64,
    best_loss: f64,
}

impl<M: Model> AnnealTrainingState<M> {
    /// Create new training state
    pub fn new(model: M) -> AnnealTrainingState<M> {
        AnnealTrainingState {
            model,
            schedule: LearnableSchedule::new(),
            meta_optimizer: MetaOptimizer::new(LearnableSchedule::new()),
            step: 0,
            epoch: 0,
            best_loss: f64::MAX,
        }
    }

    /// Run one training step with annealed learning rate
    pub fn step(&mut self, batch: &Batch) -> StepResult {
        // Get temperature from learned schedule
        let temp = self.schedule.temperature(
            dual::constant(self.step as f64),
            dual::constant(self.stagnation_steps() as f64),
        );

        // Compute loss with soft acceptance
        let loss = self.model.forward(batch);
        let delta_loss = loss.val - self.best_loss;

        // Soft acceptance (differentiable)
        let accept_prob = self.schedule.accept_probability(
            dual::constant(delta_loss),
            temp,
        );

        // Update model parameters
        let lr = self.schedule.learning_rate(self.step);
        self.model.update_parameters(lr * accept_prob.val);

        // Update meta-optimizer (learn schedule)
        if self.step % 100 == 0 {
            let meta_grad = self.schedule.gradient();
            self.meta_optimizer.step(&meta_grad);
        }

        self.step += 1;
        StepResult::new(loss, self.step, lr)
    }
}
```

#### 2.2 Annealed Hyperparameter Search

```simplex
// simplex-training/src/anneal/hyperparam.sx

/// Hyperparameter configuration as annealing state
pub struct HyperConfig {
    learning_rate: dual,
    batch_size: dual,
    lora_rank: dual,
    dropout: dual,
    weight_decay: dual,
}

/// Find optimal hyperparameters using self-learning annealing
pub fn find_optimal_hyperparams<M: Model>(
    model_builder: impl Fn(&HyperConfig) -> M,
    train_data: &Dataset,
    val_data: &Dataset,
) -> HyperConfig {
    let objective = |config: &HyperConfig| -> f64 {
        let model = model_builder(config);
        let trainer = AnnealTrainingState::new(model);

        // Quick training run
        for _ in 0..100 {
            trainer.step(&train_data.sample_batch(config.batch_size.val as usize));
        }

        // Validation loss
        model.evaluate(val_data)
    };

    self_learn_anneal(
        objective,
        HyperConfig::default(),
        HyperConfig::neighbor,
        AnnealConfig::for_hyperparam_search(),
    )
}
```

### Phase 3: Dual-Mode Neural Gates COMPLETE (v0.9.2)

**Status:** Complete in `simplex-training/src/neural/`

Implemented components:
- `gate.sx`: DualGate, GateMode (Training/Inference/Hybrid), GumbelSoftmax, StraightThroughEstimator, MultiGate
- `soft_logic.sx`: SoftLogic (AND, OR, NOT, XOR, IMPLIES, IFF), SoftLogicGate, LogicCircuit
- `temperature_attention.sx`: TemperatureAttention, LearnableTemperature

**Objective (for reference):** Implement neural gates that operate in training (soft) and inference (hard) modes.

#### 3.1 Dual-Mode Gate

```simplex
// simplex-training/src/neural/gate.sx

/// Neural gate operating mode
pub enum GateMode {
    /// Training: soft outputs with gradients
    Training { temperature: f64 },
    /// Inference: hard binary decisions
    Inference,
    /// Hybrid: soft with straight-through estimator
    Hybrid { temperature: f64 },
}

/// Dual-mode neural gate
pub struct DualGate {
    mode: GateMode,
    threshold: dual,
    temperature: dual,
}

impl DualGate {
    pub fn new(threshold: f64) -> DualGate {
        DualGate {
            mode: GateMode::Training { temperature: 1.0 },
            threshold: dual::variable(threshold),
            temperature: dual::variable(1.0),
        }
    }

    /// Forward pass through gate
    pub fn forward(&self, input: dual) -> dual {
        match self.mode {
            GateMode::Training { temperature } => {
                // Soft gate: sigmoid with temperature
                let scaled = (input - self.threshold) / dual::constant(temperature);
                scaled.sigmoid()
            }
            GateMode::Inference => {
                // Hard gate: binary decision
                if input.val >= self.threshold.val {
                    dual::constant(1.0)
                } else {
                    dual::constant(0.0)
                }
            }
            GateMode::Hybrid { temperature } => {
                // Straight-through estimator
                let soft = ((input - self.threshold) / dual::constant(temperature)).sigmoid();
                let hard = if input.val >= self.threshold.val { 1.0 } else { 0.0 };
                // Forward: hard, Backward: soft gradient
                dual::new(hard, soft.der)
            }
        }
    }

    pub fn set_mode(&mut self, mode: GateMode) {
        self.mode = mode;
    }
}
```

#### 3.2 Temperature-Aware Attention

```simplex
// simplex-training/src/neural/temperature_attention.sx

/// Attention with learnable temperature
pub struct TemperatureAttention {
    attention: MultiHeadAttention,
    temperature: LearnableSchedule,
    mode: GateMode,
}

impl TemperatureAttention {
    pub fn forward(&self, query: &DualTensor, key: &DualTensor, value: &DualTensor,
                   step: u64) -> DualTensor {
        // Get current temperature from learned schedule
        let temp = self.temperature.temperature(
            dual::constant(step as f64),
            dual::constant(0.0),
        );

        // Compute attention scores
        let scores = self.compute_scores(query, key);

        // Apply temperature-scaled softmax
        let scaled_scores = scores / temp.val;
        let weights = match self.mode {
            GateMode::Training { .. } => scaled_scores.softmax(-1),
            GateMode::Inference => self.hard_attention(&scaled_scores),
            GateMode::Hybrid { .. } => self.gumbel_softmax(&scaled_scores, temp.val),
        };

        weights.matmul(value)
    }

    fn gumbel_softmax(&self, logits: &DualTensor, temperature: f64) -> DualTensor {
        // Gumbel-Softmax for differentiable discrete sampling
        let gumbel_noise = self.sample_gumbel(logits.shape());
        let perturbed = logits.add(&gumbel_noise);
        (perturbed / dual::constant(temperature)).softmax(-1)
    }
}
```

#### 3.3 Soft Logic Gates

```simplex
// simplex-training/src/neural/soft_logic.sx

/// Differentiable logic operations
pub struct SoftLogic {
    temperature: dual,
}

impl SoftLogic {
    /// Soft AND: product of sigmoids
    pub fn and(&self, a: dual, b: dual) -> dual {
        let sig_a = (a / self.temperature).sigmoid();
        let sig_b = (b / self.temperature).sigmoid();
        sig_a * sig_b
    }

    /// Soft OR: 1 - (1-a)(1-b)
    pub fn or(&self, a: dual, b: dual) -> dual {
        let sig_a = (a / self.temperature).sigmoid();
        let sig_b = (b / self.temperature).sigmoid();
        dual::constant(1.0) - (dual::constant(1.0) - sig_a) * (dual::constant(1.0) - sig_b)
    }

    /// Soft NOT: 1 - sigmoid
    pub fn not(&self, a: dual) -> dual {
        dual::constant(1.0) - (a / self.temperature).sigmoid()
    }

    /// Soft XOR: a*(1-b) + (1-a)*b
    pub fn xor(&self, a: dual, b: dual) -> dual {
        let sig_a = (a / self.temperature).sigmoid();
        let sig_b = (b / self.temperature).sigmoid();
        sig_a * (dual::constant(1.0) - sig_b) + (dual::constant(1.0) - sig_a) * sig_b
    }
}
```

### Phase 4: Data Generation in Simplex COMPLETE (v0.9.2)

**Status:** Complete in `simplex-training/src/data/`

Implemented components:
- `generator.sx`: DataGenerator trait, TrainingExample, ExampleMetadata, Rng
- `loader.sx`: DataLoader, Batch, DataLoaderConfig, DatasetSplit
- `specialists/document.sx`: DocumentGenerator (invoices, receipts, contracts)
- `specialists/code.sx`: CodeGenerator (functions, bugfixes, multi-language)
- `specialists/sentiment.sx`: SentimentGenerator (balanced sentiment analysis)
- `specialists/reasoning.sx`: ReasoningGenerator (logic, math, common sense)
- `specialists/neural_ir.sx`: NeuralIRGenerator (temperature-aware, soft logic)
- `specialists/classification.sx`: ClassificationGenerator (configurable labels)

**Objective (for reference):** Implement synthetic data generators natively in Simplex.

#### 4.1 Data Generator Trait

```simplex
// simplex-training/src/data/generator.sx

/// Trait for generating training data
pub trait DataGenerator {
    /// Generate a single training example
    fn generate(&self) -> TrainingExample;

    /// Generate a batch of examples
    fn generate_batch(&self, size: usize) -> Vec<TrainingExample>;

    /// Specialist domain
    fn domain(&self) -> SpecialistDomain;
}

/// Training example
pub struct TrainingExample {
    pub prompt: String,
    pub response: String,
    pub metadata: ExampleMetadata,
}

pub struct ExampleMetadata {
    pub domain: String,
    pub confidence_target: f64,
    pub source: String,
}
```

#### 4.2 Specialist Data Generators

```simplex
// simplex-training/src/data/specialists/mod.sx

pub mod document;
pub mod coding;
pub mod sentiment;
pub mod reasoning;
pub mod neural_ir;

// Document extraction generator
pub struct DocumentExtractionGenerator {
    rng: Random,
    companies: Vec<String>,
    doc_types: Vec<String>,
}

impl DataGenerator for DocumentExtractionGenerator {
    fn generate(&self) -> TrainingExample {
        let doc_type = self.doc_types.random(&self.rng);
        let company = self.companies.random(&self.rng);
        let date = self.rng.date();
        let amount = self.rng.float(100.0, 10000.0);

        let prompt = format!(
            "<document type=\"{doc_type}\">\n\
             Company: {company}\n\
             Date: {date}\n\
             Total: ${amount:.2}\n\
             </document>\n\n\
             Extract all structured data from this document."
        );

        let response = format!(
            "Extracted data:\n\
             - Document Type: {doc_type}\n\
             - Company: {company}\n\
             - Date: {date}\n\
             - Total Amount: ${amount:.2}\n\
             - Confidence: High\n\n\
             [confidence: 0.92]"
        );

        TrainingExample {
            prompt,
            response,
            metadata: ExampleMetadata {
                domain: "document_extraction".to_string(),
                confidence_target: 0.92,
                source: "synthetic".to_string(),
            },
        }
    }

    fn domain(&self) -> SpecialistDomain {
        SpecialistDomain::Document
    }
}
```

#### 4.3 Neural IR Training Data

```simplex
// simplex-training/src/data/specialists/neural_ir.sx

/// Generator for Neural IR gate training data
pub struct NeuralIRGenerator {
    rng: Random,
}

impl DataGenerator for NeuralIRGenerator {
    fn generate(&self) -> TrainingExample {
        match self.rng.int(0, 3) {
            0 => self.generate_temperature_aware(),
            1 => self.generate_soft_logic(),
            2 => self.generate_probability_output(),
            _ => self.generate_confidence_calibration(),
        }
    }

    fn generate_temperature_aware(&self) -> TrainingExample {
        let temp = self.rng.choice(&[0.1, 0.3, 0.5, 0.7, 1.0, 2.0, 5.0]);
        let semantics = match temp {
            t if t <= 0.3 => "very deterministic",
            t if t <= 0.7 => "balanced",
            t if t <= 1.0 => "standard sampling",
            _ => "high exploration",
        };

        // Generate probability distribution based on temperature
        let probs = self.temperature_to_probs(temp);

        TrainingExample {
            prompt: format!(
                "You are operating at temperature {temp} ({semantics}).\n\n\
                 Task: Select between options for code review approach.\n\
                 Options: Approve, Request Changes, Comment Only\n\n\
                 Provide your selection with probability distribution."
            ),
            response: format!(
                "**Temperature Analysis**: {temp} ({semantics})\n\n\
                 **Probability Distribution**:\n\
                 Approve: {:.2}\n\
                 Request Changes: {:.2}\n\
                 Comment Only: {:.2}\n\n\
                 [confidence: {:.2}]",
                probs[0], probs[1], probs[2], probs.iter().max_by(|a, b| a.partial_cmp(b).unwrap()).unwrap()
            ),
            metadata: ExampleMetadata {
                domain: "neural_ir".to_string(),
                confidence_target: probs[0].max(probs[1]).max(probs[2]),
                source: "synthetic".to_string(),
            },
        }
    }

    fn generate_soft_logic(&self) -> TrainingExample {
        let confidence = self.rng.float(0.1, 0.95);
        let threshold = self.rng.choice(&[0.3, 0.5, 0.7, 0.8, 0.9]);
        let passes = confidence >= threshold;
        let margin = (confidence - threshold).abs();

        TrainingExample {
            prompt: format!(
                "Soft Logic Gate Evaluation:\n\n\
                 Input confidence: {confidence:.3}\n\
                 Threshold: {threshold:.2}\n\n\
                 Should this pass the gate?"
            ),
            response: format!(
                "**Soft Logic Gate Analysis**\n\n\
                 | Metric | Value |\n\
                 |--------|-------|\n\
                 | Input Confidence | {confidence:.3} |\n\
                 | Threshold | {threshold:.2} |\n\
                 | Margin | {}{margin:.3} |\n\
                 | Result | {} |\n\n\
                 **Gate Output**: {} (hard)\n\
                 **Soft Output**: {:.3} (continuous)\n\n\
                 [confidence: 0.99]",
                if passes { "+" } else { "-" },
                if passes { "PASS" } else { "FAIL" },
                if passes { 1.0 } else { 0.0 },
                (confidence / threshold).min(1.0)
            ),
            metadata: ExampleMetadata {
                domain: "neural_ir".to_string(),
                confidence_target: 0.99,
                source: "synthetic".to_string(),
            },
        }
    }
}
```

### Phase 5: LoRA Implementation COMPLETE (v0.9.2)

**Status:** Complete in `simplex-training/src/lora/`

Implemented components:
- `layer.sx`: LoRALayer (forward, merge, unmerge, trainable_parameters), QLoRALayer
- `config.sx`: LoRAConfig (simple, standard, complex, code presets), LoRAConfigBuilder
- `adapter.sx`: LoRAAdapter (multi-layer management), LoRAModel, LoRAModelBuilder

Supporting layers in `simplex-training/src/layers/`:
- `linear.sx`: Linear layer with bias support
- `attention.sx`: MultiHeadAttention, GroupedQueryAttention
- `embedding.sx`: Embedding, PositionalEmbedding, RotaryEmbedding, AlibiEmbedding
- `norm.sx`: LayerNorm, RMSNorm, GroupNorm, BatchNorm

**Objective (for reference):** Implement LoRA (Low-Rank Adaptation) natively in Simplex.

#### 5.1 LoRA Layer

```simplex
// simplex-training/src/lora/layer.sx

/// LoRA adapter for a linear layer
pub struct LoRALayer {
    /// Original frozen weights (not trained)
    base_weight: DualTensor,
    /// Low-rank matrix A: [in_features, rank]
    lora_a: DualTensor,
    /// Low-rank matrix B: [rank, out_features]
    lora_b: DualTensor,
    /// Scaling factor
    scaling: f64,
    /// Rank
    rank: usize,
    /// Dropout for training
    dropout: f64,
}

impl LoRALayer {
    pub fn new(in_features: usize, out_features: usize, rank: usize, alpha: f64) -> LoRALayer {
        LoRALayer {
            base_weight: DualTensor::zeros(&[out_features, in_features]),
            lora_a: DualTensor::rand_normal(&[in_features, rank], 0.0, 1.0 / rank as f64),
            lora_b: DualTensor::zeros(&[rank, out_features]),
            scaling: alpha / rank as f64,
            rank,
            dropout: 0.05,
        }
    }

    pub fn from_linear(linear: &Linear, rank: usize, alpha: f64) -> LoRALayer {
        var lora = LoRALayer::new(linear.in_features, linear.out_features, rank, alpha);
        lora.base_weight = linear.weight.clone();
        lora
    }

    pub fn forward(&self, input: &DualTensor) -> DualTensor {
        // Base output (frozen)
        let base_out = input.matmul(&self.base_weight.transpose());

        // LoRA output: input @ A @ B * scaling
        let lora_out = input
            .matmul(&self.lora_a)
            .matmul(&self.lora_b)
            .scale(self.scaling);

        // Combined
        base_out.add(&lora_out)
    }

    /// Get trainable parameters (only LoRA weights)
    pub fn trainable_parameters(&self) -> Vec<&DualTensor> {
        vec![&self.lora_a, &self.lora_b]
    }

    /// Merge LoRA into base weights (for inference)
    pub fn merge(&self) -> DualTensor {
        let lora_weight = self.lora_a.matmul(&self.lora_b).scale(self.scaling);
        self.base_weight.add(&lora_weight)
    }
}
```

#### 5.2 LoRA Configuration

```simplex
// simplex-training/src/lora/config.sx

/// LoRA configuration
#[derive(Clone)]
pub struct LoRAConfig {
    /// Rank (8, 16, 32 typical)
    pub rank: usize,
    /// Alpha scaling factor
    pub alpha: f64,
    /// Dropout rate
    pub dropout: f64,
    /// Target modules to adapt
    pub target_modules: Vec<String>,
}

impl LoRAConfig {
    /// Configuration for simple classification tasks
    pub fn simple() -> LoRAConfig {
        LoRAConfig {
            rank: 8,
            alpha: 16.0,
            dropout: 0.05,
            target_modules: vec![
                "q_proj".to_string(),
                "v_proj".to_string(),
            ],
        }
    }

    /// Configuration for standard NLU tasks
    pub fn standard() -> LoRAConfig {
        LoRAConfig {
            rank: 16,
            alpha: 32.0,
            dropout: 0.05,
            target_modules: vec![
                "q_proj".to_string(),
                "k_proj".to_string(),
                "v_proj".to_string(),
                "o_proj".to_string(),
            ],
        }
    }

    /// Configuration for complex generation tasks
    pub fn complex() -> LoRAConfig {
        LoRAConfig {
            rank: 32,
            alpha: 64.0,
            dropout: 0.1,
            target_modules: vec![
                "q_proj".to_string(),
                "k_proj".to_string(),
                "v_proj".to_string(),
                "o_proj".to_string(),
                "gate_proj".to_string(),
                "up_proj".to_string(),
                "down_proj".to_string(),
            ],
        }
    }
}
```

### Phase 6: Full Training Pipeline COMPLETE (v0.9.2)

**Status:** Complete in `simplex-training/src/pipeline/`

Implemented components:
- `specialist.sx`: SpecialistPipeline (train, train_step, validate, export_gguf), PipelineConfig, TrainedSpecialist, PipelineResult, TrainingMetrics
- `batch.sx`: BatchTrainer (train_all, train_one), BatchConfig, BatchResult, BatchStats, SpecialistCatalog, SpecialistCategory, SpecialistDefinition
- `anneal.sx`: AnnealOptimizer (get_lr, step, reheat, meta_step), AnnealTrainingState (record, learning_rate, temperature, should_stop)

Pipeline integrates:
- Data generation from specialist generators
- LoRA model training with self-learning annealing
- Validation with early stopping
- Optional compression
- GGUF export

**Objective (for reference):** Implement the complete end-to-end training pipeline.

#### 6.1 Specialist Trainer

```simplex
// simplex-training/src/pipeline/specialist.sx

/// Complete specialist training pipeline
pub struct SpecialistPipeline {
    /// Model with LoRA adapters
    model: LoRAModel,
    /// Data generator
    generator: Box<dyn DataGenerator>,
    /// Optimizer with learned schedule
    optimizer: AnnealOptimizer,
    /// Training config
    config: TrainingConfig,
    /// Compression pipeline
    compressor: CompressionPipeline,
    /// GGUF exporter
    exporter: GgufExporter,
}

impl SpecialistPipeline {
    pub fn new(
        base_model: &Model,
        domain: SpecialistDomain,
        config: TrainingConfig,
    ) -> SpecialistPipeline {
        let lora_config = match domain {
            SpecialistDomain::Code => LoRAConfig::complex(),
            SpecialistDomain::Classification => LoRAConfig::simple(),
            _ => LoRAConfig::standard(),
        };

        let generator: Box<dyn DataGenerator> = match domain {
            SpecialistDomain::Document => Box::new(DocumentExtractionGenerator::new()),
            SpecialistDomain::Code => Box::new(CodeGenerationGenerator::new()),
            SpecialistDomain::Classification => Box::new(ClassificationGenerator::new()),
            _ => Box::new(GenericGenerator::new(domain)),
        };

        SpecialistPipeline {
            model: LoRAModel::from_base(base_model, lora_config),
            generator,
            optimizer: AnnealOptimizer::new(),
            config,
            compressor: CompressionPipeline::default(),
            exporter: GgufExporter::new(),
        }
    }

    /// Run full training pipeline
    pub fn train(&mut self) -> TrainedSpecialist {
        // Phase 1: Generate training data
        let train_data = self.generator.generate_batch(self.config.train_samples);
        let val_data = self.generator.generate_batch(self.config.val_samples);

        // Phase 2: Train with self-learning annealing
        let mut best_loss = f64::MAX;
        for epoch in 0..self.config.epochs {
            for batch in train_data.batches(self.config.batch_size) {
                let loss = self.train_step(&batch);

                // Validate periodically
                if self.optimizer.step % self.config.val_every == 0 {
                    let val_loss = self.validate(&val_data);
                    if val_loss < best_loss {
                        best_loss = val_loss;
                        self.save_checkpoint();
                    }
                }
            }
        }

        // Phase 3: Load best checkpoint
        self.load_best_checkpoint();

        // Phase 4: Compress (prune + quantize)
        let compressed = self.compressor.compress(&self.model);

        // Phase 5: Export to GGUF
        let gguf = self.exporter.export(&compressed);

        TrainedSpecialist {
            domain: self.generator.domain(),
            model: compressed,
            gguf,
            metrics: self.training_metrics(),
        }
    }

    fn train_step(&mut self, batch: &[TrainingExample]) -> f64 {
        // Forward pass
        let loss = self.model.forward_loss(batch);

        // Get learning rate from annealing schedule
        let lr = self.optimizer.annealed_lr();

        // Backward pass (via dual numbers)
        let grads = self.model.gradients();

        // Update with gradient clipping
        self.model.update(&grads, lr);

        // Update annealing schedule
        self.optimizer.step(loss);

        loss.val
    }
}
```

#### 6.2 Batch Training All Specialists

```simplex
// simplex-training/src/pipeline/batch.sx

/// Train all 52+ specialists
pub struct BatchTrainer {
    /// Base model path
    base_model_path: String,
    /// Specialist catalog
    catalog: SpecialistCatalog,
    /// Output directory
    output_dir: String,
    /// Parallel training limit
    parallel: usize,
}

impl BatchTrainer {
    pub fn train_all(&self) -> BatchTrainingResult {
        let base_model = Model::load(&self.base_model_path);
        var results = Vec::new();

        for category in self.catalog.categories() {
            for specialist in category.specialists() {
                let config = TrainingConfig::for_domain(&specialist.domain);
                let mut pipeline = SpecialistPipeline::new(
                    &base_model,
                    specialist.domain.clone(),
                    config,
                );

                let result = pipeline.train();
                results.push(result);

                // Save immediately
                result.save(&format!("{}/{}", self.output_dir, specialist.id));
            }
        }

        BatchTrainingResult { specialists: results }
    }
}
```

### Phase 7: Testing and Validation COMPLETE (v0.9.2)

**Status:** Complete in `tests/training/`

Test files implemented:
- `unit_tensor_ops.sx`: 40+ tests for DualTensor operations, construction, shapes, activations, losses, gradients
- `unit_lora.sx`: 30+ tests for LoRA layers, configs, adapters, gradient flow
- `unit_attention.sx`: 25+ tests for MultiHeadAttention, GQA, temperature attention, masking, normalization
- `unit_generators.sx`: 35+ tests for all specialist generators, data loaders, RNG
- `unit_annealing.sx`: 30+ tests for AnnealOptimizer, AnnealTrainingState, meta-gradients, schedules
- `unit_neural_gates.sx`: 40+ tests for DualGate, GumbelSoftmax, STE, SoftLogic gates
- `integration_pipeline.sx`: 30+ tests for end-to-end pipeline, batch training, data flow

**Objective (for reference):** Comprehensive tests for the training pipeline.

#### 7.1 Test Suite Structure

```
tests/training/
├── unit_tensor_ops.sx          # Tensor operations with duals
├── unit_lora.sx                # LoRA layer correctness
├── unit_attention.sx           # Attention mechanism
├── unit_generators.sx          # Data generation
├── unit_annealing.sx           # Self-learning annealing
├── unit_neural_gates.sx        # Dual-mode gates
├── integ_training_loop.sx      # Training loop integration
├── integ_compression.sx        # Compression pipeline
├── integ_export.sx             # GGUF export
└── e2e_specialist.sx           # Full specialist training
```

#### 7.2 Example Test

```simplex
// tests/training/unit_tensor_ops.sx

#[test]
fn test_dual_tensor_matmul() {
    // Create variable tensors
    let a = DualTensor::variable(&[2, 3], &[1.0, 2.0, 3.0, 4.0, 5.0, 6.0]);
    let b = DualTensor::variable(&[3, 2], &[1.0, 2.0, 3.0, 4.0, 5.0, 6.0]);

    // Matrix multiply
    let c = a.matmul(&b);

    // Check shape
    assert_eq!(c.shape(), &[2, 2]);

    // Check values: [[22, 28], [49, 64]]
    assert_approx_eq!(c.get(&[0, 0]).val, 22.0);
    assert_approx_eq!(c.get(&[0, 1]).val, 28.0);
    assert_approx_eq!(c.get(&[1, 0]).val, 49.0);
    assert_approx_eq!(c.get(&[1, 1]).val, 64.0);

    // Check gradients exist
    assert!(c.get(&[0, 0]).der != 0.0);
}

#[test]
fn test_dual_tensor_softmax() {
    let logits = DualTensor::variable(&[1, 4], &[1.0, 2.0, 3.0, 4.0]);
    let probs = logits.softmax(-1);

    // Sum should be 1
    assert_approx_eq!(probs.sum().val, 1.0);

    // Largest logit should have highest prob
    assert!(probs.get(&[0, 3]).val > probs.get(&[0, 0]).val);
}

#[test]
fn test_lora_forward() {
    let linear = Linear::new(64, 32, true);
    let lora = LoRALayer::from_linear(&linear, 8, 16.0);

    let input = DualTensor::rand(&[4, 64]);
    let output = lora.forward(&input);

    assert_eq!(output.shape(), &[4, 32]);
}

#[test]
fn test_annealed_training() {
    let model = SimpleModel::new();
    let mut trainer = AnnealTrainingState::new(model);

    // Run 100 steps
    for _ in 0..100 {
        let batch = generate_dummy_batch();
        let result = trainer.step(&batch);
        assert!(result.loss.val >= 0.0);
    }

    // Schedule should have adapted
    assert!(trainer.schedule.cool_rate.val != trainer.schedule.cool_rate.der);
}
```

---

## File Structure

After implementation, the library will have this structure:

```
simplex-training/
├── Modulus.toml
└── src/
    ├── mod.sx                      # Module root (updated)
    ├── tensor/
    │   ├── mod.sx                  # Tensor module
    │   ├── dual_tensor.sx          # DualTensor implementation
    │   ├── ops.sx                  # Element-wise operations
    │   └── matmul.sx               # Matrix multiplication
    ├── layers/
    │   ├── mod.sx
    │   ├── linear.sx               # Linear layer
    │   ├── attention.sx            # Multi-head attention
    │   ├── embedding.sx            # Token embeddings
    │   └── norm.sx                 # Layer normalization
    ├── neural/
    │   ├── mod.sx
    │   ├── gate.sx                 # Dual-mode gates
    │   ├── soft_logic.sx           # Soft logic operations
    │   └── temperature_attention.sx       # Temperature-aware attention
    ├── lora/
    │   ├── mod.sx
    │   ├── layer.sx                # LoRA layer
    │   ├── config.sx               # LoRA configuration
    │   └── adapter.sx              # Full model adapter
    ├── anneal/
    │   ├── mod.sx
    │   ├── training.sx             # Annealed training loop
    │   └── hyperparam.sx           # Hyperparameter search
    ├── data/
    │   ├── mod.sx
    │   ├── generator.sx            # Generator trait
    │   ├── loader.sx               # Data loading (existing)
    │   └── specialists/
    │       ├── mod.sx
    │       ├── document.sx         # Document extraction
    │       ├── coding.sx           # Code generation
    │       ├── sentiment.sx        # Sentiment analysis
    │       ├── reasoning.sx        # Reasoning tasks
    │       └── neural_ir.sx        # Neural IR training
    ├── pipeline/
    │   ├── mod.sx
    │   ├── specialist.sx           # Single specialist training
    │   └── batch.sx                # Batch training
    ├── schedules/                  # (existing, enhanced)
    │   ├── mod.sx
    │   ├── lr.sx
    │   ├── distill.sx
    │   ├── prune.sx
    │   └── quant.sx
    ├── trainer/                    # (existing, enhanced)
    │   ├── mod.sx
    │   ├── meta.sx
    │   └── specialist.sx
    ├── compress/                   # (existing)
    │   ├── mod.sx
    │   ├── pruning.sx
    │   └── quantization.sx
    └── export/                     # (existing)
        ├── mod.sx
        └── gguf.sx
```

---

## Migration Strategy

### Step 1: Parallel Development
- Build new Simplex components alongside Python
- Test parity between Python and Simplex outputs

### Step 2: Validation
- Compare training curves
- Compare final model quality
- Validate GGUF exports match

### Step 3: Deprecation
- Mark Python code as deprecated
- Update documentation
- Remove Python after validation period

---

## Success Criteria

1. **Functional Parity**: All 52+ specialists trainable in pure Simplex
2. **Quality**: Trained models achieve same metrics as Python version
3. **Performance**: Training speed within 2x of Python/PyTorch
4. **Self-Learning**: Schedules improve over baseline fixed schedules
5. **Export**: GGUF files compatible with llama.cpp inference

---

## Timeline

| Phase | Components | Status | Effort |
|-------|------------|--------|--------|
| Phase 1 | Core Tensor Operations | Yes Complete (v0.9.0) | - |
| Phase 1.5 | DualTensor Type | Yes Complete (v0.9.2) | - |
| Phase 2 | Self-Learning Annealing Integration | Yes Complete (v0.9.0) | - |
| Phase 3 | Dual-Mode Neural Gates | Yes Complete (v0.9.2) | Medium |
| Phase 4 | Data Generation | Yes Complete (v0.9.2) | Medium |
| Phase 5 | LoRA Implementation | Yes Complete (v0.9.2) | Medium |
| Phase 6 | Full Training Pipeline | Yes Complete (v0.9.2) | High |
| Phase 7 | Testing and Validation | Yes Complete (v0.9.2) | Medium |

**v0.9.2:** All phases complete!

---

## Dependencies

- **TASK-005 (Dual Numbers)**: Yes Complete - dual number type with forward-mode AD
- **TASK-006 (Self-Learning Annealing)**: Yes Complete - LearnableSchedule, MetaOptimizer
- **simplex-learning**: Yes Available - Tensor operations, autograd, optimizers, distributed
- **simplex-training**: Yes Available - Schedules, compression, GGUF export
- **simplex-std**: Yes Available - Core types and utilities

---

## References

- [LoRA: Low-Rank Adaptation of Large Language Models](https://arxiv.org/abs/2106.09685)
- [Gumbel-Softmax: Categorical Reparameterization](https://arxiv.org/abs/1611.01144)
- [GGUF Format Specification](https://github.com/ggerganov/ggml/blob/master/docs/gguf.md)
- Python training code: `/Users/rod/code/simplex/training/`
- Simplex training library: `/Users/rod/code/simplex/simplex-training/`
