# TASK-006: Self-Learning Annealing

**Status**: Complete (~85%)
**Priority**: High
**Created**: 2026-01-11
**Updated**: 2026-03-16 (audited against codebase)
**Target Version**: 0.9.0

> **Audit Note (2026-03-16):** Core self-learning annealing is fully implemented:
> - `simplex-std/src/anneal.sx` (1085 lines) — LearnableSchedule, MetaOptimizer,
>   AnnealState, MetaLoss, MetaHistory, AnnealConfig (default/quick/thorough presets)
> - Differentiable temperature: exponential cooling + oscillation + stagnation-triggered reheat
> - Soft acceptance function: sigmoid-based with gradient flow
> - Meta-gradient extraction and parameter updates with clipping
> - Epistemic extension (bonus): `simplex-learning/src/epistemic/schedule.sx` (407 lines)
>   and `meta_optimizer.sx` (830 lines) — temperature modulation based on epistemic health
> - Tests present for core annealing and epistemic extensions
**Depends On**: TASK-005 (Dual Numbers) - Complete

## Overview

Implement self-learning annealing in Simplex, where the optimization schedule (cooling rate, reheating triggers, temperature bounds) is itself learned through meta-gradients. Instead of hand-tuning annealing hyperparameters, the system discovers optimal schedules through differentiable optimization.

**Key Insight**: Dual numbers (TASK-005) already provide differentiable neural gates (sigmoid, tanh, exp). By composing these primitives, we can build fully differentiable annealing schedules that learn themselves through gradient descent on the meta-objective.

```simplex
// Self-learning annealing: the schedule learns itself
let schedule = AnnealSchedule::learnable();
let optimizer = MetaOptimizer::new(schedule);

for epoch in 0..epochs {
    let (solution, meta_loss) = optimizer.anneal_with_grad(objective);
    schedule.update(meta_loss.gradient());  // Schedule improves each epoch
}
// After training: schedule.cool_rate, schedule.reheat_threshold are optimal
```

---

## Background

### Traditional Simulated Annealing

Simulated annealing optimizes by probabilistically accepting worse solutions based on a temperature parameter that decreases over time:

```
P(accept) = exp(-ΔE / T)
```

Where:
- `ΔE` = energy difference (objective change)
- `T` = temperature (decreases according to schedule)

The **cooling schedule** `T(t)` is critical to performance:
- Cool too fast → get stuck in local minima
- Cool too slow → waste computation
- Wrong reheat triggers → miss global optima

Traditional approaches use fixed schedules (linear, exponential, logarithmic) with hand-tuned parameters.

### The Meta-Learning Insight

Instead of hand-tuning, we can:
1. **Parameterize** the schedule with learnable variables
2. **Differentiate** the entire annealing process w.r.t. schedule parameters
3. **Optimize** the schedule using gradients on a meta-objective

This requires:
- Differentiable acceptance probability (soft Gumbel-softmax)
- Differentiable temperature dynamics
- Meta-objective that measures optimization quality

### Why Dual Numbers Enable This

From TASK-005, we have:
- `dual` type with automatic derivative propagation
- Differentiable `sigmoid`, `tanh`, `exp` (neural gate primitives)
- Zero-overhead compilation

The temperature schedule and acceptance function are compositions of these primitives. No new primitives needed—the dual number algebra handles everything.

---

## Mathematical Foundation

### 1. Differentiable Annealing Framework

Let θ be the schedule parameters: θ = {α, β, γ, ...}

The annealing process produces a solution trajectory:
```
x₀ → x₁ → x₂ → ... → xₙ
```

Each transition is governed by:
```
xₜ₊₁ = xₜ + Δxₜ · A(ΔE, T(t; θ))
```

Where:
- `Δxₜ` = proposed move (from neighborhood sampling)
- `A(ΔE, T)` = soft acceptance function
- `T(t; θ)` = temperature at step t, parameterized by θ

### 2. Soft Acceptance Function

Traditional acceptance is binary (accept/reject). We use a differentiable relaxation:

**Hard acceptance**:
```
A_hard = 1 if ΔE < 0 else (1 if random() < exp(-ΔE/T) else 0)
```

**Soft acceptance** (differentiable):
```
A_soft(ΔE, T; τ) = σ((τ - ΔE) / T)
```

Where:
- `σ` = sigmoid function
- `τ` = learnable acceptance threshold (initially 0)
- As T → 0, this approaches hard acceptance

In dual numbers:
```simplex
fn soft_accept(delta_e: dual, temp: dual, tau: dual) -> dual {
    let scaled = (tau - delta_e) / temp;
    scaled.sigmoid()  // Differentiable!
}
```

### 3. Learnable Temperature Schedule

The temperature schedule is a parameterized function of time:

**Base schedule** (exponential decay with perturbations):
```
T(t; θ) = T₀ · exp(-α·t) · (1 + β·sin(ω·t)) + γ·R(t)
```

Where:
- `T₀` = initial temperature (learnable)
- `α` = cooling rate (learnable)
- `β` = oscillation amplitude (learnable, for reheating)
- `ω` = oscillation frequency (learnable)
- `γ` = reheat intensity (learnable)
- `R(t)` = reheat trigger function

**Reheat trigger** (learnable threshold):
```
R(t) = σ((stagnation(t) - ρ) / ε)
```

Where:
- `stagnation(t)` = steps since last improvement
- `ρ` = reheat threshold (learnable)
- `ε` = sharpness parameter

In dual numbers:
```simplex
struct LearnableSchedule {
    t0: dual,           // Initial temperature
    alpha: dual,        // Cooling rate
    beta: dual,         // Oscillation amplitude
    omega: dual,        // Oscillation frequency
    gamma: dual,        // Reheat intensity
    rho: dual,          // Reheat threshold
}

impl LearnableSchedule {
    fn temperature(&self, t: dual, stagnation: dual) -> dual {
        let base = self.t0 * (-self.alpha * t).exp();
        let oscillation = dual::constant(1.0) + self.beta * (self.omega * t).sin();
        let reheat_trigger = ((stagnation - self.rho) / dual::constant(0.1)).sigmoid();
        let reheat = self.gamma * reheat_trigger;

        base * oscillation + reheat
    }
}
```

### 4. Meta-Objective Function

The meta-objective measures how well the schedule performs:

```
L_meta(θ) = E[f(x_final)] + λ₁·T_convergence + λ₂·||θ||² + λ₃·H(trajectory)
```

Where:
- `f(x_final)` = final objective value (lower is better)
- `T_convergence` = steps to convergence (efficiency)
- `||θ||²` = regularization on schedule parameters
- `H(trajectory)` = entropy of solution trajectory (exploration)
- `λ₁, λ₂, λ₃` = weighting coefficients

### 5. Meta-Gradient Computation

Using forward-mode AD (dual numbers), we compute:

```
∂L_meta/∂θ = ∂L_meta/∂x_final · ∂x_final/∂θ
```

The chain through the annealing process:
```
∂x_final/∂θ = Σₜ (∂xₜ₊₁/∂xₜ · ∂xₜ/∂θ + ∂xₜ₊₁/∂Aₜ · ∂Aₜ/∂θ)
```

Since A depends on T, and T depends on θ:
```
∂Aₜ/∂θ = ∂A/∂T · ∂T/∂θ
```

All these derivatives are computed automatically by dual number propagation.

### 6. Meta-Gradient Update Rule

After each annealing run:
```
θ ← θ - η · ∂L_meta/∂θ
```

With adaptive learning rate η based on meta-gradient variance:
```
η = η₀ / (1 + σ(∂L_meta/∂θ))
```

---

## Core Technical Specification

### 1. AnnealState Type

```simplex
/// State of a differentiable annealing process
@zero_overhead
pub struct AnnealState<S> {
    /// Current solution
    solution: S,
    /// Current objective value (dual for gradient tracking)
    energy: dual,
    /// Current temperature
    temperature: dual,
    /// Steps since last improvement
    stagnation: dual,
    /// Total steps taken
    step: i64,
    /// Best solution found
    best_solution: S,
    /// Best energy found
    best_energy: dual,
}
```

