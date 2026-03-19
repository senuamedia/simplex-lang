# TASK-043: Neural ODEs & Continuous Depth Networks

**Version:** 0.16.0
**Status:** Planned
**Priority:** P2 — Medium
**Depends on:** v0.15.0 release, dual numbers (TASK-005), existing layer infrastructure

## Why This Feature Is Needed

Standard neural networks have fixed depth: Layer 1 → Layer 2 → ... → Layer N. Every
input, regardless of difficulty, passes through the same number of layers and costs the
same computation. A trivial query costs the same as a complex reasoning task. This is
wasteful — and for specialist SLMs on constrained hardware, waste is unacceptable.

Neural ODEs reconceptualize depth as continuous. Instead of discrete layers, the model
defines a vector field (the ODE dynamics), and a numerical solver integrates through
"depth space" from input to output. The solver adaptively chooses how many steps to take
based on the difficulty of the input:

- Simple inputs: few integration steps → fast inference
- Complex inputs: many integration steps → more computation, better accuracy
- The model decides *how much to think* per input automatically

This is not just efficiency. It is a fundamentally different computational paradigm:
- Constant memory training via the adjoint method (backprop through the ODE solver
  uses O(1) memory regardless of depth, vs O(n) for standard networks)
- Continuous normalizing flows for density estimation and generation
- Time-series modeling where the dynamics are the learned quantity
- Infinite-depth networks that learn their own depth

For Simplex's vision of adaptive, efficient specialist SLMs, Neural ODEs are the
architecture for variable-cost inference. A specialist that dynamically adjusts its
computational budget per query — spending microseconds on easy questions and milliseconds
on hard ones — is uniquely suited to edge deployment where every FLOP matters.

## Why It Adds Value

1. **Adaptive computation.** The ODE solver automatically spends more compute on harder
   inputs. No manual depth tuning — the model learns its own depth profile.

2. **Constant memory training.** The adjoint method computes gradients through the ODE
   solver using O(1) memory, regardless of how many integration steps were taken. This
   enables training very deep effective networks on memory-constrained devices.

3. **Continuous-time modeling.** For specialists handling time-series data (sensors,
   financial data, physiological signals), Neural ODEs model the continuous dynamics
   directly, handling irregular time intervals naturally (no resampling needed).

4. **Composition with dual numbers.** The ODE dynamics are differentiable functions.
   Simplex's dual number infrastructure provides exact sensitivity analysis through the
   ODE solver — how does the output change if we perturb the initial condition or the
   dynamics parameters?

5. **Tolerance-based budgets.** The solver tolerance directly controls the quality-speed
   tradeoff. At deployment time, set tolerance high for fast approximate answers, or
   low for precise answers. This is a *compile-time* knob for inference cost.

6. **Generative modeling.** Continuous normalizing flows (CNFs) transform a simple
   distribution (Gaussian) into a complex data distribution through a learned ODE. This
   enables data generation, anomaly detection, and density estimation in a single
   framework.

## Why It Changes Systems Built With Simplex

Neural ODEs introduce adaptive computation to every level of Simplex AI systems:

- **Variable-cost specialists.** Each specialist in a cognitive hive adjusts its compute
  per query. A medical diagnostic specialist spends 1ms on "common cold" symptoms and
  50ms on ambiguous presentations. The hive's total compute budget is used efficiently.

- **Battery-aware edge AI.** On mobile or IoT, the solver tolerance can be dynamically
  adjusted based on battery level or thermal state. Low battery → higher tolerance →
  faster but slightly less accurate. The model degrades gracefully.

- **Continuous learning dynamics.** The online learning system's update rule itself can
  be a Neural ODE — learning the *dynamics* of how the specialist should adapt over
  time, not just the step direction. This is meta-learning at the ODE level.

- **Irregularly-sampled data.** Specialists handling real-world sensor data (which
  arrives at irregular intervals) process it natively without resampling. Medical
  monitors, financial tick data, IoT sensors — all handled naturally by continuous-time
  dynamics.

- **Infinite-depth compression.** A Neural ODE is parameterized by a single dynamics
  function, regardless of effective depth. A model with "100 effective layers" uses the
  same parameter count as one with "10 effective layers." The depth is free — only the
  dynamics cost parameters.

## Deliverables

### Phase 1: Core ODE Infrastructure (~500 lines)

Location: `simplex-learning/src/ode/`

- **ODESolver** trait — interface for numerical ODE solvers
- **DormandPrince** — adaptive Runge-Kutta 4(5) solver (the standard choice for Neural
  ODEs, adaptive step size)
- **EulerSolver** — fixed-step Euler (fast, for inference when tolerance is loose)
- **NeuralODELayer** — a neural network layer defined by ODE dynamics, integrates from
  t=0 to t=1 (or configurable endpoint)

