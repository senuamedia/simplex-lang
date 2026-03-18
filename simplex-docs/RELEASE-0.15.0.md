# Simplex v0.15.0 — Production Hardening & Quantum Maturity

**Release Date:** 2026-03-19

## Overview

v0.15.0 is a maturity release focused on quantum error mitigation, circuit optimization,
compiler improvements, performance optimization, and production hardening. This release
brings the quantum computing framework to production readiness and establishes comprehensive
testing infrastructure for long-term reliability.

Building on the Quantum Bridge (v0.14.0), this release adds the missing pieces for real
quantum hardware: noise modeling, error mitigation, error correction codes, and circuit
optimization for target backends. The compiler gains collection iteration, keyword-safe
method dispatch, and where-clause parsing. A new production hardening suite provides
fuzzing, property-based testing, sanitizer integration, and formal invariant verification.

---

## NEW: Quantum Error Mitigation

Real quantum hardware is noisy. v0.15.0 provides a complete toolkit for managing noise.

### Noise Models

```simplex
use simplex_quantum::noise::{NoiseModel, DepolarizingNoise, AmplitudeDamping};

// Build a realistic noise model
let noise = noise_model_new();
let depol = depolarizing_new(0.001);        // 0.1% single-qubit error
let amp_damp = amplitude_damping_new(0.01); // T1 relaxation
noise_model_add_single_qubit(noise, depol);
noise_model_add_single_qubit(noise, amp_damp);
noise_model_add_readout(noise, 0.02);       // 2% readout error

// Run circuit with noise
let result = noisy_run(circuit, 4, noise, 1000);
```

Supported channels:
- **Depolarizing** — symmetric single/two-qubit depolarizing channels
- **Amplitude damping** — T1 energy relaxation
- **Phase damping** — T2 dephasing
- **Readout error** — asymmetric measurement bit-flip
- **Custom Kraus** — user-defined operator channels with completeness validation

### Zero-Noise Extrapolation (ZNE)

Mitigate noise without full error correction by measuring at multiple noise levels
and extrapolating to zero noise:

```simplex
use simplex_quantum::mitigation::{zne_new, zne_set_scale_factors, zne_execute};

let zne = zne_new();
zne_set_scale_factors(zne, factors);     // [1.0, 2.0, 3.0]
zne_set_extrapolation(zne, EXTRAP_RICHARDSON);

// Collect expectation values at each noise scale
let values = vec_new();
vec_push(values, f64_to_bits(0.92));  // scale 1
vec_push(values, f64_to_bits(0.88));  // scale 2
vec_push(values, f64_to_bits(0.88));  // scale 3

let mitigated = zne_execute(zne, values);  // extrapolates to ~1.0
```

Extrapolation methods: **Linear**, **Polynomial**, **Richardson**, **Exponential**

### Error Correction Codes

Protect quantum information from decoherence:

```simplex
use simplex_quantum::ecc::{bitflip_encode_instructions, bitflip_correction};

// 3-qubit bit-flip code: |ψ⟩ → α|000⟩ + β|111⟩
let encode_circuit = bitflip_encode_instructions();

// After error, extract syndrome and correct
let syndrome = 3;  // (1,1) → error on qubit 1
let correction = bitflip_correction(syndrome);  // returns qubit to correct
```

Implemented codes:
| Code | Parameters | Corrects |
|------|-----------|----------|
| Bit-flip | [[3,1,1]] | 1 bit-flip error |
| Phase-flip | [[3,1,1]] | 1 phase-flip error |
| Shor | [[9,1,3]] | Any single-qubit error |
| Steane | [[7,1,3]] | Any single-qubit error (CSS) |
| Surface | [[d²,1,d]] | ⌊(d-1)/2⌋ errors |

### Probabilistic Error Cancellation (PEC)

For circuits too deep for ZNE:

```simplex
use simplex_quantum::mitigation::{pec_decomposition_new, pec_add_term, pec_estimate};

let decomp = pec_decomposition_new();
pec_add_term(decomp, 0, 1.02);   // identity with weight 1.02
pec_add_term(decomp, 1, -0.007); // X correction with weight -0.007
// ... quasi-probability decomposition

let samples = vec_new();
// Monte Carlo sampling from quasi-probability distribution
let estimate = pec_estimate(samples, n_samples);
```

