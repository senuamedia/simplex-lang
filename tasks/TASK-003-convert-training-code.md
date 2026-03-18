# TASK-003: Convert Training Pipeline to Simplex

**Status**: Phases 1-3 Complete, Phases 4-7 Partial (~75% overall)
**Priority**: High
**Created**: 2026-01-09
**Updated**: 2026-03-16 (audited against codebase)
**Target Version**: 0.9.0+

> **Audit Note (2026-03-16):**
> - Phase 1 (Core Differentiable Schedules): COMPLETE — all 4 schedules in `simplex-training/src/schedules/` (lr.sx, distill.sx, prune.sx, quant.sx)
> - Phase 2 (Differentiable Pruning): COMPLETE — soft masks, importance scoring, layer sensitivity
> - Phase 3 (Differentiable Quantization): COMPLETE — bit-width scheduling, QAT noise, STE
> - Phase 4 (Learnable Curriculum): PARTIAL — core structure in `schedules/curriculum.sx`, needs meta-optimization integration
> - Phase 5 (Meta-Training Framework): PARTIAL — MetaOptimizer skeleton in `trainer/meta.sx`
> - Phase 6 (Compression Pipeline): PARTIAL — pipeline structure exists, staged compression in progress
> - Phase 7 (Production Training): NOT STARTED — full production integration pending
**Depends On**: TASK-001 (Neural IR) - Done, TASK-005 (Dual Numbers) - Done, TASK-006 (Self-Learning Annealing) - Done

---

## Overview

Port the entire model training pipeline from Python to pure Simplex, leveraging **self-learning annealing** (TASK-006) to automatically discover optimal training schedules, pruning strategies, and distillation temperatures. Instead of hand-tuned hyperparameters, the training system learns them through meta-gradients.

**Key Insight**: Training neural networks involves multiple annealing-like processes—learning rate decay, temperature in distillation, pruning schedules, quantization progression. By making all of these differentiable and learnable, we create training pipelines that optimize themselves.

```simplex
// Self-optimizing training pipeline
let pipeline = TrainingPipeline::new()
    .with_learnable_lr_schedule()      // Learning rate learns itself
    .with_learnable_distillation()     // Distillation temp learns itself
    .with_learnable_pruning()          // Pruning schedule learns itself
    .with_learnable_quantization();    // Quantization schedule learns itself

// Meta-training: pipeline improves over multiple runs
for specialist in specialists {
    let (model, meta_loss) = pipeline.train_with_grad(specialist);
    pipeline.update_schedules(meta_loss.gradient());
}
// After meta-training: pipeline knows optimal schedules for all specialists
```

---

## Background

### Current State: Python with Fixed Schedules

```
training/
├── scripts/
│   ├── train_all_specialists.py      # 33 specialist generators (~1600 LOC)
│   ├── train_context_protocol.py     # Fixed cosine LR schedule
│   ├── train_confidence_calibration.py # Fixed temperature
│   ├── train_belief_revision.py      # Hand-tuned hyperparameters
│   ├── train_neural_ir_gates.py      # Manual schedule selection
│   ├── curate_datasets.py
│   └── export_to_gguf.py
└── configs/
    └── specialists_catalog.yaml      # Hand-tuned configs per specialist
```

**Problems with fixed schedules**:
- Each specialist has different optimal learning rates
- Distillation temperature is trial-and-error
- Pruning ratios are guessed
- Quantization timing is arbitrary
- No transfer of knowledge between specialists

### Target State: Simplex with Learned Schedules

```
training/
├── src/
│   ├── main.sx                       # CLI with meta-training mode
│   ├── schedules/
│   │   ├── mod.sx
│   │   ├── lr_schedule.sx            # Learnable LR schedule
│   │   ├── distill_schedule.sx       # Learnable distillation temp
│   │   ├── prune_schedule.sx         # Learnable pruning schedule
│   │   └── quant_schedule.sx         # Learnable quantization schedule
│   ├── train/
│   │   ├── mod.sx
│   │   ├── meta_trainer.sx           # Meta-optimization loop
│   │   ├── specialists.sx            # Specialist training with learned schedules
│   │   └── distillation.sx           # Knowledge distillation
│   ├── compress/
│   │   ├── mod.sx
│   │   ├── pruning.sx                # Differentiable pruning
│   │   └── quantization.sx           # Differentiable quantization
│   └── export/
│       └── gguf.sx
└── learned_schedules/
    └── schedules.sxb                 # Serialized learned schedules
```

---

## Self-Learning Annealing in Training

### 1. Learnable Learning Rate Schedule

Traditional: Fixed cosine or linear decay
```python
# Python: hand-tuned
lr = 2e-4 * cosine_decay(step / total_steps)
```

