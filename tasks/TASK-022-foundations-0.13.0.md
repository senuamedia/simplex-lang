# TASK-022: Completion & Foundations (v0.13.0)

**Status**: Planning
**Priority**: Critical (Foundation)
**Created**: 2026-03-16
**Updated**: 2026-03-16
**Target Version**: 0.13.0
**Depends On**: TASK-005 (Dual Numbers Phase 1 - Complete), TASK-019 (Compiler Bugs - Partial)
**Baseline**: v0.12.0 — 154/154 tests passing (100%). All new work must maintain this.

---

## Overview

v0.13.0 is a **Completion & Foundations** release with two mandates:

1. **Complete all outstanding partial work** from v0.9.0 through v0.12.0 — every incomplete phase from TASK-001, TASK-003, TASK-005, TASK-011, TASK-013-A, TASK-017, TASK-019, and TASK-020 is resolved here.
2. **Add mathematical foundations** for the Quantum Bridge (v0.14.0, TASK-021) — complex numbers, matrices, eigendecomposition, and full dual number support.

After v0.13.0, there should be zero dangling work from prior releases. The project enters v0.14.0 with a clean slate.

All code must be pure Simplex (core project constraint). No external tools or libraries except where the runtime C layer necessarily interfaces with the OS.

**Estimated scope**: ~12,000-16,000 lines of new/modified code across 12 phases.

**Source tasks consolidated here:**
- TASK-001 Phase 2 (Contract Logic)
- TASK-003 Phases 4-7 (Training Pipeline)
- TASK-005 Phases 2-5 (Dual Numbers completion)
- TASK-011 (Toolchain Integration)
- TASK-013-A (Belief-Gated Receive, remaining 30%)
- TASK-017 (API Documentation, 85% -> 100%)
- TASK-019 (5 Compiler Bugs)
- TASK-020 (Stabilization: codegen refactor, arena allocator, string interning, SLM bindings, shared libraries)

---

## Phase 1: Compiler Stability & Refactoring (Critical)

**Priority**: Blocker — everything else depends on this
**Source**: TASK-019 (5 compiler bugs) + TASK-020 (codegen refactoring)

### Part A: Fix 5 Remaining Compiler Bugs (TASK-019)

#### Bug 1: vec_set redefinition in LLVM IR generation
- `vec_set` is emitted multiple times in generated IR, causing LLVM linker errors
- Root cause is likely in `compiler/bootstrap/codegen.sx` duplicate symbol emission

#### Bug 2: Undefined 'Get' variable in actor message patterns
- Actor pattern matching generates references to an undefined `Get` variable
- Affects all actor-based code (`tests/actors/spec_actor_basic.sx`)

#### Bug 3: Async/await exit code 240 (state machine crashes)
- Async functions compile but crash at runtime with exit code 240
- The async state machine codegen produces invalid state transitions
- Related to Phase 9 (async runtime verification)

#### Bug 4: Compiler segfaults on belief/epistemic modules
- Compiling modules that use belief types causes segfaults in the compiler itself
- Blocks TASK-014 (Epistemic Integrity) integration

#### Bug 5: Missing FFI declarations for native libraries
- FFI calls to native C functions lack proper declarations in generated IR
- Causes undefined symbol errors at link time

### Part B: Codegen Monolith Split (TASK-020)

The `compiler/bootstrap/codegen.sx` file is a 10,654-line monolith. Split into logical modules:

- `codegen/emit_types.sx` — type emission, struct layouts, enum layouts
- `codegen/emit_functions.sx` — function codegen, calling conventions
- `codegen/emit_expressions.sx` — expression trees, operators, literals
- `codegen/emit_control.sx` — if/match/loop/async state machines
- `codegen/emit_actors.sx` — actor message passing, mailbox codegen
- `codegen/emit_ffi.sx` — FFI declarations, C interop bridge
- `codegen/core.sx` — shared utilities, symbol tables, IR builder

Each module must be independently compilable. The main `codegen.sx` becomes a thin dispatcher that imports submodules.

### Part C: Parser Arena Allocator (TASK-020)

