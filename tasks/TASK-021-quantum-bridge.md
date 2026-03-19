# TASK-021: Quantum Computing Bridge

> **Update (2026-03-16):** Retargeted from v0.13.0 to v0.14.0. A dependency audit revealed
> critical gaps in v0.12.0 that must be resolved first: complex number arithmetic,
> matrix operations, higher-order dual numbers (multidual/Hessian/Jacobian), HTTP client,
> full JSON parsing, and async/await stability. These are addressed in TASK-022 (v0.13.0
> "Mathematical Foundations & Stability" prep release). The quantum bridge builds on top
> of that foundation.

**Status**: Planning
**Priority**: High
**Created**: 2026-03-16
**Target Version**: 0.14.0
**Depends On**: TASK-022 (Mathematical Foundations v0.13.0), TASK-005 (Dual Numbers), TASK-006 (Self-Learning Annealing), TASK-014 (Epistemic Integrity)

### Language Features Carried Forward from v0.12.0

These were listed as "What's Next" in the v0.12.0 release notes but deferred past v0.13.0. They should land in v0.14.0 alongside the quantum bridge:

- **Associated types** — `type Output` in traits for more expressive generic programming. Parser support exists but codegen is incomplete.
- **`&mut self` syntax** — Ergonomic mutable method receivers. Parser recognizes `&mut` references but no ownership/borrow semantics yet.
- **Self-hosted compiler** — The struct field lookup workaround is the last known gap before full self-hosting.
- **Performance optimizations** — Optimization passes, inlining heuristics, and codegen improvements.

### TASK-022 Dependencies (by phase)

- TASK-022 Phase 2 (Complex Numbers) — quantum amplitudes
- TASK-022 Phase 3 (Matrix Operations) — unitary gate application
- TASK-022 Phase 4 (Dual Numbers Phase 2-4) — variational gradient optimization
- TASK-022 Phase 5 (HTTP Client) — cloud QPU API calls
- TASK-022 Phase 6 (JSON Parser) — API response parsing
- TASK-022 Phase 7 (Async/Await) — non-blocking QPU submission

---

## Overview

### The Hybrid Approach

Simplex is not becoming a quantum programming language. The goal is a **hybrid classical-quantum bridge** where Simplex orchestrates the full computation pipeline: classical AI/ML workloads run on CPUs/GPUs as they do today, and specific subproblems that benefit from quantum speedup are offloaded to cloud QPU services (Amazon Braket, IBM Quantum, Azure Quantum) via a cost-aware dispatcher.

The key architectural insight is that Simplex already has the infrastructure that makes variational quantum computing natural:

- **Dual numbers** (TASK-005) provide exact forward-mode autodiff. Variational quantum algorithms (VQE, QAOA) require classical gradient optimization of parameterized quantum circuits. The dual number system can compute parameter gradients for the classical optimizer loop without building a computation graph.
- **Self-learning annealing** (TASK-006) implements differentiable temperature schedules with meta-gradient optimization. Quantum annealing and variational algorithms share the same mathematical structure: exploring a cost landscape with tunable parameters. The meta-optimizer can learn when to shift computation between classical annealing and quantum sampling.
- **Epistemic beliefs** (TASK-014) provide calibrated confidence and falsification. Quantum measurements are inherently probabilistic. The belief system gives Simplex a principled way to handle quantum uncertainty: treat measurement distributions as evidence, maintain calibrated confidence over quantum results, and trigger re-measurement when confidence degrades.

### What This Is Not

This bridge does not attempt to:
- Implement a quantum simulator in Simplex (simulators exist on the cloud backends)
- Replace classical algorithms where they are already efficient
- Abstract away the fundamental cost/latency tradeoffs of QPU access
- Pretend current NISQ hardware is more capable than it is

### What Quantum Problems Are Worth the Cost

Current NISQ-era quantum hardware (50-1000+ noisy qubits) provides potential advantage for a narrow set of problems. The bridge should target these honestly:

**Strong candidates for quantum offload:**
- **Combinatorial optimization** (QAOA): Routing, scheduling, portfolio optimization where the solution space is exponentially large and classical heuristics plateau. At 100+ qubits, QAOA can explore solution spaces that simulated annealing cannot reach efficiently.
- **Molecular simulation** (VQE): Ground state energy estimation for small molecules (drug discovery, materials science). Quantum hardware directly represents quantum systems — this is the most natural use case.
- **Sampling from complex distributions**: Boltzmann sampling, generative modeling priors. Quantum samplers can draw from distributions that are classically intractable.