Self-learning: Schedule parameters learned via meta-gradients
```simplex
/// Learnable LR schedule with meta-gradient support
struct LearnableLRSchedule {
    // All parameters are dual for gradient tracking
    initial_lr: dual,        // Starting learning rate
    decay_rate: dual,        // How fast to decay
    warmup_steps: dual,      // Warmup duration
    min_lr: dual,            // Floor learning rate

    // Oscillation for escaping plateaus
    oscillation_amp: dual,
    oscillation_freq: dual,

    // Plateau detection
    plateau_threshold: dual, // Loss stagnation threshold
    plateau_boost: dual,     // LR boost when stuck
}

impl LearnableLRSchedule {
    fn learning_rate(&self, step: dual, loss_history: &[dual]) -> dual {
        // Warmup phase
        let warmup_factor = (step / self.warmup_steps).min(dual::constant(1.0));

        // Decay phase
        let decay = (-self.decay_rate * step).exp();

        // Oscillation (helps escape local minima)
        let osc = dual::constant(1.0) +
                  self.oscillation_amp * (self.oscillation_freq * step).sin();

        // Plateau boost (increase LR when stuck)
        let stagnation = compute_stagnation(loss_history);
        let plateau_factor = dual::constant(1.0) +
            self.plateau_boost * ((stagnation - self.plateau_threshold) /
                                  dual::constant(10.0)).sigmoid();

        // Combined LR
        let lr = self.initial_lr * warmup_factor * decay * osc * plateau_factor;
        lr.max(self.min_lr)
    }
}
```

**Meta-objective for LR schedule**:
```
L_meta = final_loss + λ₁·convergence_time + λ₂·loss_variance
```

The schedule learns:
- Optimal initial LR for each model size
- How fast to decay without losing performance
- When to boost LR to escape plateaus
- Warmup duration that prevents instability

### 2. Learnable Knowledge Distillation

Knowledge distillation uses temperature to soften logits:
```
soft_targets = softmax(teacher_logits / T)
```

The temperature T is traditionally hand-tuned. With self-learning:

```simplex
/// Learnable distillation with adaptive temperature
struct LearnableDistillation {
    // Temperature schedule (learned)
    initial_temp: dual,      // Starting temperature
    temp_decay: dual,        // How to reduce temp over training
    min_temp: dual,          // Floor temperature

    // Soft/hard target mixing (learned)
    alpha_start: dual,       // Initial soft target weight
    alpha_decay: dual,       // How to shift to hard targets

    // Layer-wise temperature (different layers need different temps)
    layer_temp_scale: Vec<dual>,
}

impl LearnableDistillation {
    /// Compute distillation loss with learned temperature
    fn distill_loss(
        &self,
        student_logits: Tensor<dual>,
        teacher_logits: Tensor<f64>,
        hard_labels: Tensor<i64>,
        step: dual
    ) -> dual {
        // Temperature schedule
        let temp = self.initial_temp * (-self.temp_decay * step).exp();
        let temp = temp.max(self.min_temp);

        // Soft targets from teacher
        let soft_targets = softmax(teacher_logits / temp.val);
        let student_soft = log_softmax(student_logits / temp);

        // KL divergence (differentiable through temperature)
        let kl_loss = kl_divergence(student_soft, soft_targets) * temp * temp;

        // Hard target loss
        let hard_loss = cross_entropy(student_logits, hard_labels);

        // Mixing coefficient (learned schedule)
        let alpha = self.alpha_start * (-self.alpha_decay * step).exp();

        // Combined loss
        alpha * kl_loss + (dual::constant(1.0) - alpha) * hard_loss
    }
}
```

**Meta-objective for distillation**:
```
L_meta = student_eval_loss + λ·(teacher_eval - student_eval)²
```

The distillation learns:
- Optimal starting temperature for each teacher-student pair
- How fast to cool temperature during training
- When to shift from soft to hard targets
- Layer-specific temperatures for deep models

### 3. Learnable Pruning Schedule

Pruning removes weights to compress models. Traditional approaches use fixed sparsity targets. Self-learning discovers optimal schedules:

```simplex
/// Differentiable pruning with learned schedule
struct LearnablePruning {
    // Sparsity schedule (learned)
    initial_sparsity: dual,    // Starting sparsity (usually 0)
    final_sparsity: dual,      // Target sparsity
    pruning_rate: dual,        // How fast to prune

    // Importance scoring (learned)
    magnitude_weight: dual,    // Weight magnitude importance
    gradient_weight: dual,     // Gradient importance
    activation_weight: dual,   // Activation importance

    // Layer-wise pruning (some layers more pruneable)
    layer_sensitivity: Vec<dual>,

    // Recovery phases (temporary un-pruning)
    recovery_threshold: dual,  // When to trigger recovery
    recovery_intensity: dual,  // How much to recover
}

impl LearnablePruning {
    /// Compute soft mask for weights (differentiable)
    fn compute_mask(
        &self,
        weights: &Tensor<dual>,
        gradients: &Tensor<f64>,
        activations: &Tensor<f64>,
        step: dual,
        layer_idx: usize
    ) -> Tensor<dual> {
        // Current target sparsity
        let target_sparsity = self.initial_sparsity +
            (self.final_sparsity - self.initial_sparsity) *
            (dual::constant(1.0) - (-self.pruning_rate * step).exp());

        // Importance score (learned combination)
        let importance =
            self.magnitude_weight * weights.abs() +
            self.gradient_weight * dual::constant(gradients.abs()) +
            self.activation_weight * dual::constant(activations.abs());

        // Layer sensitivity adjustment
        let layer_scale = self.layer_sensitivity[layer_idx];
        let adjusted_sparsity = target_sparsity * layer_scale;

        // Soft threshold (differentiable pruning)
        let threshold = importance.quantile(adjusted_sparsity.val);
        let mask = ((importance - dual::constant(threshold)) /
                    dual::constant(0.01)).sigmoid();

        mask
    }

    /// Apply pruning during training
    fn apply(&self, model: &mut Model, step: dual) {
        for (layer_idx, layer) in model.layers.iter_mut().enumerate() {
            let mask = self.compute_mask(
                &layer.weights,
                &layer.grad_history,
                &layer.activation_history,
                step,
                layer_idx
            );
            layer.weights = layer.weights * mask;
        }
    }
}
```

