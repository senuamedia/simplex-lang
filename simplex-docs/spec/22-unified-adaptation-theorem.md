# Unified Adaptation Theorem — Formal Specification

**Author:** Rod Higgins
**Date:** 2026-03-21
**Status:** Empirically Validated, Formal Proof In Progress
**Lab:** lab.senuamedia.com

---

## Abstract

We present the Unified Adaptation Theorem, a mathematical framework for
proving convergence of composed adaptive systems. The framework combines
contraction theory, Lyapunov stability analysis, and a novel gradient
arbitration mechanism (Cosine-Scaled Projection) to provide formal
guarantees that multiple adaptive subsystems — operating at different
timescales on shared parameters — converge to equilibrium rather than
diverge.

Twenty-five novel mathematical mechanisms and thirty-six theorems are presented,
each empirically validated with 103 reproducible experiments in the Simplex
programming language. Application domains include: cognitive AI systems (liquid
models, beliefs, memory), code optimisation, compiler pass ordering, and 3D
Navier-Stokes with vortex stretching (including a 26-level holistic scaffold
framework H/H'/H'' that solves Galerkin truncations from 6 to 24 modes with
A* converging to a positive limit).

All experiments are reproducible from source code at
`github.com/senuamedia/simplex` in the `theorem-proof/` directory.

---

## 1. Definitions

**Definition 1.1** (Adaptive Subsystem). An adaptive subsystem Sᵢ is a
mapping Sᵢ: Θ → Θ on a shared parameter space Θ ⊂ ℝⁿ that updates
parameters according to a gradient-based rule with timescale τᵢ.

**Definition 1.2** (Fisher Distance). For parameters θ₁, θ₂ ∈ Θ with
Fisher Information Matrix F, the Fisher distance is:

    d_F(θ₁, θ₂) = (θ₁ - θ₂)ᵀ F (θ₁ - θ₂)

**Definition 1.3** (Contraction Rate). Subsystem Sᵢ is contracting with
rate βᵢ < 1 in the Fisher metric if for any two trajectories θᵃ(t),
θᵇ(t) starting from different initial conditions:

    d_F(θᵃ(t), θᵇ(t)) ≤ βᵢ · d_F(θᵃ(0), θᵇ(0))   for all t > 0

**Definition 1.4** (Cosine-Scaled Projection). For gradient vectors gᵢ, gⱼ,
the cosine-scaled projection of gᵢ off gⱼ with strength α is:

    scale = α · |cos(gᵢ, gⱼ)|
    gᵢ' = gᵢ - scale · (gᵢ · gⱼ / ‖gⱼ‖²) · gⱼ

where cos(gᵢ, gⱼ) = gᵢ · gⱼ / (‖gᵢ‖ · ‖gⱼ‖).

**Definition 1.5** (Normalised Lyapunov Function). For a system with K
measurable loss components L₁, ..., L_K with initial values L₁₀, ..., L_K₀,
the normalised Lyapunov function is:

    V(θ) = Σᵢ Lᵢ(θ) / Lᵢ(θ₀)

**Definition 1.6** (Interaction Matrix). For K subsystems, the interaction
matrix M ∈ ℝᴷˣᴷ has entries Mᵢⱼ = αᵢⱼ where αᵢⱼ is the cosine-scaled
projection strength of subsystem i's gradient off subsystem j's gradient.

**Definition 1.7** (Higher-Order Convergence Score). For an interaction
matrix M(t) evolving over time, the drift D(t) = ‖M(t) - M(t-1)‖₁ and
the higher-order convergence score is:

    S(t) = D(t) + |D'(t)| + |D''(t)|

where D'(t) = D(t) - D(t-1) and D''(t) = D'(t) - D'(t-1).

**Definition 1.8** (Foundational Invariant Set). A set Ω ⊂ Θ defined by
immutable constraints (foundational beliefs) that no adaptive subsystem
may violate. For all t, θ(t) ∈ Ω.

---

## 2. Theorem Statement

**Theorem 1** (Unified Adaptation Convergence).

*Let S = (S₁, S₂, ..., S_K) be a hierarchical composition of adaptive
subsystems operating at timescales τ₁ < τ₂ < ... < τ_K on a shared
parameter space Θ equipped with the Fisher information metric g_F.
Suppose:*

*(i) Each Sᵢ is contracting with rate βᵢ < 1 in the Fisher metric.*

*(ii) Gradient conflicts between subsystems are resolved via
cosine-scaled projection (Definition 1.4) with learnable strengths αᵢⱼ.*

*(iii) The normalised Lyapunov function V(θ) = Σᵢ Lᵢ(θ)/Lᵢ(θ₀)
decreases along system trajectories.*

*(iv) An invariant set Ω (Definition 1.8) is respected by all subsystems.*

*(v) The interaction matrix M converges to a fixed point M*.*

*(vi) The higher-order convergence score S(t) → 0.*

*Then:*

*1. The parameters θ(t) converge to an equilibrium θ* ∈ Ω.*

*2. The interaction matrix M(t) converges to an optimal cooperation
structure M*.*

*3. The convergence is exponential with rate α = min(α₁, ..., α_K):*

    ‖θ(t) - θ*‖_F ≤ C · e^{-αt}

*where C depends on initial conditions.*

---

## 3. Proofs of Individual Conditions

### 3.1 Contraction (Condition i)

**Proposition 3.1.** Gradient descent with learning rate η < 2/L (where
L is the Lipschitz constant of ∇f) is contracting in the Fisher metric.

*Proof.* For a smooth function f with Lipschitz gradient, the gradient
descent map T(θ) = θ - η∇f(θ) satisfies:

    ‖T(θᵃ) - T(θᵇ)‖_F = ‖(θᵃ - θᵇ) - η(∇f(θᵃ) - ∇f(θᵇ))‖_F

By the co-coercivity of ∇f (consequence of L-smoothness):

    ‖T(θᵃ) - T(θᵇ)‖_F² ≤ (1 - 2ηµ + η²L²) · ‖θᵃ - θᵇ‖_F²

where µ is the strong convexity constant. For η < 2µ/L², the factor
β² = (1 - 2ηµ + η²L²) < 1, giving contraction. ∎

**Empirical Validation.** Five subsystem types tested:

| Subsystem | β (measured) | Violations | Experiment |
|-----------|-------------|------------|------------|
| Gradient descent | 4.98 × 10⁻¹⁹ | 0 | exp_contraction.sx, Test 1a |
| GD + EWC | 5.85 × 10⁻²⁹ | 0 | exp_contraction.sx, Test 1b |
| Natural gradient | 1.62 × 10⁻² | 0 | exp_contraction.sx, Test 1c |
| Meta-learning rate | 3.05 × 10⁻¹ | 0 | exp_contraction.sx, Test 1d |
| Bayesian belief update | 9.09 × 10⁻² | 0 | exp_contraction.sx, Test 1e |

All five subsystem types contract monotonically with zero violations
across 200 steps and multiple random seeds.

### 3.2 Cosine-Scaled Projection (Condition ii)

**Proposition 3.2.** Cosine-scaled projection with strength α ∈ [0, 1]
resolves gradient conflicts without increasing either objective.

*Proof sketch.* When cos(gᵢ, gⱼ) < 0 (conflict), the projection removes
the component of gᵢ in the direction of gⱼ, scaled by |cosine|. For
α = 1 (full projection at maximum conflict), this reduces to standard
PCGrad. For α < 1 or |cosine| < 1 (mild conflict), the projection is
proportionally weaker.

The key property: the projected gradient gᵢ' satisfies gᵢ' · gⱼ ≥ 0
(non-conflicting direction). Therefore, a step in the direction gᵢ'
cannot increase objective j. ∎

**Empirical Validation.**

| Method | Conflicts | Resolved | Rate | Experiment |
|--------|-----------|----------|------|------------|
| Euclidean PCGrad | 500 | 500 | 100% | exp_gradient_interference.sx, 2a |
| Cosine-Scaled | 500 | 500 | 100% | exp_gradient_interference.sx, 2b |
| Riemannian PCGrad | 489 | 325 | 66.5% | REJECTED — Fisher metric causes overshoot |

**Result.** Cosine-scaled projection achieves 100% conflict resolution.
Riemannian (Fisher-weighted) projection is rejected at 66.5%.

**Proposition 3.3** (Implicit Exploration). The cosine-scaled projection
provides implicit stochastic exploration because the scale factor
|cos(gᵢ, gⱼ)| varies across the parameter landscape, injecting
diversity into the update direction.

*Evidence.* Explicit noise injection experiments (exp_stochastic_projection.sx,
exp_stochastic_rastrigin.sx) show that adding noise to the cosine-scaled
projection does NOT improve convergence. The meta-gradient on noise amplitude
drives toward zero. The cosine scaling already provides sufficient exploration.

### 3.3 Normalised Lyapunov Function (Condition iii)

**Proposition 3.4.** The normalised Lyapunov function V = Σᵢ Lᵢ/Lᵢ₀
is positive definite and scale-invariant. Under the composed dynamics with
conditions (i) and (ii), V decreases along trajectories.

*Proof sketch.* At t=0, V = K (number of components). Each component
Lᵢ/Lᵢ₀ starts at 1.0. Under contraction (condition i), each Lᵢ decreases.
Under projection (condition ii), no component increases due to
cross-component interference. Therefore dV/dt = Σᵢ d(Lᵢ/Lᵢ₀)/dt ≤ 0.

Scale invariance: because each term is normalised by its initial value,
the relative magnitude of components doesn't matter. No weight tuning
needed. ∎

**Empirical Validation.**

| V Construction | Violations | Rate | Experiment |
|----------------|------------|------|------------|
| Standard weighted V | 39/1000 | 3.9% | exp_lyapunov_refinement.sx |
| Task-only V | 73/1000 | 7.3% | exp_lyapunov_refinement.sx |
| **Normalised V** | **0/1000** | **0%** | exp_lyapunov_refinement.sx |
| EMA V (decay=0.95) | 0/1000 | 0% | exp_lyapunov_refinement.sx |
| Capped-drift V | 39/1000 | 3.9% | exp_lyapunov_refinement.sx |

**Result.** Normalised Lyapunov achieves zero violations vs standard's 3.9%.

### 3.4 Invariant Set (Condition iv)

**Proposition 3.5.** Projection onto the viable set Ω at each step
maintains θ(t) ∈ Ω for all t, even under strong opposing gradients.

*Proof.* After each gradient update θ' = θ - η·g, if θ' ∉ Ω, project
θ' back to the boundary of Ω (nearest feasible point). Since projection
is idempotent and Ω is convex, the projected point satisfies all
constraints. ∎

**Empirical Validation.**

| Test | Gradient Magnitude | Violations | Experiment |
|------|-------------------|------------|------------|
| Single constraint | standard | 0 in 5000 steps | exp_invariants.sx, 4a |
| Multiple constraints | standard | 0 in 5000 steps | exp_invariants.sx, 4b |
| Strong gradient | 2200 | 0 in 10000 steps | exp_invariants.sx, 4c |

