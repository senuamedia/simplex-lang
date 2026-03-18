# TASK-039: Information Geometry & Natural Gradients

**Version:** 0.16.0
**Status:** Planned
**Priority:** P1 — High
**Depends on:** v0.15.0 release, dual numbers (TASK-005), existing optimizer infrastructure

## Why This Feature Is Needed

Every optimizer in every ML framework (SGD, Adam, AdamW) treats the parameter space as
flat. They compute gradients in Euclidean space and step along those gradients. This is
mathematically wrong. Neural network loss landscapes are *curved manifolds* — a step of
0.01 in one direction might change the model's output distribution dramatically, while a
step of 1.0 in another direction might change nothing.

Information geometry fixes this. The Fisher Information Matrix (FIM) describes the local
curvature of the probability distribution space. The *natural gradient* — the gradient
preconditioned by the inverse FIM — steps in the direction of steepest descent on the
*statistical manifold*, not the parameter manifold. This means:

- Convergence in 10-100x fewer steps for many problems
- Invariance to parameterization (the same model with different parameterizations
  produces the same natural gradient)
- Mathematically principled learning rate: the step size has meaning relative to how
  much the output distribution changes

The problem has always been cost: computing and inverting the full FIM is O(n²) in
parameters. But Kronecker-Factored Approximate Curvature (K-FAC) makes this practical by
approximating the FIM as a Kronecker product of two smaller matrices — one per layer.
This reduces the cost to near-zero overhead.

No compiled language has natural gradient as a built-in optimizer. Even in Python, K-FAC
implementations are rare and fragile. In Simplex, `NaturalGradient` is a first-class
optimizer that the compiler can fuse with the backward pass, computing the FIM
approximation during backpropagation with zero extra memory overhead.

## Why It Adds Value

1. **Faster training convergence.** Natural gradient is the optimal descent direction
   on the statistical manifold. Specialist SLMs converge faster, requiring less data
   and less compute for the same quality.

2. **Better fine-tuning.** When adapting a pre-trained SLM to a new domain (via LoRA or
   full fine-tuning), natural gradient prevents catastrophic steps that destroy the
   pre-trained knowledge. The FIM encodes which parameters are important for the current
   distribution.

3. **Principled learning rate.** The natural gradient automatically scales step size
   based on how much the model's output distribution changes. No more learning rate
   tuning — the geometry tells you the right scale.

4. **Integration with dual numbers.** Simplex's dual number infrastructure can compute
   FIM entries during the forward pass as a byproduct of automatic differentiation.
   This is a compiler opportunity no other language can exploit.

5. **Trust region optimization.** The FIM defines a natural trust region (KL-divergence
   ball). Natural gradient steps stay within this region, preventing the large,
   destructive updates that cause training instability.

6. **Meta-learning geometry.** The existing meta-gradient system (learning temperature
   schedules, learning rates) operates in flat space. Information geometry makes
   meta-learning geometrically aware — learning *how to learn* on the right manifold.

## Why It Changes Systems Built With Simplex

Information geometry changes how specialist SLMs are trained and adapted:

- **10x faster specialist training.** A coding specialist SLM that took 10 hours to
  fine-tune now converges in 1 hour with natural gradient, because the optimizer
  understands the curvature of the loss landscape.

- **Stable continual learning.** The Cognitive Hive's online learning (federated
  updates, real-time adaptation) becomes geometrically stable. The FIM acts as a
  regularizer: "don't change parameters that are important for what you already know."
  This directly prevents catastrophic forgetting without needing EWC as a separate
  mechanism.

- **Automatic learning rate.** Deployment teams no longer need to tune learning rates
  for each specialist. The natural gradient adapts to the local curvature automatically.

- **Principled model merging.** When merging specialist models (federated averaging),
  the FIM tells you *how* to merge: weight each parameter by its curvature, so important
  parameters are changed less. Better merging = better federated learning across hives.

## Deliverables

### Phase 1: Fisher Information Framework (~400 lines)

Location: `simplex-learning/src/geometry/`

- **FisherMatrix** — compute the empirical Fisher information matrix (full and diagonal)
- **FisherDiagonal** — efficient diagonal FIM approximation (per-parameter curvature)
- **KroneckerFactor** — Kronecker-factored FIM approximation (K-FAC): one pair of
  smaller matrices per layer
- **StatisticalManifold trait** — any probability distribution becomes a geometric object
  with a metric tensor

