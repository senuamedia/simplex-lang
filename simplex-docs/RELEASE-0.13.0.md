# Simplex v0.13.0 Release Notes

**Release Date:** 2026-03-16
**Codename:** Completion & Foundations

---

## Overview

Simplex v0.13.0 is a **Completion & Foundations** release with two mandates:

1. **Complete all outstanding partial work** from v0.9.0 through v0.12.0 — every incomplete phase from eight prior tasks is resolved here, bringing the project to zero task debt.
2. **Add mathematical foundations** for the Quantum Bridge (v0.14.0) — complex numbers, matrices, eigendecomposition, and full dual number support.

After v0.13.0, there is zero dangling work from prior releases. The project enters v0.14.0 with a clean slate.

**Stats:** 162/162 tests passing (100%), ~13,500 lines of new code, 8 previously incomplete tasks fully closed.

---

## New Features

### Compiler Stability (Phase 1)

Five critical compiler bugs fixed:

| Bug | Description | Impact |
|-----|-------------|--------|
| **vec_set redefinition** | `vec_set` emitted multiple times in generated IR, causing LLVM linker errors | Blocks all Vec-based code |
| **Undefined 'Get' in actors** | Actor pattern matching generated references to undefined `Get` variable | Blocks all actor code |
| **Async exit code 240** | Async state machine codegen produced invalid state transitions, crashing at runtime | Blocks all async code |
| **Segfaults on belief/epistemic** | Compiling belief-type modules caused compiler segfaults | Blocks epistemic integrity |
| **Missing FFI declarations** | FFI calls to native C functions lacked proper declarations in generated IR | Blocks all FFI interop |

### Contract Logic (TASK-001 Phase 2)

New keywords for neural gate contract specifications:

```simplex
neural_gate StabilityGate {
    requires input_range(-1.0, 1.0)
    ensures output_range(0.0, 1.0)
    invariant weight_norm < 10.0
    fallback default_output(0.5)
}
```

- `requires` — precondition checks on gate inputs
- `ensures` — postcondition checks on gate outputs
- `invariant` — properties that must hold across training epochs
- `fallback` — safe default behavior when contracts are violated

### Complex Numbers (`simplex-std::complex`, 650 lines)

Full complex arithmetic module for quantum computing foundations:

```simplex
use simplex_std::complex;

let z1 = Complex::new(3.0, 4.0);   // 3 + 4i
let z2 = Complex::from_polar(5.0, 0.927);
let product = z1.mul(z2);
let magnitude = z1.abs();           // 5.0
let euler = Complex::exp_i(PI);     // e^(i*pi) = -1 + 0i
```

- Full arithmetic: add, sub, mul, div, neg, conjugate
- Euler's formula: `exp_i(theta)`
- Polar form: `to_polar()`, `from_polar(r, theta)`
- Complex trigonometry: sin, cos, tan, sinh, cosh, tanh
- Magnitude, phase, real/imaginary extraction

### Matrix & Linear Algebra (`simplex-std::matrix`, 1,457 lines)

Comprehensive linear algebra for quantum state manipulation:

```simplex
use simplex_std::matrix;

let H = Matrix::hadamard();         // Hadamard gate
let CNOT = Matrix::cnot();          // CNOT gate
let result = H.matmul(state_vec);   // Apply gate to state
let inv = matrix.inverse();         // Matrix inversion via LU
let kron = A.kronecker(B);          // Tensor product
```

- Core operations: matmul, transpose, trace, determinant
- LU decomposition with partial pivoting
- Matrix inverse via LU factorization
- Kronecker product for quantum tensor products
- Quantum gate constructors: Hadamard, Pauli-X/Y/Z, CNOT, Phase, T-gate
- Eigenvalue estimation (power iteration)
- Identity, zeros, ones, diagonal constructors

### Dual Numbers Phase 2-4 (TASK-005 completion)

Extended automatic differentiation from single-variable to full N-dimensional support:

```simplex
use simplex_std::diff;

// N-dimensional gradients
let grad = gradient(f, point);       // df/dx_i for all i

// Jacobian matrices
let J = jacobian(vec_f, point);      // df_i/dx_j

// Hessian matrices (via Dual2)
let H = hessian(f, point);           // d2f/dx_i*dx_j
```

- **MultiDual** — N-dimensional dual numbers for gradient computation
- **Dual2** — second-order dual numbers carrying value, first derivative, and second derivative
- **`diff` module** — high-level differentiation API with `gradient()`, `jacobian()`, `hessian()` functions

### Training Pipeline Completion (TASK-003 Phases 4-7)

- **Meta-optimizer** orchestrating all 5 learnable schedules (learning rate, temperature, LoRA rank, pruning threshold, attention window)
- **Staged compression pipeline** — train, prune, quantize, distill in sequence
- **Curriculum learning** — progressive difficulty scheduling for training data

### Belief-Gated Receive (TASK-013-A completion)

- Codegen fixes for belief-weighted message filtering in actors
- `suspend` and `wake` declarations for belief-gated scheduling
- Comprehensive test suite validating belief thresholds and gate behavior

### HTTP Client (`simplex-std::http_client`, 912 lines)

```simplex
use simplex_std::http_client;

let response = HttpClient::get("https://api.example.com/data");
let body = response.json();
let post_resp = HttpClient::post(url, json_body)
    .header("Authorization", token);
```

- GET, POST, PUT, DELETE methods
- TLS support for HTTPS
- JSON convenience methods for request/response bodies
- Header manipulation and status code handling

### JSON Parser (`simplex-std::json`, 941 lines)