### 2. LearnableSchedule Type

```simplex
/// Learnable annealing schedule with meta-gradient support
@zero_overhead
pub struct LearnableSchedule {
    // Core parameters (all dual for gradient tracking)
    initial_temp: dual,      // T₀: starting temperature
    cool_rate: dual,         // α: exponential decay rate
    min_temp: dual,          // T_min: temperature floor

    // Reheating parameters
    reheat_threshold: dual,  // ρ: stagnation steps before reheat
    reheat_intensity: dual,  // γ: how much to increase temp on reheat
    reheat_decay: dual,      // How quickly reheat effect fades

    // Oscillation parameters (for periodic reheating)
    oscillation_amp: dual,   // β: amplitude of temperature oscillation
    oscillation_freq: dual,  // ω: frequency of oscillation

    // Acceptance parameters
    accept_threshold: dual,  // τ: soft acceptance threshold
    accept_sharpness: dual,  // How sharp the acceptance boundary is
}

impl LearnableSchedule {
    /// Create schedule with default initial values
    pub fn new() -> LearnableSchedule {
        LearnableSchedule {
            initial_temp: dual::variable(1.0),
            cool_rate: dual::variable(0.01),
            min_temp: dual::constant(0.001),
            reheat_threshold: dual::variable(50.0),
            reheat_intensity: dual::variable(0.5),
            reheat_decay: dual::variable(0.1),
            oscillation_amp: dual::variable(0.0),
            oscillation_freq: dual::variable(0.1),
            accept_threshold: dual::variable(0.0),
            accept_sharpness: dual::variable(1.0),
        }
    }

    /// Compute temperature at given step with stagnation info
    pub fn temperature(&self, step: dual, stagnation: dual) -> dual {
        // Base exponential cooling
        let base = self.initial_temp * (-self.cool_rate * step).exp();

        // Periodic oscillation (optional reheating cycles)
        let osc = dual::constant(1.0) +
                  self.oscillation_amp * (self.oscillation_freq * step).sin();

        // Stagnation-triggered reheat
        let reheat_trigger = ((stagnation - self.reheat_threshold) /
                              dual::constant(10.0)).sigmoid();
        let reheat = self.reheat_intensity * reheat_trigger *
                     (-self.reheat_decay * (stagnation - self.reheat_threshold)).exp();

        // Combined temperature (clamped to minimum)
        let temp = base * osc + reheat;
        temp.max(self.min_temp)
    }

    /// Soft acceptance probability (differentiable)
    pub fn accept_probability(&self, delta_e: dual, temp: dual) -> dual {
        // If delta_e < 0 (improvement), accept with high probability
        // If delta_e > 0 (worse), accept with Boltzmann probability
        let scaled = (self.accept_threshold - delta_e) /
                     (temp * self.accept_sharpness);
        scaled.sigmoid()
    }

    /// Extract gradient vector for meta-update
    pub fn gradient(&self) -> ScheduleGradient {
        ScheduleGradient {
            d_initial_temp: self.initial_temp.der,
            d_cool_rate: self.cool_rate.der,
            d_reheat_threshold: self.reheat_threshold.der,
            d_reheat_intensity: self.reheat_intensity.der,
            d_reheat_decay: self.reheat_decay.der,
            d_oscillation_amp: self.oscillation_amp.der,
            d_oscillation_freq: self.oscillation_freq.der,
            d_accept_threshold: self.accept_threshold.der,
            d_accept_sharpness: self.accept_sharpness.der,
        }
    }

    /// Apply meta-gradient update
    pub fn update(&mut self, grad: ScheduleGradient, learning_rate: f64) {
        self.initial_temp = dual::new(
            self.initial_temp.val - learning_rate * grad.d_initial_temp,
            1.0
        );
        self.cool_rate = dual::new(
            self.cool_rate.val - learning_rate * grad.d_cool_rate,
            1.0
        );
        // ... update all parameters
    }
}
```