- Implement arena allocator for AST nodes in `compiler/bootstrap/parser.sx`
- Currently parser allocates individual AST nodes with no bulk deallocation
- Arena allows fast allocation and single-call free of entire parse tree
- Location: new `compiler/bootstrap/arena.sx` + integration into parser

### Part D: String Interning (TASK-020)

- Implement string interning table for identifiers and keywords
- Reduces memory usage and speeds up string comparisons in lexer/parser
- Location: `compiler/bootstrap/intern.sx`

### Success Criteria
- All 154 existing tests pass
- Zero compiler segfaults on any test file
- Clean compilation of actor, async, belief, and FFI modules
- `codegen.sx` split into <=7 submodules, each under 2,000 lines
- Arena allocator reduces parser peak memory by measurable amount
- String interning eliminates duplicate identifier storage

---

## Phase 2: Contract Logic (TASK-001 Phase 2)

**Depends on**: Phase 1 (compiler stability + codegen refactor)
**Source**: TASK-001 Phase 2 — Contract Logic (NOT implemented)
**Location**: `compiler/bootstrap/parser.sx`, `compiler/bootstrap/codegen.sx` (submodules), `runtime/standalone_runtime.c`

### Syntax Support

Add first-class contract syntax to the compiler:

```simplex
fn transfer(amount: f64, balance: f64) -> f64
    requires amount > 0.0
    requires amount <= balance
    ensures result >= 0.0
{
    balance - amount
}

struct Account {
    balance: f64
    invariant balance >= 0.0
    fallback fn on_violation(self) -> Self {
        Account { balance: 0.0 }
    }
}
```

### Parser Changes
- `requires` clause — precondition expressions parsed after function signature
- `ensures` clause — postcondition expressions with `result` keyword bound to return value
- `invariant` clause — struct-level invariants checked on construction and mutation
- `fallback` block — recovery function invoked when invariant fails

### Codegen Changes
- Emit precondition checks at function entry (before body executes)
- Emit postcondition checks at function exit (after return value computed)
- Emit invariant checks at struct construction and field mutation sites
- On contract failure: invoke `fallback` if defined, otherwise emit runtime error with contract text

### Static Analysis (Basic)
- For simple bounds (e.g., `requires x > 0`), propagate constraint into function body
- Warn at compile time if a provably-false contract is detected
- Runtime confidence checking: contracts that cannot be statically proven are checked at runtime

### Graceful Degradation
- When a contract fails and `fallback` is defined, invoke fallback instead of crashing
- Emit structured error with contract text, actual values, and source location
- Integrate with Simplex's existing `Result` error handling

### Success Criteria
- `requires`/`ensures`/`invariant`/`fallback` all parse and compile
- Contract violations at runtime produce clear error messages with source location
- `fallback` blocks execute on invariant failure instead of crashing
- Existing 154 tests unaffected (no regressions)
- New test: `tests/contracts/spec_contracts.sx`

---

## Phase 3: Complex Number Type

**Depends on**: Phase 1 (compiler stability)
**New module**: `simplex-std/src/complex.sx`

### Core Type
```simplex
struct Complex {
    re: f64,
    im: f64
}
```

### Operations
- **Arithmetic**: `+`, `-`, `*`, `/` (full complex division)
- **Unary**: `conjugate()`, `magnitude()` (|z|), `phase()` (argument/angle)
- **Exponential**: `exp(z)` implementing Euler's formula: `exp(i*theta) = cos(theta) + i*sin(theta)`
- **Trigonometric**: `sin(z)`, `cos(z)`, `tan(z)` for complex arguments
- **Polar conversion**: `to_polar()` -> `(r, theta)`, `from_polar(r, theta)` -> `Complex`
- **Utilities**: `abs()`, `sqrt()`, `pow(z, n)`, `log(z)`

### DualComplex Integration
- `DualComplex` type combining dual numbers with complex arithmetic
- Enables differentiable complex-valued functions
- Bridges `simplex-learning/src/dual/dual.sx` with complex module