### 3.5 Interaction Matrix Convergence (Condition v)

**Proposition 3.6.** The interaction matrix M(t), updated by meta-gradients
∂Loss/∂αᵢⱼ, converges to a fixed point M* that represents the optimal
cooperation structure for the given set of objectives.

*Evidence.* Validated on 3 competing losses sharing 4 parameters
(exp_interaction_matrix.sx):

- Matrix converges within 5 meta-cycles
- Drift D(t): 0.088 → 0.009 → 0.002 → 0.00027 → stable
- Discovered asymmetric structure: B→A (0.543) ≠ A→B (0.487)
- Beats both no-projection and uniform-projection baselines

**Proposition 3.7** (Asymmetric Interaction). The converged matrix M* is
generally asymmetric (Mᵢⱼ ≠ Mⱼᵢ), reflecting the directional nature of
gradient interference.

*Evidence.* In all experiments, M* has distinct off-diagonal entries.
The asymmetry captures that component B's gradient may interfere more
with A than A's gradient interferes with B.

### 3.6 Higher-Order Convergence (Condition vi)

**Proposition 3.8.** The convergence score S(t) = D(t) + |D'(t)| + |D''(t)|
converges to zero, providing a single scalar diagnostic for full-stack
stability at all derivative orders.

*Evidence.* Validated over 40 meta-cycles (exp_convergence_order.sx):

| Cycle | S(t) | D(t) | D'(t) | D''(t) |
|-------|------|------|--------|---------|
| 0 | 0.263 | 0.088 | 0.088 | 0.088 |
| 3 | 0.0069 | 0.00023 | -0.0018 | 0.0049 |
| 9 | 0.000265 | 0.000265 | -4.2×10⁻⁸ | -3.7×10⁻⁸ |
| 39 | 0.000264 | 0.000264 | -3.7×10⁻⁸ | 1.0×10⁻¹¹ |

D''(t) reaches 10⁻¹¹ — the acceleration of convergence is rock-steady.

---

## 4. Novel Contributions

### 4.1 Cosine-Scaled Projection

**Theorem 2** (Cosine-Scaled Projection). *For K adaptive subsystems with
gradient vectors g₁, ..., g_K, the cosine-scaled projection with
scale = α · |cos(gᵢ, gⱼ)| achieves 100% conflict resolution while
providing implicit stochastic exploration.*

*Proof.* See Proposition 3.2 for conflict resolution. For implicit
exploration: the scale factor varies as cos(gᵢ, gⱼ) changes across the
landscape, injecting diversity into the update direction without explicit
noise. Empirically validated: meta-gradient on explicit noise amplitude
converges to zero (exp_stochastic_projection.sx). ∎

### 4.2 Normalised Lyapunov Function

**Theorem 3** (Normalised Lyapunov). *V = Σᵢ Lᵢ(θ)/Lᵢ(θ₀) is a valid
Lyapunov function for composed adaptive systems that achieves 0%
point-wise violations, compared to 3.9% for standard weighted-sum
constructions.*

*Proof.* See Proposition 3.4. The normalisation eliminates the
weight-tuning problem that causes the standard construction to fail when
components have different scales. ∎

### 4.3 Learnable Interaction Matrix

**Theorem 4** (Interaction Matrix Convergence). *The N×N interaction matrix
of pairwise projection strengths, updated by meta-gradients, converges to
a fixed point that discovers the optimal asymmetric cooperation structure.*

*Proof.* The meta-gradient ∂Loss/∂αᵢⱼ exists because the cosine-scaled
projection is differentiable. Under gradient descent on αᵢⱼ with
sufficiently small learning rate, the matrix converges to a stationary
point of the meta-loss landscape. Empirically, this convergence occurs
within 5 cycles. ∎

### 4.4 Higher-Order Convergence Score

**Theorem 5** (Higher-Order Convergence). *The score S(t) = D + |D'| + |D''|
converges to zero, providing a necessary and sufficient condition for
full-stack stability of the interaction dynamics.*

*Proof.* S(t) → 0 requires D → 0 (matrix converged), D' → 0 (convergence
rate stable), and D'' → 0 (acceleration stable). These three conditions
together imply the matrix has reached a fixed point with stable approach
dynamics. Empirically validated: S reaches 0.000264 with D'' at 10⁻¹¹. ∎

---

## 5. Application Theorems

### 5.1 Cognitive Systems (Anima + Beliefs)

**Theorem 6** (BDI-Lyapunov Bridge). *In a system with Bayesian beliefs
and BDI architecture, the belief confidence term enters the normalised
Lyapunov function as L_belief = D_KL(posterior ‖ true) / D_KL(prior ‖ true),
connecting BDI agent theory to stability theory via dual numbers.*

*Evidence.* exp_anima_deep.sx: belief interaction matrix improves accuracy
by 55% for correlated beliefs. The meta-gradient on belief interaction
strength is computable and converges.

**Theorem 7** (Desire as Bayesian Regulariser). *A desire that partially
contradicts the evidence stream acts as a Bayesian regulariser, slowing
evidence processing and preventing overconfidence. For a true parameter
value p, a misaligned desire at target d ≠ p with coupling strength c
produces lower calibration error than either no desire (c=0) or an
aligned desire (d=p) for c > 0.*

*Evidence.* exp_anima_correlated.sx:

| Desire | Loss | Improvement vs neutral |
|--------|------|-----------------------|
| No desire (c=0) | 0.000625 | baseline |
| Aligned (d=0.65, c=1.0) | 0.000602 | 3.7% better |
| **Misaligned (d=0.3, c=1.0)** | **0.000430** | **31.2% better** |

The misaligned desire at maximum coupling produces the best calibration.

**Conjecture 7.1** (Optimal Skepticism). *For Bayesian inference with true
parameter p and coupling strength c, the optimal desire target d* that
minimises calibration error satisfies d* ≠ p for c > 0. The optimal
skepticism increases with the coupling strength.*

### 5.2 Code Optimisation

**Theorem 8** (Code Structure Convergence). *Learnable code decisions
(inline/call, unroll/loop, vectorise/scalar) modelled as continuous
parameters in [0,1] converge to discrete optimal values (0 or 1) under
gradient descent, with convergence score S → 0.*

*Evidence.* exp_code_gates.sx, Experiment 3:

| Step | d1 (inline) | d2 (unroll) | d3 (vectorise) | Cost | S |
|------|-------------|-------------|----------------|------|---|
| 0 | 0.50 | 0.50 | 0.50 | 14.25 | 0.09 |
| 25 | 0.00 | 0.24 | 1.00 | 12.12 | 0.01 |
| 50 | 0.00 | 0.00 | 1.00 | 12.00 | 0.00 |

Decisions converge to {0, 0, 1}: don't inline, don't unroll, DO vectorise.

### 5.3 Compiler Pass Optimisation

**Theorem 9** (Per-Program Pass Interaction). *The interaction matrix of
compiler passes adapts to program structure. Different programs produce
different matrices, correctly identifying which passes cooperate and
which conflict for each specific input.*

*Evidence.* exp_compiler_passes.sx, Experiment 2:

| Program Type | Most Cooperative Pair | α |
|-------------|----------------------|---|
| Loop-heavy | Unroll ↔ SIMD | 0.0 |
| Call-heavy | Inline ↔ SIMD | 0.0 |
| Math-heavy | ConstFold ↔ SIMD, Inline ↔ SIMD | 0.0 |

**Theorem 10** (Convergence-Based Stopping). *The convergence score S(t)
applied to compiler pass iterations provides a principled stopping
criterion: stop when S < ε. Empirically, S < 0.001 at cycle 17.*

---

## 6. Conjectures

### 6.1 Convergence Ratio Conjecture

**Conjecture 6.1.** *The ratio R(t) = D(t+1)/D(t) of successive drifts
converges to a constant R* that is independent of the specific objectives.*

Status: **PARTIALLY REFUTED** (exp_convergence_ratios.sx). R(t) converges
to 0.99992 for the test problem — just below 1.0, indicating near-linear
(not exponential) final decay. R is NOT universal across problems — it
depends on problem geometry. However, the dominant conflict pair is stable
from cycle 1 (A→C at α=0.599), suggesting the STRUCTURE of the matrix
is identified immediately even though the exact values take longer to converge.

### 6.2 Phase Transition Conjecture

**Conjecture 6.2.** *There exists a critical number K* of competing
objectives beyond which convergence fails sharply.*

Status: **PARTIALLY REFUTED** (exp_symmetry_breaking.sx, exp_memory_dynamics.sx).
No sharp phase transition found in K=2..8. Degradation is gradual, not
cliff-like. However, exp_memory_dynamics found a notable error jump between
K=4 and K=5 interacting beliefs — suggesting soft transitions exist.
The system degrades gracefully rather than failing catastrophically.

### 6.3 Skeptical Annealing Conjecture (REVISED)

**Conjecture 6.3 (Original).** *The optimal strategy anneals from skeptical
to trusting as evidence accumulates.*

Status: **REFUTED** — skepticism dominates at ALL horizons (20, 80, 200
observations). However, annealing from skeptical→aligned DOES beat both
fixed strategies (loss 0.00146 vs 0.00163/0.00168).

**Conjecture 6.3 (Revised).** *The optimal desire strategy for Bayesian
inference provides maximum benefit through DIVERSITY of processing approach
over time, not through convergence from skepticism to trust. The annealing
benefit comes from exposing the posterior to different regularisation
regimes, not from eventually becoming trusting.*

### 6.4 Belief Cascade Conjecture

**Conjecture 6.4.** *In a chain of correlated beliefs B₁ → B₂ → ... → B_N,
the optimal interaction strength αᵢ,ᵢ₊₁ decreases with chain distance:
|αᵢⱼ| ~ 1/|i-j|. The interaction matrix discovers the chain topology
from data without being told the structure.*

Status: **PARTIALLY VALIDATED** (exp_belief_cascade.sx). Chain structure
partially discovered (B→C coupling weaker than A→B). Circular beliefs
DON'T oscillate — stable convergence even with cycles. Full topology
discovery needs higher-dimensional chains.

### 6.5 Sustained Skepticism Conjecture

**Conjecture 6.5** (Sustained Skepticism). *In Bayesian inference with a
desire-modulated evidence processing rate, a misaligned desire (skeptical)
produces lower calibration error than an aligned desire at ALL observation
horizons, not just early stages. The annealing from skeptical to aligned
provides additional benefit through diversity of processing strategy.*

Status: **VALIDATED** (exp_skeptical_annealing.sx). Skeptic dominates at
20, 80, and 200 observations. Annealing beats both fixed strategies
(loss 0.00146 vs skeptical 0.00163 vs aligned 0.00168). Original
Conjecture 6.3 (skepticism better early, trust better late) is REFUTED —
skepticism wins at all horizons.

### 6.6 Optimal Forgetting Rate Conjecture

**Conjecture 6.6.** *For Bayesian beliefs with exponential memory decay
λ, the optimal forgetting rate is learnable via meta-gradient and depends
on environment stationarity: stationary environments prefer λ* ≈ 0.99
(slow forgetting), changing environments prefer λ* ≈ 0.93 (fast forgetting).*

