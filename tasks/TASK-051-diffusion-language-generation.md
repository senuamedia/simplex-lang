# TASK-051: Diffusion Language Generation

**Version:** 0.17.0
**Status:** Complete
**Priority:** P2 — Medium
**Depends on:** v0.16.0 release, existing tensor/layer infrastructure

## Why This Feature Is Needed

Every language model today generates text one token at a time, left to right. This
autoregressive paradigm has a fundamental bottleneck: **generation time scales linearly
with output length, and the model cannot revise earlier tokens based on later context.**
The first word of a 500-word response is committed before the model has any idea what
the last word will be.

Diffusion language models break this constraint entirely. Instead of generating one
token at a time, they:

1. Start with a fully masked (or random) sequence of the target length
2. Iteratively refine ALL tokens in parallel
3. Each step unmasks or corrects tokens across the entire sequence simultaneously

LLaDA (Large Language Diffusion Architecture) demonstrated in 2025-2026 that diffusion
language models can match LLaMA 3's performance while generating tokens **10x faster**
on hardware with parallel compute. Dream showed strong instruction-following capability
with the diffusion paradigm.

The mathematical foundation is discrete diffusion — adapting the continuous diffusion
framework (which revolutionized image generation via DALL-E, Stable Diffusion, Midjourney)
to discrete token spaces. The forward process randomly masks tokens. The reverse process
learns to predict masked tokens conditioned on the unmasked context — generating text by
progressively revealing it, like developing a photograph.

For Simplex specialist SLMs, diffusion generation means:

- **Parallel generation.** All tokens generated simultaneously on parallel hardware.
- **Global coherence.** Later tokens influence earlier tokens through iterative
  refinement — the response is globally planned, not locally committed.
- **Length awareness.** The model naturally generates outputs of appropriate length,
  not truncated or padded.
- **Contract-governed refinement.** Each refinement step can be checked against
  Simplex's neural gate contracts — reject incoherent intermediate states.

## Why It Adds Value

1. **10x faster generation on parallel hardware.** Autoregressive generation is
   inherently sequential — each token depends on the previous. Diffusion generates
   all tokens in parallel across refinement steps. On GPUs, NPUs, and multi-core CPUs,
   this translates to dramatic speedup.

2. **Global planning.** The model sees the entire response at every refinement step.
   It can plan the ending before committing the beginning. Responses are structurally
   coherent in ways autoregressive models struggle with.

3. **Natural editing and revision.** Diffusion inherently supports editing — mask out
   part of a response and re-diffuse to generate revisions. Fill in the middle, rewrite
   a paragraph, extend a conclusion — all native operations.

4. **Length-aware generation.** Research shows diffusion language models are natively
   length-aware — they generate appropriately-length responses without explicit length
   control. "Write a haiku" and "write an essay" naturally produce different lengths.

5. **Composability with contracts.** Each refinement step produces a complete (if noisy)
   output. Simplex contracts can check each intermediate output, rejecting refinement
   paths that violate constraints early.

6. **Complementary to autoregressive.** Not a replacement — an alternative. Some tasks
   suit autoregressive (streaming, real-time chat). Others suit diffusion (document
   generation, code completion, batch processing). Simplex specialists choose the right
   generation paradigm per task.

## Why It Changes Systems Built With Simplex

Diffusion generation opens new specialist capabilities:

- **Document-level specialists.** A legal specialist generating a contract produces the
  entire document at once, ensuring structural coherence (preamble references conclusion,
  clauses reference each other) through iterative refinement.

- **Code generation specialists.** A code specialist generates an entire function body
  simultaneously, ensuring type consistency, variable scoping, and logical flow through
  global refinement — not hoping that token 500 is consistent with token 1.

- **Batch processing.** Specialists processing queues of requests generate multiple
  responses in parallel. Throughput scales with hardware parallelism, not with response
  length.

- **Interactive editing.** A user highlights a paragraph and says "rewrite this." The
  diffusion specialist masks those tokens and re-generates — natively supported by the
  architecture, not hacked on top.

- **Contract-checked generation.** Each refinement step checked against ensures/requires.
  The specialist can detect early that a response is heading toward constraint violation
  and redirect before wasting compute on a doomed generation path.

## Deliverables

### Phase 1: Discrete Diffusion Core (~500 lines)

Location: `simplex-learning/src/diffusion/`

- **MaskSchedule** — define how tokens are masked during the forward (corruption)
  process: linear, cosine, or learned schedule
- **DiscreteDiffusion** — the core discrete diffusion framework: forward masking process
  and reverse denoising process
- **MaskPredictor** — transformer-based model that predicts masked tokens given
  partially masked sequence
- **DiffusionSampler** — sampling strategies: ancestral, top-k filtering at each
  refinement step, nucleus sampling per-step

