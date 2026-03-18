# TASK-053: Model Training & Evaluation Framework

**Version:** 0.18.0
**Status:** Planned
**Priority:** P0 — Critical
**Depends on:** v0.17.0 release, training data pipeline (TASK-052)

## Why This Feature Is Needed

v0.16.0 gives Simplex advanced mathematical optimizers. v0.17.0 gives it a new model
architecture. But neither provides the *training loop* — the orchestration that takes
a model definition, a dataset, and a compute budget and produces a trained model. This
is the missing piece between "Simplex can define models" and "Simplex can build models."

The training framework must handle the full lifecycle: model initialization on the
Cognitive Substrate, distributed training across available hardware, checkpoint
management, evaluation against Simplex-specific benchmarks, and model export to
deployable formats.

Critically, this framework trains models *in Simplex*. Not in Python with PyTorch.
Not via external tools. The entire training pipeline — from data loading to gradient
computation to weight update to checkpoint save — runs as compiled Simplex code. This
proves that Simplex is not just a language that *describes* AI but a language that
*builds* AI.

## Why It Adds Value

1. **End-to-end in Simplex.** No Python dependency. No PyTorch. The training framework
   is pure Simplex, using the tensor operations, optimizers, and Cognitive Substrate
   layers built in v0.16.0-v0.17.0. This validates the entire stack.

2. **Cognitive Substrate native.** The training loop is aware of liquid dynamics,
   hyperbolic embeddings, Hopfield memory, and all substrate components. It knows how
   to train each component correctly (Riemannian optimizer for hyperbolic, ODE solver
   for liquid, pattern storage for Hopfield).

3. **Simplex-specific evaluation.** Benchmarks that test what matters for Simplex models:
   code correctness (does generated code compile?), test validity (do generated tests
   catch real bugs?), specification adherence (does output match the language spec?).

4. **Reproducible training.** Deterministic data loading, seeded initialization, and
   checkpoint management ensure that any training run can be exactly reproduced.

5. **Self-hosted model iteration.** Train a model, evaluate it, identify weaknesses,
   generate targeted training data, retrain. The entire improvement loop runs in Simplex.

## Deliverables

### Phase 1: Training Loop (~600 lines)

Location: `simplex-training/src/trainer/`

- **SubstrateTrainer** — the core training loop for Cognitive Substrate models:
  handles mixed optimizer requirements (Riemannian for hyperbolic, natural gradient
  for standard layers, ODE integration for liquid)
- **CheckpointManager** — save/load model state, optimizer state, training progress;
  periodic and best-model checkpointing
- **TrainingConfig** — declarative training configuration: learning rates, schedules,
  batch size, epochs, warmup, gradient accumulation
- **DistributedTrainer** — data-parallel training across multiple devices with gradient
  aggregation

```simplex
/// Training loop for Cognitive Substrate models
struct SubstrateTrainer {
    model: SubstrateModel,
    config: TrainingConfig,
    optimizers: OptimizerSet,  // different optimizers for different components
    checkpoint_mgr: CheckpointManager,
    evaluator: ModelEvaluator,
    logger: TrainingLogger,
}

/// Different components need different optimizers
struct OptimizerSet {
    standard: NaturalAdam,           // for standard layers (from v0.16.0)
    hyperbolic: RiemannianAdam,      // for hyperbolic embeddings (from v0.17.0)
    liquid: AdamW,                   // for liquid ODE parameters
    hopfield: Option<()>,            // Hopfield patterns are stored, not trained
}

struct TrainingConfig {
    epochs: usize,
    batch_size: usize,
    learning_rate: f64,
    warmup_steps: usize,
    gradient_accumulation: usize,
    max_grad_norm: f64,
    checkpoint_interval: usize,
    eval_interval: usize,
    seed: u64,
}

impl SubstrateTrainer {
    fn train(mut self: &mut Self, dataset: &Dataset) -> TrainingResult {
        let mut global_step = 0;
        let mut best_eval_score = 0.0;

        for epoch in 0..self.config.epochs {
            for batch in dataset.batches(self.config.batch_size) {
                // Forward pass through Cognitive Substrate
                let output = self.model.forward(&batch.input);
                let loss = self.model.loss(&output, &batch.target);

                // Backward pass
                let gradients = loss.backward();

                // Gradient accumulation
                if global_step % self.config.gradient_accumulation == 0 {
                    // Clip gradients
                    let clipped = clip_grad_norm(&gradients, self.config.max_grad_norm);

                    // Apply different optimizers to different components
                    self.optimizers.standard.step(
                        &mut self.model.standard_params(), &clipped.standard);
                    self.optimizers.hyperbolic.step(
                        &mut self.model.hyperbolic_params(), &clipped.hyperbolic);
                    self.optimizers.liquid.step(
                        &mut self.model.liquid_params(), &clipped.liquid);

                    // Store new patterns in Hopfield memory (no gradient)
                    if let Some(new_patterns) = self.model.extract_patterns(&batch) {
                        self.model.hopfield_memory.store_batch(new_patterns);
                    }
                }

                // Logging
                self.logger.log_step(global_step, loss.val());

                // Evaluation
                if global_step % self.config.eval_interval == 0 {
                    let eval_score = self.evaluator.evaluate(&self.model);
                    self.logger.log_eval(global_step, &eval_score);
                    if eval_score.overall > best_eval_score {
                        best_eval_score = eval_score.overall;
                        self.checkpoint_mgr.save_best(&self.model, &self.optimizers);
                    }
                }

                // Periodic checkpoint
                if global_step % self.config.checkpoint_interval == 0 {
                    self.checkpoint_mgr.save(&self.model, &self.optimizers, global_step);
                }

                global_step += 1;
            }
        }
        TrainingResult { final_loss: loss.val(), best_eval: best_eval_score, steps: global_step }
    }
}
```