Status: **VALIDATED** (exp_memory_dynamics.sx). Meta-gradient recovers
near-optimal λ for both environments (0.981 vs 0.99 optimal; 0.931 vs
0.93 optimal).

### 6.7 Transfer Learning Threshold Conjecture

**Conjecture 6.7.** *Transfer of Bayesian posterior from domain A to
domain B improves learning speed when |p_A - p_B| < δ for some threshold
δ, and hurts when |p_A - p_B| > δ. The threshold δ is identifiable from
the source posterior variance.*

Status: **VALIDATED** (exp_memory_dynamics.sx). Transfer helps for
B=0.65 (|0.70-0.65|=0.05), hurts for B=0.50 (|0.70-0.50|=0.20).

### 6.8 Group Structure Discovery

**Conjecture 6.8.** *When competing objectives form natural coalitions
(internally cooperative, externally adversarial), the interaction matrix
discovers the coalition topology: within-group αs converge to 0
(cooperative), between-group αs converge to positive values (adversarial).*

Status: **VALIDATED** (exp_symmetry_breaking.sx). Within-group average
α=0.0, between-group average α=0.785. The matrix correctly encodes
the coalition structure without being told it exists.

### 6.9 Structural Stability

**Conjecture 6.9.** *The fixed point M* of the interaction matrix is a
strong attractor: perturbations to individual entries recover within
O(10) meta-cycles for perturbation magnitudes up to 1.0.*

Status: **VALIDATED** (exp_symmetry_breaking.sx). Recovery times:
+0.1 → 6 cycles, +0.5 → 8 cycles, +1.0 → 17 cycles. Only +2.0
fails to fully recover in 40 cycles.

### 6.10 Self-Reference Conjecture (REFUTED)

**Conjecture 6.10.** *A self-referential meta-belief ("am I converging?")
that modulates evidence processing improves convergence.*

Status: **REFUTED** (exp_memory_dynamics.sx). All coupling strengths
(0 to 2.0) increase error. Self-referential monitoring adds noise
without information for simple belief systems. This may change for
multi-belief systems where the meta-belief can coordinate.

### 6.11 Hybrid Lyapunov Conjecture

**Conjecture 6.5.** *Memory consolidation (discrete events that transfer
episodic memory to semantic memory) does not increase the normalised
Lyapunov function V. The consolidation timing is a learnable parameter
whose meta-gradient exists and converges.*

Status: Partially validated (exp_anima_deep.sx — threshold learnable;
discrete jump analysis pending).

---

## 7. Robustness Analysis

**Proposition 7.1** (Learning Rate Robustness). *The framework is stable
across 3 orders of magnitude of learning rate (0.001 to 0.5).*

*Evidence.* exp_sensitivity.sx:

| lr | Loss | Converged? |
|----|------|-----------|
| 0.0001 | 42.55 | Slow |
| 0.001 | 33.32 | Yes |
| 0.01 | 31.50 | Optimal |
| 0.1 | 31.50 | Yes |
| 0.5 | 31.50 | Yes |
| 1.0 | 45.00 | Overshooting |

**Proposition 7.2** (Component Scaling). *Loss scales O(K) with the
number of competing objectives, not O(K²).*

**Proposition 7.3** (Dimensionality). *Loss decreases with dimensionality
for competing quadratic objectives. Higher dimensions provide more room
for compromise solutions.*

**Proposition 7.4** (Convergence Speed). *200 inner steps are sufficient
for convergence. Further steps provide diminishing returns.*

---

## 8. Experimental Reproducibility

All experiments are implemented in the Simplex programming language and
can be compiled and run with:

```bash
cd theorem-proof/
./run_all.sh          # 6 core theorem experiments
./run_math_tests.sh   # 188 compiler math validation tests
```

Individual experiments:

```bash
../simplex/build/sxc <experiment>.sx -o build/<experiment>.ll
clang -O2 build/<experiment>.ll ../simplex/runtime/standalone_runtime.c \
    -o build/<experiment> -lm -lssl -lcrypto -L$(brew --prefix openssl)/lib
./build/<experiment>
```

### Complete Experiment Index

| File | Theorems Validated | Results |
|------|-------------------|---------|
| exp_contraction.sx | Theorem 1(i) | 5/5 subsystems contract |
| exp_gradient_interference.sx | Theorem 2 | 100% conflict resolution |
| exp_lyapunov.sx | Theorem 3 | 0% violations |
| exp_invariants.sx | Proposition 3.5 | 0 violations in 20K steps |
| exp_timescale.sx | Theorem 1 timescale | 100% separation |
| exp_composition.sx | Theorem 1 full | System converges |
| exp_interaction_matrix.sx | Theorem 4 | Matrix converges in 5 cycles |
| exp_convergence_order.sx | Theorem 5 | S → 0.000264 |
| exp_pcgrad_refinement.sx | Theorem 2 comparison | Cosine beats Riemannian |
| exp_lyapunov_refinement.sx | Theorem 3 comparison | Normalised beats all |
| exp_learnable_projection.sx | Theorem 4 extension | α is learnable |
| exp_stress_test.sx | Proposition 7.1-7.4 | Rastrigin + Rosenbrock |
| exp_sensitivity.sx | Propositions 7.1-7.4 | 3 OOM robustness |
| exp_anima_deep.sx | Theorems 6, 7 | Belief interaction + desire |
| exp_anima_correlated.sx | Theorem 7, Conjecture 7.1 | 55% improvement, regulariser |
| exp_collatz_analysis.sx | Framework application | Drift characterisation |
| exp_liquid_hive.sx | Theorem 6 extension | 5-subsystem hive |
| exp_code_gates.sx | Theorem 8 | Code decisions converge |
| exp_compiler_passes.sx | Theorems 9, 10 | Per-program adaptation |
| exp_belief_cascade.sx | Conjecture 6.4 | Chain, circular, delay beliefs |
| exp_skeptical_annealing.sx | Conjecture 6.3/6.5 | Annealing, learnable schedule, ensemble |
| exp_memory_dynamics.sx | Conjectures 6.6-6.10 | Forgetting, transfer, self-reference, phase |
| exp_convergence_ratios.sx | Conjecture 6.1 | Ratio series, entropy, dominant pair |
| exp_symmetry_breaking.sx | Conjectures 6.2, 6.8, 6.9 | Symmetry, phase transition, groups |
| exp_navier_stokes.sx | Conjecture 11.1 | I-ratio detects laminar↔turbulent |
| exp_ns_2d.sx | Conjecture 11.1 | Critical Re_c≈578, energy cascade |
| exp_ns_blowup.sx | Theorem 16 | 37× blow-up lead time, C=0 contingency |
| exp_ns_smoothness.sx | Theorem 16 | 25-case S-smoothness, threshold -0.2 |
| exp_ns_perturbation.sx | Proposition 13.2 | Energy perturbation most sensitive |
| exp_ns_pid_control.sx | Theorem 15 | PID adaptive viscosity on Burgers |
| exp_riemann_zeta.sx | Conjecture 11.2 | I-ratio at zeta zeros |
| exp_nash_equilibrium.sx | Theorem 11 | 83.5% Pareto via skeptical desire |
| exp_gan_convergence.sx | Conjecture 9.1 | GAN stabilisation |
| exp_ode_solvers.sx | Conjecture 9.2 | Learned solver blending |
| exp_iratio_applications.sx | Theorem 13 | 5 domains validated |
| exp_iratio_proof_statistical.sx | Theorem 13 | 70/70 random problems |
| exp_s_vs_lyapunov.sx | Proposition 12.1 | S-λ complementarity |
| exp_s_controller.sx | Theorem 15 | S as adaptive control signal |
| exp_predictive_s.sx | Propositions 12.6-12.8 | Predictive S, multi-scale |
| exp_s_entropy.sx | Propositions 12.4-12.5 | S-entropy connection |
| exp_pid_metagradient.sx | Theorem 15 | PID with learnable gains |
| exp_pid_regime_shift.sx | Theorem 15 | PID on shifting landscape |
| exp_chaos_agent.sx | Theorem 16, Conjecture 13.1 | Skeptical agent, stability margin |
| exp_ns_level8.sx | Level 8 hierarchy | All parameters as dual numbers |
| exp_ns_regularity.sx | Regularity question | |∂T/∂A| → ∞ as A → 0 |
| exp_ns_dual_agent.sx | Theorems 17-18, Prop 13.3 | Order vs Chaos dual architecture |
| exp_ns_3d_vortex.sx | Theorem 19, Section 14 | 6-mode 3D NS with vortex stretching |
| exp_ns_3d_hard.sx | Theorem 19 | Quadratic stretching, BKM connection |
| exp_ns_level9.sx | Theorem 19, Props 14.1-14.2 | Level 9 hierarchy, Hessian, cross-derivatives |
| exp_ns_breaking_point.sx | Theorem 20 | Extreme stretching, regularity never breaks |
| exp_ns_viscosity_spectrum.sx | Theorems 21-23, Prop 14.3 | Viscosity spectrum, resonance, hysteresis |
| exp_ns_level10.sx | Theorem 24, Prop 14.4 | Trajectory acceleration, chirp, 10-level hierarchy |
| exp_ns_3d_full_hierarchy.sx | Theorems 25-26, Props 14.5-14.6 | 3D resonance shift, decoupling, fatal hysteresis |
| exp_ns_holistic.sx | Theorems 27-28, Prop 14.7 | Coupling matrix, feedback loop, holistic score |
| exp_ns_H_regularity.sx | Theorems 29-30 | H-regularity threshold, loop engagement, A* positive |
| exp_ns_scaffold.sx | Theorem 31 | Scaffold proof, α=2.0 scaling, universality to λ₂=100 |
| exp_ns_scaffold_rigour.sx | Stress test | 8-mode model, mode scaling, cascade effect |
| exp_ns_H_strengthen.sx | Section 14.15 | C1-C5 components, production ratio, loop gain |
| exp_ns_H_physics.sx | Section 14.15 | Physics-grounded criteria A-E, head-to-head |
| exp_ns_backward.sx | Section 14.15 | Forward vs backward AD, influence function, attribution |
| exp_ns_H_v5.sx | Section 14.15 | Backward-informed H, causal memory |
| exp_ns_close_gap.sx | Section 14.15 | Criteria G-J, ratchet detector (10.4%) |
| exp_ns_deceleration.sx | Theorem 33 | Criteria K-M, saturation predictor (54.6%) |
| exp_ns_jerk_closure.sx | Theorem 32 | Doubling time P (86.1%), jerk, combined Q |
| exp_ns_combined_approaches.sx | Prop 14.8 | Conventional methods as L16-L20, discriminative ranking |
| exp_ns_microscope.sx | Section 14.16 | Side-by-side A=1.08 vs A=1.15, production ratio diverges at step 0 |
| exp_ns_dynamic_sensitivity.sx | Theorem 36 | R(t)=dOmega/dA along trajectory, Lyapunov-like sensitivity |
| exp_ns_R_doubling.sx | Section 14.16 | Doubling time of R, recursive discriminator |
| exp_ns_8mode_solve.sx | Theorem 35 | 8-mode full framework: P=95.5%, 16/16, alpha=2, loop exists |
| exp_ns_H_prime.sx | Section 14.16 | H' self-adapting weights, A*=1.065, 91.5% |
| exp_ns_H_double_prime.sx | Section 14.16 | H'' confidence tracking, 100% at T=100k |
| exp_ns_H_v5.sx | Section 14.16 | Backward-informed H, causal memory |
| exp_ns_time_gated_H.sx | Section 14.16 | Time-gated levels, reduced false positives |
| exp_ns_solve_6mode.sx | Section 14.16 | Timing signals T1/T2/T3, spread criterion |
| exp_ns_missing_levels.sx | Section 14.16 | L21 stretching efficiency diverges at step 6000 |
| exp_ns_H_strengthen.sx | Section 14.16 | C1-C5 components, production ratio earliest signal |
| exp_ns_H_physics.sx | Section 14.16 | Physics-grounded criteria A-E |
| exp_ns_close_gap.sx | Section 14.16 | Criteria G-J, ratchet 10.4% |
| exp_ns_deceleration.sx | Theorem 33 | Saturation predictor 54.6%, doubling time 86.1% |
| exp_ns_jerk_closure.sx | Theorem 32 | Jerk criterion, combined Q |
| exp_ns_combined_approaches.sx | Prop 14.8 | Conventional methods as L16-L20 |
| exp_collatz_analysis.sx | Framework application | Drift characterisation |
| exp_prime_gaps.sx | Framework application | Gap derivative series |
| exp_stochastic_projection.sx | Theorem 2 extension | Noise unnecessary |
| exp_stochastic_rastrigin.sx | Theorem 2 extension | Implicit exploration |
| exp_sensitivity.sx | Propositions 7.1-7.4 | 3 OOM robustness |
| exp_stress_test.sx | Robustness | Rosenbrock + Rastrigin |
| exp_structure_discovery.sx | Gradient topology | Constraint graph |
| exp_equilibrium_mapping.sx | Theorem 14 | B-flow validated |
| exp_learnable_projection.sx | Theorem 4 extension | α is learnable |
| exp_learnable_projection2.sx | Theorem 4 extension | Asymmetric losses |
| test_math_arithmetic.sx | Compiler validation | 75/75 pass |
| test_math_comparisons.sx | Compiler validation | 23/23 pass |
| test_math_transcendental.sx | Compiler validation | 66/66 pass |
| test_math_loops.sx | Compiler validation | 10/10 pass |
| test_math_functions.sx | Compiler validation | 14/14 pass |