```simplex
/// The Fisher Information Matrix for a model
struct FisherMatrix {
    factors: Vec<KroneckerFactor>,  // one per layer (K-FAC)
    diagonal: Option<Vec<f64>>,     // optional diagonal approximation
    damping: f64,                    // Tikhonov damping for inversion
}

/// Kronecker-factored approximation: F ≈ A ⊗ B per layer
struct KroneckerFactor {
    A: Tensor,  // input covariance (activation statistics)
    B: Tensor,  // gradient covariance (backprop statistics)
    A_inv: Option<Tensor>,  // cached inverse
    B_inv: Option<Tensor>,
}

impl KroneckerFactor {
    /// Update running statistics from a minibatch
    fn update(mut self: &mut Self, activations: &Tensor, gradients: &Tensor, decay: f64) {
        // Exponential moving average of Kronecker factors
        self.A = decay * self.A + (1.0 - decay) * activations.t().matmul(activations);
        self.B = decay * self.B + (1.0 - decay) * gradients.t().matmul(gradients);
        // Invalidate cached inverses
        self.A_inv = None;
        self.B_inv = None;
    }

    /// Compute natural gradient: F^{-1} g ≈ (A^{-1} ⊗ B^{-1}) g
    fn precondition(mut self: &mut Self, gradient: &Tensor, damping: f64) -> Tensor {
        if self.A_inv.is_none() {
            self.A_inv = Some((self.A + damping * Tensor::eye(self.A.rows())).inverse());
        }
        if self.B_inv.is_none() {
            self.B_inv = Some((self.B + damping * Tensor::eye(self.B.rows())).inverse());
        }
        // Efficient Kronecker product inversion
        self.B_inv.unwrap().matmul(gradient).matmul(&self.A_inv.unwrap())
    }
}
```

Files:
- `simplex-learning/src/geometry/mod.sx` — module root
- `simplex-learning/src/geometry/fisher.sx` — FisherMatrix and FisherDiagonal
- `simplex-learning/src/geometry/kronecker.sx` — KroneckerFactor (K-FAC)
- `simplex-learning/src/geometry/manifold.sx` — StatisticalManifold trait

### Phase 2: Natural Gradient Optimizer (~400 lines)

- **NaturalGradient** — optimizer that preconditions gradients by inverse FIM (via K-FAC)
- **NaturalAdam** — Adam with natural gradient preconditioning (best of both worlds)
- **TrustRegionStep** — constrain step size by KL divergence (natural trust region)
- **DampingScheduler** — adaptive damping that balances curvature approximation accuracy
  with numerical stability

```simplex
/// Natural gradient optimizer using K-FAC approximation
struct NaturalGradient {
    fisher: FisherMatrix,
    learning_rate: f64,
    damping: DampingScheduler,
    momentum: f64,
    velocity: Vec<Tensor>,  // momentum buffer
    update_interval: usize,  // how often to recompute Fisher factors
    steps: usize,
}

impl Optimizer for NaturalGradient {
    fn step(mut self: &mut Self, params: &mut Vec<Tensor>, grads: &[Tensor]) {
        self.steps += 1;

        // Update Fisher factors periodically (not every step)
        if self.steps % self.update_interval == 0 {
            self.fisher.update_from_batch();
        }

        // Precondition each layer's gradient by its K-FAC inverse
        for (i, (param, grad)) in params.iter_mut().zip(grads).enumerate() {
            let natural_grad = self.fisher.factors[i]
                .precondition(grad, self.damping.current());

            // Apply momentum
            self.velocity[i] = self.momentum * &self.velocity[i]
                + (1.0 - self.momentum) * &natural_grad;

            *param = &*param - self.learning_rate * &self.velocity[i];
        }

        self.damping.step();
    }
}
```

Files:
- `simplex-learning/src/geometry/natural_gradient.sx` — NaturalGradient optimizer
- `simplex-learning/src/geometry/natural_adam.sx` — NaturalAdam hybrid optimizer
- `simplex-learning/src/geometry/trust_region.sx` — KL trust region enforcement
- `simplex-learning/src/geometry/damping.sx` — adaptive damping scheduler

### Phase 3: Applications & Integration (~350 lines)

- **GeometricEWC** — Elastic Weight Consolidation using the FIM directly (the
  mathematically correct version), replacing the current approximation
- **FisherMerging** — merge specialist models weighted by Fisher information (better
  federated averaging)
- **GeometricMetaLearning** — meta-gradients on the statistical manifold for learning
  rate and temperature schedule optimization
- **CurvatureAware pruning** — prune parameters with lowest Fisher information (least
  impact on output distribution)

Files:
- `simplex-learning/src/geometry/ewc.sx` — Fisher-based continual learning
- `simplex-learning/src/geometry/merging.sx` — Fisher-weighted model merging
- `simplex-learning/src/geometry/meta.sx` — geometric meta-learning
- `simplex-learning/src/geometry/pruning.sx` — curvature-aware pruning

### Phase 4: Tests (~350 lines)

Location: `tests/geometry/`

- K-FAC approximation quality vs full Fisher (on small model)
- NaturalGradient converges faster than Adam on ill-conditioned problem
- Trust region prevents large KL steps
- Fisher merging produces better ensemble than naive averaging
- Geometric EWC prevents catastrophic forgetting on sequential tasks
- Curvature-aware pruning outperforms magnitude pruning

## Success Criteria

- [ ] K-FAC approximation within 10% of true Fisher on 2-layer test model
- [ ] NaturalGradient converges in <50% steps vs Adam on Rosenbrock function
- [ ] Trust region constraint holds (KL divergence below threshold per step)
- [ ] Fisher merging reduces test loss vs naive parameter averaging
- [ ] Geometric EWC maintains >90% accuracy on old task after learning new task
- [ ] All geometry types compose with existing dual number infrastructure

## Estimated Scope

~1,500 lines across library code and tests.
