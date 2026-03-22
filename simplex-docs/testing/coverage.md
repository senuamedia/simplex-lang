# Simplex Test Coverage

**Version:** 0.17.0
**Last Updated:** v0.17.0 Release
**Test Results:** 228/228 passing (100%)

## Coverage Summary

| Category | Tests | Features Covered | Coverage |
|----------|:-----:|------------------|----------|
| Language | 42 | Core syntax, types, async, closures, traits, modules | High |
| Types | 12 | Generics, patterns, type aliases, references, turbofish | High |
| Basics | 6 | Closures, enums, for loops, match, try operator | High |
| Async | 3 | Async/await, closures in async, multi-await | High |
| Actors | 1 | Actor model, message passing | Medium |
| Neural | 16 | Neural gates, contracts, pruning, hardware annotations | High |
| Learning | 4 | Dual numbers, tensors, AD | High |
| Stdlib | 29 | Collections, crypto, HTTP, JSON, regex, sync, signals | High |
| Runtime | 8 | Actors, async, distribution, I/O, memory safety, networking | Medium |
| AI | 21 | Anima, hive, SLM, memory, orchestration, tools | High |
| Integration | 7 | End-to-end scenarios | High |
| Observability | 1 | Metrics, tracing | Medium |
| Toolchain | 16 | Compiler, parser, codegen, sxpm, verification, self-host | High |
| Contracts | 1 | Contract verification | Medium |
| Training | 9 | Annealing, attention, LoRA, compression, meta-training | High |
| Math | 3 | Mathematical functions | Medium |
| Quantum | 18 | Core types, gates, simulator, variational, noise, ZNE, ECC, PEC, optimization | High |
| Fuzz | 4 | Lexer, parser, codegen, grammar robustness | High |
| Properties | 4 | Lexer, parser, codegen, type system properties | High |
| Safety | 3 | AddressSanitizer, UBSan, ThreadSanitizer | High |
| Formal | 3 | Compiler, runtime, protocol invariants | High |
| **Total** | **211** | - | **High** |

## Quantum Tests Coverage (v0.14.0-v0.15.0)

| Test File | Features Covered |
|-----------|------------------|
| `unit_quantum_types.sx` | Qubit, QuantumState, CircuitStats, QuantumError |
| `unit_quantum_gates.sx` | Gate matrices, unitarity, Hadamard, Pauli, rotations |
| `spec_simulator.sx` | Statevector simulation, Bell states, GHZ states |
| `spec_circuit_builder.sx` | Circuit construction, gate sequencing, depth |
| `spec_measurement.sx` | Shot aggregation, expectation values, entropy |
| `spec_variational.sx` | Parameterized circuits, parameter-shift gradients |
| `spec_optimization.sx` | QAOA, VQE, Grover search, hybrid annealing |
| `spec_cost_dispatch.sx` | Cost model, budget tracking, dispatch decisions |
| `spec_ecosystem.sx` | Multi-provider backend integration |
| `spec_noise_models.sx` | Depolarizing, amplitude damping, readout error |
| `spec_zne.sx` | Linear, Richardson, exponential extrapolation |
| `spec_error_correction.sx` | Bit-flip, phase-flip, Shor, Steane, surface code |
| `spec_pec.sx` | Quasi-probability decomposition, Monte Carlo |
| `spec_gate_fusion.sx` | Identity elimination, rotation merging, ZYZ fusion |
| `spec_commutation.sx` | Commutation rules, DAG, non-adjacent cancellation |
| `spec_depth_reduction.sx` | ASAP scheduling, routing, SWAP decomposition |
| `spec_routing.sx` | Topology graphs, BFS shortest path, qubit mapping |
| `spec_optimization_pipeline.sx` | Multi-pass optimization, cost functions |

## Production Hardening Coverage (v0.15.0)

### Fuzz Tests

| Test File | Features Covered |
|-----------|------------------|
| `fuzz_lexer.sx` | Random strings, long identifiers, special characters |
| `fuzz_parser.sx` | Nested expressions, many bindings, malformed input |
| `fuzz_codegen.sx` | Empty functions, deep nesting, edge cases |
| `fuzz_grammar.sx` | LCG-based random program generation |

### Property-Based Tests

| Test File | Properties Verified |
|-----------|---------------------|
| `prop_lexer.sx` | Span validity, keyword identity, roundtrip |
| `prop_parser.sx` | Precedence, associativity, parameter parsing |
| `prop_codegen.sx` | Arithmetic, control flow, function calls |
| `prop_types.sx` | Integer/float/bool closure, vec roundtrip |

### Safety Tests

| Test File | Sanitizer |
|-----------|-----------|
| `integ_asan.sx` | AddressSanitizer — memory allocation, vec, strings |
| `integ_ubsan.sx` | UndefinedBehaviorSanitizer — arithmetic, shifts, bounds |
| `integ_tsan.sx` | ThreadSanitizer — sequential consistency |

### Formal Invariant Tests

| Test File | Invariants |
|-----------|------------|
| `invariant_compiler.sx` | Token types, AST nodes, error codes, scope depth |
| `invariant_runtime.sx` | Vec length, push/get, malloc, arithmetic identities |
| `invariant_nexus.sx` | Serialization roundtrip, comparison transitivity |

## Version History

| Version | Total Tests | Categories | Pass Rate |
|---------|:-----------:|:----------:|:---------:|
| 0.7.0 | 147 | 10 | 100% |
| 0.8.0 | 150 | 11 | 100% |
| 0.9.0 | 156 | 13 | 100% |
| 0.12.0 | 154 | 13 | 100% |
| 0.13.0 | 162 | 13 | 100% |
| 0.14.0 | 171 | 14 | 100% |
| 0.15.0 | 211 | 21 | 100% |
| 0.17.0 | 228 | 21 | 100% |

## Coverage Gaps

Areas with limited test coverage:

| Area | Status | Priority |
|------|--------|----------|
| Distributed actors | Basic only | Medium |
| GPU acceleration | Not tested | Low |
| Cross-platform | macOS primarily | Medium |
| Full borrow checking | Not implemented | High |
| Module visibility | Partial | Medium |