```simplex
/// An ODE solver that adaptively chooses step size
trait ODESolver {
    /// Solve dy/dt = f(t, y) from t0 to t1 with initial condition y0
    fn solve(
        self: &Self,
        dynamics: &dyn Fn(f64, &Tensor) -> Tensor,
        y0: Tensor,
        t0: f64,
        t1: f64,
    ) -> ODESolution;
}

/// Adaptive Dormand-Prince solver (RK45)
struct DormandPrince {
    atol: f64,    // absolute tolerance
    rtol: f64,    // relative tolerance
    max_steps: usize,
    dt_init: f64, // initial step size
}

impl ODESolver for DormandPrince {
    fn solve(
        self: &Self,
        dynamics: &dyn Fn(f64, &Tensor) -> Tensor,
        y0: Tensor,
        t0: f64,
        t1: f64,
    ) -> ODESolution {
        let mut t = t0;
        let mut y = y0;
        let mut dt = self.dt_init;
        let mut steps = 0;

        while t < t1 && steps < self.max_steps {
            // Compute RK45 stages
            let (y_next, y_err, k_stages) = rk45_step(dynamics, t, &y, dt);

            // Adaptive step size control
            let error_ratio = compute_error_ratio(&y_err, &y, self.atol, self.rtol);
            if error_ratio <= 1.0 {
                // Accept step
                t += dt;
                y = y_next;
                steps += 1;
            }
            // Adjust step size (accept or reject)
            dt = adjust_step_size(dt, error_ratio);
        }

        ODESolution { final_state: y, num_steps: steps, final_time: t }
    }
}

/// A neural network layer defined by continuous dynamics
struct NeuralODELayer {
    dynamics: Box<dyn Layer>,  // f(t, y) — the learned vector field
    solver: Box<dyn ODESolver>,
    t_span: (f64, f64),        // integration interval (typically 0 to 1)
}

impl Layer for NeuralODELayer {
    fn forward(self: &Self, input: Tensor) -> Tensor {
        self.solver.solve(
            &|t, y| self.dynamics.forward(concat(t, y)),
            input,
            self.t_span.0,
            self.t_span.1,
        ).final_state
    }
}
```

Files:
- `simplex-learning/src/ode/mod.sx` — module root
- `simplex-learning/src/ode/solver.sx` — ODESolver trait
- `simplex-learning/src/ode/dormand_prince.sx` — adaptive RK45 solver
- `simplex-learning/src/ode/euler.sx` — fixed-step Euler solver
- `simplex-learning/src/ode/layer.sx` — NeuralODELayer

### Phase 2: Adjoint Method & Training (~400 lines)

- **AdjointODE** — memory-efficient backpropagation through ODE solver via the adjoint
  method (O(1) memory regardless of integration steps)
- **RegularizedDynamics** — add regularization to the ODE dynamics to prevent overly
  complex trajectories (kinetic energy regularization)
- **TimeDependent** — dynamics that change over the integration interval (different
  "behavior" at different depths)
- **AugmentedODE** — augment the state dimension to increase expressiveness (provably
  more powerful than standard NeuralODE)

```simplex
/// Adjoint method: O(1) memory backpropagation through ODE
struct AdjointODE {
    ode_layer: NeuralODELayer,
}

impl AdjointODE {
    /// Backward pass using the adjoint method
    /// Instead of storing all intermediate states (O(steps) memory),
    /// solve the adjoint ODE backward in time (O(1) memory)
    fn backward(self: &Self, output_grad: Tensor, final_state: Tensor) -> (Tensor, Vec<Tensor>) {
        // Augmented state: [state, adjoint, param_gradients]
        let adjoint_dynamics = |t: f64, aug: &Tensor| {
            let (state, adjoint, _) = split_augmented(aug);
            let f = self.ode_layer.dynamics.forward(concat(t, &state));
            let df_dy = jacobian(&f, &state);
            let df_dtheta = jacobian(&f, &self.ode_layer.dynamics.parameters());

            concat3(
                f,                           // dy/dt = f(t, y)
                -adjoint.matmul(&df_dy),    // da/dt = -a * df/dy
                -adjoint.matmul(&df_dtheta), // dL/dθ = -a * df/dθ
            )
        };

        let aug_init = concat3(final_state, output_grad, Tensor::zeros_like(&params));
        let solution = self.ode_layer.solver.solve(
            &adjoint_dynamics,
            aug_init,
            self.ode_layer.t_span.1,  // integrate backward
            self.ode_layer.t_span.0,
        );

        let (_, input_grad, param_grad) = split_augmented(&solution.final_state);
        (input_grad, param_grad)
    }
}
```

