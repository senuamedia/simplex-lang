# TASK-044: Liquid Neural Networks — Dynamic Weight Computation

**Version:** 0.17.0
**Status:** Planned
**Priority:** P0 — Critical
**Depends on:** v0.16.0 release, Neural ODEs (TASK-043), dual numbers (TASK-005)

## Why This Feature Is Needed

Every neural network deployed today uses fixed weights. After training, the model is
frozen — a static function that maps input to output the same way regardless of context.
A 7B parameter model uses the same 7 billion numbers whether the input is trivial or
complex, familiar or novel. This is fundamentally wasteful and inflexible.

Liquid Neural Networks, developed at MIT CSAIL, solve this by making weights *dynamic*.
The network is governed by continuous-time differential equations where parameters are
computed as a function of the input itself. The network literally rewires for each input.
MIT demonstrated that 19 liquid neurons can drive a car — a task that requires millions
of parameters in conventional networks. Liquid AI's LFM2.5-1.2B matches models 1.5x
its size while running 2x faster on 45% less memory.

The mathematical foundation is the Neural Circuit Policy (NCP) — a system of coupled
ODEs inspired by the C. elegans worm's nervous system (302 neurons controlling complex
behavior). Each neuron's state evolves according to:

```
τ_i · dx_i/dt = -x_i + f(Σ_j w_ij(x_j) · x_j + I_i)
```

Where `w_ij(x_j)` is the key innovation: weights are *functions of pre-synaptic
activity*, not constants. This means the network's connectivity pattern changes with
every input.

Simplex already has Neural ODEs (TASK-043) for continuous-depth networks. Liquid Neural
Networks extend this by making the ODE dynamics themselves input-dependent — the vector
field changes shape based on what flows through it. This is a natural evolution of
Simplex's differentiable programming paradigm.

## Why It Adds Value

1. **Orders-of-magnitude parameter efficiency.** 19 neurons driving a car. 1.2B
   parameters matching 1.7B+ models. Liquid networks achieve the same capability with
   drastically fewer parameters because every parameter is used adaptively.

2. **Post-training adaptation.** Liquid networks continue to adapt to changing input
   distributions without retraining. The dynamic weights respond to distribution shift
   inherently — the network automatically adjusts its behavior when the world changes.

3. **Natural fit with Simplex's ODE infrastructure.** TASK-043 provides ODE solvers and
   adjoint backpropagation. Liquid neurons are ODEs with input-dependent dynamics — the
   infrastructure is already there.

4. **Causal structure preservation.** Liquid networks preserve temporal causal structure
   in their representations. Unlike transformers (which treat sequence positions as
   interchangeable), liquid networks naturally represent "this happened before that" in
   their continuous dynamics.

5. **Interpretability by construction.** Because the network is small (tens to thousands
   of neurons) and the dynamics are explicit ODEs, liquid networks are inherently more
   interpretable than million-parameter black boxes.

6. **Edge-first architecture.** Liquid networks were designed for resource-constrained
   deployment. A liquid specialist on a watch, phone, or microcontroller is not a
   compromise — it's the intended use case.

## Why It Changes Systems Built With Simplex

Liquid networks redefine what "small model" means:

- **Specialist SLMs shrink by 10-100x.** A liquid specialist for sentiment analysis
  might use 1M parameters instead of 100M — and perform equally well, because it
  rewires for each input.

- **Always-adapting hives.** Cognitive hive specialists with liquid dynamics adapt to
  changing user behavior, seasonal patterns, and domain drift without retraining cycles.
  The hive stays current automatically.

- **Real-time control systems.** Specialists for robotics, autonomous systems, and
  real-time control use liquid dynamics to process sensor data with temporal awareness
  that transformers lack.

- **Interpretable specialist decisions.** Because liquid networks are small and
  ODE-based, the mechanistic interpretability tools from TASK-042 can fully trace every
  decision. Complete transparency at the neuron level.

## Deliverables

### Phase 1: Liquid Neuron Foundation (~500 lines)

Location: `simplex-learning/src/liquid/`

- **LiquidNeuron** — single neuron with input-dependent synaptic weights governed by
  continuous-time ODE
- **NeuralCircuitPolicy** — coupled system of liquid neurons forming a complete
  network (NCP architecture)
- **WiringConfig** — define connectivity patterns: fully connected, sparse random,
  neural architecture search
- **LiquidCell** — recurrent cell interface for step-by-step processing (inference) and
  parallel ODE solving (training)

