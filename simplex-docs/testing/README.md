# Simplex Testing Documentation

**Version:** 0.17.0
**Status:** 215/215 tests passing (100%)

This directory contains comprehensive documentation for the Simplex testing framework, including test organization, coverage reports, testing methodologies, and best practices.

## Documentation Index

| Document | Description |
|----------|-------------|
| [Framework Overview](framework.md) | Testing framework architecture and components |
| [Running Tests](running-tests.md) | How to execute and interpret test results |
| [Test Coverage](coverage.md) | Current test coverage by category |
| [Testing Methods](methods.md) | Testing patterns, conventions, and best practices |
| [Writing Tests](writing-tests.md) | Guide for writing new tests |

## Quick Start

Run all tests:
```bash
./tests/run_tests.sh
```

Run tests in a specific category:
```bash
./tests/run_tests.sh language
./tests/run_tests.sh quantum
./tests/run_tests.sh fuzz
```

Filter by test type:
```bash
./tests/run_tests.sh all unit    # Only unit tests
./tests/run_tests.sh all spec    # Only spec tests
./tests/run_tests.sh all integ   # Only integration tests
./tests/run_tests.sh all e2e     # Only end-to-end tests
```

Run a single test:
```bash
./tests/run_tests.sh file quantum/spec_simulator.sx
```

## Naming Convention

All tests follow a consistent naming convention based on test type:

| Prefix | Type | Description | Color |
|--------|------|-------------|-------|
| `unit_` | Unit | Tests individual functions/types in isolation | Blue |
| `spec_` | Specification | Tests language specification compliance | Cyan |
| `integ_` | Integration | Tests integration between components | Magenta |
| `e2e_` | End-to-End | Tests complete workflows | Yellow |
| `fuzz_` | Fuzz | Tests compiler robustness with random input | - |
| `prop_` | Property | Tests structural invariants | - |
| `invariant_` | Formal | Tests critical system invariants | - |

## Test Categories (v0.15.0)

The Simplex test suite is organized into 21 categories with 211 test files:

```
tests/
├── run_tests.sh                 # Test runner script
│
├── language/                    # Core language features (42 tests)
├── types/                       # Type system tests (12 tests)
├── basics/                      # Basic constructs (6 tests)
├── async/                       # Async/await (3 tests)
├── actors/                      # Actor model (1 test)
├── neural/                      # Neural IR and gates (16 tests)
├── learning/                    # Automatic differentiation (4 tests)
├── stdlib/                      # Standard library (29 tests)
├── runtime/                     # Runtime systems (8 tests)
├── ai/                          # AI/Cognitive systems (21 tests)
├── integration/                 # End-to-end scenarios (7 tests)
├── observability/               # Metrics and tracing (1 test)
├── toolchain/                   # Compiler toolchain (16 tests)
├── contracts/                   # Contract verification (1 test)
├── training/                    # Training pipeline (9 tests)
├── math/                        # Mathematical functions (3 tests)
├── quantum/                     # Quantum computing (18 tests)
├── fuzz/                        # Compiler fuzzing (4 tests)
├── properties/                  # Property-based tests (4 tests)
├── safety/                      # Sanitizer integration (3 tests)
└── formal/                      # Formal invariants (3 tests)
```

## Test Statistics (v0.15.0)

| Category | Tests | Pass Rate | Description |
|----------|:-----:|:---------:|-------------|
| Language | 42 | 100% | Core syntax, modules, traits, closures, generics |
| Types | 12 | 100% | Generics, pattern matching, type aliases, references |
| Basics | 6 | 100% | Closures, enums, for loops, match, try operator |
| Async | 3 | 100% | Async/await patterns, multi-await |
| Actors | 1 | 100% | Actor message passing |
| Neural | 16 | 100% | Neural gates, contracts, pruning |
| Learning | 4 | 100% | Dual numbers, automatic differentiation |
| Stdlib | 29 | 100% | Collections, crypto, HTTP, JSON, regex, sync |
| Runtime | 8 | 100% | Actors, async, distribution, I/O, networking |
| AI | 21 | 100% | Anima, hive, SLM, memory, orchestration |
| Integration | 7 | 100% | End-to-end data processing, knowledge persistence |
| Observability | 1 | 100% | Metrics, tracing, counters, gauges |
| Toolchain | 16 | 100% | Compiler, parser, codegen, sxpm, verification |
| Contracts | 1 | 100% | Contract verification logic |
| Training | 9 | 100% | Annealing, attention, LoRA, neural gates |
| Math | 3 | 100% | Mathematical functions |
| Quantum | 18 | 100% | Types, gates, simulator, variational, noise, ZNE, ECC |
| Fuzz | 4 | 100% | Lexer, parser, codegen, grammar fuzzing |
| Properties | 4 | 100% | Lexer, parser, codegen, type properties |
| Safety | 3 | 100% | ASan, UBSan, TSan integration |
| Formal | 3 | 100% | Compiler, runtime, protocol invariants |
| **Total** | **211** | **100%** | **All categories passing** |

## v0.15.0 Changes

### New Test Categories

Seven new test categories added in v0.15.0:

- **quantum** (9 new tests): Noise models, ZNE, error correction, PEC, gate fusion, commutation, depth reduction, routing, optimization pipeline
- **fuzz** (4 new tests): Grammar-aware compiler fuzzing
- **properties** (4 new tests): Property-based testing for compiler correctness
- **safety** (3 new tests): Sanitizer integration (ASan, UBSan, TSan)
- **formal** (3 new tests): Formal invariant verification
- **toolchain/compiler** (3 new tests): Self-hosting, optimization, stability

### Test Runner Updates

- 7 new categories registered: `training`, `contracts`, `math`, `fuzz`, `properties`, `safety`, `formal`
- Single-file mode: `./run_tests.sh file path/to/test.sx`
- All 21 categories now runnable individually

### Production Hardening Infrastructure

- `scripts/run-fuzz.sh` — Fuzz target runner with configurable duration
- `scripts/run-sanitizers.sh` — Sanitizer sweep with ASan/UBSan/TSan
- `.github/workflows/hardening.yml` — CI workflow for hardening checks

## See Also

- [Simplex Specification](../spec/)
- [Release Notes v0.15.0](../RELEASE-0.15.0.md)
- [Production Hardening Spec](../spec/21-production-hardening.md)