### 3. MetaOptimizer Type

```simplex
/// Meta-optimizer that learns the annealing schedule
pub struct MetaOptimizer<S, F> {
    schedule: LearnableSchedule,
    objective: F,
    meta_learning_rate: f64,
    regularization: f64,
    history: Vec<MetaEpoch>,
}

struct MetaEpoch {
    final_energy: f64,
    convergence_step: i64,
    schedule_params: ScheduleSnapshot,
}

impl<S: Clone, F: Fn(&S) -> dual> MetaOptimizer<S, F> {
    pub fn new(objective: F) -> MetaOptimizer<S, F> {
        MetaOptimizer {
            schedule: LearnableSchedule::new(),
            objective,
            meta_learning_rate: 0.001,
            regularization: 0.0001,
            history: vec_new(),
        }
    }

    /// Run one annealing episode with gradient tracking
    pub fn anneal_episode(
        &self,
        initial: S,
        neighbor_fn: impl Fn(&S) -> S,
        max_steps: i64
    ) -> (S, dual) {
        var state = AnnealState::new(initial, &self.objective);

        for step in 0..max_steps {
            let step_dual = dual::constant(step as f64);
            let stagnation_dual = dual::constant(state.stagnation as f64);

            // Get current temperature (differentiable)
            let temp = self.schedule.temperature(step_dual, stagnation_dual);

            // Propose neighbor
            let neighbor = neighbor_fn(&state.solution);
            let neighbor_energy = (self.objective)(&neighbor);
            let delta_e = neighbor_energy - state.energy;

            // Soft acceptance (differentiable)
            let accept_prob = self.schedule.accept_probability(delta_e, temp);

            // Soft transition (mix current and neighbor based on acceptance)
            // This keeps the gradient flowing through rejected moves too
            state.energy = state.energy * (dual::constant(1.0) - accept_prob) +
                          neighbor_energy * accept_prob;

            // Hard transition for actual solution (but gradient flows through energy)
            if accept_prob.val > random_f64() {
                state.solution = neighbor;
                if state.energy.val < state.best_energy.val {
                    state.best_solution = state.solution.clone();
                    state.best_energy = state.energy;
                    state.stagnation = 0;
                }
            } else {
                state.stagnation += 1;
            }
        }

        (state.best_solution, state.best_energy)
    }

    /// Run meta-optimization loop
    pub fn optimize(
        &mut self,
        initial: S,
        neighbor_fn: impl Fn(&S) -> S,
        meta_epochs: i64,
        steps_per_epoch: i64
    ) -> S {
        var best_overall = initial.clone();
        var best_overall_energy = f64::INFINITY;

        for epoch in 0..meta_epochs {
            // Run annealing with current schedule (gradients tracked)
            let (solution, final_energy) = self.anneal_episode(
                initial.clone(),
                &neighbor_fn,
                steps_per_epoch
            );

            // Compute meta-loss
            let meta_loss = self.compute_meta_loss(final_energy, epoch);

            // Extract gradients and update schedule
            let grad = self.schedule.gradient();
            self.schedule.update(grad, self.meta_learning_rate);

            // Track best
            if final_energy.val < best_overall_energy {
                best_overall = solution;
                best_overall_energy = final_energy.val;
            }

            // Record history
            vec_push(&mut self.history, MetaEpoch {
                final_energy: final_energy.val,
                convergence_step: steps_per_epoch,  // TODO: track actual
                schedule_params: self.schedule.snapshot(),
            });
        }

        best_overall
    }

    fn compute_meta_loss(&self, final_energy: dual, epoch: i64) -> dual {
        // Primary objective: minimize final energy
        let energy_term = final_energy;

        // Regularization: prefer simple schedules
        let reg_term = self.schedule.l2_norm() * dual::constant(self.regularization);

        // Exploration bonus: encourage trying different temperatures
        let exploration_term = -self.schedule.temperature_variance() *
                               dual::constant(0.01);

        energy_term + reg_term + exploration_term
    }
}
```