```simplex
/// A single liquid neuron with input-dependent weights
struct LiquidNeuron {
    tau: Dual,              // time constant (learnable)
    backbone: Linear,       // base weight computation
    w_func: Linear,         // weight modulation function: w(x) = sigmoid(Ax + b)
    bias: Dual,
    state: Tensor,          // continuous hidden state
}

impl LiquidNeuron {
    /// Compute dynamics: τ · dx/dt = -x + f(w(pre) · pre + bias)
    fn dynamics(self: &Self, t: f64, state: &Tensor, input: &Tensor) -> Tensor {
        // Input-dependent weights — the key innovation
        let w_modulation = self.w_func.forward(input.clone()).sigmoid();
        let weighted_input = w_modulation * self.backbone.forward(input.clone());
        let dx_dt = (-state + (weighted_input + self.bias).tanh()) / self.tau;
        dx_dt
    }
}

/// Neural Circuit Policy — coupled liquid neurons
struct NeuralCircuitPolicy {
    sensory: Vec<LiquidNeuron>,   // input-processing neurons
    inter: Vec<LiquidNeuron>,     // intermediate processing
    command: Vec<LiquidNeuron>,   // decision-making neurons
    motor: Vec<LiquidNeuron>,     // output neurons
    wiring: WiringConfig,
    solver: Box<dyn ODESolver>,
}

impl NeuralCircuitPolicy {
    /// Forward pass: solve coupled ODE system
    fn forward(self: &Self, input: Tensor, dt: f64) -> Tensor {
        let coupled_dynamics = |t: f64, states: &Tensor| {
            self.compute_all_dynamics(t, states, &input)
        };
        let solution = self.solver.solve(&coupled_dynamics, self.states(), 0.0, dt);
        self.motor_output(&solution.final_state)
    }

    /// Step-by-step inference (streaming / real-time)
    fn step(mut self: &mut Self, input: &Tensor) -> Tensor {
        let dt = 0.01; // fixed step for real-time
        for neuron in self.all_neurons_mut() {
            let dx = neuron.dynamics(0.0, &neuron.state, input);
            neuron.state = neuron.state.clone() + dt * dx;
        }
        self.motor_output_from_states()
    }
}
```

Files:
- `simplex-learning/src/liquid/mod.sx` — module root
- `simplex-learning/src/liquid/neuron.sx` — LiquidNeuron
- `simplex-learning/src/liquid/ncp.sx` — NeuralCircuitPolicy
- `simplex-learning/src/liquid/wiring.sx` — WiringConfig and connectivity patterns
- `simplex-learning/src/liquid/cell.sx` — LiquidCell recurrent interface

### Phase 2: Closed-Form Continuous-Time Models (~400 lines)

- **CfC (Closed-form Continuous-time)** — Liquid AI's approximation that replaces ODE
  solving with a closed-form solution, making liquid networks as fast as standard RNNs
  while preserving dynamic weight behavior
- **LiquidS4** — hybrid combining liquid dynamics with state space models for linear-
  time sequence processing with adaptive weights
- **LiquidAttention** — attention mechanism where query/key/value projections are
  input-dependent liquid functions instead of fixed linear maps

```simplex
/// Closed-form Continuous-time model — fast approximation of liquid dynamics
struct CfC {
    backbone: Linear,
    tau_net: Linear,      // predicts time constants from input
    gate_net: Linear,     // predicts gating from input
    state_dim: usize,
}

impl CfC {
    /// Closed-form solution — no ODE solver needed
    fn forward(self: &Self, input: Tensor, state: &Tensor) -> (Tensor, Tensor) {
        let tau = self.tau_net.forward(input.clone()).sigmoid();
        let gate = self.gate_net.forward(input.clone()).sigmoid();
        let candidate = self.backbone.forward(input).tanh();

        // Closed-form exponential decay + gated update
        let decay = (-1.0 / (tau + 1e-6)).exp();
        let new_state = decay * state + (1.0 - decay) * gate * candidate;
        let output = new_state.clone();
        (output, new_state)
    }
}
```

Files:
- `simplex-learning/src/liquid/cfc.sx` — Closed-form Continuous-time model
- `simplex-learning/src/liquid/liquid_s4.sx` — hybrid liquid + SSM
- `simplex-learning/src/liquid/liquid_attention.sx` — liquid attention mechanism

### Phase 3: Specialist Integration (~400 lines)

- **LiquidSpecialist** — cognitive hive specialist backed by liquid dynamics
- **LiquidRouter** — hive router using liquid dynamics for context-adaptive routing
- **AutoWiring** — neural architecture search for optimal liquid network topology
- **LiquidGate** — neural gate with liquid dynamics (contract-governed, adaptive)

Files:
- `simplex-learning/src/liquid/specialist.sx` — LiquidSpecialist
- `simplex-learning/src/liquid/router.sx` — LiquidRouter
- `simplex-learning/src/liquid/autowiring.sx` — topology search
- `simplex-learning/src/liquid/gate.sx` — LiquidGate

### Phase 4: Tests (~350 lines)

Location: `tests/liquid/`

- LiquidNeuron state evolves correctly under known dynamics
- NCP with 19 neurons learns synthetic control task
- CfC matches full ODE solution within tolerance
- LiquidSpecialist adapts to distribution shift without retraining
- Dynamic weights change measurably between different input types
- Parameter count comparison: liquid vs fixed-weight for equivalent accuracy

## Success Criteria

- [ ] NCP with <100 neurons learns synthetic sequence-to-sequence task
- [ ] CfC achieves <5% error vs full ODE solution while running 10x faster
- [ ] Liquid specialist adapts to injected distribution shift (measured by loss recovery)
- [ ] Dynamic weights are demonstrably input-dependent (weight variance > 0 across inputs)
- [ ] 50x parameter reduction vs fixed-weight MLP for equivalent accuracy on test task
- [ ] Integration with existing ODE solvers and dual number AD

## Estimated Scope

~1,650 lines across library code and tests.