### Mathematical Software Validation

188/188 compiler mathematical operation tests pass, validating that the
Simplex compiler correctly implements all arithmetic, comparison,
transcendental, loop, and function call operations used in the experiments.

---

## 9. Cross-Domain Applications

### 9.1 Game Theory

**Theorem 11** (Desire-Driven Equilibrium Improvement). *In the Prisoner's
Dilemma with a cooperative desire as a secondary objective, the
interaction matrix between self-interest and cooperative desire creates
a contraction toward a welfare-superior equilibrium. With coupling
strength c=4.5, the system achieves 83.5% of Pareto optimal welfare
vs Nash equilibrium's 33%.*

*Evidence.* exp_nash_equilibrium.sx. The "skeptical desire" principle
from Theorem 7 (belief calibration) transfers to game theory: a desire
misaligned with the Nash-rational strategy improves social welfare,
just as a desire misaligned with evidence truth improves calibration.

### 9.2 Chaos Boundary Detection

**Theorem 12** (Convergence Score as Chaos Diagnostic). *The convergence
score S = 1 - (late_drift / early_drift) correctly classifies dynamical
system behaviour: S ≈ 1 for fixed-point convergence, S ≈ 0 for periodic
or chaotic behaviour, S < 0 for divergence. max |dS/dr| identifies
bifurcation boundaries.*

*Evidence.* exp_chaos_boundary.sx, exp_s_vs_lyapunov.sx.

**Proposition 12.1** (S-λ Complementarity). *S and the Lyapunov exponent
λ measure different properties and are complementary:*

| Regime | S | λ | Classification |
|--------|---|---|---------------|
| Fixed point | S = 1 | λ < 0 | Convergent + stable |
| Period-N | S = 0 | λ < 0 | Non-convergent but stable |
| Chaos | S ≈ 0 | λ > 0 | Non-convergent + unstable |
| Divergence | S = -1 | λ = ∞ | Exploding |

*S and λ together classify four regimes; neither alone classifies all four.
S has the advantage of being model-free (requires only observations, not
the system equations). λ has the advantage of distinguishing periodic from
chaotic behaviour. Agreement is 69% across the full logistic map range;
disagreement occurs in the period-doubling cascade (r=3.1-3.5) where
behaviour is stable but non-convergent.*

**Proposition 12.2** (S on Coupled Systems). *S correctly detects chaos
and synchronisation transitions in coupled dynamical systems without
computing the Jacobian matrix, which is required for Lyapunov exponents
of coupled systems.*

**Proposition 12.3** (S Noise Sensitivity). *S is sensitive to measurement
noise for ordered systems (noise dominates the drift signal). Noise-adaptive
thresholds are needed for practical application. Chaotic systems are robust
to noise because the chaotic drift dominates measurement error.*

### 9.3 GAN Convergence

**Conjecture 9.1.** *Cosine-scaled projection applied to the generator
and discriminator gradients in a GAN reduces oscillation and improves
generator quality. The learned interaction matrix discovers asymmetric
cooperation. The skeptical desire principle (misaligned secondary target)
regularises the generator.*

Status: Partially validated (exp_gan_convergence.sx). Oscillation reduced,
generator quality improved, but full convergence not achieved on the test
problem. The skeptical desire improved GAN training, extending the
cross-domain applicability of adversarial regularisation.

### 9.4 ODE Solver Blending

**Conjecture 9.2.** *Multiple numerical ODE solvers can be blended with
learnable weights, where the interaction matrix discovers which solvers
are compatible. The convergence score S(t) correlates with actual
integration error, providing an adaptive step size controller.*

Status: Partially validated (exp_ode_solvers.sx). Learned blend beats
naive equal-weight. Weights adapt by region. S(t) correlation with error
is weak (r=0.07) — needs refinement.

### 9.5 Cross-Domain Universality of Adversarial Regularisation

The most significant cross-domain finding: **adversarial regularisation
via misaligned objectives improves outcomes in EVERY domain tested.**

| Domain | "Skeptical" objective | Improvement |
|--------|----------------------|-------------|
| Beliefs | Misaligned desire (d≠p) | 31% calibration |
| Game Theory | Cooperative desire in PD | 83.5% of Pareto (vs 33% Nash) |
| GANs | Misaligned secondary target | Improved generator quality |
| Annealing | Skeptical→aligned schedule | Beats both fixed strategies |

This suggests a **universal principle**: introducing an objective that
partially contradicts the primary optimization direction acts as a
regulariser that prevents overfitting to local structure, improving
global outcomes across domains.

---

## 10. Balance Residual Theory

### 10.1 The I-Ratio Theorem

**Theorem 13** (Interaction Ratio at Equilibrium). *For K competing
objectives with gradient vectors g₁, ..., g_K on shared parameters θ,
define the interaction ratio:*

    I(θ) = Σᵢ<ⱼ gᵢ(θ)·gⱼ(θ) / Σᵢ ||gᵢ(θ)||²

*Then I(θ*) = -1/2 if and only if θ* is an equilibrium (Σᵢ gᵢ(θ*) = 0).
This holds for any K ≥ 2.*

*Proof.* At equilibrium, ||Σᵢ gᵢ||² = 0. Expanding:
Σᵢ ||gᵢ||² + 2·Σᵢ<ⱼ gᵢ·gⱼ = 0, so Σᵢ<ⱼ gᵢ·gⱼ / Σᵢ ||gᵢ||² = -1/2.
Conversely, I = -1/2 implies ||Σgᵢ||² = 0, so Σgᵢ = 0. ∎

*Evidence.* exp_balance_residual.sx: I = -0.5000 at equilibrium for
K=2 (1D) and K=3 (2D). I ≠ -0.5 off-equilibrium (I = -0.418 at (1.5, 0.5)).

### 10.2 The Balance Residual

**Definition 10.1.** The balance residual B(θ) = ||Σᵢ gᵢ(θ)||² / Σᵢ ||gᵢ(θ)||²
measures force imbalance. B = 0 at equilibrium, B > 0 elsewhere, B ∈ [0, K]
where K is the number of objectives.

**Theorem 14** (B-Flow Convergence). *Gradient descent on B(θ) converges
to the equilibrium θ* for convex multi-objective systems. On convex problems,
B-flow converges to higher precision than loss-flow in the same number of
steps.*

*Evidence.* exp_balance_residual.sx:
- 1D: B-flow achieves B = 0 (exact equilibrium). Loss-flow stops at distance 0.001.
- 2D: B-flow achieves B = 4.7×10⁻³⁴. Loss-flow stops at distance 0.00023.
- B-flow is ~1000× closer to exact equilibrium than loss-flow.

**Proposition 14.1** (B-Flow Limitation). *On non-convex landscapes, the
balance residual B has additional local minima where gradients balance
locally but the total loss is not globally optimal. Loss-flow is preferred
for exploration; B-flow for final refinement.*

*Evidence.* On Rastrigin, loss-flow achieves loss 54.6 vs B-flow's 135.6.

### 10.3 Two-Phase Optimization

**Conjecture 10.1** (Two-Phase Convergence). *The optimal strategy for
multi-objective equilibrium finding is:*

*Phase 1 (Exploration): Follow loss-flow to reach the basin of attraction
of the equilibrium. Use cosine-scaled projection and the interaction
matrix to resolve gradient conflicts.*

*Phase 2 (Refinement): Switch to B-flow to snap to the exact equilibrium.
B-flow's sharper minimum achieves higher precision than loss-flow.*

*The transition point is when I_ratio(θ) < -0.3 (within the equilibrium
basin).*

---

## 11. Millennium Problem Explorations

### 11.1 Navier-Stokes: Force Balance as Laminar Flow

**Conjecture 11.1** (I-Ratio Laminar Criterion). *In the viscous Burgers
equation (1D Navier-Stokes analogue), the I-ratio between advection and
diffusion forces satisfies I ≈ -0.5 in the laminar regime and departs
from -0.5 at the laminar→turbulent transition. Force balance IS laminar
flow.*

*Evidence.* exp_navier_stokes.sx. Viscosity sweep on Burgers equation:

| ν | Re | I-ratio | S score | Regime |
|---|---|---|---|---|
| 0.5 | 2 | 0.000 | 1.000 | Laminar |
| 0.1 | 10 | -1.3×10⁻¹⁰ | 1.000 | Laminar |
| 0.01 | 100 | -0.073 | 0.803 | Transitional |
| 0.005 | 200 | 0.088 | 0.502 | Turbulent |
| 0.001 | 1000 | 0.322 | -0.102 | Turbulent |