```simplex
/// Discrete diffusion for language generation
struct DiscreteDiffusion {
    mask_schedule: MaskSchedule,
    predictor: MaskPredictor,
    num_steps: usize,        // refinement steps (typically 10-50)
    vocab_size: usize,
}

/// How aggressively to mask at each timestep
struct MaskSchedule {
    schedule_type: ScheduleType,  // Linear, Cosine, Learned
}

impl MaskSchedule {
    /// Fraction of tokens masked at timestep t (0 = fully unmasked, 1 = fully masked)
    fn mask_ratio(self: &Self, t: f64) -> f64 {
        match self.schedule_type {
            ScheduleType::Linear => t,
            ScheduleType::Cosine => 0.5 * (1.0 + (PI * t).cos()),
            ScheduleType::Learned(params) => learned_schedule(t, params),
        }
    }
}

impl DiscreteDiffusion {
    /// Forward process: corrupt clean sequence by masking tokens
    fn forward_corrupt(self: &Self, clean: &[usize], t: f64) -> Vec<usize> {
        let mask_ratio = self.mask_schedule.mask_ratio(t);
        clean.iter().map(|&token| {
            if random() < mask_ratio { MASK_TOKEN } else { token }
        }).collect()
    }

    /// Reverse process: generate by iteratively unmasking
    fn generate(self: &Self, length: usize) -> Vec<usize> {
        // Start fully masked
        let mut sequence = vec![MASK_TOKEN; length];

        // Iteratively unmask
        for step in (0..self.num_steps).rev() {
            let t = step as f64 / self.num_steps as f64;
            let predictions = self.predictor.predict(&sequence, t);

            // Determine which tokens to unmask at this step
            let unmask_count = self.tokens_to_unmask(step, length);
            let most_confident = predictions.top_k_confident(unmask_count);

            for (pos, token) in most_confident {
                if sequence[pos] == MASK_TOKEN {
                    sequence[pos] = token;
                }
            }
        }
        sequence
    }

    /// Training loss: predict masked tokens from partially masked input
    fn loss(self: &Self, clean: &[usize]) -> Dual {
        let t = random_uniform(0.0, 1.0);
        let corrupted = self.forward_corrupt(clean, t);
        let predictions = self.predictor.predict(&corrupted, t);

        // Cross-entropy only on masked positions
        let mut total_loss = Dual::zero();
        let mut count = 0;
        for (i, &token) in clean.iter().enumerate() {
            if corrupted[i] == MASK_TOKEN {
                total_loss = total_loss + cross_entropy(predictions.logits(i), token);
                count += 1;
            }
        }
        total_loss / count as f64
    }
}
```

Files:
- `simplex-learning/src/diffusion/mod.sx` — module root
- `simplex-learning/src/diffusion/schedule.sx` — MaskSchedule
- `simplex-learning/src/diffusion/core.sx` — DiscreteDiffusion
- `simplex-learning/src/diffusion/predictor.sx` — MaskPredictor
- `simplex-learning/src/diffusion/sampler.sx` — sampling strategies

### Phase 2: Generation Modes & Control (~400 lines)

- **InfillGeneration** — mask middle of sequence and regenerate (code/document editing)
- **GuidedDiffusion** — steer generation toward satisfying constraints at each step
- **LengthControlledGeneration** — generate sequences of target length naturally
- **ParallelBatchGeneration** — generate multiple sequences in parallel for batch
  throughput

```simplex
/// Guided diffusion: each refinement step is checked against contracts
struct GuidedDiffusion {
    base: DiscreteDiffusion,
    guide: Box<dyn Fn(&[usize]) -> f64>,  // scoring function for guidance
    guidance_scale: f64,
}

impl GuidedDiffusion {
    fn generate(self: &Self, length: usize) -> Vec<usize> {
        let mut sequence = vec![MASK_TOKEN; length];

        for step in (0..self.base.num_steps).rev() {
            let t = step as f64 / self.base.num_steps as f64;

            // Generate multiple candidates per step
            let candidates: Vec<Vec<usize>> = (0..8)
                .map(|_| self.base.unmask_step(&sequence, t))
                .collect();

            // Score candidates and select best
            let best = candidates.into_iter()
                .max_by_key(|c| (self.guide)(c))
                .unwrap();

            sequence = best;
        }
        sequence
    }
}

/// Infill: mask a region and regenerate
fn infill(diffusion: &DiscreteDiffusion, sequence: &[usize],
          start: usize, end: usize) -> Vec<usize> {
    let mut masked = sequence.to_vec();
    for i in start..end {
        masked[i] = MASK_TOKEN;
    }
    // Run reverse process only on masked region
    diffusion.generate_from_partial(&masked)
}
```

Files:
- `simplex-learning/src/diffusion/infill.sx` — infill generation
- `simplex-learning/src/diffusion/guided.sx` — guided diffusion with constraints
- `simplex-learning/src/diffusion/length.sx` — length-controlled generation
- `simplex-learning/src/diffusion/batch.sx` — parallel batch generation

### Phase 3: Specialist Integration (~350 lines)

- **DiffusionSpecialist** — cognitive hive specialist with diffusion generation head
- **HybridGeneration** — choose autoregressive or diffusion per-query based on task
- **ContractRefinement** — check neural gate contracts at each refinement step, reject
  violating intermediate states
- **DiffusionStreaming** — emit partially refined output for streaming UX (show
  progressive refinement to user)

Files:
- `simplex-learning/src/diffusion/specialist.sx` — DiffusionSpecialist
- `simplex-learning/src/diffusion/hybrid.sx` — autoregressive/diffusion switching
- `simplex-learning/src/diffusion/contracts.sx` — contract-checked refinement
- `simplex-learning/src/diffusion/streaming.sx` — progressive output streaming

### Phase 4: Tests (~350 lines)

Location: `tests/diffusion/`

- Forward corruption at t=1.0 produces fully masked sequence
- Forward corruption at t=0.0 produces clean sequence
- Reverse process generates valid token sequences (all non-MASK)
- Infill correctly regenerates masked region while preserving context
- Guided diffusion produces higher-scoring outputs than unguided
- Diffusion generation parallelizes across batch dimension

## Success Criteria

- [ ] Reverse process generates fully unmasked sequences within num_steps
- [ ] Training loss decreases over training steps on synthetic sequence task
- [ ] Infill produces coherent text in masked region given surrounding context
- [ ] Guided diffusion scores higher than unguided on synthetic scoring function
- [ ] Batch generation scales linearly with hardware parallelism
- [ ] ContractRefinement rejects intermediate states that violate constraints

## Estimated Scope

~1,600 lines across library code and tests.