### 4. Convenience API

```simplex
module simplex::optimize::anneal;

/// Simple interface for self-learning annealing
pub fn self_learn_anneal<S, F, N>(
    objective: F,
    initial: S,
    neighbor: N,
    config: AnnealConfig
) -> S
where
    S: Clone,
    F: Fn(&S) -> f64,
    N: Fn(&S) -> S,
{
    // Wrap objective in dual for gradient tracking
    let dual_objective = |s: &S| dual::constant(objective(s));

    var optimizer = MetaOptimizer::new(dual_objective);
    optimizer.meta_learning_rate = config.meta_learning_rate;
    optimizer.regularization = config.regularization;

    optimizer.optimize(
        initial,
        neighbor,
        config.meta_epochs,
        config.steps_per_epoch
    )
}

pub struct AnnealConfig {
    pub meta_epochs: i64,          // How many times to update schedule
    pub steps_per_epoch: i64,      // Annealing steps per meta-epoch
    pub meta_learning_rate: f64,   // Learning rate for schedule params
    pub regularization: f64,       // L2 regularization strength
}

impl AnnealConfig {
    pub fn default() -> AnnealConfig {
        AnnealConfig {
            meta_epochs: 100,
            steps_per_epoch: 1000,
            meta_learning_rate: 0.001,
            regularization: 0.0001,
        }
    }
}
```

---

## Integration with Existing Features

### 1. Integration with Neural Gates

Self-learning annealing for neural architecture search:

```simplex
// Optimize neural gate structure using learned annealing
neural_gate Classifier {
    @learnable
    hidden_size: i64,  // Structure parameter to optimize

    @differentiable
    fn forward(&self, x: Tensor) -> Tensor {
        let h = self.layer1(x);
        h.relu().layer2()
    }
}

fn optimize_architecture() -> Classifier {
    let objective = |config: &ArchConfig| -> f64 {
        let model = Classifier::from_config(config);
        model.train_and_validate()  // Returns validation loss
    };

    let neighbor = |config: &ArchConfig| -> ArchConfig {
        config.mutate()  // Random architecture mutation
    };

    let best_config = self_learn_anneal(
        objective,
        ArchConfig::default(),
        neighbor,
        AnnealConfig::default()
    );

    Classifier::from_config(&best_config)
}
```

### 2. Integration with Belief System

Learning optimal belief revision schedules:

```simplex
// Beliefs with learnable update dynamics
struct BeliefAnnealState {
    beliefs: BeliefGraph,
    schedule: LearnableSchedule,
}

fn learn_belief_dynamics(evidence_stream: Stream<Evidence>) -> LearnableSchedule {
    let objective = |state: &BeliefAnnealState| -> f64 {
        state.beliefs.consistency_score() +
        state.beliefs.prediction_accuracy(evidence_stream)
    };

    // The schedule learns when to be "open-minded" (high temp)
    // vs "committed" (low temp) in belief updates
    var optimizer = MetaOptimizer::new(objective);

    optimizer.optimize(
        BeliefAnnealState::initial(),
        |s| s.with_random_belief_update(),
        100,  // meta epochs
        500   // belief updates per epoch
    )
}
```

### 3. Integration with HiveOS

Learning coordination schedules for specialist agents:

```simplex
// Hive annealing: learn when agents should explore vs exploit
specialist Coordinator {
    schedule: LearnableSchedule,

    fn allocate_tasks(&self, tasks: Vec<Task>) -> TaskAllocation {
        let temp = self.schedule.temperature(
            dual::constant(self.step),
            dual::constant(self.idle_specialists)
        );

        if temp.val > 0.5 {
            // High temperature: explore new specialist combinations
            self.exploratory_allocation(tasks)
        } else {
            // Low temperature: exploit known-good allocations
            self.greedy_allocation(tasks)
        }
    }
}
```

### 4. Integration with Safety Constraints

Safe annealing with constraint-aware schedules:

```simplex
// Learn schedules that respect safety constraints
fn safe_anneal<S>(
    objective: impl Fn(&S) -> dual,
    constraint: impl Fn(&S) -> dual,  // Must be > 0 for safety
    initial: S,
    neighbor: impl Fn(&S) -> S,
) -> S {
    let safe_objective = |s: &S| {
        let obj = objective(s);
        let margin = constraint(s);

        // Penalize constraint violations heavily
        if margin.val < 0.0 {
            obj + dual::constant(1000.0) * (-margin)
        } else {
            obj
        }
    };

    let safe_neighbor = |s: &S| {
        var candidate = neighbor(s);
        // Gradient of constraint tells us which direction is safe
        let margin = constraint(&candidate);
        if margin.val < 0.0 && margin.der != 0.0 {
            // Project back to safe region using gradient
            candidate = project_to_safe(candidate, margin);
        }
        candidate
    };

    self_learn_anneal(safe_objective, initial, safe_neighbor, AnnealConfig::default())
}
```

---

## Implementation Phases

### Phase 1: Core Differentiable Annealing

**Deliverables**:
1. `AnnealState<S>` struct with dual-valued energy/temperature
2. `LearnableSchedule` struct with core parameters
3. Soft acceptance function using sigmoid
4. Basic `temperature()` function with exponential cooling
5. Integration tests with dual number verification

**Success Criteria**:
- [ ] Soft acceptance probability is differentiable (verify with dual numbers)
- [ ] Temperature schedule gradients flow correctly
- [ ] Basic annealing matches traditional simulated annealing behavior
- [ ] Zero-overhead compilation for schedule computations

### Phase 2: Meta-Gradient Framework

**Deliverables**:
1. `MetaOptimizer<S, F>` struct
2. `compute_meta_loss()` with energy + regularization terms
3. `gradient()` extraction from schedule
4. `update()` for schedule parameter updates
5. History tracking for convergence analysis

**Success Criteria**:
- [ ] Meta-gradients computed correctly (verify with finite differences)
- [ ] Schedule parameters update based on meta-loss
- [ ] Learning curves show improvement over epochs
- [ ] Regularization prevents parameter explosion

### Phase 3: Advanced Schedule Features

**Deliverables**:
1. Stagnation-triggered reheating
2. Periodic oscillation in temperature
3. Adaptive acceptance threshold
4. Multi-parameter schedule types
5. Schedule serialization/loading

**Success Criteria**:
- [ ] Reheating triggers correctly when stuck
- [ ] Oscillation helps escape local minima
- [ ] Learned schedules outperform fixed schedules on benchmarks
- [ ] Schedules can be saved and reused

### Phase 4: Integration and Optimization

**Deliverables**:
1. Neural Gate integration for architecture search
2. Belief System integration for revision dynamics
3. HiveOS integration for coordination
4. Safety constraint-aware annealing
5. SIMD optimization for batch annealing

**Success Criteria**:
- [ ] Neural architecture search finds better architectures
- [ ] Belief systems learn effective update dynamics
- [ ] Hive coordination improves with learned schedules
- [ ] Safety constraints never violated
- [ ] >10x speedup with SIMD batch annealing

### Phase 5: Production Hardening

**Deliverables**:
1. Comprehensive benchmark suite
2. Schedule visualization tools
3. Hyperparameter tuning guidelines
4. Production-ready API
5. Documentation and tutorials

**Success Criteria**:
- [ ] Benchmark suite covers 10+ optimization problems
- [ ] Visualization shows schedule evolution
- [ ] Default hyperparameters work well across problems
- [ ] API is stable and well-documented
- [ ] Tutorial demonstrates end-to-end usage

---

## Benchmark Problems

### 1. Continuous Optimization