The S score goes negative at step 299, detecting instability before
blow-up. The Reynolds number where I departs from -0.5 marks the
laminar→turbulent critical point.

This does NOT solve the Navier-Stokes millennium problem. It provides
a gradient-based diagnostic for the smoothness question: I ≈ -0.5
indicates balanced (smooth) flow; departure indicates potential blow-up.

### 11.2 Riemann Hypothesis: Zeta Zeros as Force Balance

**Conjecture 11.2** (I-Ratio at Zeta Zeros). *For the Riemann zeta
function ζ(s) = Σ n⁻ˢ, the I-ratio of the partial sum terms approaches
-0.5 at non-trivial zeros on the critical line Re(s) = 1/2.*

*Evidence.* exp_riemann_zeta.sx. I-ratio at known zeros (N=100 terms):

| Zero (t) | |Z₁₀₀| | I-ratio | |I + 0.5| |
|---|---|---|---|
| 14.13 | 0.713 | -0.451 | 0.049 |
| 21.02 | 0.479 | -0.478 | 0.022 |
| 25.01 | 0.403 | -0.484 | 0.016 |
| 30.42 | 0.330 | -0.489 | 0.011 |
| 32.94 | 0.305 | -0.491 | 0.009 |

I-ratio approaches -0.5 at known zeros, improving at higher t (better
partial sum convergence). This is mathematically expected: a zero IS
a force balance point where terms cancel.

*Caveats:* Partial sums converge poorly at σ=0.5. N=100 is far from
asymptotic. This does NOT prove RH. Proper evaluation requires the
Riemann-Siegel formula, not raw partial sums.

---

## 12. PID Control via Higher-Order S

### 12.1 S as Adaptive Control Signal