---

## NEW: Quantum Circuit Optimization

Every gate introduces noise. Fewer gates = better results.

### Gate Fusion

```simplex
use simplex_quantum::circuit_opt::{identity_elimination, rotation_merge, single_qubit_fusion};

// Remove self-inverse pairs: H·H=I, X·X=I, CNOT·CNOT=I
let opt1 = identity_elimination(instructions);

// Merge rotations: Rz(π/4)·Rz(π/4) → Rz(π/2)
let opt2 = rotation_merge(opt1);

// Fuse consecutive single-qubit gates via ZYZ decomposition
let opt3 = single_qubit_fusion(opt2);
// 47 gates → 23 gates (51% reduction)
```

### Commutation-Aware Cancellation

```simplex
use simplex_quantum::circuit_opt::{commutation_cancel, build_commutation_dag};

// Find non-adjacent cancelling gates by checking commutation of intervening gates
// Z(q0), X(q1), Z(q0) → X(q1) since Z commutes past X on different qubits
let optimized = commutation_cancel(instructions);
```

### Qubit Routing

Map logical circuits to physical hardware connectivity:

```simplex
use simplex_quantum::circuit_opt::{topology_grid, route_circuit};

// 4×4 grid topology (IBM-style)
let topo = topology_grid(4, 4);

// Insert SWAPs to satisfy connectivity constraints
let routed = route_circuit(instructions, 16, topo);
```

Topologies: **Linear**, **Grid**, **All-to-all**, custom adjacency

### Multi-Pass Optimization Pipeline

```simplex
use simplex_quantum::circuit_opt::{pipeline_default, pipeline_run};

let pipe = pipeline_default();  // identity_elim → rotation_merge → commutation_cancel
let optimized = pipeline_run(pipe, instructions, n_qubits, 10);
// Iterates until convergence (gate count stable)
```

### Native Gate Decomposition

```simplex
use simplex_quantum::circuit_opt::{decompose_to_native, GATESET_IBM};

// Decompose to IBM native gates: {CX, Rz, SX, X}
let ibm_circuit = decompose_to_native(instructions, GATESET_IBM);
```

Gate sets: **Clifford+T**, **CX+Rz**, **IBM** (CX, Rz, SX, X)

---

## Compiler Improvements

### Collection Iteration

The `for` loop now supports iterating over collections, not just ranges:

```simplex
let items = vec_new();
vec_push(items, 10);
vec_push(items, 20);
vec_push(items, 30);

// NEW: iterate over collection
for x in items {
    println(int_to_string(x));
}

// Also works with reference prefix (& is accepted and skipped)
for x in &items {
    println(int_to_string(x));
}
```

Desugars to index-based iteration: `for i in 0..vec_len(collection) { let x = vec_get(collection, i); ... }`

### Keywords as Method Names

Keywords can now appear in method position after a dot. This fixes the `.send()` conflict:

```simplex
// Previously failed because 'send' is a keyword
channel.send(message);    // Now works
actor.match(pattern);     // Now works
result.type();            // Now works
```

### Where Clauses

```simplex
fn process<T>(items: Vec<T>) -> i64
    where T: Display + Clone
{
    // T must implement Display and Clone
    for item in items {
        println(item.display());
    }
    0
}
```

### Nested Generics

`Arc<Mutex<T>>`, `Vec<Option<T>>`, and other nested generic types now parse correctly.

### Match Exhaustiveness Warnings

The compiler now emits warnings when a `match` on an enum type lacks a wildcard `_` arm:

```
warning[W0050]: non-exhaustive match on enum type 'Color'
  --> theme.sx:42:5
   |
42 |     match color {
   |     ^^^^^ add a '_ => ...' arm to handle remaining variants
```

---

## Performance Optimization

### Extended Constant Folding

The compiler now folds more expressions at compile time:
- Boolean short-circuit: `true || x` → `true`, `false && x` → `false`
- Identity operations: `x + 0` → `x`, `x * 1` → `x`, `x * 0` → `0`
- Nested constant expressions: `(2 + 3) * 4` → `20`

### LLVM Optimization Hints

Generated IR now includes metadata to help LLVM's backend optimizer:
- `!prof` metadata for branch prediction (likely/unlikely)
- `noalias` on function parameters for alias analysis
- `readonly` on pure functions
- `add nsw` for loop increments (enables vectorization)

