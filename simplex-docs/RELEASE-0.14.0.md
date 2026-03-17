# Simplex v0.14.0 Release Notes

**Release Date:** 2026-03-17
**Codename:** Quantum Bridge

---

## Overview

Simplex v0.14.0 is the **Quantum Bridge** release -- the first programming language to integrate hybrid classical-quantum computation as a first-class library alongside AI-native actors, neural gates, and self-learning optimization.

Built on the mathematical foundations laid in v0.13.0 (complex numbers, matrices, eigendecomposition, full dual number support), v0.14.0 delivers a complete quantum computing framework: core types and circuit builder, a multi-provider backend abstraction layer, variational quantum algorithms with parameter-shift gradients, a cost-aware task dispatcher with budget tracking, and quantum-enhanced optimization primitives (MaxCut, molecular simulation, Grover search, hybrid annealing).

**Stats:** 171+ tests passing, ~8,500 lines of new quantum code across 29 source files and 9 test files, 5 new Modulus packages.

---

## New Features

### Quantum Core (`simplex-quantum`, Phase 1)

Core quantum types, circuit builder, gate library, and measurement simulation.

```simplex
use simplex_quantum::{Qubit, Circuit, Hadamard, CNOT, measure};

// Build a Bell state circuit
let circuit = Circuit::new(2);
circuit.add(Hadamard::on(0));
circuit.add(CNOT::on(0, 1));

// Measure -- produces |00> or |11> with equal probability
let result = measure(circuit);
```

- **Qubit** -- quantum bit representation with complex amplitude pairs
- **Circuit** -- sequential gate builder with qubit count validation
- **Gate library** -- Hadamard, Pauli-X/Y/Z, CNOT, Phase, T-gate, Toffoli, SWAP, custom unitaries
- **Measurement** -- probabilistic collapse with Born rule sampling
- **Error handling** -- quantum-specific error types (QubitIndexOutOfRange, UnitarityViolation, etc.)

### Backend Abstraction (`simplex-quantum/backend`, Phase 2)

Unified interface for submitting circuits to cloud providers and local simulators.

```simplex
use simplex_quantum::backend::{LocalSimulator, BraketBackend, BackendRegistry};

// Local development
let sim = LocalSimulator::new();
let result = sim.run(circuit, shots: 1000);

// Production: Amazon Braket
let braket = BraketBackend::new(region: "us-east-1", device: "sv1");
let result = braket.run(circuit, shots: 1000);

// Registry for dynamic backend selection
let registry = BackendRegistry::new();
registry.register("local", sim);
registry.register("braket", braket);
let backend = registry.get("braket");
```

- **LocalSimulator** -- full statevector simulation for testing and development
- **BraketBackend** -- Amazon Braket integration (SV1, TN1, DM1, IonQ, Rigetti)
- **IbmBackend** -- IBM Quantum via Qiskit Runtime
- **AzureBackend** -- Azure Quantum (IonQ, Quantinuum)
- **BackendRegistry** -- dynamic backend registration and lookup
- **BackendTrait** -- common interface: `run(circuit, shots) -> MeasurementResult`

### Variational Quantum Algorithms (`simplex-quantum/variational`, Phase 3)

Parameterized circuits, gradient estimation, and hybrid classical-quantum optimization.

```simplex
use simplex_quantum::variational::{ParamCircuit, VQE, QAOA, parameter_shift_gradient};

// Variational Quantum Eigensolver
let ansatz = ParamCircuit::ry_entangled(num_qubits: 4, depth: 3);
let vqe = VQE::new(ansatz, hamiltonian, backend);
let (energy, optimal_params) = vqe.minimize(max_iter: 100, lr: 0.1);

// QAOA for combinatorial optimization
let qaoa = QAOA::new(cost_hamiltonian, mixer, backend, depth: 3);
let (cost, params) = qaoa.optimize(max_iter: 50);

// Parameter-shift gradient for any parameterized circuit
let gradient = parameter_shift_gradient(circuit, params, backend);
```

- **ParamCircuit** -- parameterized quantum circuits with named rotation angles
- **VQE** -- variational quantum eigensolver with classical optimizer loop
- **QAOA** -- quantum approximate optimization algorithm
- **parameter_shift_gradient** -- exact gradient estimation via the parameter-shift rule
- **Classical optimizer bridge** -- gradient descent, Adam, and custom optimizers

### Cost-Aware Quantum Dispatch (`simplex-quantum/dispatch`, Phase 4)

Problem analysis, multi-provider cost modeling, and budget enforcement.

```simplex
use simplex_quantum::dispatch::{Dispatcher, Budget, CostPolicy};

let budget = Budget::new(max_usd: 100.0);
let dispatcher = Dispatcher::new()
    .add_backend("braket_sv1", braket_sv1)
    .add_backend("ibm_brisbane", ibm)
    .budget(budget)
    .policy(CostPolicy::CheapestFirst);

// Dispatcher analyzes circuit, estimates cost per backend, routes optimally
let result = dispatcher.submit(circuit, shots: 10000);
// Automatically picks the cheapest backend that fits within budget
```