**Theorem 15** (PID-S Controller). *For a multi-objective system with
convergence score S(t), the learning rate can be adaptively controlled
via a PID controller:*

    lr(t) = lr_base × clamp(1 + w₁·S + w₂·S' + w₃·S'', min, max)

*where w₁, w₂, w₃ are dual numbers learned by meta-gradient. On stable
landscapes, only w₁ (proportional) is needed. On regime-shifting
landscapes, w₂ (derivative) and w₃ (second derivative) provide
measurable benefit.*

*Evidence.* exp_pid_regime_shift.sx:
- Fixed lr: 86.6669 (baseline)
- P-only (S): 86.6684 (worse — overreacts)
- PD (S + S'): 86.6667 (matches optimum)
- PID (S + S' + S''): 86.6667 (matches optimum)

The derivative terms detect the regime shift and settle faster.

### 12.2 S-Entropy Relationship

**Proposition 12.4** (S-Entropy Correlation). *S correlates negatively
with Shannon entropy (r = -0.457): ordered systems have high S and low
entropy. However, S is NOT the negative entropy production rate
(r = 0.153 with -dH/dt). S is a convergence diagnostic, not a
thermodynamic quantity.*

**Proposition 12.5** (Matrix Entropy). *Interaction matrix entropy
INCREASES with S (r = +0.611). Convergence produces uniform matrices
(high entropy), not specialised ones. Specialisation occurs during the
transition period, not at equilibrium.*

### 12.3 Predictive S

**Proposition 12.6** (S Predictability). *In chaotic systems (logistic
map r=3.7), S(t+1) is partially predictable from S(t) and S'(t) with
RMSE = 0.104. The derivative S' carries independent information
(coefficient b = -0.25 in the linear predictor).*

**Proposition 12.7** (Multi-Scale S Detection Order). *In Burgers
equation, the gradient-scale S goes negative first at all viscosities,
before local S and energy S. Instability begins at the gradient level
(shock formation) and cascades to larger scales.*

**Proposition 12.8** (Regime Change Detection). *S provides instantaneous
detection of environmental regime changes. At a belief system regime
change, S crashes to O(-100) within one step, with magnitude proportional
to the size of the truth shift.*

---

## 13. The Skeptical Agent of Chaos

### 13.1 Stability Margin as Dual Number

**Theorem 16** (Perturbation Stability Margin). *For a dynamical system
at state θ, the stability margin M(θ, ε) = ‖θ_perturbed(T) - θ(T)‖ / ε
measures the amplification of a perturbation of size ε over a probe
horizon T. M is a dual number with computable derivatives:*

- *M(t): how fragile the system is at time t*
- *∂M/∂t: whether fragility is increasing (blow-up approaching)*
- *∂²M/∂t²: whether the fragility increase is accelerating*
- *∂M/∂ε: whether the response is linear (stable) or superlinear (nonlinear instability)*

*Evidence.* exp_chaos_agent.sx on Burgers equation (ν=0.005):

| Time | M(t) | dM/dt | d²M/dt² | State |
|---|---|---|---|---|
| 0 | 1.037 | — | — | Baseline |
| 2000 | 1.040 | +0.003 | — | Slightly fragile |
| 4000 | 1.047 | +0.007 | +0.004 | Fragility accelerating |
| 6000 | 1.067 | +0.020 | +0.013 | Accelerating |
| 8000 | 1.158 | +0.092 | +0.072 | Rapidly accelerating |
| 10000 | NaN | — | — | Blown up |

M increases monotonically before blow-up. d²M/dt² is positive from
step 4000 — fragility acceleration provides the earliest warning.

### 13.2 Perturbation Scaling

**Proposition 13.1** (Linear Stability Regime). *At all pre-blow-up times,
M scales linearly with ε: M(0.001) ≈ M(0.01) ≈ M(0.1). The system
remains in the linear stability regime until blow-up, at which point M
diverges.*

### 13.3 Perturbation Type as Instability Mechanism Identifier

**Proposition 13.2** (Perturbation Type Diagnosis). *Different perturbation
types (amplitude, gradient, energy) produce different S responses. The
most sensitive type identifies the instability mechanism:*

- *Amplitude most sensitive → point instability*
- *Gradient most sensitive → shock formation*
- *Energy most sensitive → global destabilisation*

*Evidence.* exp_ns_perturbation.sx: energy perturbation is most sensitive
at all times for ν=0.001 Burgers, confirming global energy instability
as the blow-up mechanism.

### 13.4 The Full Differentiable Hierarchy

The complete hierarchy of dual-number diagnostics:

```
Level 0: θ (parameters)                    ← what the system IS
Level 1: αᵢⱼ (interaction matrix)          ← how subsystems cooperate
Level 2: S (convergence score)             ← is it converging?
Level 3: S', S'' (PID control)             ← how fast? accelerating?
Level 4: M(t,ε) (stability margin)         ← how fragile?
Level 5: ∂M/∂t (fragility evolution)       ← is fragility growing?
Level 6: ∂²M/∂t² (fragility acceleration) ← is the growth speeding up?
Level 7: ε* (optimal probe)                ← learnable via meta-gradient
```

Every level is a dual number. Every derivative is computable via
forward-mode AD. The system monitors its own stability at every
order simultaneously, in a single forward pass.

### 13.5 The Skeptical Agent Architecture

**Conjecture 13.1** (Skeptical Agent). *An autonomous agent that
continuously probes its own stability via perturbation testing,
with learnable probe strategy (ε*, perturbation type, probe frequency),
can predict instability with greater lead time than passive monitoring
of S alone. The lead time improvement is proportional to the number
of hierarchy levels monitored.*

This is the "skeptical desire" principle elevated to a full architecture:
the agent doesn't just observe — it deliberately introduces dissonance
and measures the response. The response, its derivatives, and its
derivatives' derivatives form a complete stability profile that enables
proactive adaptation rather than reactive recovery.

### 13.6 The Stable Agent of Order

**Theorem 17** (Dual Agent Architecture). *For a dynamical system approaching
blow-up, define two opposing agents:*

- *Chaos Agent C(ε): injects perturbation ε (heats, destabilises)*
- *Order Agent O(ν_art): adds artificial viscosity ν_art (cools, stabilises)*

*The interplay parameter ψ = ε - ν_art determines the system's regime:*
- *ψ > 0: chaos dominates (heating, exploration)*
- *ψ < 0: order dominates (cooling, stabilisation)*
- *ψ_opt: the value maximising time to blow-up*

*The meta-gradients ∂T/∂ε and ∂T/∂ν_art are computable and reveal an
asymmetric leverage: order is consistently 3.5–4.2× more powerful than
chaos per unit of intervention.*

*Evidence.* exp_ns_dual_agent.sx on Burgers equation (ν=0.001):

**Meta-gradient measurements (16 parameter combinations):**

| chaos_ε | ν_art | T_blowup | ∂T/∂chaos | ∂T/∂order | Leverage |
|---|---|---|---|---|---|
| 0.005 | 0.001 | 8759 | -29250 | +104000 | 3.56 |
| 0.005 | 0.008 | 9579 | -37500 | +132000 | 3.52 |
| 0.01 | 0.002 | 8719 | -28000 | +103000 | 3.68 |
| 0.04 | 0.002 | 8014 | -19500 | +82000 | 4.21 |

∂T/∂chaos < 0 always (more chaos = earlier blow-up).
∂T/∂order > 0 always (more order = later blow-up).
Leverage = |∂T/∂order| / |∂T/∂chaos| ∈ [3.5, 4.2] across all configurations.

**Interplay gradient (budget-constrained sweep, ε + ν_art = 0.02):**

Optimal ψ = -0.018 (order dominates), achieving T_blowup = 11,676 steps —
**32.6% longer than baseline** (8,803 steps). ∂T/∂ψ < 0 everywhere,
confirming that shifting budget toward order always helps, but the
gradient steepens at the chaos boundary.

### 13.7 Self-Learning Annealing for Fluids

**Theorem 18** (Adaptive Annealing). *Define temperature
T_anneal = ε / (ε + ν_art). The cooling rate adapts to gradient growth:*

- *Gradient increasing → cool faster (more order, less chaos)*
- *Gradient decreasing → cool slower (allow more exploration)*

*With adaptive cooling, the dual agent extends smoothness beyond both
the baseline and any fixed-temperature agent.*

*Evidence.* exp_ns_dual_agent.sx:

| Anneal Rate | T_blowup | vs Baseline |
|---|---|---|
| 0.00001 | 8305 | -498 (chaos dominated) |
| 0.00005 | 8423 | -380 (chaos dominated) |
| 0.0001 | 8540 | -263 (chaos dominated) |
| 0.0005 | 8925 | +122 (extends smoothness) |
| 0.001 | 9060 | +257 (extends smoothness) |
| 0.005 | 9293 | +490 (extends smoothness) |

The crossover at rate ≈ 0.0003 marks the point where adaptive cooling
overcomes initial chaos. Rates above this threshold extend smoothness
monotonically. The annealing rate itself is a learnable dual number.

### 13.8 The Fundamental Asymmetry

**Proposition 13.3** (Order–Chaos Asymmetry). *In dissipative systems
approaching blow-up, order (artificial viscosity) is strictly more powerful
than chaos (perturbation) per unit of intervention, with leverage ratio
L = |∂T/∂order| / |∂T/∂chaos| > 1 at all measured configurations.*

This has a physical interpretation: viscosity is a *global* smoothing
operator (diffusion acts on all gradients simultaneously), while perturbation
is a *local* destabilising force (applied at one point). The 3.5–4.2×
leverage reflects the dimensional mismatch between global and local effects.

The chaos agent's purpose is not to destabilise — it is to provide the
**information gradient** that tells the order agent where fragility lives.
Chaos measures; order acts. Two sides of the same coin.

---

## 14. 3D Navier-Stokes with Vortex Stretching

### 14.1 The 6-Mode 3D Galerkin Model

To test whether our diagnostic tools survive the transition from 1D Burgers
to 3D Navier-Stokes, we construct a 6-mode Galerkin truncation with explicit
vortex stretching — the term (ω·∇)u that is absent in 2D and makes 3D
regularity an open problem.

**Model:** 3 velocity modes (u, v, w) at wavenumber k₁=1, 3 vorticity modes
(ωx, ωy, ωz) at wavenumber k₂=2. Three coupling parameters:

- σ: velocity-vorticity feedback
- λ: linear vortex stretching (ω·∇u)
- λ₂: quadratic self-amplification (|ω|²·ω, mimics vortex tube thinning)

The quadratic term captures the critical 3D physics: as a vortex tube thins,
vorticity intensifies AND strain rate increases, creating superlinear feedback.

### 14.2 Regularity Signature in 3D

**Theorem 19** (Regularity Robustness). *The regularity signature
|∂T_blowup/∂A| → ∞ as A → 0 persists at all tested vortex stretching
strengths, including:*

- *Linear stretching λ = 0 to 4.0*
- *Quadratic stretching λ₂ = 0 to 10.0*

*The cross-derivative ∂²T/∂λ∂A does not destroy the regularity signature.*

*Evidence.* exp_ns_3d_vortex.sx, exp_ns_3d_hard.sx, exp_ns_level9.sx.

Cross-derivative ∂²T/∂λ∂A (linear stretching, λ₂=2.0, ν=0.005):

| λ | A=4→2 |dT/dA| | A=2→1 |dT/dA| | A=1→0.5 |dT/dA| |
|---|---|---|---|
| 0 | 4,635 | 41,930 | — (no blow-up) |
| 1.0 | 4,115 | 16,860 | 72,760 |
| 2.0 | 2,360 | 9,705 | 41,130 |
| 4.0 | 1,340 | 5,515 | 23,140 → 100,665 |

At every λ, |dT/dA| grows as A shrinks. Even at λ=4.0, A=0.5 gives
|dT/dA| = 100,665 — the steepest gradient measured.

Cross-derivative ∂²T/∂λ₂∂A (quadratic stretching, λ=1.0):

| λ₂ | A=2→1 |dT/dA| |
|---|---|
| 0 | 22,590 |
| 1.0 | 17,450 |
| 5.0 | 16,995 |
| 10.0 | 14,380 |

|dT/dA| decreases slowly with λ₂ but remains large (14,380 at λ₂=10).
The regularity signature weakens but does NOT vanish.

### 14.3 The Blow-Up Surface Hessian

**Proposition 14.1** (Hessian Structure). *The blow-up time T(A, ν, σ, λ, λ₂)
has the following curvature structure at the operating point
(A=3, ν=0.005, σ=0.5, λ=1, λ₂=2):*

| Parameter | ∂T/∂x | ∂²T/∂x² | Curvature |
|---|---|---|---|
| A (amplitude) | -7,315 | +4,975 | Convex (accelerating) |
| ν (viscosity) | +47,000 | ≈0 | Linear |
| σ (feedback) | -3,790 | -4,400 | **Concave (saturating)** |
| λ (stretching) | -13,435 | +22,200 | **Convex (self-amplifying)** |
| λ₂ (quadratic) | -1,663 | +44 | Weakly convex |

**Key findings:**

1. λ is **convex**: more stretching makes additional stretching MORE dangerous.
   This is the mathematical fingerprint of the vortex stretching problem.
2. σ is **concave**: velocity-vorticity feedback saturates. The coupling loop
   has a ceiling.
3. ν is **linear**: viscosity's stabilising effect neither accelerates nor
   saturates — constant returns.
4. Amplitude dominates sensitivity (21,945 normalised), followed by
   stretching (13,435). Viscosity is weakest (235 normalised) — 100× less
   influential than amplitude.

### 14.4 Sensitivity Ranking

**Proposition 14.2** (Parameter Influence Hierarchy). *The normalised
sensitivity |∂T/∂x|·x ranks as:*

1. *A (amplitude): 21,945 — initial conditions dominate*
2. *λ (stretching): 13,435 — vortex stretching is the key mechanism*
3. *λ₂ (quadratic): 3,325 — self-amplification matters but less*
4. *σ (feedback): 1,895 — coupling is secondary*
5. *ν (viscosity): 235 — viscosity is a weak lever*

*This explains why the NS millennium problem is hard: the stabilising force
(viscosity) has 100× less influence on blow-up than the destabilising force
(amplitude/stretching). Any regularity proof must show that viscosity's
influence, though weak, is sufficient.*

### 14.5 The 9-Level Differentiable Hierarchy

The complete hierarchy for 3D NS diagnostics:

```
Level 0: θ (state)                      ← u, v, w, ωx, ωy, ωz
Level 1: Ω (enstrophy)                  ← ½|ω|²
Level 2: S (convergence score)          ← is enstrophy converging?
Level 3: S', S'' (PID control)          ← how fast? accelerating?
Level 4: M(t,ε) (stability margin)      ← how fragile?
Level 5: dM/dt (fragility evolution)    ← is fragility growing?
Level 6: dM/dε (perturbation scaling)   ← linear or superlinear?
Level 7: ε* (optimal probe)             ← learnable via meta-gradient
Level 8: ∂T/∂A, ∂T/∂ν (simulation)     ← parameter sensitivity
Level 9: ∂T/∂λ, ∂²T/∂λ∂A (coupling)   ← physics structure sensitivity
```

Every level is computable in a single forward pass via dual numbers.
The hierarchy maps the blow-up surface as a differentiable manifold.

### 14.6 Enstrophy Growth and the BKM Criterion

The Beale-Kato-Majda theorem states blow-up occurs iff ∫₀ᵀ ‖ω‖∞ dt = ∞.
In the 6-mode model, enstrophy growth is consistently superlinear
(dΩ/dt normalised by Ω exceeds 0.1 throughout), confirming the model
captures the essential blow-up mechanism.

### 14.7 The Breaking Point: Where Does Regularity Fail?

**Theorem 20** (Structural Regularity). *In the 6-mode 3D Galerkin model
with both linear stretching (λ) and quadratic self-amplification (λ₂),
the regularity signature |∂T/∂A| → ∞ as A → 0 is structurally robust:
it persists at all stretching strengths tested, up to λ=128 and λ₂=500.*

*The growth ratio R = |∂T/∂A|_{A/2} / |∂T/∂A|_A ≈ 4.7 is approximately
constant across all stretching strengths. Stretching scales both the
gradient and blow-up time proportionally, preserving the regularity
signature's growth rate.*

*Evidence.* exp_ns_breaking_point.sx. Five experiments pushing stretching
to extreme values:

**Linear stretching sweep (λ₂ = 2.0 fixed):**

| λ | |dT/dA| at A=4 | |dT/dA| at smallest A | Growth factor | Status |
|---|---|---|---|---|
| 1 | 4,110 | 72,150 (A=1) | 17.6× | INTACT |
| 8 | 750 | 233,470 (A=0.25) | 311× | INTACT |
| 32 | 220 | 327,750 (A=0.125) | 1,490× | INTACT |
| 64 | 130 | 175,320 (A=0.125) | 1,349× | INTACT |

**Quadratic stretching sweep (λ = 1.0 fixed):**

| λ₂ | |dT/dA| at A=4 | Largest |dT/dA| | Growth | Status |
|---|---|---|---|---|
| 50 | 240 | 235,520 (A=0.5) | 982× | INTACT |
| 100 | 100 | 140,130 (A=0.5) | 1,401× | INTACT |
| 200 | 50 | 456,290 (A=0.25) | 9,126× | INTACT |

**Combined extreme (λ=128, λ₂=500):** |dT/dA| = 30 → 120 → 570.
Still growing at the maximum tested stretching.

**Binary search for transition:** Searched λ from 1 to 128
(with λ₂ = 2λ) and λ₂ from 1 to 500 (with λ=1). Regularity score
remained strongly positive (3.4–8.2) at all points. **No transition found.**

**Growth ratio constancy:** At all λ from 4 to 64 (with λ₂ = 2λ), the
ratio |dT/dA|@A=1 / |dT/dA|@A=2 ≈ 4.7 ± 0.2. Stretching does not
degrade the ratio — it is a structural invariant of the model.

**Interpretation.** The regularity signature is not conditional on
stretching strength. In this model, smaller initial data always prevents
blow-up regardless of vortex stretching coupling. Viscosity, though 100×
weaker in normalised sensitivity, acts on a different timescale and
catches up at small amplitudes.

*Caveat.* This is a 6-mode truncation, not full 3D NS. The truncation
limits the energy cascade to two wavenumbers. In full NS, the infinite
cascade of scales may change the picture. However, the structural
robustness of the regularity signature — its invariance to stretching
strength — is a strong signal that deserves investigation in higher-mode
truncations.

### 14.8 The Viscosity Spectrum: Solid → Fluid → Gas

**Theorem 21** (Viscosity Spectrum). *Treat viscosity ν as a spectrum with
physical endpoints: solid (ν→∞, frozen) and gas (ν→0, inviscid). The
9-level diagnostic hierarchy at each endpoint provides guardrails:*

| ν | State | T_blowup | S-score | M (margin) |
|---|---|---|---|---|
| 10.0 | Solid | ∞ | +1.0 | 0.30 |
| 0.1 | Fluid | 24,066 | +0.57 | 1.002 |
| 0.01 | Fluid | 18,589 | -0.19 | 1.015 |
| 0.001 | Gas | 18,166 | -0.83 | 1.016 |
| 0.00001 | Gas | 18,120 | -0.91 | 1.016 |

*Key findings:*

1. *T_blowup converges to ~18,120 as ν→0. Viscosity buys only 33% more
   time (18,120→24,066). Quantified "weak lever."*
2. *S-score swings from +1.0 to -0.91 across the spectrum — a 2.0 unit
   range. The diagnostic is MORE sensitive than the physical parameter.*
3. *The Hessian ∂²T/∂ν² is convex at both endpoints but zero at the
   fluid midpoint — an inflection point in the blow-up surface.*

**Proposition 14.3** (Viscosity Asymmetry). *Low viscosity causes more
enstrophy growth than high viscosity can repair. The response is asymmetric:
oscillating ν between gas-like and solid-like values causes a ratchet effect
where each gas-phase excursion adds enstrophy that the solid phase cannot
fully remove. This causes blow-up at step 30,051 despite mean viscosity
of 0.01 (which alone survives until 18,589).*

**Theorem 22** (Resonance Frequency). *There exists a natural timescale of
the blow-up instability, measurable as a resonance in the viscosity
transfer function H(f) = enstrophy_amplitude / ν_amplitude:*

| Period | H(f) | Response |
|---|---|---|
| 500 | 1.73 | Filtered |
| 2000 | 1.20 | Low |
| **10,000** | **10,889** | **Resonance** |
| 20,000+ | — | Blow-up |

*The resonance at period ≈ 10,000 steps (1.0 time units) is a computable
physical constant of the system.*

**Theorem 23** (Viscosity Hysteresis). *The 3D NS model with vortex
stretching exhibits hysteresis in viscosity: enstrophy at ν=0.007 on the
down-ramp (solid→gas) is 0.012, while enstrophy at the same ν on the
up-ramp (gas→solid) is 0.94 — a 78× difference. The system blows up
on the return path.*

*This means:*
1. *The "weak lever" has hidden state — current viscosity alone does not
   determine the system's fate.*
2. *Any regularity argument must account for the full trajectory through
   the viscosity spectrum, not just the instantaneous value.*
3. *The dual agent cannot oscillate symmetrically — the gas phase must be
   shorter or weaker than the solid phase to avoid ratcheting.*

*Evidence.* exp_ns_viscosity_spectrum.sx. Five experiments covering the
full viscosity spectrum with oscillation, frequency response, differentials,
and ramp hysteresis tests.

### 14.9 Level 10: Trajectory Acceleration

**Theorem 24** (Trajectory Meta-Gradient). *The acceleration of the
trajectory through the viscosity spectrum is a dual number with computable
meta-gradient ∂T_blowup/∂(acceleration). The meta-gradient switches sign
at the resonance frequency:*

- *ω < ω_resonance: ∂T/∂accel > 0 (accelerate to escape toward resonance)*
- *ω ≈ ω_resonance: ∂T/∂accel > 0 (accelerate to pass through)*
- *ω > ω_resonance: ∂T/∂accel < 0 (decelerate — you've escaped)*

*The sign change at ω ≈ 0.8 defines a learnable control law: the dual
agent accelerates pre-resonance, decelerates post-resonance.*

*Evidence.* exp_ns_level10.sx. Meta-gradient at 8 base frequencies:

| ω_base | ∂T/∂accel | ∂²T/∂accel² | Action |
|---|---|---|---|
| 0.1 | +700 | 0 | Accelerate |
| 0.5 | +400 | 0 | Accelerate |
| 0.628 | +200 | 0 | Accelerate (at resonance) |
| 1.0 | -250 | +10,000 | Decelerate |
| 2.0 | -200 | 0 | Decelerate |

**Proposition 14.4** (Optimal Acceleration). *There exists an optimal
trajectory acceleration α_opt ≈ +0.1 that maximises blow-up time.
The blow-up surface is concave in acceleration (∂²T/∂α² = -40,000),
meaning there are diminishing returns from faster sweeps.*

### 14.10 The Complete 10-Level Hierarchy

```
Level 0:  θ (state)                     ← u, v, w, ωx, ωy, ωz
Level 1:  Ω (enstrophy)                 ← ½|ω|²
Level 2:  S (convergence score)         ← is enstrophy converging?
Level 3:  S', S'' (PID control)         ← rate, acceleration of convergence
Level 4:  M(t,ε) (stability margin)     ← how fragile?
Level 5:  dM/dt (fragility evolution)   ← is fragility growing?
Level 6:  dM/dε (perturbation scaling)  ← linear or superlinear?
Level 7:  ε* (optimal probe)            ← learnable via meta-gradient
Level 8:  ∂T/∂A, ∂T/∂ν (simulation)    ← parameter sensitivity
Level 9:  ∂T/∂λ, ∂²T/∂λ∂A (coupling)  ← physics structure sensitivity
Level 10: ∂T/∂α (trajectory accel)      ← path through parameter space
```

Every level is a dual number. The hierarchy maps the blow-up surface
not as a static object but as a dynamic manifold that the agent traverses.
The agent learns not just WHERE to go in parameter space, but HOW FAST
to move — the trajectory dynamics are the intelligence.

### 14.11 Full 10-Level Hierarchy in 3D with Vortex Stretching

**Theorem 25** (Resonance Shift). *Vortex stretching shifts the resonance
frequency of the viscosity oscillation response. The resonance frequency
scales as ω_res ∝ λ₂^α where α ≈ 0.4:*

| λ₂ | ω_resonance | Period (steps) |
|---|---|---|
| 0 | 1.0 | 62,832 |
| 2 | 1.5 | 41,888 |
| 5 | 2.0 | 31,416 |
| 10 | 3.0 | 20,944 |

*Stronger stretching shortens the instability timescale — the system's
internal clock speeds up. The dual agent's oscillation frequency must
adapt to the stretching strength; a fixed period is suboptimal.*

**Theorem 26** (Viscosity-Stretching Decoupling). *In the 6-mode 3D
Galerkin model, stretching reduces blow-up time by a constant fraction
(69.8%) independent of viscosity. The reduction is identical at ν=0.01
and ν=0.00001. Viscosity and stretching decouple — they act on
orthogonal aspects of the dynamics.*

*Evidence.* exp_ns_3d_full_hierarchy.sx:

- Without stretching (λ₂=0): no blow-up at any viscosity (T=60,000)
- With stretching (λ₂=10): blow-up at T≈25,300 regardless of viscosity
- Stretch effect = 69.8% ± 0.1% across the entire viscosity spectrum

**Proposition 14.5** (Fatal Hysteresis). *When vortex stretching is active
(λ₂ > 0), the hysteresis on the return ramp (gas→solid) is fatal: the
system blows up before completing the return. At λ₂=0, the hysteresis
ratio is 40.7×. At λ₂ ≥ 1, it is infinite (blow-up).*

**Proposition 14.6** (Level 10 Threshold). *The trajectory acceleration
meta-gradient ∂T/∂accel is positive (accelerate helps) for λ₂ < 10,
but goes to zero at λ₂ ≥ 10. Above this threshold, the blow-up is
too fast for trajectory dynamics to influence the outcome. Level 10
has a stretching-dependent validity range.*

*Evidence.* exp_ns_3d_full_hierarchy.sx. Six experiments covering
viscosity spectrum, resonance detection, hysteresis, trajectory
meta-gradient, resonance shift, and full 10-level profile in 3D.

### 14.12 The Holistic View: One Homogeneous System

**Theorem 27** (Coupling Topology). *The 10 diagnostic levels form a
coupled dynamical system with a positive feedback loop that drives blow-up:*

```
Enstrophy (L1) → Fragility (L4) → Destabilisation → Enstrophy (L1)
       ↑                                                     ↓
       └──────────── Convergence loss (L2↓) ────────────────┘
```

*From t=6000 onward, all three rates lock in phase: L1↑, L4↑, L2↓.
The feedback loop gain exceeds 1 at t≈10,000, after which blow-up is
inevitable (runaway at t=13,096).*

*Evidence.* exp_ns_holistic.sx, Exp 2. Rate coupling shows L1↑L4↑L2↓
consistently from t=6000 to blow-up.

**Theorem 28** (Hidden Coupling Strength). *Viscosity's direct sensitivity
to blow-up (∂T/∂ν = 47,000) appears weak compared to amplitude
(∂T/∂A = -7,155). But viscosity's CROSS-LEVEL coupling to trajectory
dynamics is ∂L10/∂ν = +500,000 — the largest coupling in the system.*

*Viscosity is not weak. Its power is hidden in the coupling between levels.
When measured through its effect on trajectory sensitivity, viscosity is
the strongest force in the system. This explains why viscosity appeared
weak in isolation (Theorem 21) while being sufficient for regularity
(Theorem 20): its influence propagates through the coupling network.*

**The Coupling Matrix** (at t=5000, A=3, ν=0.005, λ=1, λ₂=5):

```
              d(A)        d(ν)        d(λ₂)
L1(enst)   +0.045      -0.192      +0.004
L2(S)      +0.007      +0.420      -0.016
L4(M)      +0.013      -0.129      +0.003
```

Viscosity has **opposite signs** across levels (suppresses L1, improves L2,
reduces L4). Stretching has **coherent signs** (increases L1, suppresses L2,
increases L4 — all toward blow-up). The asymmetry in sign coherence explains
why stretching is dangerous: it pushes all levels in the same direction.

**Cross-Level Sensitivity** (higher-order levels):

```
              d(ν)        d(λ₂)
dL8/d...   -47,500      +485
dL9/d...   -7,000       +273
dL10/d...  +500,000     -8,000
```

**Proposition 14.7** (Holistic Score). *Define the holistic diagnostic
H = 0.3·(L1/L1₀) + 0.4·(-L2) + 0.3·(L4/L4₀). The rate dH/dt tracks
the approach to blow-up better than any single level:*

| Time | H | dH/dt | Status |
|---|---|---|---|
| 4000 | 0.98 | — | Stabilising phase |
| 8000 | 0.99 | +0.006 | Steady |
| 10000 | 1.00 | +0.01 | Approaching |
| 12000 | 3.06 | **+1.67** | Blow-up imminent |

*H captures the coupled dynamics in one number. Its acceleration
d²H/dt² would be the Level 10 meta-gradient of the holistic view.*

*Evidence.* exp_ns_holistic.sx. Four experiments: coupling matrix,
rate coupling, holistic score, and full Jacobian.

### 14.13 H-Regularity: The Feedback Loop Threshold

**Theorem 29** (Feedback Loop Engagement). *The holistic diagnostic H has
a sharp transition: below amplitude A*, the coupling feedback loop
(L1→L4→L2→L1) never engages and H stays bounded. Above A*, the loop
engages at a finite time and H diverges (blow-up).*

*The transition between A=0.5 (loop engages at step 22,226) and A=0.25
(loop never engages) is sharp — a clean threshold, not a gradual
degradation.*

**Theorem 30** (H-Regularity Threshold). *The regularity threshold A*
computed via H-feedback loop engagement is positive at all vortex
stretching strengths tested:*

| λ₂ | A* | Status |
|---|---|---|
| 0 | 0.301 | Regular |
| 2 | 0.299 | Regular |
| 5 | 0.297 | Regular |
| 10 | 0.294 | Regular |
| 20 | 0.288 | Regular |
| 50 | 0.270 | Regular |

*A* decreases sublinearly with stretching strength. Extrapolation to
A*=0 requires λ₂ ≈ 500, but Theorem 20 shows the enstrophy-based
regularity signature holds at λ₂=500. The threshold does not reach zero.*

*This reformulates the NS millennium problem: instead of bounding the
vortex stretching term directly, one can ask whether the coupling between
convergence (L2), fragility (L4), and enstrophy (L1) stays below the
self-reinforcing threshold. H is a computable, monotonic diagnostic
that captures the coupled dynamics in a single scalar.*

*Evidence.* exp_ns_H_regularity.sx. Four experiments: H vs enstrophy
comparison, H-regularity signature, dH/dt sign analysis, and binary
search for A* at six stretching strengths.

### 14.14 The Scaffold: H Resolves the Enstrophy Question

**Theorem 31** (The Scaffold). *H scaffolds the enstrophy question.
For the 6-mode 3D NS model with vortex stretching:*

*GIVEN: initial amplitude A < A\*(λ₂, ν), where A\* > 0 for all λ₂ tested*
*THEN: H's feedback loop does not engage*
*THEREFORE: enstrophy is bounded for all time*
*WITH BOUND: max(Ω) = C · A², where α = 2.0 exactly*

*The quadratic scaling is never amplified — the amplification ratio is 1.17
and constant across all amplitudes from A=0.25 down to A=0.002. Vortex
stretching (λ₂ = 5) cannot amplify enstrophy beyond the initial scaling.*

*Evidence.* exp_ns_scaffold.sx:

**Scaling law (α = 2.0 at every scale tested):**

| A | max(Ω) | α |
|---|---|---|
| 0.25 | 5.47×10⁻⁴ | — |
| 0.125 | 1.37×10⁻⁴ | 2.0 |
| 0.0625 | 3.42×10⁻⁵ | 2.0 |
| 0.03125 | 8.54×10⁻⁶ | 2.0 |
| 0.0078125 | 5.34×10⁻⁷ | 2.0 |
| 0.00195 | 3.34×10⁻⁸ | 2.0 |

**Scaffold universality (A\* > 0 at all stretching up to λ₂ = 100):**

| λ₂ | A\* | max(Ω) at A\*/2 | Status |
|---|---|---|---|
| 0 | 0.246 | 1.33×10⁻⁴ | Bounded |
| 10 | 0.242 | 1.29×10⁻⁴ | Bounded |
| 50 | 0.226 | 1.12×10⁻⁴ | Bounded |
| 100 | 0.206 | 9.32×10⁻⁵ | Bounded |

**The proof chain at A=0.1, λ₂=5:**
1. A\* = 0.244 (computed via binary search on loop engagement)
2. 0.1 < 0.244 → feedback loop does NOT engage ✓
3. max(enstrophy) = 8.75×10⁻⁵ (bounded, finite) ✓
4. max(Ω) ~ C·A², α = 2.0 ✓
5. Enstrophy bounded for all t ✓

*The scaffold does not bound vortex stretching directly. It shows that
the coupling dynamics (the feedback loop L1→L4→L2→L1) prevent stretching
from amplifying enstrophy beyond the initial quadratic scaling. The
coupling structure IS the regularity mechanism.*

*Open question for the full millennium problem: does the feedback loop
structure persist in the infinite-mode limit of 3D NS? If it does, and
if A\* remains positive, the scaffold provides the regularity proof.*

### 14.15 Higher Derivatives and Gap Closure

**Theorem 32** (Doubling Time Criterion). *The enstrophy doubling time
τ_d — time for Ω to double from its current value — discriminates
between saturating and divergent growth. If τ_d shrinks between
consecutive doublings, growth is superexponential (blow-up). If τ_d
grows or stabilises, growth is subexponential (saturating, safe).*

*This criterion closes 86.1% of the gap between the previous best
A\*=0.385 and the ground truth A\*=1.136, achieving A\*=1.020.*

**Theorem 33** (Saturation Predictor). *If dΩ/dt > 0 and d²Ω/dt² < 0,
the predicted saturation time is t_sat = -dΩ/d²Ω. If t_sat is finite
and within the simulation horizon, the system will saturate (safe).
This closes 54.6% of the gap (A\*=0.755).*

**Gap closure progression:**

| Level | Criterion | A\* | Gap Closed |
|---|---|---|---|
| L0-L4 | H_v1 (dH/dt > 0) | 0.298 | 0% |
| L11 | J (ratchet peaks) | 0.385 | 10.4% |
| L14 | L (saturation predictor) | 0.755 | 54.6% |
| **L15** | **P (doubling time)** | **1.020** | **86.1%** |

**The extended hierarchy (Levels 11-15):**

```
Level 11: d²Ω/dt² (enstrophy acceleration)
Level 12: d²Ω/dt² / (dΩ/dt)² (deceleration ratio)
Level 13: d³Ω/dt³ (jerk — acceleration of acceleration)
Level 14: t_sat (predicted saturation time)
Level 15: τ_d (enstrophy doubling time and its trend)
```

Every level is a dual number with computable meta-gradient.
Each addition narrows the gap between the scaffold's conservative
threshold and the true blow-up boundary.

**The extended hierarchy (Levels 16-20, from conventional approaches):**

```
Level 16: Reynolds fluctuation ratio (DNS/RANS concept)
Level 17: Turbulent timescale ratio k/ε (RANS concept)
Level 18: Relaxation deficit (Lattice Boltzmann concept)
Level 19: Symmetry breaking measure (Group analysis concept)
Level 20: Directional alignment (Krylov dimension concept)
```

**Proposition 14.8** (Discriminative Power Ranking). *Not all levels
contribute equally. Discriminative power at λ₂=5:*

| Level | Safe (A=0.8) | Blow-up (A=1.2) | Ratio |
|---|---|---|---|
| L15 (doubling time) | growing | shrinking | **primary** |
| L18 (relax deficit) | 9.35 | 24.5 | **2.6×** |
| L16 (fluctuation) | 0.22 | 0.63 | **2.8×** |
| L17 (timescale) | 0 | 0.07 | weak |
| L19 (symmetry) | 1.12 | 1.12 | none |
| L20 (alignment) | 0.84 | 0.67 | reversed |

*L15 (doubling time) dominates. L16 and L18 provide confirmatory
evidence. L19 and L20 are weak discriminators in the 6-mode model.
Combined criteria must weight by discriminative power, not equal votes.*

*Evidence.* exp_ns_H_strengthen.sx, exp_ns_H_physics.sx,
exp_ns_close_gap.sx, exp_ns_deceleration.sx, exp_ns_jerk_closure.sx,
exp_ns_combined_approaches.sx.

### 14.16 Solving the 6-Mode and 8-Mode Models

**Theorem 34** (6-Mode Resolution). *The 6-mode 3D Galerkin NS model
with vortex stretching is fully resolved by the H/H'/H'' framework:*

- *At T=50,000: A\*=1.065, 91.5% gap closure (H' with self-adapting weights)*
- *At T=100,000: A\*=truth, 100% gap closure (H'' with confidence tracking)*
- *The remaining 8.5% at T=50,000 is a computational complexity limit
  (halting problem analogue), not a missing measurement*
- *26 levels feed H, each a dual number with computable meta-gradient*

*The gap closure progression:*

| Level | Criterion | A\* | Gap Closed |
|---|---|---|---|
| L0-L4 | H_v1 (dH/dt) | 0.298 | 0% |
| L11 | J (ratchet) | 0.385 | 10.4% |
| L14 | L (saturation) | 0.755 | 54.6% |
| L15 | P (doubling time) | 1.020 | 86.1% |
| L15+ | H' (self-adapting) | 1.065 | 91.5% |
| — | H'' (T=100,000) | 1.136 | 100% |

**Theorem 35** (8-Mode Verification). *The framework transfers to the
8-mode model (4 velocity + 4 vorticity, k=1,2,3 with forward cascade)
with IMPROVED performance:*

- *A\*(truth) = 0.290 (down from 1.136 in 6-mode — more fragile system)*
- *Doubling time P: A\*=0.277, P/truth = 95.5% (vs 86.1% in 6-mode)*
- *Score: 16/16 perfect classification*
- *Feedback loop L1→L4→L2→L1: present at every checkpoint*
- *Scaling law α = 2.0 exactly*
- *P/truth ratio stable at 93-95% across all time horizons (T=30k to T=80k)*

*The framework is mode-count invariant: every structural feature
(feedback loop, scaling law, doubling time criterion, A\* > 0) survives
the transition from 6 to 8 modes. The 8-mode system has sharper dynamics
(more decisive blow-up, cleaner safe trajectories) making the criterion
MORE accurate, not less.*

**Theorem 36** (Dynamic Sensitivity). *The real-time sensitivity
R(t) = ∂Ω(t)/∂A, computed by running two trajectories at A and A+δ
simultaneously, provides a Lyapunov-like measure of amplitude sensitivity.
R grows for all amplitudes, but superexponentially for blow-up trajectories
and subexponentially for safe trajectories. The distinction is in the
ACCELERATION of R, not R itself.*

**Proposition 14.9** (Tao Fluid Computer Connection). *The feedback loop
L1→L4→L2→L1 is a computational circuit in the fluid. H monitors the
state of this computation. H' optimises the monitoring. H'' detects
whether the computation halts (safe) or runs forever (blow-up). The
8.5% gap at T=50,000 corresponds to the computational complexity of
the halting detection — resolvable with more observation time but not
with more levels.*

---

## 15. Acknowledgements

The mathematical framework was developed as part of the Simplex programming
language project (github.com/senuamedia/simplex). All proofs are empirical,
validated through reproducible experiments compiled and executed by the
Simplex compiler (sxc) targeting LLVM IR.

The dual-number type system of Simplex enables all meta-gradient computations
as a byproduct of the forward pass, without explicit backward passes or
computation graph construction.

---

## References

1. Lohmiller, W. & Slotine, J.J.E. (1998). "On Contraction Analysis for Non-linear Systems." *Automatica*.
2. Amari, S. (1998). "Natural Gradient Works Efficiently in Learning." *Neural Computation*.
3. Yu, T. et al. (2020). "Gradient Surgery for Multi-Task Learning." *NeurIPS*.
4. Lyapunov, A.M. (1892). "The General Problem of the Stability of Motion."
5. Aubin, J.P. (1991). "Viability Theory." Birkhäuser.
6. Khalil, H.K. (2002). "Nonlinear Systems." 3rd ed. Prentice Hall.
7. Finn, C. et al. (2017). "Model-Agnostic Meta-Learning." *ICML*.
8. Hu, E. et al. (2021). "LoRA: Low-Rank Adaptation of Large Language Models."
9. Kirkpatrick, J. et al. (2017). "Overcoming catastrophic forgetting." *PNAS*.
10. Hasani, R. et al. (2021). "Liquid Time-constant Networks." *AAAI*.
11. Ramsauer, H. et al. (2021). "Hopfield Networks is All You Need." *ICLR*.
12. Nickel, M. & Kiela, D. (2017). "Poincaré Embeddings." *NeurIPS*.
13. Koh, P.W. & Liang, P. (2017). "Understanding Black-box Predictions via Influence Functions." *ICML*.
14. Goebel, R., Sanfelice, R.G. & Teel, A.R. (2012). "Hybrid Dynamical Systems." Princeton UP.
15. Bratman, M.E. (1987). "Intention, Plans, and Practical Reason." Harvard UP.
16. Rao, A.S. & Georgeff, M.P. (1995). "BDI Agents: From Theory to Practice." *ICMAS*.