**Meta-objective for pruning**:
```
L_meta = final_loss + λ₁·(1 - achieved_sparsity/target_sparsity)² + λ₂·loss_degradation
```

The pruning learns:
- Which layers tolerate more pruning
- Optimal pruning schedule (cubic? exponential?)
- Importance metric weighting
- When to pause pruning for recovery

### 4. Learnable Quantization Schedule

Quantization reduces precision progressively. Self-learning discovers optimal bit-width schedules:

```simplex
/// Differentiable quantization with learned schedule
struct LearnableQuantization {
    // Bit-width schedule (learned)
    initial_bits: dual,        // Starting precision (e.g., 16)
    final_bits: dual,          // Target precision (e.g., 4)
    quant_rate: dual,          // How fast to reduce precision

    // Per-layer precision (some layers need more bits)
    layer_precision: Vec<dual>,

    // Quantization-aware training
    noise_scale: dual,         // Simulated quantization noise
    ste_slope: dual,           // Straight-through estimator slope
}

impl LearnableQuantization {
    /// Current effective bit-width
    fn current_bits(&self, step: dual) -> dual {
        let progress = (dual::constant(1.0) - (-self.quant_rate * step).exp());
        self.initial_bits - (self.initial_bits - self.final_bits) * progress
    }

    /// Differentiable quantization (STE + learned noise)
    fn quantize(&self, x: Tensor<dual>, step: dual, layer_idx: usize) -> Tensor<dual> {
        let bits = self.current_bits(step) * self.layer_precision[layer_idx];
        let scale = (dual::constant(2.0).pow(bits) - dual::constant(1.0)) /
                    (x.max() - x.min());

        // Forward: actual quantization
        // Backward: straight-through estimator with learned slope
        let quantized = ((x * scale).round() / scale)
            .with_ste_grad(self.ste_slope);

        // Add learned noise during training (simulates quantization error)
        let noise = random_normal(x.shape()) * self.noise_scale / scale;
        quantized + noise
    }
}
```

**Meta-objective for quantization**:
```
L_meta = final_loss + λ₁·memory_savings + λ₂·inference_speedup
```

The quantization learns:
- Optimal precision per layer
- How fast to reduce precision
- Noise levels that prepare for final quantization
- STE slopes that preserve gradients

---

## Learnable Training Curriculum

### The Ordering Problem

When training 33 specialists from a base model, what's the optimal order?

**Option A: Generic → Specific (Coarse-to-Fine)**
```
Base Model (Qwen 70B)
    ↓ distill
General Simplex Model (8B)
    ↓ fine-tune
├── Invoice Specialist
├── Contract Specialist
├── Code Review Specialist
└── ... 30 more specialists
```

Pros:
- Shared knowledge base across all specialists
- Transfer learning from general capabilities
- Single distillation step

Cons:
- May need to "unlearn" generic patterns
- Specialists constrained by general model's biases
- One-size-fits-all compression

**Option B: Specific → Generic (Fine-to-Coarse)**
```
Base Model (Qwen 70B)
    ↓ fine-tune directly
├── Invoice Specialist (70B)
├── Contract Specialist (70B)
├── Code Review Specialist (70B)
└── ... then distill each separately
```

Pros:
- Each specialist maximally tuned for domain
- No interference between domains
- Can use domain-specific compression

Cons:
- No knowledge sharing between specialists
- 33x more distillation compute
- Redundant learning across specialists

**Option C: Learned Curriculum (Self-Learning Annealing)**

The key insight: **the curriculum itself is an annealing schedule**. We can learn:
- Which specialists to train first
- How much to share vs specialize at each stage
- When to branch from shared to specialized
- Mixing ratios between domains during training