- **ProblemAnalyzer** -- circuit depth, gate count, qubit count analysis
- **CostModel** -- per-backend pricing (per-shot, per-task, per-minute models)
- **Budget** -- USD budget tracking with per-backend spend breakdown
- **Dispatcher** -- policy-driven routing (CheapestFirst, FastestFirst, LowestError)
- **Providers modeled** -- Amazon Braket (SV1 $0.075/min, IonQ $0.01/shot, Rigetti $0.00035/shot), IBM Quantum (free tier + premium), Azure Quantum (IonQ, Quantinuum)

### Quantum-Enhanced Optimization (`simplex-quantum/optimize`, Phase 5)

Production-ready quantum optimization primitives integrated with self-learning annealing.

```simplex
use simplex_quantum::optimize::{maxcut_qaoa, molecular_vqe, grover_search, hybrid_anneal};

// MaxCut via QAOA
let (cut_value, partition) = maxcut_qaoa(graph, depth: 3, backend);

// Molecular ground state via VQE
let (energy, state) = molecular_vqe(hamiltonian, ansatz, backend);

// Grover search for structured problems
let solution = grover_search(oracle, num_qubits: 8, backend);

// Hybrid classical-quantum annealing
let result = hybrid_anneal(objective, quantum_sampler, classical_optimizer);
```

- **maxcut_qaoa** -- QAOA-based MaxCut solver for graph partitioning
- **molecular_vqe** -- VQE for molecular ground state energy estimation
- **grover_search** -- Grover's algorithm for unstructured and structured search
- **hybrid_anneal** -- hybrid classical-quantum annealing combining quantum sampling with classical optimization schedules from `simplex-training`

### Ecosystem Integration (Phase 6)

Bridges between the quantum framework and existing Simplex subsystems.

- **Quantum neural gates** -- neural gates that use quantum circuits as learnable components, with Gumbel-Softmax differentiation through measurement
- **Epistemic quantum beliefs** -- belief system extended with quantum uncertainty: superposition beliefs, entangled belief pairs, measurement-triggered belief collapse
- **Gradient bridge** -- parameter-shift gradients from quantum circuits feed into the dual number autodiff system for end-to-end hybrid optimization

### Language Improvements

- **Associated types in traits** -- `type Output` declarations in trait definitions, enabling more expressive generic programming. Parser and codegen support complete.
- **`&mut self` syntax** -- ergonomic mutable method receivers. Parser recognizes `&mut` references with borrow semantics.
- **Compiler optimizations** -- improved codegen for struct field access, reduced IR size for large modules.
- **Self-hosted compiler progress** -- struct field lookup fix brings the self-hosted compiler to approximately 85% completion.

---

## Module Layout

```
simplex-quantum/
  Modulus.toml                     -- simplex-quantum (core)
  src/
    mod.sx                         -- module root and re-exports
    types.sx                       -- Qubit, Amplitude, QuantumState
    gates.sx                       -- Gate enum, unitary matrices
    circuit.sx                     -- Circuit builder
    measurement.sx                 -- Born rule measurement, sampling
    error.sx                       -- Quantum error types
  backend/
    Modulus.toml                   -- simplex-quantum-backend
    src/
      mod.sx                       -- module root
      trait.sx                     -- BackendTrait definition
      simulator.sx                 -- LocalSimulator (statevector)
      braket.sx                    -- Amazon Braket integration
      ibm.sx                       -- IBM Quantum integration
      azure.sx                     -- Azure Quantum integration
      registry.sx                  -- BackendRegistry
  variational/
    Modulus.toml                   -- simplex-quantum-variational
    src/
      mod.sx                       -- module root
      param_circuit.sx             -- ParamCircuit, rotation gates
      parameter_shift.sx           -- Parameter-shift gradient rule
      vqe.sx                       -- Variational Quantum Eigensolver
      qaoa.sx                      -- Quantum Approximate Optimization
      optimizer.sx                 -- Classical optimizer bridge
  dispatch/
    Modulus.toml                   -- simplex-quantum-dispatch
    src/
      mod.sx                       -- module root
      analyzer.sx                  -- ProblemAnalyzer
      cost_model.sx                -- Per-backend pricing
      budget.sx                    -- Budget tracking
      dispatcher.sx                -- Policy-driven routing
  optimize/
    Modulus.toml                   -- simplex-quantum-optimize
    src/
      mod.sx                       -- module root
      combinatorial.sx             -- MaxCut via QAOA
      molecular.sx                 -- VQE for molecular simulation
      search.sx                    -- Grover search
      hybrid_anneal.sx             -- Hybrid classical-quantum annealing
```