```simplex
// Rastrigin function (many local minima)
fn rastrigin(x: Vec<dual>) -> dual {
    let n = vec_len(&x) as f64;
    var sum = dual::constant(10.0 * n);
    for xi in x {
        sum = sum + xi * xi - dual::constant(10.0) * (dual::constant(2.0 * PI) * xi).cos();
    }
    sum
}

// Test: does learned schedule outperform exponential cooling?
fn benchmark_rastrigin() {
    let fixed_result = fixed_anneal(rastrigin, random_init(10));
    let learned_result = self_learn_anneal(rastrigin, random_init(10));

    assert(learned_result < fixed_result);  // Learned should be better
}
```

### 2. Combinatorial Optimization

```simplex
// Traveling Salesman Problem
fn tsp_energy(tour: &Vec<i64>, distances: &Matrix) -> f64 {
    var total = 0.0;
    for i in 0..vec_len(tour) - 1 {
        total += distances.get(tour[i], tour[i + 1]);
    }
    total + distances.get(tour[vec_len(tour) - 1], tour[0])
}

fn benchmark_tsp() {
    let problem = generate_tsp(50);  // 50 cities
    let learned_tour = self_learn_anneal(...);
    // Verify tour length is competitive with known heuristics
}
```

### 3. Neural Architecture Search

```simplex
fn benchmark_nas() {
    // Search space: layer sizes, activation functions, skip connections
    let best_arch = optimize_architecture();
    // Verify: found architecture beats random search
}
```

---

## Technical Dependencies

- TASK-005: Dual Numbers (complete) - provides differentiable primitives
- LLVM for zero-overhead compilation
- Random number generation intrinsics
- Optional: SIMD for batch operations

---

## Research References

- Kirkpatrick et al., "Optimization by Simulated Annealing" (1983) - Original SA paper
- Andrieu et al., "Simulated Annealing" (2003) - Survey and convergence theory
- Maclaurin et al., "Gradient-based Hyperparameter Optimization" (2015) - Meta-gradients
- Lorraine et al., "Optimizing Millions of Hyperparameters by Implicit Differentiation" (2020)
- Metz et al., "Learning to Learn with Compound HD Models" (2019)
- Baydin et al., "Automatic Differentiation in Machine Learning: A Survey" (2018)
- Henderson et al., "Differentiable Physics for Deep Learning" (2021)

---

## API Summary

```simplex
// Core types
AnnealState<S>              // State of annealing process
LearnableSchedule           // Learnable temperature schedule
MetaOptimizer<S, F>        // Meta-optimizer for schedule learning
AnnealConfig               // Configuration for annealing runs

// Schedule methods
schedule.temperature(step, stagnation) -> dual    // Get current temperature
schedule.accept_probability(delta_e, temp) -> dual // Soft acceptance
schedule.gradient() -> ScheduleGradient           // Extract meta-gradients
schedule.update(grad, lr)                         // Apply meta-update

// Meta-optimizer methods
optimizer.anneal_episode(initial, neighbor, steps) -> (S, dual)
optimizer.optimize(initial, neighbor, epochs, steps) -> S
optimizer.schedule() -> &LearnableSchedule

// Convenience function
self_learn_anneal(objective, initial, neighbor, config) -> S

// Preset schedules (for comparison)
FixedSchedule::exponential(t0, alpha)
FixedSchedule::linear(t0, t_final, steps)
FixedSchedule::logarithmic(t0, c)
```

---

## Notes

Self-learning annealing represents a paradigm shift in optimization. Instead of treating the cooling schedule as a hyperparameter to tune, we treat it as a **learned function** that adapts to the problem structure.

The key enabler is dual numbers (TASK-005). Because sigmoid, exp, and other neural gate primitives are already differentiable through dual number composition, we can build complex temperature schedules that remain fully differentiable. No special autodiff framework needed—the dual number algebra handles everything automatically.

Expected benefits:
1. **Reduced tuning**: No more grid search over cooling schedules
2. **Problem adaptation**: Learned schedules exploit problem structure
3. **Transferability**: Schedules learned on one problem may transfer to similar problems
4. **Interpretability**: Learned parameters reveal what makes good schedules

This lays the groundwork for more ambitious meta-learning in Simplex, where not just schedules but entire optimization algorithms can be learned through gradient descent.