**Marginal candidates (use simulators first, QPU only at scale):**
- **Quantum machine learning kernels**: Feature maps for classification where classical kernels underperform. Advantage is problem-dependent and often marginal on current hardware.
- **Unstructured search** (Grover's): Quadratic speedup is real but only matters for large search spaces, and circuit depth requirements exceed current hardware coherence times for most practical problems.

**Not worth quantum offload (stay classical):**
- Standard neural network training (classical GPUs dominate)
- Linear algebra on dense matrices (no quantum advantage)
- Problems with efficient classical approximations (most real-world ML)

The cost-aware dispatcher (Phase 4) encodes this knowledge directly: it estimates whether a given problem instance justifies the QPU cost based on problem structure, qubit requirements, and current cloud pricing.

---

## Hybrid Quantum-Classical Example

The following pseudo-code demonstrates the full variational quantum loop in Simplex syntax, using actual language constructs and proposed quantum bridge types. This is what a realistic hybrid quantum-classical optimization looks like end-to-end.

```simplex
use simplex_learning::dual::Dual;
use simplex_learning::tensor::DualTensor;
use quantum::circuit::QuantumCircuit;
use quantum::gates::{H, CNOT, Rz, Ry};
use quantum::variational::ParameterizedCircuit;
use quantum::backend::QuantumBackend;
use quantum::dispatch::{Dispatcher, Budget, DispatchDecision};

// --- 1. Define a parameterized quantum circuit ---

struct VQECircuit {
    n_qubits: int,
    params: Vec<Dual>,
}

impl VQECircuit {
    pub fn new(n_qubits: int) -> VQECircuit {
        // Each parameter is a Dual variable: val tracks the angle,
        // der tracks the gradient (seeded to 1.0 for the active param).
        let params: Vec<Dual> = vec![
            Dual::variable(0.5),   // theta_0
            Dual::variable(0.3),   // theta_1
            Dual::variable(-0.1),  // theta_2
            Dual::variable(0.7),   // theta_3
        ];
        VQECircuit { n_qubits: n_qubits, params: params }
    }

    // Build a concrete circuit with current parameter values bound in.
    pub fn build(self) -> QuantumCircuit {
        let qc = QuantumCircuit::new(self.n_qubits);

        // Layer 1: superposition
        qc.h(0);
        qc.h(1);

        // Layer 2: parameterized rotations (accepts Dual values for theta)
        qc.ry(0, self.params[0]);
        qc.rz(1, self.params[1]);

        // Layer 3: entanglement
        qc.cnot(0, 1);

        // Layer 4: more parameterized rotations
        qc.ry(0, self.params[2]);
        qc.rz(1, self.params[3]);

        // Measure all qubits
        qc.measure_all();
        qc
    }
}

// --- 2. Classical loss function using dual numbers ---

fn compute_loss(measurements: MeasurementResult, target_energy: Dual) -> Dual {
    // Expectation value from measurement statistics, wrapped as a Dual
    let expectation = Dual::new(measurements.expectation_value(), 0.0);

    // Classical loss: squared difference from target
    // Dual arithmetic propagates gradients automatically
    let diff = expectation - target_energy;
    diff * diff
}

// --- 3. The variational optimization loop ---

fn run_vqe(backend: impl QuantumBackend, budget: Budget) -> Result<f64, QuantumError> {
    let vqe = VQECircuit::new(2);
    let target_energy = Dual::constant(-1.137);  // Known H2 ground state
    let learning_rate: f64 = 0.1;
    let max_iterations: int = 100;
    let tolerance: f64 = 1e-5;

    for iter in 0..max_iterations {
        // Build circuit from current dual-valued parameters
        let circuit = vqe.build();

        // Submit to QPU (or simulator) and get measurements
        let job = backend.submit(circuit, 1000, "default")?;
        let result = backend.result(job)?;

        // Compute loss classically — this is free, no QPU needed
        let loss = compute_loss(result, target_energy);

        // Check convergence
        if loss.val() < tolerance {
            return Ok(vqe.params[0].val());  // Converged
        }

        // Parameter-shift gradient estimation:
        // For each parameter, evaluate circuit at theta +/- pi/2
        for i in 0..vqe.params.len() {
            let original = vqe.params[i].val();

            // Forward shift: theta + pi/2
            vqe.params[i] = Dual::variable(original + 3.14159 / 2.0);
            let circuit_plus = vqe.build();
            let job_plus = backend.submit(circuit_plus, 1000, "default")?;
            let result_plus = backend.result(job_plus)?;

            // Backward shift: theta - pi/2
            vqe.params[i] = Dual::variable(original - 3.14159 / 2.0);
            let circuit_minus = vqe.build();
            let job_minus = backend.submit(circuit_minus, 1000, "default")?;
            let result_minus = backend.result(job_minus)?;

            // Gradient via parameter-shift rule
            let grad = (result_plus.expectation_value()
                      - result_minus.expectation_value()) / 2.0;

            // Update parameter using gradient descent
            vqe.params[i] = Dual::variable(original - learning_rate * grad);
        }
    }

    Ok(vqe.params[0].val())
}

// --- 4. Cost-aware dispatch: the dispatcher chooses the backend ---

fn solve_optimization(problem: ProblemInstance) -> Result<Solution, QuantumError> {
    let budget = Budget::new()
        .max_spend(50.0)       // $50 hard limit
        .warn_at_percent(80);  // Warn at $40

    let dispatcher = Dispatcher::new(budget);

    // Dispatcher analyzes the problem and decides classical vs quantum
    let decision = dispatcher.analyze(problem);

    match decision {
        DispatchDecision::Classical => {
            // Problem too small or no quantum advantage — solve classically
            classical_optimizer::solve(problem)
        },
        DispatchDecision::Quantum(backend, device) => {
            // QPU is worth the cost — run the variational loop
            run_vqe(backend, budget)
        },
        DispatchDecision::Hybrid(strategy) => {
            // Split: classical preprocessing, then quantum refinement
            let reduced = classical_optimizer::reduce(problem);
            run_vqe(strategy.backend, budget)
        },
    }
}

// --- 5. Integration with self-learning annealing for meta-optimization ---

fn meta_optimize_vqe(backend: impl QuantumBackend, budget: Budget) -> Result<f64, QuantumError> {
    // The annealing schedule controls how aggressively we explore
    // parameter space vs. exploit current best parameters.
    let schedule = AnnealingSchedule::new()
        .initial_temperature(1.0)
        .decay(0.95);

    let vqe = VQECircuit::new(2);
    let best_energy = Dual::constant(0.0);

    for epoch in 0..50 {
        let temp = schedule.current_temperature();

        // At high temperature: large random perturbations to escape local minima
        // At low temperature: fine-grained gradient updates
        for i in 0..vqe.params.len() {
            let noise = temp * random_normal();
            let current = vqe.params[i].val();
            vqe.params[i] = Dual::variable(current + noise);
        }

        // Run one VQE iteration at perturbed parameters
        let circuit = vqe.build();
        let job = backend.submit(circuit, 1000, "default")?;
        let result = backend.result(job)?;
        let energy = Dual::new(result.expectation_value(), 0.0);

        // Meta-gradient: the annealing schedule itself learns
        // whether to cool faster or slower based on progress
        if energy.val() < best_energy.val() {
            best_energy = energy;
            schedule.reward();   // Good step — slow down cooling
        } else {
            schedule.penalize(); // Bad step — cool faster
        }

        schedule.step();
    }

    Ok(best_energy.val())
}
```

### Example: Quantum-Enhanced Belief Resolution

```simplex
// Quantum-enhanced counterfactual evaluation during dissent phase
fn resolve_belief_conflict(
    beliefs: Vec<GroundedBelief>,
    conflicts: Vec<ConflictPair>,
    backend: &QuantumBackend
) -> Vec<GroundedBelief> {
    // Encode belief conflicts as a QUBO (Quadratic Unconstrained Binary Optimization)
    // Each belief gets a qubit: |1⟩ = accept, |0⟩ = reject
    let n_beliefs = beliefs.len();
    let circuit = QuantumCircuit::new(n_beliefs);

    // QAOA layers: encode conflict penalties as ZZ interactions
    for conflict in conflicts {
        let penalty = conflict.severity * dual::variable(1.0);
        circuit.add_gate(Gate::ZZ(conflict.belief_a, conflict.belief_b, penalty));
    }

    // Add confidence bias: prefer higher-confidence beliefs
    for i in 0..n_beliefs {
        let bias = beliefs[i].confidence.val;
        circuit.add_gate(Gate::Rz(i, dual::constant(bias)));
    }

    // Execute: find minimum-conflict configuration
    let result = backend.execute(circuit, shots: 2000);
    let optimal = result.most_frequent_bitstring();

    // Apply resolution: keep accepted beliefs, revise rejected ones
    let mut resolved = Vec::new();
    for i in 0..n_beliefs {
        if optimal[i] == 1 {
            resolved.push(beliefs[i].clone());
        } else {
            resolved.push(beliefs[i].with_confidence(
                beliefs[i].confidence * dual::constant(0.1)  // Demote rejected
            ));
        }
    }
    resolved
}
```

### Example: Quantum-Enhanced Specialist Routing

```simplex
// Optimal task-to-specialist assignment via quantum optimization
fn quantum_route_tasks(
    tasks: Vec<Task>,
    specialists: Vec<Specialist>,
    backend: &QuantumBackend,
    budget: &Budget
) -> Vec<Assignment> {
    let n_tasks = tasks.len();
    let n_specs = specialists.len();

    // Only use quantum for large routing problems
    if n_tasks * n_specs < 64 {
        return classical_greedy_route(tasks, specialists);
    }

    // Encode as QAOA: n_tasks * n_specs binary variables
    // x[i][j] = 1 means task i assigned to specialist j
    let circuit = QuantumCircuit::new(n_tasks * n_specs);

    // Objective: maximize total confidence of assignments
    for i in 0..n_tasks {
        for j in 0..n_specs {
            let fitness = specialists[j].confidence_for(tasks[i]);
            let qubit_idx = i * n_specs + j;
            circuit.add_gate(Gate::Rz(qubit_idx, dual::constant(fitness)));
        }
    }

    // Constraint: each task assigned to exactly one specialist
    for i in 0..n_tasks {
        let task_qubits: Vec<usize> = (0..n_specs).map(|j| i * n_specs + j).collect();
        circuit.add_constraint_one_hot(task_qubits);
    }

    // Constraint: respect specialist capacity
    for j in 0..n_specs {
        let spec_qubits: Vec<usize> = (0..n_tasks).map(|i| i * n_specs + j).collect();
        circuit.add_constraint_max(spec_qubits, specialists[j].capacity);
    }

    let result = backend.execute(circuit, shots: 1000);
    decode_assignments(result.most_frequent_bitstring(), n_tasks, n_specs)
}
```

### Why This Hybrid Approach Works

- **Dual numbers provide exact gradients.** The `Dual::variable()` type carries both value and derivative through all classical arithmetic. For the parameter-shift rule, we need the classical optimizer to compute `d(loss)/d(theta)` from the shifted measurement results. Dual numbers make this exact, with no numerical approximation or computation graph overhead.

- **Classical hardware handles data preprocessing and gradient updates for free.** The loss function, gradient aggregation, and parameter updates are all pure arithmetic on `Dual` values running on the CPU. The only expensive operation is the QPU circuit evaluation.

- **QPU is only invoked for the quantum circuit evaluation.** Each iteration submits the parameterized circuit and collects measurement statistics. This is the part that cannot be efficiently done classically for large quantum systems, and it is the only part that costs money.

- **The cost-aware dispatcher prevents budget overruns.** Before any QPU submission, the dispatcher estimates total cost (iterations times parameters times 2 shifted circuits times per-shot pricing) and compares against the remaining budget. If the math does not work out, it falls back to the local simulator or classical solver.

- **Self-learning annealing meta-optimizes the variational parameters.** Variational quantum algorithms are notorious for barren plateaus (regions where gradients vanish). The annealing schedule adds controlled noise that helps escape these flat regions early in optimization, then cools down to let gradient descent converge precisely. The meta-optimizer (from TASK-006) learns the cooling rate from the optimization trajectory itself.

---

## When Quantum is Worth the Cost

A practical decision matrix for when to dispatch to QPU versus staying classical:

| Problem Type | Classical Performance | Quantum Advantage | Recommendation |
|---|---|---|---|
| Combinatorial optimization (MaxCut, TSP, scheduling) | Good heuristics (SA, genetic) plateau at ~100 variables | QAOA explores solution spaces classical heuristics miss; advantage grows with problem size | **Quantum** at 50+ variables if budget allows; classical below that |
| Molecular simulation (ground state energy) | Exact methods scale as O(N!) with active orbitals | VQE scales polynomially — quantum hardware directly represents quantum systems | **Quantum** for 10+ active orbitals; simulator sufficient below that |
| Cryptography (factoring, discrete log) | RSA-2048 is classically intractable | Shor's algorithm is exponentially faster, but requires fault-tolerant hardware (not NISQ) | **Classical** today; quantum when fault-tolerant hardware exists (2030+) |
| General ML training (neural networks, transformers) | GPUs are highly optimized; mature ecosystem | No demonstrated quantum advantage for standard architectures | **Classical** — no reason to use QPU |
| Search over unstructured data (database search) | Linear scan: O(N) | Grover's gives O(sqrt(N)), but circuit depth exceeds NISQ coherence for large N | **Classical** on current hardware; revisit with error-corrected qubits |
| Linear algebra (matrix inversion, eigenvalues) | O(N^3) well-optimized (LAPACK, cuBLAS) | HHL algorithm offers exponential speedup in theory, but input/output bottleneck limits practical advantage | **Classical** for dense systems; quantum may help for sparse/structured problems at scale |

The dispatcher (Phase 4) encodes this matrix and refines it over time as QPU hardware improves and pricing changes.

---

## Quantum Use Cases in Simplex — Prioritized

The following prioritization maps quantum methods to existing Simplex components, ranked by return on investment. Priority 1 is the single best candidate because the classical infrastructure is already mature and the quantum enhancement is a direct drop-in improvement.

### Priority 1: Meta-Annealing (Highest ROI)

- **Component:** `simplex-std/src/anneal.sx` (1085 lines)
- **What exists:** MetaOptimizer with differentiable temperature schedules, AnnealState, MetaLoss
- **Quantum method:** Quantum Annealing (D-Wave) / QAOA
- **Enhancement:** Classical annealing explores the meta-parameter landscape sequentially. Quantum tunneling penetrates narrow energy barriers, finding global optima for cooling rate, reheat triggers, and temperature bounds that classical search misses. This is the single highest-ROI use case because the classical infrastructure is already complete.

### Priority 2: Epistemic Belief Resolution

- **Component:** `simplex-learning/src/epistemic/` (dissent.sx, counterfactual.sx, skeptic.sx)
- **What exists:** DissentWindow for scheduled disagreement, CounterfactualProber for "what-if" scenarios, Skeptic specialist that challenges beliefs
- **Quantum method:** Grover's search / QAOA
- **Enhancement:** When specialists disagree during dissent phase, the system must evaluate exponentially many counterfactual scenarios ("if belief A is wrong, what happens to B, C, D?"). This is a constraint satisfaction problem. Quantum could find the minimum-conflict belief configuration provably, replacing the current heuristic approach.

### Priority 3: Specialist Routing in Cognitive Hive

- **Component:** `simplex-edge-hive/src/specialist.sx`
- **What exists:** Confidence-based greedy routing of tasks to specialists
- **Quantum method:** QAOA / VQE
- **Enhancement:** With N specialists and M tasks with dependency constraints, optimal routing is NP-hard (generalized assignment problem). Quantum finds globally optimal specialist-task assignments and handles real-time rebalancing when specialists fail.

### Priority 4: Training Schedule Meta-Optimization

- **Component:** `simplex-training/src/schedules/` (lr.sx, distill.sx, prune.sx, quant.sx)
- **What exists:** 4 learnable schedules optimized sequentially via dual number gradients
- **Quantum method:** VQE
- **Enhancement:** Joint optimization of learning rate x distillation temperature x pruning threshold x quantization bit-width creates a 4D landscape with many local minima. Quantum explores the joint space simultaneously.

### Priority 5: Neural Gate Architecture Search

- **Component:** `simplex-training/src/neural/gate.sx`
- **What exists:** DualGate with temperature, Gumbel-Softmax, StraightThroughEstimator
- **Quantum method:** Variational quantum circuits
- **Enhancement:** Searching for optimal gate topology (connections, temperatures) is combinatorial. Variational circuits explore architectures that classical search cannot.

### Priority 6: Federated Belief Consensus

- **Component:** `simplex-nexus/src/federation.sx`, `simplex-nexus/src/crdt.sx`
- **What exists:** Multi-hive belief propagation with CRDTs and vector clocks
- **Quantum method:** QAOA
- **Enhancement:** Finding the belief state that minimizes total conflict across all hives simultaneously when multiple hives disagree.

---

## Where Quantum Does NOT Help

Not every component benefits from quantum computation. The following table identifies areas where quantum adds no value and classical approaches should be preserved unconditionally.

| Component | Why Quantum Adds No Value |
|---|---|
| Tensor operations (matmul, forward pass) | Classical GPUs are faster and cheaper for linear algebra |
| Gradient computation (backprop) | Dual numbers already provide exact derivatives |
| Edge deployment | Quantum hardware isn't on phones/laptops; edge stays classical |
| Nexus bit-packing | Deterministic encoding, no optimization dimension |
| Safety/no-learn zones | Binary constraints, not optimization problems |
| Basic CRUD/IO operations | Sequential logic, no parallelism benefit |

---

## Module Layout

All code is pure Simplex. No external tools, libraries, or FFI bindings.

```
simplex-quantum/                          # Core quantum bridge library
  src/
    types.sx                          # Qubit, QuantumState, MeasurementResult
    circuit.sx                        # QuantumCircuit builder, gate application
    gates.sx                          # Standard gate definitions (H, CNOT, Rz, Ry, X, Y, Z, T, S, SWAP)
    measurement.sx                    # Measurement strategies, shot aggregation, statistics
    error.sx                          # Quantum-specific error types, noise models

simplex-quantum/backend/                  # QPU backend abstraction
  src/
    trait.sx                          # QuantumBackend trait definition
    braket.sx                         # Amazon Braket backend (HTTP/JSON protocol)
    ibm.sx                            # IBM Quantum backend (Qiskit runtime protocol)
    azure.sx                          # Azure Quantum backend (REST protocol)
    simulator.sx                      # Local statevector simulator (development/testing)
    registry.sx                       # Backend discovery and capability negotiation

simplex-quantum/variational/              # Variational quantum bridge
  src/
    param_circuit.sx                  # ParameterizedCircuit with dual number parameters
    optimizer.sx                      # Classical optimizer loop (gradient descent on circuit params)
    parameter_shift.sx                # Parameter-shift rule for quantum gradient estimation
    vqe.sx                            # Variational Quantum Eigensolver
    qaoa.sx                           # Quantum Approximate Optimization Algorithm

simplex-quantum/dispatch/                 # Cost-aware task dispatcher
  src/
    analyzer.sx                       # Problem complexity analysis
    cost_model.sx                     # QPU pricing models (per-task, per-shot, simulator rates)
    dispatcher.sx                     # Classical vs quantum decision engine
    budget.sx                         # Budget tracking, cost limits, usage reporting

simplex-quantum/optimize/                 # Quantum-enhanced optimization
  src/
    combinatorial.sx                  # QAOA-based combinatorial optimization (MaxCut, TSP, scheduling)
    molecular.sx                      # VQE-based molecular ground state estimation
    search.sx                         # Grover-based search for structured problems
    hybrid_anneal.sx                  # Hybrid classical-quantum annealing (bridges TASK-006)

simplex-learning/src/quantum/         # Integration with learning infrastructure
  bridge.sx                           # Connects quantum results to tensor/autograd system
  epistemic.sx                        # Quantum measurement uncertainty as epistemic beliefs
  neural_gate.sx                      # Quantum-aware neural gate variants
```

---

## Phase 1: Core Quantum Types and Circuit Builder

### Objective

Define the fundamental quantum types and a circuit builder API that is idiomatic Simplex. The circuit representation is abstract — it describes quantum operations without executing them. Execution happens through backends (Phase 2).

### Deliverables

1. **Qubit and quantum state types** (`simplex-quantum/src/types.sx`)
   - `Qubit` type: logical qubit identifier with index and optional label
   - `QuantumState` enum: computational basis states, superposition metadata
   - `MeasurementResult` struct: per-qubit measurement outcomes, shot counts, probability distribution
   - `CircuitStats` struct: gate count, depth, qubit count, estimated execution time

2. **Standard gate library** (`simplex-quantum/src/gates.sx`)
   - Single-qubit gates: H (Hadamard), X, Y, Z (Pauli), S, T, Rx(theta), Ry(theta), Rz(theta)
   - Two-qubit gates: CNOT, CZ, SWAP, CRz(theta)
   - Gate metadata: unitary matrix representation (as nested arrays), inverse computation, gate decomposition rules
   - All parameterized gates accept `dual` values for theta — this is critical for Phase 3

3. **Circuit builder** (`simplex-quantum/src/circuit.sx`)
   - `QuantumCircuit` struct with builder pattern: `QuantumCircuit::new(n_qubits).h(0).cnot(0, 1).rz(0, theta).measure_all()`
   - Circuit validation: qubit index bounds, gate compatibility checks
   - Circuit serialization to backend-neutral intermediate representation (JSON-compatible struct)
   - Circuit composition: append, tensor product, controlled-circuit wrapping

4. **Measurement utilities** (`simplex-quantum/src/measurement.sx`)
   - Shot aggregation: raw counts to probability distribution
   - Expectation value computation for Pauli observables
   - Statistical convergence estimation (how many shots needed for target precision)

### Success Criteria

- Circuit builder can express Bell state preparation, GHZ states, and a 4-qubit QAOA circuit
- All parameterized gates accept dual number parameters without special handling
- Circuit serialization round-trips correctly (serialize then deserialize produces identical circuit)
- Measurement statistics correctly compute expectation values from simulated shot data

---

## Phase 2: Backend Abstraction Layer

### Objective

Define a trait-based backend system so circuits built in Phase 1 can be submitted to any supported QPU provider. The abstraction must handle authentication, job submission, result polling, and error recovery uniformly across providers.

### Deliverables

1. **QuantumBackend trait** (`simplex-quantum/backend/src/trait.sx`)
   ```simplex
   trait QuantumBackend {
       fn name(self) -> str;
       fn available_devices(self) -> Vec<DeviceInfo>;
       fn max_qubits(self, device: str) -> int;
       fn submit(self, circuit: QuantumCircuit, shots: int, device: str) -> Result<JobHandle, QuantumError>;
       fn poll(self, job: JobHandle) -> Result<JobStatus, QuantumError>;
       fn result(self, job: JobHandle) -> Result<MeasurementResult, QuantumError>;
       fn cancel(self, job: JobHandle) -> Result<(), QuantumError>;
       fn cost_estimate(self, circuit: QuantumCircuit, shots: int, device: str) -> CostEstimate;
   }
   ```

2. **Amazon Braket backend** (`simplex-quantum/backend/src/braket.sx`)
   - Authentication via environment credentials (AWS access key/secret/region)
   - Circuit translation to Amazon Braket JSON format (OpenQASM 3.0 subset)
   - Task submission via Braket HTTP API
   - Supported devices: Rigetti Ankaa-3, IonQ Aria, IonQ Forte, SV1 (simulator)
   - Cost estimation using Braket pricing: $0.30/task base + per-shot device rates

3. **IBM Quantum backend** (`simplex-quantum/backend/src/ibm.sx`)
   - Authentication via IBM Quantum API token
   - Circuit translation to Qiskit runtime format
   - Job submission via IBM Quantum REST API
   - Supported devices: Eagle (127 qubit), Heron (133 qubit) processors

4. **Azure Quantum backend** (`simplex-quantum/backend/src/azure.sx`)
   - Authentication via Azure AD token
   - Circuit translation to Azure Quantum format
   - Job submission via Azure Quantum REST API
   - Supported providers: IonQ, Quantinuum, Rigetti via Azure marketplace

5. **Local simulator** (`simplex-quantum/backend/src/simulator.sx`)
   - Statevector simulation for circuits up to ~20 qubits (memory-bounded)
   - Exact probability computation (no shot noise) with optional shot sampling
   - Zero cost, zero latency — default backend for development and testing
   - Validates circuit correctness before submitting to paid cloud backends

6. **Backend registry** (`simplex-quantum/backend/src/registry.sx`)
   - Discover available backends from environment configuration
   - Capability negotiation: match circuit requirements (qubit count, gate set, connectivity) to backend capabilities
   - Automatic fallback: if preferred backend is unavailable, try alternatives

### Success Criteria

- Same circuit runs on local simulator and (with credentials) on Braket without code changes
- Cost estimation matches actual Braket billing within 10% for standard circuits
- Backend registry correctly rejects circuits that exceed device capabilities
- Simulator results match analytic expectations for Bell state and GHZ state circuits

---

## Phase 3: Variational Quantum Bridge

### Objective

This is the core innovation: connect Simplex's dual number autodiff infrastructure to variational quantum parameter optimization. Variational quantum algorithms (VQE, QAOA) work by parameterizing a quantum circuit with classical angles, measuring the output, computing a cost function, and updating the parameters via gradient descent. Simplex's dual numbers make the classical gradient computation exact and efficient.

### Deliverables

1. **Parameterized circuit** (`simplex-quantum/variational/src/param_circuit.sx`)
   - `ParameterizedCircuit` wraps `QuantumCircuit` with named `dual`-typed parameters
   - Parameter binding: substitute concrete dual values into circuit rotation angles
   - Parameter count and metadata extraction for optimizer initialization

2. **Parameter-shift gradient estimation** (`simplex-quantum/variational/src/parameter_shift.sx`)
   - Implement the parameter-shift rule: `df/dtheta = [f(theta + pi/2) - f(theta - pi/2)] / 2`
   - This requires two circuit evaluations per parameter per gradient step
   - Shot budget allocation: distribute shots across shifted circuits based on variance
   - Gradient caching: avoid redundant circuit submissions when parameters share gates

3. **Classical optimizer loop** (`simplex-quantum/variational/src/optimizer.sx`)
   - `VariationalOptimizer` struct that coordinates:
     a. Bind current dual-valued parameters to circuit
     b. Submit circuit(s) to backend (base + parameter-shifted variants)
     c. Collect measurement results
     d. Compute cost function and gradients
     e. Update parameters using dual number gradient descent
   - Support for optimizer variants: vanilla gradient descent, Adam, L-BFGS
   - Convergence detection with configurable tolerance
   - Integration with self-learning annealing (TASK-006): temperature-modulated parameter updates for escaping local minima

4. **Dual number bridge** (`simplex-learning/src/quantum/bridge.sx`)
   - Convert quantum measurement statistics to dual number values for downstream classical computation
   - Propagate gradients from classical loss functions back through quantum parameter updates
   - Maintain computation graph continuity across classical-quantum-classical boundaries

### Success Criteria

- Parameter-shift gradients match finite-difference gradients (on simulator) to within shot noise
- Variational optimizer converges on a known VQE problem (H2 ground state energy) within 5% of exact
- Dual number gradient flow is continuous: a classical loss function wrapping a quantum subroutine produces correct parameter gradients
- Annealing integration demonstrates escape from local minimum that pure gradient descent gets stuck in

---

## Phase 4: Cost-Aware Task Dispatcher

### Objective

Build an intelligent dispatcher that decides whether a given computation should run classically or be offloaded to quantum hardware. The decision is based on problem structure, estimated quantum advantage, QPU cost, and user-configured budget constraints.

### Deliverables

1. **Problem complexity analyzer** (`simplex-quantum/dispatch/src/analyzer.sx`)
   - Analyze problem instance to estimate:
     - Classical runtime (heuristic, based on problem size and known algorithmic complexity)
     - Required qubit count for quantum formulation
     - Estimated circuit depth
     - Expected quantum advantage factor (if any)
   - Problem type classification: combinatorial, simulation, sampling, search

2. **Cost model** (`simplex-quantum/dispatch/src/cost_model.sx`)
   - Encode current cloud QPU pricing:
     ```
     Amazon Braket:
       Per-task fee: $0.30 per task (one circuit submission)
       Per-shot fees (vary by device):
         Rigetti Ankaa-3:  $0.00090 per shot
         IonQ Aria:        $0.03000 per shot
         IonQ Forte:       $0.03000 per shot
         SV1 simulator:    $0.075 per minute
       Typical job: 1000 shots on Rigetti = $0.30 + $0.90 = $1.20
       Typical job: 1000 shots on IonQ Aria = $0.30 + $30.00 = $30.30
     ```
   - Cost estimation for a variational algorithm: `n_iterations * n_parameters * 2 * cost_per_circuit`
     (factor of 2 from parameter-shift rule requiring two evaluations per parameter)
   - Comparison: classical GPU cost for equivalent computation (amortized hardware or cloud GPU rates)

3. **Dispatch decision engine** (`simplex-quantum/dispatch/src/dispatcher.sx`)
   - `DispatchDecision` enum: `Classical`, `Quantum(backend, device)`, `Hybrid(split_strategy)`
   - Decision factors (weighted):
     - Expected speedup vs classical (must exceed cost premium threshold)
     - Budget remaining (hard limit — never exceed user budget)
     - Problem size vs device capability (reject if circuit exceeds available qubits)
     - Queue wait time estimate (if QPU queue is long, classical may finish first)
     - Result quality requirement (if high precision needed, more shots needed, higher cost)
   - Configurable dispatch policy: `always_classical`, `always_quantum`, `cost_optimized`, `quality_optimized`

4. **Budget tracker** (`simplex-quantum/dispatch/src/budget.sx`)
   - Per-session and per-project cost tracking
   - Configurable cost limits with warning thresholds (e.g., warn at 80%, hard stop at 100%)
   - Usage reporting: cost breakdown by backend, device, algorithm
   - Cost-per-result metric: actual QPU spend divided by number of useful results

### Success Criteria

- Dispatcher correctly routes a 5-qubit QAOA problem to quantum and a 50-variable LP to classical
- Budget tracker prevents overspend: stops quantum submission when budget exhausted
- Cost estimates for Braket tasks are within 15% of actual billing
- Dispatch decisions are logged with full reasoning for auditability

---

## Phase 5: Quantum-Enhanced Optimization Module

### Objective

Implement concrete quantum algorithms for optimization problems, built on the variational bridge (Phase 3) and dispatched via the cost-aware system (Phase 4).

### Deliverables

1. **QAOA for combinatorial optimization** (`simplex-quantum/optimize/src/combinatorial.sx`)
   - QAOA circuit construction for MaxCut, Traveling Salesman (via QUBO encoding), job-shop scheduling
   - Problem encoding: classical cost function to Ising Hamiltonian
   - Depth-adaptive QAOA: start with p=1, increase depth until convergence plateaus or budget exhausted
   - Result decoding: quantum measurement bitstrings to classical solution candidates
   - Solution quality comparison against classical simulated annealing (from TASK-006)

2. **VQE for molecular simulation** (`simplex-quantum/optimize/src/molecular.sx`)
   - Ansatz construction: hardware-efficient ansatz, UCCSD-inspired ansatz
   - Hamiltonian encoding: molecular Hamiltonians in Pauli basis (user provides Hamiltonian terms)
   - Energy estimation via expectation value measurement
   - Active space reduction: identify which molecular orbitals benefit from quantum treatment

3. **Grover-based structured search** (`simplex-quantum/optimize/src/search.sx`)
   - Oracle construction from classical boolean predicates
   - Amplitude amplification with optimal iteration count
   - Hybrid search: classical pre-filtering to reduce search space, then quantum amplification on remainder
   - Honest assessment: report estimated circuit depth and compare to device coherence time

4. **Hybrid annealing** (`simplex-quantum/optimize/src/hybrid_anneal.sx`)
   - Bridge between Simplex's self-learning annealing (TASK-006) and quantum annealing
   - Classical annealing explores broad landscape, quantum sampling refines promising regions
   - Temperature schedule from meta-optimizer (TASK-006) controls classical-quantum transition point
   - Dual number gradients from classical phase inform initial parameters for quantum phase

### Success Criteria

- QAOA on MaxCut (10 nodes) finds solution within 95% of optimal on simulator
- VQE on H2 molecule produces ground state energy within chemical accuracy (1.6 mHa)
- Hybrid annealing outperforms pure classical annealing on at least one benchmark problem
- All optimizers respect budget constraints and fall back to classical when budget exhausted

---

## Phase 6: Simplex Ecosystem Integration

### Objective

Connect the quantum bridge to the broader Simplex ecosystem: neural gates, epistemic beliefs, and hive coordination.

### Deliverables

1. **Quantum-aware neural gates** (`simplex-learning/src/quantum/neural_gate.sx`)
   - Neural gate variant that can dispatch sub-computations to quantum hardware
   - Gate training mode: use quantum circuits for forward pass, parameter-shift for backward pass
   - Gate inference mode: cached quantum results with classical fallback when QPU unavailable
   - Dual-mode operation inherited from existing neural gate architecture (TASK-005)

2. **Epistemic quantum beliefs** (`simplex-learning/src/quantum/epistemic.sx`)
   - Treat quantum measurement distributions as evidence sources in the belief system
   - Calibrated confidence: map shot statistics to epistemic confidence levels
   - Measurement sufficiency: use belief convergence to determine when enough shots have been taken
   - Falsification: if quantum and classical results disagree beyond confidence bounds, trigger investigation
   - Integration with skeptic specialist (TASK-014): skeptic can challenge quantum results

3. **Hive quantum coordination** (integration patterns, not a new module)
   - Distribute quantum sub-tasks across hive specialists via Nexus protocol
   - Quantum job specialist: dedicated specialist that manages QPU queue, batches circuits, optimizes shot allocation
   - Result aggregation: combine quantum results from multiple backends/devices
   - Consensus: hive votes on whether quantum result is trustworthy (using epistemic framework)

### Success Criteria

- Neural gate with quantum dispatch produces correct gradients in training mode
- Epistemic system correctly identifies when quantum measurement has insufficient shots
- Hive specialist successfully manages concurrent quantum jobs across two backends
- End-to-end demo: hive receives optimization problem, dispatches to quantum, validates result epistemically

---

## Phase 7: Testing, Simulation, and Validation

### Objective

Comprehensive testing using the local simulator (Phase 2) to validate correctness before any QPU spend. Integration tests against live QPU backends with strict budget limits.

### Deliverables

1. **Unit tests** (`simplex-quantum/tests/`)
   - Gate correctness: verify all gates produce correct unitary matrices
   - Circuit builder: construction, serialization, round-trip
   - Measurement: shot aggregation, expectation values, convergence
   - Cost model: pricing calculation accuracy
   - Dispatcher: routing decisions for known problem classes

2. **Simulator integration tests**
   - Bell state: verify entanglement correlation (should be 100% correlated)
   - GHZ state: verify N-qubit entanglement
   - Teleportation protocol: verify state transfer
   - QAOA on 5-qubit MaxCut: verify convergence to known solution
   - VQE on H2: verify ground state energy

3. **Variational bridge tests**
   - Parameter-shift gradient accuracy vs finite difference
   - Dual number gradient flow through quantum-classical boundary
   - Optimizer convergence on toy problems
   - Annealing integration: verify temperature schedule affects quantum parameter updates

4. **Live QPU tests** (opt-in, requires credentials and budget)
   - Braket integration: submit circuit, retrieve result, verify format
   - Cost tracking accuracy: compare estimated vs actual bill
   - Error handling: graceful degradation on QPU errors, queue timeouts
   - Budget enforcement: verify hard stop at budget limit

### Success Criteria

- 100% pass rate on simulator-based tests
- All quantum algorithms produce correct results on simulator before QPU submission
- Live QPU tests (when run) complete within budget and produce valid results
- Test suite runs in under 60 seconds (simulator only, no QPU)

---

## Phase 8: Documentation and Examples

### Deliverables

1. **API documentation** for all public types and functions in `simplex-quantum/`
2. **Tutorial examples**:
   - Getting started: build and run a Bell state circuit on the simulator
   - QAOA walkthrough: solve a small MaxCut problem
   - VQE walkthrough: estimate H2 ground state energy
   - Cost-aware dispatch: configure budget and let the dispatcher choose
   - Hive integration: distribute quantum optimization across specialists
3. **Architecture guide**: how the variational bridge connects dual numbers to quantum parameters
4. **Cost planning guide**: realistic cost estimates for common quantum workloads, how to develop on simulators and minimize QPU spend

### Success Criteria

- All tutorial examples compile and run on the local simulator without modification
- Architecture guide accurately reflects implemented code structure
- Cost guide includes worked examples with actual Braket pricing

---

## Implementation Notes

### Constraints

- **All code must be pure Simplex.** No Python, no Qiskit, no Cirq, no external quantum frameworks. The backend implementations communicate with cloud QPU services directly via HTTP/JSON using Simplex's networking primitives from the Nexus protocol (TASK-012). This is a core project constraint.
- QPU backends require HTTP client capability. The Nexus protocol already implements HTTP communication for hive coordination — the quantum backends reuse this infrastructure.
- The local simulator is implemented in pure Simplex. It handles statevector evolution via matrix multiplication on complex number arrays. This limits it to ~20 qubits (2^20 complex amplitudes = ~16MB), which is sufficient for development and testing.

### Risk Factors

- **NISQ noise**: Current quantum hardware is noisy. Variational algorithms are somewhat noise-tolerant, but results may not match simulator predictions. The epistemic belief system (Phase 6) helps by treating QPU results as uncertain evidence rather than ground truth.
- **QPU availability**: Cloud QPU services have queues and downtime. The backend abstraction handles this with job polling and fallback, but latency can be minutes to hours for queued jobs.
- **Cost management**: A poorly configured variational loop on IonQ Aria could burn through budget quickly ($30+ per 1000-shot circuit, multiplied by iterations and parameter-shift evaluations). The budget tracker (Phase 4) is a safety-critical component.
- **API stability**: Cloud QPU APIs change. Backend implementations should isolate API-specific details behind the trait interface so updates are localized.

### Estimated Effort

| Phase | Estimated Lines | Estimated Duration |
|-------|----------------|--------------------|
| Phase 1: Core Types & Circuit | ~1500 | 2 weeks |
| Phase 2: Backend Abstraction | ~2500 | 3 weeks |
| Phase 3: Variational Bridge | ~2000 | 3 weeks |
| Phase 4: Cost-Aware Dispatch | ~1200 | 2 weeks |
| Phase 5: Optimization Module | ~2000 | 3 weeks |
| Phase 6: Ecosystem Integration | ~1500 | 2 weeks |
| Phase 7: Testing & Validation | ~2000 | 2 weeks |
| Phase 8: Documentation | ~1000 | 1 week |
| **Total** | **~13,700** | **~18 weeks** |