Files:
- `simplex-learning/src/ode/adjoint.sx` — AdjointODE backward pass
- `simplex-learning/src/ode/regularization.sx` — dynamics regularization
- `simplex-learning/src/ode/augmented.sx` — augmented Neural ODE
- `simplex-learning/src/ode/time_dependent.sx` — time-varying dynamics

### Phase 3: Applications & Integration (~400 lines)

- **ContinuousNormalizingFlow** — learn complex data distributions via ODE-based density
  transformation (for generation and anomaly detection)
- **AdaptiveDepthModel** — model that reports its effective depth per input (for
  monitoring and compute budgeting)
- **IrregularTimeSeries** — Neural ODE for irregularly-sampled time series (medical
  monitors, financial ticks, IoT sensors)
- **ToleranceBudget** — compile-time annotation for maximum compute per inference
  (solver adjusts tolerance to meet budget)

```simplex
/// Continuous normalizing flow for density estimation
struct ContinuousNormalizingFlow {
    ode: NeuralODELayer,
}

impl ContinuousNormalizingFlow {
    /// Transform base distribution to data distribution
    fn sample(self: &Self, z: Tensor) -> Tensor {
        self.ode.forward(z)  // integrate from base to data
    }

    /// Compute log-probability of data point
    fn log_prob(self: &Self, x: Tensor) -> f64 {
        // Solve ODE backward from data to base, accumulating log-det-jacobian
        let (z, log_det) = self.inverse_with_log_det(x);
        base_log_prob(z) + log_det
    }
}

/// Model that adapts compute per input
struct AdaptiveDepthModel {
    ode: NeuralODELayer,
    min_tolerance: f64,  // max quality
    max_tolerance: f64,  // min quality, max speed
}

impl AdaptiveDepthModel {
    /// Set tolerance based on compute budget
    @tolerance_budget(max_flops = 1_000_000)
    fn forward(self: &Self, input: Tensor) -> Tensor {
        self.ode.forward(input)
    }

    /// Report effective depth (number of integration steps taken)
    fn effective_depth(self: &Self, input: &Tensor) -> usize {
        let solution = self.ode.solver.solve(
            &|t, y| self.ode.dynamics.forward(concat(t, y)),
            input.clone(), 0.0, 1.0,
        );
        solution.num_steps
    }
}

/// Neural ODE for irregularly-sampled time series
struct IrregularTimeSeries {
    dynamics: NeuralODELayer,
    encoder: Linear,
    decoder: Linear,
}

impl IrregularTimeSeries {
    /// Process observations arriving at irregular times
    fn forward(self: &Self, observations: &[(f64, Tensor)]) -> Vec<Tensor> {
        let mut state = self.encoder.forward(&observations[0].1);
        let mut outputs = Vec::new();

        for window in observations.windows(2) {
            let (t0, _) = window[0];
            let (t1, obs) = window[1];
            // Integrate dynamics from t0 to t1
            state = self.dynamics.solver.solve(
                &|t, y| self.dynamics.dynamics.forward(concat(t, y)),
                state, t0, t1,
            ).final_state;
            // Update state with new observation
            state = state + self.encoder.forward(&obs);
            outputs.push(self.decoder.forward(&state));
        }
        outputs
    }
}
```

Files:
- `simplex-learning/src/ode/cnf.sx` — ContinuousNormalizingFlow
- `simplex-learning/src/ode/adaptive.sx` — AdaptiveDepthModel
- `simplex-learning/src/ode/irregular.sx` — IrregularTimeSeries
- `simplex-learning/src/ode/budget.sx` — ToleranceBudget annotation

### Phase 4: Tests (~350 lines)

Location: `tests/ode/`

- DormandPrince solver matches analytical solution for known ODEs (harmonic oscillator)
- NeuralODELayer learns to transform Gaussian to spiral distribution
- Adjoint method gradients match finite-difference gradients (within 1e-4)
- Adaptive depth: simple inputs use fewer steps than complex inputs
- ContinuousNormalizingFlow learns 2D density estimation
- IrregularTimeSeries handles variable time gaps correctly

## Success Criteria

- [ ] DormandPrince solver achieves 1e-6 accuracy on harmonic oscillator ODE
- [ ] NeuralODELayer trains without divergence on synthetic spiral dataset
- [ ] Adjoint method uses O(1) memory (verified by memory profiling)
- [ ] Adaptive depth model uses >2x fewer steps on "easy" vs "hard" synthetic inputs
- [ ] CNF log-probabilities match kernel density estimate on 2D test distribution
- [ ] Irregular time series model handles 10x variation in observation intervals

## Estimated Scope

~1,650 lines across library code and tests.