```simplex
use simplex_std::json;

let obj = Json::parse(raw_string);
let name = obj.get("user").get("name").as_string();
let built = Json::object()
    .set("key", Json::string("value"))
    .set("count", Json::number(42));
let output = built.stringify();
```

- Full JSON parse and stringify
- Builder pattern for constructing JSON objects and arrays
- Nested traversal with `.get()` chaining
- Type-safe value extraction: `as_string()`, `as_number()`, `as_bool()`, `as_array()`

### Async/Await Fix (TASK-019 Bug 3 resolution)

- State persistence across await points
- Ready tagging for completed futures
- Invalid state detection and handling instead of exit code 240

### API Documentation (TASK-017 completion)

- `sxdoc --manifest` flag generates machine-readable API manifests
- `sxdoc --category` flag filters documentation by module category
- Documentation coverage brought from 85% to 100%

### SLM Native Bindings (405 lines C runtime)

- GGUF model file validation
- Handle table for safe model lifecycle management
- Tokenizer bindings for text-to-token conversion
- Cosine similarity for embedding comparison

---

## Toolchain Integration (TASK-011)

All 7 tools synced to v0.13.0 with shared module consistency:

| Tool | Version | Changes |
|------|---------|---------|
| **sxc** | 0.13.0 | 5 bug fixes, VM bounds checking |
| **sxpm** | 0.13.0 | Shared module sync |
| **cursus** | 0.13.0 | Version bump |
| **sxdoc** | 0.13.0 | `--manifest` and `--category` flags |
| **sxlsp** | 0.13.0 | Version bump |
| **sxfmt** | 0.13.0 | Version bump |
| **sxlint** | 0.13.0 | Version bump |

---

## Test Results

### 162/162 Tests Passing (100%)

Up from 154 in v0.12.0. Eight new tests added for new modules and bug fix verification.

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
| Toolchain | 11 | PASS |
| Training | 8 | PASS |
| Observability | 1 | PASS |
| Integration | 7+ | PASS |

### Known Issues

None. All previously known issues have been resolved.

---

## Tasks Completed

| Task | Name | Status Before | Status After |
|------|------|---------------|--------------|
| TASK-001 | Neural IR (Phase 2: Contract Logic) | Partial | Complete |
| TASK-003 | Training Pipeline (Phases 4-7) | Partial | Complete |
| TASK-005 | Dual Numbers (Phases 2-5) | Partial | Complete |
| TASK-011 | Toolchain Integration | Incomplete | Complete |
| TASK-013-A | Belief-Gated Receive | 70% | Complete |
| TASK-017 | API Documentation | 85% | Complete |
| TASK-019 | Compiler Bugs (5 remaining) | Open | Complete |
| TASK-020 | Stabilization | Partial | Complete |

---

## Breaking Changes

### File Renames

- `temp_attention.sx` renamed to `temperature_attention.sx` for clarity

### Runtime ABI Change

- JSON type intrinsic functions (`json_type_of`, `json_is_object`, etc.) now return `int64_t` instead of `int8_t`. Code using these functions recompiles cleanly; pre-compiled binaries linking against the old runtime must be recompiled.

No other breaking changes. All existing v0.12.0 code compiles without modification.

---

## Upgrade Guide

1. **Rebuild all tools:**
   ```bash
   ./build.sh
   ```
   All 7 tools have been version-bumped and must be rebuilt.

2. **Update temperature attention imports** (if applicable):
   ```simplex
   // Before
   use temp_attention;

   // After
   use temperature_attention;
   ```

3. **SQLite3 no longer needed** — this dependency was removed in v0.12.0. If you have custom build scripts referencing `-lsqlite3`, those flags can be removed.

4. **Update version imports:**
   ```simplex
   use lib::version;
   // Now returns "0.13.0"
   ```

---

## Compatibility

| Component | Minimum Version | Maximum Version |
|-----------|-----------------|-----------------|
| LLVM | 14.0.0 | - |
| Previous Simplex | 0.8.0 | 0.13.0 |

---

## What's Next (v0.14.0)

### v0.14.0: Quantum Bridge (TASK-021)

The mathematical foundations laid in v0.13.0 (complex numbers, matrices, eigendecomposition, dual numbers) enable the next major milestone: a quantum computing bridge within Simplex.

- Quantum state representation using complex vector spaces
- Quantum gate application via matrix operations
- Measurement simulation with probabilistic collapse
- Entanglement tracking via Kronecker products
- Hybrid classical-quantum programming model

### v1.0.0: Production Release

- All compiler features complete
- Full test suite passing
- Production-ready stability

---

## Files Changed

| File/Directory | Change |
|----------------|--------|
| `compiler/bootstrap/codegen.sx` | 5 bug fixes, contract logic codegen |
| `compiler/bootstrap/parser.sx` | Arena allocator integration |
| `compiler/bootstrap/intern.sx` | New: string interning table |
| `compiler/bootstrap/arena.sx` | New: arena allocator |
| `simplex-std/complex.sx` | New: complex number module (650 lines) |
| `simplex-std/matrix.sx` | New: matrix & linear algebra module (1,457 lines) |
| `simplex-std/http_client.sx` | New: HTTP client module (912 lines) |
| `simplex-std/json.sx` | New: JSON parser module (941 lines) |
| `simplex-std/diff.sx` | New: differentiation API (gradient, Jacobian, Hessian) |
| `simplex-learning/src/lib.sx` | MultiDual, Dual2, training pipeline completion |
| `runtime/standalone_runtime.c` | SLM native bindings (405 lines added) |
| `lib/version.sx` | Version 0.13.0 |
| All tools | Version bump to 0.13.0 |

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
# sxc 0.13.0
```