Files:
- `simplex-training/src/trainer/substrate.sx` — SubstrateTrainer
- `simplex-training/src/trainer/checkpoint.sx` — CheckpointManager
- `simplex-training/src/trainer/config.sx` — TrainingConfig
- `simplex-training/src/trainer/distributed.sx` — DistributedTrainer
- `simplex-training/src/trainer/logger.sx` — TrainingLogger

### Phase 2: Evaluation Benchmarks (~500 lines)

- **CompilationBench** — does generated code compile? Measures syntax correctness,
  type correctness, and successful LLVM IR generation
- **TestPassBench** — do generated tests actually pass when run? And do they catch
  planted bugs? (mutation testing)
- **SpecAdherenceBench** — does model output conform to the Simplex language spec?
  Tests syntax rules, semantic constraints, and idiom adherence
- **CompletionBench** — code completion quality: exact match, token-level F1, and
  functional equivalence (compiles to same behavior)
- **ExplanationBench** — quality of error explanations and documentation: human-rated
  rubric for clarity, correctness, and completeness
- **ArchitectureBench** — does the model suggest valid Simplex architecture patterns
  for design prompts?

```simplex
/// Evaluate a model against Simplex-specific benchmarks
struct ModelEvaluator {
    benchmarks: Vec<Box<dyn Benchmark>>,
}

trait Benchmark {
    fn name(self: &Self) -> &str;
    fn evaluate(self: &Self, model: &SubstrateModel) -> BenchmarkScore;
}

/// Does generated code compile?
struct CompilationBench {
    test_prompts: Vec<String>,
    compiler: Compiler,
}

impl Benchmark for CompilationBench {
    fn evaluate(self: &Self, model: &SubstrateModel) -> BenchmarkScore {
        let mut compiled = 0;
        let mut total = 0;
        for prompt in &self.test_prompts {
            let generated = model.generate(prompt);
            if self.compiler.check(&generated).is_ok() {
                compiled += 1;
            }
            total += 1;
        }
        BenchmarkScore {
            name: "compilation".to_string(),
            score: compiled as f64 / total as f64,
            details: format!("{}/{} compiled successfully", compiled, total),
        }
    }
}

/// Do generated tests catch real bugs?
struct TestPassBench {
    functions: Vec<(String, String)>,  // (function, known_bug_version)
}

impl Benchmark for TestPassBench {
    fn evaluate(self: &Self, model: &SubstrateModel) -> BenchmarkScore {
        let mut catches_bugs = 0;
        for (function, buggy) in &self.functions {
            let tests = model.generate(&format!("Generate tests for:\n{}", function));
            // Run tests against correct version (should pass)
            let passes_correct = run_tests(&tests, function);
            // Run tests against buggy version (should fail)
            let catches_bug = !run_tests(&tests, buggy);
            if passes_correct && catches_bug {
                catches_bugs += 1;
            }
        }
        BenchmarkScore {
            name: "test_quality".to_string(),
            score: catches_bugs as f64 / self.functions.len() as f64,
            details: format!("{}/{} bugs caught", catches_bugs, self.functions.len()),
        }
    }
}
```

Files:
- `simplex-training/src/eval/mod.sx` — ModelEvaluator
- `simplex-training/src/eval/compilation.sx` — CompilationBench
- `simplex-training/src/eval/test_quality.sx` — TestPassBench
- `simplex-training/src/eval/spec_adherence.sx` — SpecAdherenceBench
- `simplex-training/src/eval/completion.sx` — CompletionBench
- `simplex-training/src/eval/explanation.sx` — ExplanationBench

### Phase 3: Model Export & Deployment (~400 lines)

- **SubstrateExporter** — export trained Cognitive Substrate model to deployable format
  (GGUF for inference runtime, or native compiled Simplex for specialized models)
- **ModelRegistry** — register trained models with version, benchmark scores, and
  training provenance for `sxpm model` integration
- **InferenceServer** — lightweight inference wrapper that loads a trained model and
  serves predictions via function call or IPC
- **ToolchainIntegration** — hooks for integrating trained models into sxlsp, sxc,
  sxlint, and other tools

Files:
- `simplex-training/src/export/substrate.sx` — SubstrateExporter
- `simplex-training/src/export/registry.sx` — ModelRegistry
- `simplex-training/src/inference/server.sx` — InferenceServer
- `simplex-training/src/inference/toolchain.sx` — ToolchainIntegration

### Phase 4: Tests (~300 lines)

Location: `tests/training_framework/`

- SubstrateTrainer runs one epoch without error on synthetic data
- CheckpointManager save/load round-trips model state exactly
- CompilationBench correctly distinguishes compilable from non-compilable code
- TestPassBench correctly identifies tests that catch planted bugs
- ModelRegistry stores and retrieves model metadata
- Training loss decreases over 100 steps on synthetic task

## Success Criteria

- [ ] SubstrateTrainer trains a 1M-parameter test model end-to-end in Simplex
- [ ] Different optimizers correctly applied to different model components
- [ ] Checkpoint save/load produces identical model state
- [ ] All 6 benchmarks produce meaningful scores on a trained test model
- [ ] Exported model loads and runs inference in the Simplex runtime
- [ ] Training is deterministic (same seed → same result)

## Estimated Scope

~1,800 lines across library code and tests.