```simplex
/// Learnable curriculum for specialist training
struct LearnableCurriculum {
    // Branching schedule (when to specialize)
    branch_point: dual,          // 0=immediate, 1=after full general training

    // Specialist ordering (learned priorities)
    specialist_weights: Vec<dual>,  // Higher = train earlier

    // Cross-domain mixing (learned knowledge sharing)
    mixing_matrix: Matrix<dual>,    // mixing[i][j] = how much j helps i

    // Progressive specialization
    specialization_rate: dual,      // How fast to focus on single domain

    // Difficulty curriculum
    example_difficulty: dual,       // Start easy or hard?
    difficulty_ramp: dual,          // How fast to increase difficulty
}

impl LearnableCurriculum {
    /// Compute training mixture for specialist i at step t
    fn training_mixture(&self, specialist_idx: usize, step: dual) -> Vec<dual> {
        var mixture = vec_new();

        // Specialization factor increases over time
        let spec_factor = (self.specialization_rate * step).sigmoid();

        for j in 0..self.num_specialists {
            if j == specialist_idx {
                // Weight for target specialist increases
                vec_push(&mut mixture, spec_factor);
            } else {
                // Weight for other specialists decreases based on mixing matrix
                let cross_weight = self.mixing_matrix[specialist_idx][j];
                vec_push(&mut mixture, cross_weight * (dual::constant(1.0) - spec_factor));
            }
        }

        normalize(&mut mixture);
        mixture
    }

    /// Get training order (sorted by learned weights)
    fn training_order(&self) -> Vec<usize> {
        let mut order: Vec<(usize, f64)> = self.specialist_weights
            .iter()
            .enumerate()
            .map(|(i, w)| (i, w.val))
            .collect();
        order.sort_by(|a, b| b.1.partial_cmp(&a.1).unwrap());
        order.iter().map(|(i, _)| *i).collect()
    }
}
```

### Discovered Curriculum Patterns

Through meta-learning, we expect the curriculum to discover patterns like:

**1. Foundational First**
```
Early training:     Later training:
┌─────────────┐     ┌─────────────┐
│ Code Review │     │ Invoice     │
│ Reasoning   │     │ Contract    │
│ Analysis    │     │ Extraction  │
└─────────────┘     └─────────────┘
  (general)           (specific)
```
Specialists requiring reasoning/analysis should train first (their knowledge transfers). Extraction/formatting specialists can train later (they benefit from reasoning foundation).

**2. Domain Clustering**
```
Cluster 1: Document Processing
  Invoice → Receipt → Contract → Legal
  (transfer within cluster)

Cluster 2: Code Intelligence
  Code Review → Bug Detection → Refactoring → Documentation
  (transfer within cluster)

Cluster 3: Data Analysis
  Data Extraction → Transformation → Validation → Reporting
  (transfer within cluster)
```
The curriculum learns to train related specialists together for maximum transfer.

**3. Difficulty Progression**
```
Easy examples first → Hard examples later
Short documents → Long documents
Clean data → Noisy data
Single task → Multi-task
```
The curriculum learns the optimal difficulty ramp for each specialist.

### Curriculum Meta-Objective

```
L_curriculum = Σᵢ specialist_loss[i] +
               λ₁·training_time +
               λ₂·transfer_efficiency +
               λ₃·final_quality_variance
```

Where:
- `specialist_loss[i]` = final eval loss for specialist i
- `training_time` = total steps across all specialists
- `transfer_efficiency` = how much training one helped others
- `final_quality_variance` = we want all specialists equally good

### Integration with Other Schedules

The curriculum interacts with all other learned schedules:

```simplex
struct FullyLearnedTraining {
    // What to train and when
    curriculum: LearnableCurriculum,

    // How to train each
    lr_schedule: LearnableLRSchedule,
    distillation: LearnableDistillation,

    // How to compress each
    pruning: LearnablePruning,
    quantization: LearnableQuantization,
}

impl FullyLearnedTraining {
    fn train_all_specialists(&mut self, base_model: &Model) -> Vec<Specialist> {
        var specialists = vec_new();

        // Get learned training order
        let order = self.curriculum.training_order();

        // Shared model that accumulates knowledge
        var shared_model = base_model.clone();

        for (step, specialist_idx) in order.iter().enumerate() {
            let step_dual = dual::constant(step as f64);

            // Get training mixture (how much from shared vs fresh)
            let mixture = self.curriculum.training_mixture(specialist_idx, step_dual);

            // Initialize from mixture of shared and base
            var student = Model::mix(&shared_model, base_model, mixture[specialist_idx]);

            // Train with learned schedules
            let (trained, meta_loss) = self.train_with_schedules(
                &specialist_configs[specialist_idx],
                &shared_model,  // Teacher
                &mut student,
                step_dual
            );

            // Update shared model with new knowledge (weighted by transfer matrix)
            for j in 0..self.curriculum.num_specialists {
                let transfer_weight = self.curriculum.mixing_matrix[j][specialist_idx];
                shared_model = Model::ema_update(&shared_model, &trained, transfer_weight.val);
            }

            vec_push(&mut specialists, trained);
        }

        specialists
    }
}
```

---

## Unified Meta-Training Framework

All learned schedules are trained together through a meta-optimization loop:

```simplex
/// Meta-trainer that learns all schedules jointly
struct MetaTrainer {
    lr_schedule: LearnableLRSchedule,
    distillation: LearnableDistillation,
    pruning: LearnablePruning,
    quantization: LearnableQuantization,

    meta_lr: f64,
    meta_optimizer: AdamW,
}

impl MetaTrainer {
    /// Train one specialist with current schedules, track meta-gradients
    fn train_specialist(
        &self,
        specialist: &SpecialistConfig,
        teacher: &Model,
        student: &mut Model,
        dataset: &Dataset
    ) -> (Model, MetaLoss) {
        var total_loss = dual::constant(0.0);
        var step = dual::constant(0.0);

        for epoch in 0..specialist.epochs {
            for batch in dataset.iter_batches(specialist.batch_size) {
                // Get learning rate from learned schedule
                let lr = self.lr_schedule.learning_rate(step, &loss_history);

                // Forward pass with distillation
                let student_logits = student.forward(&batch.input);
                let teacher_logits = teacher.forward(&batch.input);

                // Distillation loss (differentiable through temperature)
                let loss = self.distillation.distill_loss(
                    student_logits,
                    teacher_logits,
                    batch.labels,
                    step
                );

                // Backward pass
                let grads = loss.backward();

                // Apply gradients with learned LR
                self.optimizer.step(student, grads, lr.val);

                // Apply pruning (differentiable)
                self.pruning.apply(student, step);

                // Apply quantization-aware training
                self.quantization.quantize_model(student, step);

                total_loss = total_loss + loss;
                step = step + dual::constant(1.0);
            }
        }

        // Compute meta-loss
        let eval_loss = evaluate(student, &validation_set);
        let meta_loss = self.compute_meta_loss(total_loss, step, eval_loss, student);

        (student.clone(), meta_loss)
    }

    /// Compute meta-loss that measures schedule quality
    fn compute_meta_loss(
        &self,
        training_loss: dual,
        steps: dual,
        eval_loss: f64,
        model: &Model
    ) -> MetaLoss {
        // Primary: final model quality
        let quality_term = dual::constant(eval_loss);

        // Efficiency: fewer steps is better
        let efficiency_term = steps / dual::constant(10000.0);

        // Compression: smaller model is better
        let sparsity = model.sparsity();
        let compression_term = dual::constant(1.0) - dual::constant(sparsity);

        // Stability: low variance in training loss
        let stability_term = training_loss.variance();

        MetaLoss {
            total: quality_term +
                   dual::constant(0.1) * efficiency_term +
                   dual::constant(0.1) * compression_term +
                   dual::constant(0.01) * stability_term,
            quality: quality_term,
            efficiency: efficiency_term,
            compression: compression_term,
        }
    }

    /// Update all schedules based on meta-loss
    fn update_schedules(&mut self, meta_loss: &MetaLoss) {
        // Extract gradients from all schedule parameters
        let lr_grad = self.lr_schedule.gradient();
        let distill_grad = self.distillation.gradient();
        let prune_grad = self.pruning.gradient();
        let quant_grad = self.quantization.gradient();

        // Update via meta-optimizer
        self.lr_schedule.update(lr_grad, self.meta_lr);
        self.distillation.update(distill_grad, self.meta_lr);
        self.pruning.update(prune_grad, self.meta_lr);
        self.quantization.update(quant_grad, self.meta_lr);
    }

    /// Meta-training loop over all specialists
    fn meta_train(&mut self, specialists: &[SpecialistConfig], teacher: &Model) {
        for meta_epoch in 0..self.meta_epochs {
            var meta_losses = vec_new();

            for specialist in specialists {
                // Train with current schedules
                let student = Model::from_config(specialist);
                let (trained, meta_loss) = self.train_specialist(
                    specialist, teacher, &mut student, &specialist.dataset
                );

                vec_push(&mut meta_losses, meta_loss);
            }

            // Aggregate meta-loss across all specialists
            let aggregate_loss = self.aggregate_meta_losses(&meta_losses);

            // Update schedules based on all specialists
            self.update_schedules(&aggregate_loss);

            println!("Meta-epoch {}: loss={}, quality={}",
                     meta_epoch, aggregate_loss.total.val, aggregate_loss.quality.val);
        }

        // Save learned schedules
        self.save_schedules("learned_schedules/schedules.sxb");
    }
}
```

---

## Seed Model Compression Pipeline

The learned schedules enable a powerful compression pipeline:

```simplex
/// Full compression pipeline with learned schedules
struct CompressionPipeline {
    meta_trainer: MetaTrainer,
    compression_stages: Vec<CompressionStage>,
}

enum CompressionStage {
    // Knowledge distillation from large to small
    Distill { teacher_size: ModelSize, student_size: ModelSize },
    // Structured pruning with learned importance
    Prune { target_sparsity: f64 },
    // Quantization with learned precision
    Quantize { target_bits: i64 },
    // Fine-tune with learned schedule
    FineTune { epochs: i64 },
}

impl CompressionPipeline {
    /// Create compression pipeline for seed models
    fn for_seed_models() -> CompressionPipeline {
        CompressionPipeline {
            meta_trainer: MetaTrainer::new(),
            compression_stages: vec![
                // Stage 1: Distill 70B → 8B
                CompressionStage::Distill {
                    teacher_size: ModelSize::B70,
                    student_size: ModelSize::B8,
                },
                // Stage 2: Prune to 50% sparsity
                CompressionStage::Prune { target_sparsity: 0.5 },
                // Stage 3: Quantize to 4-bit
                CompressionStage::Quantize { target_bits: 4 },
                // Stage 4: Fine-tune to recover quality
                CompressionStage::FineTune { epochs: 3 },
            ],
        }
    }

    /// Compress a specialist model with learned schedules
    fn compress(&self, specialist: &SpecialistConfig) -> CompressedModel {
        var model = load_teacher_model(specialist);

        for stage in &self.compression_stages {
            match stage {
                Distill { teacher_size, student_size } => {
                    let student = Model::sized(*student_size);
                    let (trained, _) = self.meta_trainer.train_specialist(
                        specialist, &model, &mut student, &specialist.dataset
                    );
                    model = trained;
                }
                Prune { target_sparsity } => {
                    self.meta_trainer.pruning.final_sparsity =
                        dual::variable(*target_sparsity);
                    self.meta_trainer.pruning.apply_full(&mut model);
                }
                Quantize { target_bits } => {
                    self.meta_trainer.quantization.final_bits =
                        dual::variable(*target_bits as f64);
                    self.meta_trainer.quantization.quantize_model(&mut model);
                }
                FineTune { epochs } => {
                    self.meta_trainer.fine_tune(&mut model, specialist, *epochs);
                }
            }
        }

        CompressedModel {
            weights: model.weights,
            sparsity: model.sparsity(),
            bits: self.meta_trainer.quantization.final_bits.val as i64,
            size_mb: model.size_bytes() / (1024 * 1024),
        }
    }
}
```

---

## Implementation Phases

### Phase 1: Core Differentiable Schedules

**Deliverables**:
1. `LearnableLRSchedule` with dual number parameters
2. `LearnableDistillation` with temperature schedule
3. Basic meta-gradient computation
4. Schedule serialization/loading

**Success Criteria**:
- [ ] LR schedule gradients flow correctly
- [ ] Distillation temperature is differentiable
- [ ] Meta-loss decreases over meta-epochs
- [ ] Schedules can be saved and loaded

### Phase 2: Differentiable Pruning

**Deliverables**:
1. `LearnablePruning` with soft masks
2. Importance scoring with learned weights
3. Layer-wise sensitivity
4. Recovery phase triggers

**Success Criteria**:
- [ ] Pruning is differentiable (gradients flow through masks)
- [ ] Learned importance outperforms magnitude-only
- [ ] Layer sensitivity reflects actual pruneability
- [ ] 50% sparsity with <2% accuracy loss

### Phase 3: Differentiable Quantization

**Deliverables**:
1. `LearnableQuantization` with STE
2. Per-layer precision learning
3. Noise injection for training
4. Integration with GGUF export

**Success Criteria**:
- [ ] Quantization-aware training works
- [ ] 4-bit models maintain quality
- [ ] Per-layer precision improves over uniform
- [ ] Exported GGUF matches training precision

### Phase 4: Learnable Curriculum

**Deliverables**:
1. `LearnableCurriculum` with specialist ordering
2. Cross-domain mixing matrix
3. Specialization rate scheduling
4. Difficulty progression learning

**Success Criteria**:
- [ ] Curriculum discovers meaningful specialist ordering
- [ ] Transfer matrix shows domain clustering
- [ ] Foundational specialists train before specific ones
- [ ] Total training time reduced vs random order

### Phase 5: Meta-Training Framework

**Deliverables**:
1. `MetaTrainer` with all schedules + curriculum
2. `FullyLearnedTraining` unified interface
3. Meta-loss aggregation across specialists
4. Schedule + curriculum joint optimization

**Success Criteria**:
- [ ] All schedules improve over meta-epochs
- [ ] Transfer to new specialists works
- [ ] Meta-training is stable
- [ ] Learned system outperforms hand-tuned

### Phase 6: Compression Pipeline

**Deliverables**:
1. `CompressionPipeline` with staged compression
2. Distill → Prune → Quantize → FineTune pipeline
3. Quality/size Pareto frontier exploration
4. Automatic pipeline selection

**Success Criteria**:
- [ ] 8B → 2B distillation preserves >90% quality
- [ ] Full pipeline achieves 10x compression
- [ ] Pareto frontier matches SOTA
- [ ] Pipeline runs unattended

### Phase 7: Production Training

**Deliverables**:
1. All 33 specialist generators in Simplex
2. Full training scripts with learned schedules
3. GGUF export with optimized precision
4. CI/CD integration

**Success Criteria**:
- [ ] All specialists trainable in pure Simplex
- [ ] Training time competitive with Python
- [ ] Models pass evaluation benchmarks
- [ ] Automated train → export → deploy

---

## Technical Specification: Tensor Library