### String Interning

Duplicate string constants across modules are deduplicated in the generated IR,
reducing binary size.

### Incremental Build

```bash
# Only recompile changed .sx files
scripts/incremental-build.sh

# Uses SHA256 hashing with dependency tracking
# Skips unchanged files, rebuilds dependents
```

---

## Production Hardening

### Compiler Fuzzing

Four fuzz harnesses test compiler robustness against malformed input:

```bash
# Run all fuzz targets for 60 seconds each
scripts/run-fuzz.sh 60
```

- **fuzz_lexer** — Random byte streams, unterminated strings, very long identifiers
- **fuzz_parser** — Missing semicolons, unmatched braces, deeply nested expressions
- **fuzz_codegen** — Empty functions, many parameters, extreme nesting
- **fuzz_grammar** — LCG-based random valid program generator (50 programs per run)

### Property-Based Testing

Tests structural invariants that must hold for all inputs:

- **Lexer**: Token spans are contiguous, non-overlapping, within source bounds
- **Parser**: Valid programs parse without error; precedence and associativity correct
- **Codegen**: Compiled arithmetic matches runtime evaluation
- **Types**: Operations preserve type consistency

### Runtime Safety Verification

```bash
# Run sanitizer sweep
scripts/run-sanitizers.sh "address undefined"
```

- **AddressSanitizer** — Buffer overflows, use-after-free, memory leaks
- **UndefinedBehaviorSanitizer** — Integer overflow, null dereference, misaligned access
- **ThreadSanitizer** — Data races in concurrent code

### Formal Invariant Verification

Tests that critical invariants hold:

- **Compiler**: Token types in valid range, AST nodes well-formed, error codes follow E#### pattern
- **Runtime**: Vec length always ≥ 0, push increases length, malloc returns non-null, arithmetic identities hold
- **Protocol**: Integer serialization roundtrips, comparison transitivity

### CI Hardening

New `.github/workflows/hardening.yml` runs on every PR:
- Fuzz regression tests
- Sanitizer sweep (address + undefined behavior)
- Property-based tests
- Formal invariant checks

---

## New Packages

| Package | Version | Description |
|---------|---------|-------------|
| `simplex-quantum-noise` | 0.15.0 | Noise models: depolarizing, damping, readout, Kraus |
| `simplex-quantum-mitigation` | 0.15.0 | ZNE, PEC, gate tomography, readout mitigation |
| `simplex-quantum-ecc` | 0.15.0 | Error correction: bit-flip, Shor, Steane, surface code |
| `simplex-quantum-circuit-opt` | 0.15.0 | Gate fusion, commutation, routing, native decomposition |

## New Scripts

| Script | Description |
|--------|-------------|
| `scripts/bootstrap-verify.sh` | Three-stage compiler bootstrap verification |
| `scripts/incremental-build.sh` | SHA256-based incremental compilation |
| `scripts/run-fuzz.sh` | Fuzz target runner with configurable duration |
| `scripts/run-sanitizers.sh` | Sanitizer sweep with ASan/UBSan/TSan |

## Test Suite

**197/197 verified passing (100% pass rate)**

| Category | Tests | Status |
|----------|:-----:|:------:|
| language | 42 | 100% |
| types | 12 | 100% |
| basics | 6 | 100% |
| async | 3 | 100% |
| actors | 1 | 100% |
| neural | 16 | 100% |
| learning | 4 | 100% |
| stdlib | 29 | 100% |
| runtime | 8 | 100% |
| ai | 21 | 100% |
| integration | 7 | 100% |
| observability | 1 | 100% |
| toolchain | 16 | 100% |
| contracts | 1 | 100% |
| quantum | 18 | 100% |
| fuzz | 4 | 100% |
| properties | 4 | 100% |
| safety | 3 | 100% |
| formal | 3 | 100% |
| **21 categories** | **197** | **100%** |

## Breaking Changes

None. All changes are additive.

## Dependencies

No new external dependencies. All new code is pure Simplex.

## Upgrade Guide

No action needed. v0.15.0 is a drop-in replacement for v0.14.0.

The new `for x in collection { }` syntax is purely additive — existing `for x in 0..n { }` continues to work exactly as before.

## Contributors

- Rod Higgins