---

## Toolchain

All 7 tools synced to v0.14.0:

| Tool | Version | Changes |
|------|---------|---------|
| **sxc** | 0.14.0 | Struct field lookup fix, associated type codegen |
| **sxpm** | 0.14.0 | Version bump |
| **cursus** | 0.14.0 | Version bump |
| **sxdoc** | 0.14.0 | Version bump |
| **sxlsp** | 0.14.0 | Version bump |
| **sxfmt** | 0.14.0 | Version bump |
| **sxlint** | 0.14.0 | Version bump |

---

## Test Results

### 171+ Tests Passing

Up from 162 in v0.13.0. Nine new quantum test files added.

| Test Category | Tests | Status |
|---------------|-------|--------|
| Language | 42+ | PASS |
| Types | 12+ | PASS |
| Basics | 6 | PASS |
| Async | 3+ | PASS |
| Actors | 1+ | PASS |
| Neural | 16 | PASS |
| Standard Library | 27+ | PASS |
| Runtime | 8 | PASS |
| AI/Cognitive | 18 | PASS |
| Learning | 4+ | PASS |
| Toolchain | 14 | PASS |
| Training | 8 | PASS |
| Math | 3 | PASS |
| Quantum | 9 | PASS |
| Observability | 1 | PASS |
| Integration | 7+ | PASS |

### Quantum Test Suite

| Test | Description |
|------|-------------|
| `unit_quantum_types` | Qubit, amplitude, quantum state primitives |
| `unit_quantum_gates` | Gate library: Hadamard, Pauli, CNOT, Phase, T-gate |
| `spec_circuit_builder` | Circuit construction, gate sequencing, validation |
| `spec_simulator` | LocalSimulator statevector execution |
| `spec_measurement` | Born rule measurement, probabilistic sampling |
| `spec_variational` | VQE, QAOA, parameter-shift gradients |
| `spec_cost_dispatch` | Cost modeling, budget enforcement, routing |
| `spec_optimization` | MaxCut, molecular VQE, Grover, hybrid annealing |
| `spec_ecosystem` | Neural gate bridge, belief bridge, gradient bridge |

### Known Issues

None. All previously known issues have been resolved.

---

## Breaking Changes

### None

All existing v0.13.0 code compiles without modification. The quantum modules are purely additive.

---

## Upgrade Guide

1. **Rebuild all tools:**
   ```bash
   ./build.sh
   ```

2. **Update version imports:**
   ```simplex
   use simplex_core::version;
   // Now returns "0.14.0"
   ```

3. **Start using quantum modules:**
   ```simplex
   use simplex_quantum::{Circuit, Hadamard, CNOT, measure};
   use simplex_quantum::backend::{LocalSimulator};
   ```

---

## Compatibility

| Component | Minimum Version | Maximum Version |
|-----------|-----------------|-----------------|
| LLVM | 14.0.0 | - |
| Previous Simplex | 0.8.0 | 0.14.0 |

---

## What's Next

### v0.15.0: Planned Focus Areas

- **Quantum error mitigation** -- noise models, error correction codes, zero-noise extrapolation
- **Quantum circuit optimization** -- gate fusion, commutation rules, depth reduction
- **Self-hosted compiler completion** -- remaining 15% of self-hosting gaps
- **Performance** -- optimization passes, inlining heuristics, compile-time improvements
- **Production hardening** -- comprehensive fuzzing, property-based testing, formal verification

### v1.0.0: Production Release

- All compiler features complete
- Full test suite passing
- Production-ready stability
- Comprehensive documentation

---

## Files Changed

| File/Directory | Change |
|----------------|--------|
| `simplex-quantum/` | New: complete quantum computing framework (29 source files) |
| `simplex-quantum/backend/` | New: backend abstraction layer (6 source files) |
| `simplex-quantum/variational/` | New: variational algorithms (5 source files) |
| `simplex-quantum/dispatch/` | New: cost-aware dispatcher (5 source files) |
| `simplex-quantum/optimize/` | New: quantum optimization (5 source files) |
| `tests/quantum/` | New: 9 test files for quantum subsystem |
| `compiler/bootstrap/codegen.sx` | Associated type codegen, struct field lookup fix |
| `compiler/bootstrap/parser.sx` | `&mut self` syntax support |
| `simplex-core/src/version.sx` | Version 0.14.0 |
| All tools | Version bump to 0.14.0 |
| All Modulus.toml | Version bump to 0.14.0 |

---

## Credits

Developed by Rod Higgins ([@senuamedia](https://github.com/senuamedia)).

---

## Installation

```bash
# Clone and build
git clone https://github.com/senuamedia/simplex-lang.git
cd simplex-lang
./build.sh

# Verify version
./sxc --version
# sxc 0.14.0
```