```simplex
/// Core tensor type with dual number support
@zero_overhead
struct Tensor<T, const SHAPE: &[usize]> {
    data: Buffer<T>,
    requires_grad: bool,
}

impl<const S: &[usize]> Tensor<dual, S> {
    /// Matrix multiplication with gradient tracking
    fn matmul<const S2: &[usize]>(self, other: Tensor<dual, S2>) -> Tensor<dual, _> {
        // Forward: C = A @ B
        // Backward: dA = dC @ B^T, dB = A^T @ dC
        // Dual numbers handle this automatically
        let result = self.data.matmul(other.data);
        Tensor { data: result, requires_grad: true }
    }

    /// Softmax with temperature (differentiable through temp)
    fn softmax_with_temp(self, temp: dual, dim: i64) -> Tensor<dual, S> {
        let scaled = self / temp;
        let max = scaled.max(dim, keepdim: true);
        let exp = (scaled - max).exp();
        let sum = exp.sum(dim, keepdim: true);
        exp / sum
    }

    /// Layer normalization
    fn layer_norm(self, normalized_shape: &[usize], eps: f64) -> Tensor<dual, S> {
        let mean = self.mean(dims: normalized_shape);
        let var = self.var(dims: normalized_shape);
        (self - mean) / (var + dual::constant(eps)).sqrt()
    }
}
```

---

## API Summary

```simplex
// Learnable schedules
LearnableLRSchedule::new() -> LearnableLRSchedule
LearnableLRSchedule::learning_rate(step, loss_history) -> dual
LearnableLRSchedule::gradient() -> LRGradient
LearnableLRSchedule::update(grad, lr)

LearnableDistillation::new() -> LearnableDistillation
LearnableDistillation::distill_loss(student, teacher, labels, step) -> dual
LearnableDistillation::gradient() -> DistillGradient

LearnablePruning::new() -> LearnablePruning
LearnablePruning::compute_mask(weights, grads, acts, step, layer) -> Tensor<dual>
LearnablePruning::apply(model, step)

LearnableQuantization::new() -> LearnableQuantization
LearnableQuantization::current_bits(step) -> dual
LearnableQuantization::quantize(tensor, step, layer) -> Tensor<dual>

// Learnable curriculum
LearnableCurriculum::new(num_specialists) -> LearnableCurriculum
LearnableCurriculum::training_order() -> Vec<usize>
LearnableCurriculum::training_mixture(specialist, step) -> Vec<dual>
LearnableCurriculum::gradient() -> CurriculumGradient
LearnableCurriculum::update(grad, lr)

// Unified training (all schedules + curriculum)
FullyLearnedTraining::new() -> FullyLearnedTraining
FullyLearnedTraining::train_all_specialists(base_model) -> Vec<Specialist>
FullyLearnedTraining::save(path)
FullyLearnedTraining::load(path) -> FullyLearnedTraining

// Meta-training
MetaTrainer::new() -> MetaTrainer
MetaTrainer::train_specialist(specialist, teacher, student, data) -> (Model, MetaLoss)
MetaTrainer::update_schedules(meta_loss)
MetaTrainer::meta_train(specialists, teacher)
MetaTrainer::save_schedules(path)
MetaTrainer::load_schedules(path)

// Compression pipeline
CompressionPipeline::for_seed_models() -> CompressionPipeline
CompressionPipeline::compress(specialist) -> CompressedModel
```

---

## Expected Improvements Over Fixed Schedules

Based on meta-learning literature, self-learning annealing should provide:

| Metric | Fixed Schedules | Learned Schedules | Improvement |
|--------|-----------------|-------------------|-------------|
| Final Loss | 1.0 (baseline) | 0.85-0.90 | 10-15% |
| Training Steps | 100K | 70-80K | 20-30% fewer |
| Pruning Quality | 50% @ 5% loss | 50% @ 2% loss | 60% less degradation |
| Quantization Quality | 4-bit @ 8% loss | 4-bit @ 4% loss | 50% less degradation |
| Distillation Efficiency | 90% of teacher | 94% of teacher | 4% more transfer |

---

## Library Architecture: simplex-training

### Decision

Training code will be implemented as a standalone library: **simplex-training**.

**Location**: `/simplex-training/`

**Key Design Decisions**:

1. **Depends on simplex-inference** - Training involves many forward passes. By depending on simplex-inference, we get all inference optimizations (batching, caching, smart routing) for free during training.

2. **AWS Inferentia exclusive** - Training runs ONLY on AWS Inferentia/Trainium instances:
   - 10-20x cost reduction vs GPU training
   - Neuron SDK for optimized forward/backward passes
   - Spot instance friendly with checkpoint/resume
   - Production feature flag: `neuron` (required)
   - Development fallback: `cpu-dev` (for local testing only)

3. **Follows simplex-inference patterns** - Same code structure, same optimization philosophy:
   - Learnable schedules mirror inference caching concepts
   - Meta-gradients leverage dual numbers from core
   - GGUF export uses same format as inference loading

### Rationale

**Why Inferentia for training?**

Training is forward-pass dominated. Each training step requires:
- Forward pass through student model
- Forward pass through teacher model (for distillation)
- Loss computation (forward)
- Backward pass (reverse of forward)

Since 3 of 4 steps are forward passes, Inferentia's inference optimization benefits training too. The Neuron SDK provides efficient gradient computation, and spot instances reduce costs further.