### Success Criteria
- All complex algebra laws satisfied (commutativity, associativity, distributivity)
- `exp(i * pi) + 1 ≈ 0` (Euler's identity within f64 epsilon)
- Complex division: `z / z = 1 + 0i` for all non-zero z
- DualComplex correctly propagates derivatives through complex operations

---

## Phase 4: Matrix Type & Linear Algebra

**Depends on**: Phase 1 (compiler stability)
**New module**: `simplex-std/src/matrix.sx`

### Core Type
```simplex
struct Matrix64 {
    rows: i64,
    cols: i64,
    data: vec<f64>    // row-major storage
}
```

### Construction
- `matrix_new(rows, cols)` -> zero-initialized matrix
- `matrix_zeros(rows, cols)` -> explicit zeros
- `matrix_identity(n)` -> n x n identity
- `matrix_from_vec(rows, cols, data)` -> from flat vector

### Core Operations
- `transpose(A)` -> A^T
- `matmul(A, B)` -> matrix multiplication
- `mat_add(A, B)`, `mat_sub(A, B)` -> element-wise
- `mat_scale(A, s)` -> scalar multiplication
- `mat_map(A, fn)` -> apply function element-wise

### Linear Algebra
- `determinant(A)` -> f64 (via LU decomposition)
- `inverse(A)` -> Matrix64 (via LU decomposition)
- `trace(A)` -> sum of diagonal elements
- `rank(A)` -> matrix rank

### Decompositions
- **LU decomposition**: `lu_decompose(A)` -> `(L, U, P)` — needed for determinant, inverse, solving linear systems
- **QR decomposition**: `qr_decompose(A)` -> `(Q, R)` — needed for eigendecomposition and quantum gate decomposition

### Eigendecomposition
- `eigenvalues(A)` -> `vec<Complex>` — QR algorithm for real matrices
- `eigenvectors(A)` -> `(vec<Complex>, Matrix64)` — eigenvalues + eigenvector matrix
- Required for quantum Hamiltonian diagonalization

### ComplexMatrix Integration
- `ComplexMatrix` type for complex-valued matrices
- Required for unitary matrices (quantum gates are unitary)
- `is_unitary(M)` -> bool (verifies M * M^dagger = I)

### DualMatrix Integration
- `DualMatrix` type for matrices with dual number entries
- Enables differentiable matrix operations (needed for variational parameter optimization)

### Success Criteria
- `matmul(identity(n), A) = A` for all A
- Matrix multiplication is associative: `matmul(matmul(A, B), C) = matmul(A, matmul(B, C))`
- `determinant(identity(n)) = 1`
- `matmul(A, inverse(A)) ≈ identity(n)` for invertible A
- Correct eigenvalues for known matrices (e.g., 2x2 rotation matrices)
- `is_unitary(M)` returns true for Pauli matrices

---

## Phase 5: Complete Dual Numbers (TASK-005 Phases 2-5)

**Depends on**: Phase 4 (matrix integration)
**Extends**: `simplex-learning/src/dual/dual.sx` (Phase 1 complete, 918 lines)
**Source**: TASK-005 Phases 2-4 (multidual, dual2, Hessian, Jacobian) + Phase 5 (Integration)

### multidual<N> Type (Phase 2)
- `multidual<N>` type carrying N partial derivative components in a single forward pass
- Avoids N separate forward passes for N-variable gradient computation
- Internal representation: value + vec<f64> of length N for partials

### Gradient & Jacobian (Phase 3)
- `gradient(fn, point)` -> `vec<f64>` — compute gradient of scalar function at a point
- `jacobian(fn, point)` -> `Matrix64` — compute Jacobian of vector-valued function
- Single forward pass through multidual arithmetic

### Second Derivatives (Phase 4)
- `dual2` type carrying value, first derivative, and second derivative
- `hessian(fn, point)` -> `Matrix64` — compute Hessian matrix
- Uses nested dual numbers or dual2 type for second-order information

### Compiler Integration (Phase 5)
- `@differentiable(mode: auto)` compiler annotation for functions
- Compiler recognizes annotated functions and auto-generates dual-number-lifted versions
- Full belief/safety integration with dual numbers (contract-checked derivatives)
- Dead derivative elimination optimization — when only value is used, strip derivative propagation

### SIMD Optimization
- Where possible, use SIMD-friendly memory layout for multidual vector operations
- Aligned storage for derivative components
- Batch operations on derivative vectors

### Success Criteria
- Gradient of 100-variable function computed in single forward pass
- Hessian matches numerical finite-difference approximation (within tolerance)
- `gradient(x^2 + y^2, [3, 4]) = [6, 8]`
- `jacobian` of rotation function produces correct rotation derivative matrix
- Performance: multidual gradient is faster than N separate dual passes
- `@differentiable` annotation compiles and produces correct gradients
- Dead derivative elimination measurably reduces generated code for value-only calls

---

## Phase 6: Training Pipeline Completion (TASK-003 Phases 4-7)

**Depends on**: Phase 5 (dual numbers for gradient computation)
**Source**: TASK-003 Phases 4-7 (incomplete training pipeline)
**Location**: `simplex-learning/src/`

### Phase 4: Learnable Curriculum
- Full meta-optimization integration in `schedules/curriculum.sx`
- Curriculum schedule that adapts training order based on loss landscape
- Integrate with gradient infrastructure from Phase 5 for curriculum parameter optimization

### Phase 5: Meta-Training Framework
- Complete `MetaOptimizer` in `trainer/meta.sx`
- Integration with all schedule types (curriculum, annealing, warmup)
- Meta-learning loop: optimize hyperparameters of the training schedules themselves

### Phase 6: Compression Pipeline
- Staged pipeline: distillation -> pruning -> quantization
- `compression/distill.sx` — knowledge distillation from large to small model
- `compression/prune.sx` — structured pruning of low-magnitude weights
- `compression/quantize.sx` — weight quantization (f64 -> f32 -> i8)
- Pipeline orchestrator that runs stages in sequence with validation between each

### Phase 7: Production Training
- Full specialist training integration
- End-to-end training run: data loading -> training loop -> evaluation -> compression -> export
- Checkpoint save/restore for long training runs
- Training metrics logging

### Success Criteria
- Curriculum schedule adapts training order during a training run
- MetaOptimizer converges on hyperparameters for a simple training task
- Compression pipeline reduces model size while maintaining accuracy within tolerance
- End-to-end training run completes without crashes
- New tests: `tests/learning/spec_training_pipeline.sx`

---

## Phase 7: Belief-Gated Receive Completion (TASK-013-A)

**Depends on**: Phase 1 (Bug 4 fix — compiler segfaults on belief modules)
**Source**: TASK-013-A (~70% done, needs remaining 30%)
**Location**: `compiler/bootstrap/codegen.sx`, `simplex-simplex-edge-hive/src/runtime.sx`, `tests/ai/`

### Codegen Completion
- Implement codegen for `EXPR_BELIEF_AND` — currently parsed but no IR emitted
- Implement codegen for `EXPR_BELIEF_OR` — currently parsed but no IR emitted
- Wire `PAT_BELIEF_GUARD` usage — pattern type is defined in parser but never matched in codegen

### WAKE Mechanism Integration
- C runtime stubs for WAKE exist in `runtime/standalone_runtime.c`
- Need Simplex-level wiring: `wake(actor, belief_condition)` function
- Actor transitions from dormant to active when belief condition crosses threshold
- Integrate with existing actor mailbox system

### HiveBeliefManager Integration
- Connect `HiveBeliefManager` (in `simplex-simplex-edge-hive/src/runtime.sx`) to wake transitions
- Belief updates from hive propagate to gated actors
- Multi-actor belief consensus protocol

### Test Suite
- Create `tests/ai/belief_guards/` directory with comprehensive tests:
  - `spec_belief_and.sx` — compound belief conditions
  - `spec_belief_or.sx` — disjunctive belief conditions
  - `spec_belief_guard_pattern.sx` — pattern matching with belief guards
  - `spec_wake_mechanism.sx` — dormant-to-active actor transitions
  - `spec_hive_belief.sx` — hive-level belief propagation

### Success Criteria
- `EXPR_BELIEF_AND` and `EXPR_BELIEF_OR` compile and produce correct IR
- `PAT_BELIEF_GUARD` works in match expressions
- WAKE transitions fire when belief thresholds are crossed
- All belief guard tests pass
- No regressions in existing actor/hive tests

---

## Phase 8: HTTP Client & JSON Parser

**Depends on**: Phase 1 (compiler stability)

### HTTP Client
**Extends**: `simplex-std/src/http.sx` (currently server-only)

#### Client API
```simplex
fn http_get(url: string, headers: vec<string>) -> Result<Response, HttpError>
fn http_post(url: string, body: string, headers: vec<string>) -> Result<Response, HttpError>
fn http_put(url: string, body: string, headers: vec<string>) -> Result<Response, HttpError>
fn http_delete(url: string, headers: vec<string>) -> Result<Response, HttpError>
```

#### Response Type
```simplex
struct Response {
    status: i64,
    headers: vec<string>,
    body: string
}
```

#### Features
- TLS/HTTPS support (required for cloud API calls to Braket, IBM, Azure)
- Configurable timeout (default 30s)
- Retry with exponential backoff
- Header management (Content-Type, Authorization, etc.)

#### Runtime Implementation
- C runtime functions in `runtime/standalone_runtime.c`
- Use raw sockets + TLS (or libcurl if available) for HTTP client
- FFI bridge from Simplex to C runtime

### JSON Parser
**New/extended module**: `simplex-std/src/json.sx`

#### Core Types
```simplex
enum JsonValue {
    Object(vec<JsonEntry>),
    Array(vec<JsonValue>),
    String(string),
    Number(f64),
    Bool(bool),
    Null
}

struct JsonEntry {
    key: string,
    value: JsonValue
}
```

#### Parser API
- `json_parse(input: string)` -> `Result<JsonValue, JsonError>` — full recursive descent parser
- `json_stringify(value: JsonValue)` -> `string` — serialize back to JSON string
- `json_stringify_pretty(value: JsonValue, indent: i64)` -> `string` — human-readable output

#### Type-Safe Extraction
- `json_get_string(value: JsonValue, key: string)` -> `Result<string, JsonError>`
- `json_get_number(value: JsonValue, key: string)` -> `Result<f64, JsonError>`
- `json_get_bool(value: JsonValue, key: string)` -> `Result<bool, JsonError>`
- `json_get_array(value: JsonValue, key: string)` -> `Result<vec<JsonValue>, JsonError>`
- `json_get_object(value: JsonValue, key: string)` -> `Result<JsonValue, JsonError>`

#### Error Handling
- Line/column information in parse errors
- Descriptive error messages for malformed JSON
- Graceful handling of deeply nested structures

### Success Criteria
- HTTP: Can `GET https://httpbin.org/get` and receive valid response
- HTTP: HTTPS/TLS works for secure endpoints
- HTTP: Timeout fires correctly for unreachable hosts
- JSON: Round-trip: `json_parse(json_stringify(value)) = value`
- JSON: Handle all JSON types: objects, arrays, strings, numbers, booleans, null
- JSON: Correct handling of escaped characters in strings
- JSON: Error on malformed input with useful error messages

---

## Phase 9: Async/Await Runtime Fix

**Depends on**: Phase 1 (compiler fixes) + Phase 8 (HTTP client)

### State Machine Fix
- Fix async state machine codegen that currently crashes with exit code 240
- Audit state transition table in `compiler/bootstrap/codegen.sx` (or `codegen/emit_control.sx` after refactor)
- Ensure all async function states have valid transitions and cleanup paths

### Runtime Verification
- Verify `block_on()` runtime function works correctly
- Test event loop integration with async operations
- Ensure proper stack management for suspended coroutines

### Integration Tests
- Async HTTP client calls (GET, POST with .await)
- Async with error handling (try/catch in async context)
- Multiple concurrent async operations
- Async timeout handling

### Success Criteria
- Async function with `.await` compiles and runs correctly
- Exit code 0 (not 240) for all async tests
- `block_on(async_http_get(url))` returns valid response
- Error propagation works through async boundaries

---

## Phase 10: API Documentation & Tooling (TASK-017 + TASK-020)

**Depends on**: Phase 1 (compiler stability)
**Source**: TASK-017 (85% -> 100%) + TASK-020 (shared lexer/AST libraries)

### sxdoc Completion (TASK-017)
- Implement `--manifest` flag in `tools/sxdoc.sx` — generates `simplex-docs/api/lib/manifest.json` from source
- Implement `--category` flag in `tools/sxdoc.sx` — filters doc generation by category (e.g., `--category math`)
- Ensure manifest JSON is valid and parseable by the doc site

### Shared Lexer/AST Libraries (TASK-020)
- Extract common lexer logic used by `sxc`, `sxfmt`, `sxlint`, `sxlsp` into `lib/lexer.sx`
- Extract common AST types and traversal utilities into `lib/ast.sx`
- All tools import from shared libraries instead of reimplementing lexer/AST code
- Reduces code duplication across 7 tools

### Success Criteria
- `sxdoc --manifest` produces valid manifest.json
- `sxdoc --category math` generates only math-related docs
- `lib/lexer.sx` and `lib/ast.sx` exist and are imported by all tools
- No duplicate lexer or AST code across tools
- New tests: `tests/toolchain/spec_sxdoc_flags.sx`

---

## Phase 11: Toolchain Integration (TASK-011 Completion)

**Depends on**: Phase 10 (shared libraries available)
**Source**: TASK-011 (Toolchain Audit)

### Shared Module Wiring
- Wire `simplex-core/src/platform.sx`, `simplex-core/src/version.sx`, `simplex-core/src/safety.sx` into all tools
- Wire `lib/lexer.sx`, `lib/ast.sx` (from Phase 10) into all tools
- All tools import shared modules instead of defining their own utilities

### Duplicate Removal
Remove duplicate `get_os_name()` and `VERSION()` functions from:
- `tools/sxc.sx`
- `tools/sxpm.sx`
- `tools/sxdoc.sx`
- `tools/sxlsp.sx`
- `tools/sxfmt.sx`
- `tools/sxlint.sx`
- `tools/cursus.sx`

### Safety Fixes
- Add bounds checking to `cursus.sx` `cvm_push()` — currently no stack overflow protection
- Wire up `codegen_free()` calls — currently allocated memory is never freed

### Success Criteria
- Zero duplicate utility functions across tools
- All tools import from `lib/` shared modules
- `cvm_push()` returns error on stack overflow instead of corrupting memory
- No memory leaks from missing `codegen_free()` calls

---

## Phase 12: SLM Native Bindings (TASK-020)

**Depends on**: Phase 1 (FFI bug fix) + Phase 6 (training pipeline)
**Source**: TASK-020 (SLM native C bindings)
**Location**: `runtime/slm.sx` (currently stubs), `runtime/standalone_runtime.c`

### Native C Bindings
- Replace stubs in `runtime/slm.sx` with real FFI calls to C implementations
- Tensor operations: create, reshape, matmul, element-wise ops via C runtime
- Memory management: C-side allocation with Simplex-side lifetime tracking
- SIMD-accelerated paths for hot tensor operations (where platform supports)

### Integration with Training Pipeline
- Training loop uses native tensor ops for forward/backward passes
- Gradient computation integrates with Phase 5 dual number infrastructure
- Model checkpoint serialization via C runtime file I/O

### Success Criteria
- SLM tensor operations execute via native C code, not Simplex interpretation
- Measurable speedup over pure-Simplex tensor operations
- Training loop completes end-to-end with native bindings
- No memory leaks (C allocations properly freed)
- New tests: `tests/learning/spec_slm_native.sx`

---

## Dependency Graph

```
Phase 1: Compiler Stability & Refactoring (BLOCKER)
    |
    +-------+-------+-------+-------+-------+-------+
    |       |       |       |       |       |       |
    v       v       v       v       v       v       v
Phase 2  Phase 3  Phase 4  Phase 7  Phase 8  Phase 10  (independent after P1)
Contract Complex  Matrix   Belief   HTTP+    API Docs
Logic    Numbers          Guards   JSON     & Shared
    |       |       |       |       |       Libs
    |       |       v       |       |       |
    |       |   Phase 5    |       v       v
    |       |   Dual Nums  |   Phase 9  Phase 11
    |       |   (complete) |   Async    Toolchain
    |       |       |       |   Verify  Integration
    |       |       v       |       |
    |       |   Phase 6    |       |
    |       |   Training   |       |
    |       |   Pipeline   |       |
    |       |       |       |       |
    |       |       v       |       |
    |       |   Phase 12   |       |
    |       |   SLM Native |       |
    |       |       |       |       |
    v       v       v       v       v
    +-------+-------+-------+-------+
    |
    v
TASK-021: Quantum Bridge (v0.14.0)
```

**Parallel tracks after Phase 1:**
- **Compiler track**: Phase 2 (Contract Logic) — independent, only needs Phase 1
- **Math track**: Phase 3 (Complex) + Phase 4 (Matrix) in parallel, then Phase 5 (Dual Numbers) after Phase 4
- **AI/ML track**: Phase 6 (Training Pipeline) after Phase 5, then Phase 12 (SLM) after Phase 6
- **Belief track**: Phase 7 (Belief Guards) — independent after Phase 1 (Bug 4 fix)
- **Infra track**: Phase 8 (HTTP + JSON), then Phase 9 (Async) after Phase 8
- **Toolchain track**: Phase 10 (Docs + Shared Libs), then Phase 11 (Integration) after Phase 10

---

## How This Enables TASK-021 (Quantum Bridge)

Each phase in v0.13.0 directly unblocks specific capabilities in the Quantum Bridge:

| v0.13.0 Phase | Source Task | Quantum Bridge Dependency | What It Unblocks |
|---|---|---|---|
| **Phase 1** — Compiler Stability & Refactoring | TASK-019, TASK-020 | Everything | Reliable compilation of all quantum modules; zero segfaults on complex type hierarchies; maintainable codegen for quantum IR extensions |
| **Phase 2** — Contract Logic | TASK-001 | Quantum safety | `requires`/`ensures` on quantum operations (e.g., ensuring unitary constraints, valid qubit indices); graceful degradation on quantum hardware errors |
| **Phase 3** — Complex Numbers | New | Quantum amplitudes, gate matrices | Quantum state vectors are complex-valued; all quantum gates are complex matrices; measurement probabilities come from |amplitude|^2 |
| **Phase 4** — Matrix Operations | New | Unitary gate application, state vectors | Applying a quantum gate = matrix-vector multiplication; multi-qubit operations = tensor products of matrices; gate decomposition requires QR/eigendecomposition |
| **Phase 5** — Complete Dual Numbers | TASK-005 | Variational parameter optimization | VQE and QAOA require gradient descent on parameterized circuit angles; Jacobian needed for multi-parameter optimization loops; `@differentiable` annotation streamlines variational circuits |
| **Phase 6** — Training Pipeline | TASK-003 | Hybrid quantum-classical training | Variational quantum algorithms use classical optimizers; complete training pipeline enables hybrid quantum-classical optimization loops |
| **Phase 7** — Belief Guards | TASK-013-A | Quantum state confidence | Belief-gated receives can model quantum measurement confidence; wake mechanism maps to quantum event-driven architectures |
| **Phase 8** — HTTP Client + JSON | New | Cloud QPU API calls | Amazon Braket, IBM Quantum, Azure Quantum all use REST APIs with JSON payloads; must POST circuit definitions and GET results over HTTPS |
| **Phase 9** — Async/Await | New | Non-blocking QPU submission | QPU jobs take seconds to minutes; async allows submitting jobs and processing results without blocking; enables concurrent multi-backend queries |
| **Phase 10** — API Docs & Shared Libs | TASK-017, TASK-020 | Developer experience | Complete documentation for quantum module users; shared libraries reduce maintenance burden for quantum tooling |
| **Phase 11** — Toolchain Integration | TASK-011 | Clean tool ecosystem | Ensures sxc, sxpm, and other tools work reliably for quantum module development workflow |
| **Phase 12** — SLM Native Bindings | TASK-020 | Quantum simulation performance | Native tensor operations needed for efficient quantum state vector simulation on classical hardware |

Without v0.13.0, the Quantum Bridge would need to implement its own math primitives inline, work around compiler crashes, lack contract-based safety, have no training pipeline for variational algorithms, and have no way to call cloud QPU APIs — making it impractical.

---

## Estimated Line Counts by Phase

| Phase | Module(s) | Est. Lines |
|---|---|---|
| Phase 1A | `compiler/bootstrap/codegen.sx`, `parser.sx` | ~500 (bug fixes) |
| Phase 1B | `compiler/bootstrap/codegen/*.sx` (split) | ~800 (refactor + glue) |
| Phase 1C | `compiler/bootstrap/arena.sx` | ~300-400 |
| Phase 1D | `compiler/bootstrap/intern.sx` | ~200-300 |
| Phase 2 | `compiler/bootstrap/parser.sx`, `codegen.sx`, `runtime/standalone_runtime.c` | ~1,200-1,500 |
| Phase 3 | `simplex-std/src/complex.sx` | ~800-1,000 |
| Phase 4 | `simplex-std/src/matrix.sx` | ~2,000-2,500 |
| Phase 5 | `simplex-learning/src/dual/dual.sx` (extend) + compiler annotation | ~1,800-2,200 |
| Phase 6 | `simplex-learning/src/` (schedules, trainer, compression) | ~2,000-2,500 |
| Phase 7 | `compiler/bootstrap/codegen.sx`, `simplex-simplex-edge-hive/`, tests | ~600-800 |
| Phase 8 | `simplex-std/src/http.sx`, `json.sx`, `runtime/standalone_runtime.c` | ~1,800-2,500 |
| Phase 9 | `compiler/bootstrap/codegen.sx` + tests | ~500-700 |
| Phase 10 | `tools/sxdoc.sx`, `lib/lexer.sx`, `lib/ast.sx` | ~800-1,000 |
| Phase 11 | `tools/*.sx`, `lib/*.sx` | ~300-500 (refactor) |
| Phase 12 | `runtime/slm.sx`, `runtime/standalone_runtime.c` | ~800-1,000 |
| **Total** | | **~14,400-17,400** |

---

## Test Plan

Each phase includes its own success criteria above. New test files to create:

### Compiler & Contracts
- `tests/contracts/spec_contracts.sx` — requires, ensures, invariant, fallback
- `tests/compiler/spec_arena_alloc.sx` — parser arena allocation correctness
- `tests/compiler/spec_string_intern.sx` — interning deduplication

### Math
- `tests/math/spec_complex.sx` — complex arithmetic, Euler's identity, polar conversion
- `tests/math/spec_matrix.sx` — matmul, determinant, inverse, eigendecomposition
- `tests/math/spec_dual_complete.sx` — gradient, jacobian, hessian, @differentiable

### AI/ML
- `tests/learning/spec_training_pipeline.sx` — curriculum, meta-training, compression, end-to-end
- `tests/learning/spec_slm_native.sx` — native tensor ops, training with native bindings

### Belief
- `tests/ai/belief_guards/spec_belief_and.sx` — compound belief conditions
- `tests/ai/belief_guards/spec_belief_or.sx` — disjunctive belief conditions
- `tests/ai/belief_guards/spec_belief_guard_pattern.sx` — pattern matching with belief guards
- `tests/ai/belief_guards/spec_wake_mechanism.sx` — dormant-to-active transitions
- `tests/ai/belief_guards/spec_hive_belief.sx` — hive-level belief propagation

### Infrastructure
- `tests/infra/spec_http_client.sx` — HTTP GET/POST, TLS, timeout
- `tests/infra/spec_json_parser.sx` — parse, stringify, round-trip, error handling
- `tests/async/spec_async_http.sx` — async HTTP calls, concurrent operations

### Toolchain
- `tests/toolchain/spec_sxdoc_flags.sx` — --manifest and --category flags
- `tests/toolchain/spec_shared_modules.sx` — verify no duplicate functions

**Release gate**: All 154 existing tests pass + all new tests pass + zero compiler segfaults.

---

## Task Debt Closure Summary

After v0.13.0, the following tasks will be fully complete:

| Task | What v0.13.0 Finishes | Status After |
|---|---|---|
| TASK-001 | Phase 2 (Contract Logic) | **Complete** |
| TASK-003 | Phases 4-7 (Training Pipeline) | **Complete** |
| TASK-005 | Phases 2-5 (Dual Numbers) | **Complete** |
| TASK-011 | Toolchain Integration | **Complete** |
| TASK-013-A | Belief-Gated Receive (remaining 30%) | **Complete** |
| TASK-017 | API Documentation (remaining 15%) | **Complete** |
| TASK-019 | 5 Compiler Bugs | **Complete** |
| TASK-020 | Stabilization (codegen split, arena, interning, SLM, shared libs) | **Complete** |

**Zero task debt entering v0.14.0 (Quantum Bridge).**