**Why depend on simplex-inference?**

```
simplex-training → simplex-inference → Neuron SDK
                                     → llama.cpp bindings
                                     → GGUF format
```

This dependency chain means:
- Training uses same batching logic as inference
- Teacher model forward passes are cached
- Smart routing can select teacher model tier
- Trained models export directly to GGUF for inference

### Library Structure

```
/simplex-training/
├── Modulus.toml                    # Dependencies on simplex-inference, simplex-s3
├── src/
│   ├── mod.sx                      # Public API, TrainingConfig, ModelSize
│   ├── schedules/
│   │   ├── mod.sx                  # LearnedSchedules unified type
│   │   ├── lr.sx                   # LearnableLRSchedule
│   │   ├── distill.sx              # LearnableDistillation
│   │   ├── prune.sx                # LearnablePruning
│   │   ├── quant.sx                # LearnableQuantization
│   │   └── curriculum.sx           # LearnableCurriculum
│   ├── trainer/
│   │   ├── mod.sx
│   │   ├── meta.sx                 # MetaTrainer, MetaLoss, MetaConfig
│   │   ├── specialist.sx           # SpecialistTrainer, SpecialistConfig
│   │   └── compression.sx          # CompressionPipeline, CompressionStage
│   ├── data/
│   │   ├── mod.sx
│   │   ├── loader.sx               # DataLoader with S3 streaming
│   │   └── generators.sx           # SpecialistGenerator, GeneratorRegistry
│   └── export/
│       ├── mod.sx
│       └── gguf.sx                 # GgufExporter, GgufConfig
```

### Configuration

```toml
# Modulus.toml
[package]
name = "simplex-training"
version = "0.9.0"

[dependencies]
simplex-inference = { path = "../simplex-inference" }
simplex-s3 = { path = "../simplex-s3" }
simplex-std = { path = "../../simplex-std" }

[features]
default = ["neuron"]
neuron = []      # Required for production
cpu-dev = []     # Local development only

[target.inferentia]
compile_flags = ["-O3", "--neuron-target=inf2"]
link_flags = ["--neuron-runtime"]
```

### Usage Example

```simplex
use simplex_training::{
    MetaTrainer, MetaConfig,
    SpecialistTrainer, SpecialistConfig,
    CompressionPipeline,
};

// Create meta-trainer with all learnable schedules
let trainer = MetaTrainer::new(MetaConfig::default())
    .with_learnable_lr()
    .with_learnable_distillation()
    .with_learnable_pruning()
    .with_learnable_quantization()
    .with_learnable_curriculum();

// Create specialists
let mut specialists = vec![
    SpecialistTrainer::new(SpecialistConfig::code()),
    SpecialistTrainer::new(SpecialistConfig::math()),
    SpecialistTrainer::new(SpecialistConfig::reasoning()),
];

// Load teacher model (uses simplex-inference under the hood)
let teacher = TeacherModel::load("qwen2.5-72b").await?;

// Meta-train across all specialists
let result = trainer.meta_train(&mut specialists, &teacher).await;
println!("Learned schedules saved, final loss: {}", result.final_val_loss);

// Compress for deployment
let pipeline = CompressionPipeline::for_seed_models();
for specialist in specialists {
    let compressed = pipeline.compress(&specialist).await;
    compressed.export_gguf(&format!("models/{}.gguf", specialist.config.domain)).await?;
}
```

---

## Technical Dependencies

| Dependency | Type | Status | Notes |
|------------|------|--------|-------|
| TASK-001 Neural IR | Internal | - Complete | Autograd foundation |
| TASK-005 Dual Numbers | Internal | - Complete | Differentiable primitives |
| TASK-006 Self-Learning Annealing | Internal | - Complete | Meta-gradient framework |
| simplex-inference | Library | - Done Created | Forward pass optimization |
| simplex-s3 | Library | - Done Available | Dataset/model storage |
| LLVM 18+ | External | Available | Backend for compilation |
| AWS Neuron SDK | External | Required | Inferentia integration |
| GGUF spec | External | Available | Export format |

---

## Success Definition

This task is complete when:

1. **All training uses learned schedules** - No hand-tuned hyperparameters
2. **Meta-training improves schedules** - Measured improvement over meta-epochs
3. **Compression pipeline works** - 10x compression with <5% quality loss
4. **Transfer learning works** - Schedules learned on one specialist help others
5. **Production quality** - All 33 specialists trainable with learned schedules

---

## References

- TASK-005: Dual Numbers and Forward-Mode AD
- TASK-006: Self-Learning Annealing
- Maclaurin et al., "Gradient-based Hyperparameter Optimization" (2015)
- Hinton et al., "Distilling the Knowledge in a Neural Network" (2015)
- Frankle & Carlin, "The Lottery Ticket Hypothesis" (2019)
- Han et al., "Learning both Weights and Connections for Efficient Neural Networks" (2015)
- Nagel et al., "A White Paper on Neural Network Quantization" (2021)

---

*Self-learning training: the pipeline that improves itself.*
