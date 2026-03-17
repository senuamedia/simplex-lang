# Simplex v0.12.0 Release Notes

**Release Date:** 2026-03-16
**Codename:** Complete Language

---

## Overview

Simplex v0.12.0 is a **complete language release** achieving **154/154 tests passing (100%)** across all language features. This release delivers fully functional neural gates with training/inference mode, forward-mode automatic differentiation with dual numbers, type-aware f64 arithmetic, a comprehensive contract verification system, structural pruning, speculative execution, and a runtime expanded to 19,589 lines with 300+ functions. SQLite3 has been removed as an external dependency.

---

## New Features

### Neural Gate Codegen

The `neural_gate` keyword now compiles with automatic training/inference mode switching.

```simplex
neural_gate my_gate(input: f64) -> f64 {
    // Training: sigmoid activation
    // Inference: hard threshold
}
```

**Behavior:**
- Training mode uses sigmoid activation for gradient flow
- Inference mode uses hard threshold for deterministic output
- Mode switching is automatic based on runtime context

### f64 Type-Aware Arithmetic

Binary operations automatically detect f64 operands and route to runtime f64 functions.

```simplex
let x: f64 = 3.14;
let y: f64 = 2.0;
let z = x * y;  // Automatically uses sx_f64_mul
```

**Detection sources:**
- Variable types and struct field types
- Function return types and cast expressions
- Sub-expression analysis

### Forward-Mode Automatic Differentiation

Dual numbers enable automatic differentiation with proper derivative propagation.

```simplex
let x = dual_variable(2.0);    // value=2.0, derivative=1.0
let y = dual_constant(3.0);    // value=3.0, derivative=0.0
let z = dual_mul(x, y);        // value=6.0, derivative=3.0
```

**Supported operations:** `add`, `mul`, `div`, `sin`, `cos`, `exp`, `ln`, `sqrt`, `tanh`, `sigmoid`, `powi`

### Contract Verification System

Runtime contract checking for requires, ensures, and invariant conditions.

```simplex
fn divide(a: f64, b: f64) -> f64 {
    contract_check_requires(b != 0.0, "divisor must be non-zero");
    let result = a / b;
    contract_check_ensures(result * b == a, "division identity");
    result
}
```

### Reference and Dereference Operators

Proper `&x` (reference) and `*x` (dereference) operators with correct LLVM IR generation.

```simplex
let x: i64 = 42;
let ptr = &x;      // ptrtoint
let val = *ptr;     // inttoptr + load
```

### StringBuilder Library

New `simplex-core/src/strings.sx` provides O(n) string building.

```simplex
use simplex_core::strings;

let sb = string_builder_new();
string_builder_append(sb, "Hello, ");
string_builder_append(sb, "world!");
let result = string_builder_to_string(sb);
```

### GGUF Format Specification

`simplex-core/src/llm.sx` provides GGUF format specification and LLM integration primitives for local model loading.

### SLM Runtime Stubs

`runtime/slm.sx` provides small language model runtime stubs for per-hive SLM inference and model management.

---

## Runtime Expansion

The standalone runtime (`standalone_runtime.c`) has been expanded to **19,589 lines** with 300+ functions.

### New Runtime Subsystems

| Subsystem | Description |
|-----------|-------------|
| **Actor Runtime** | Spawn/send/ask messaging, mailbox system, actor registry |
| **Supervision Trees** | One-for-one, one-for-all restart strategies |
| **Circuit Breaker** | Fault-tolerant actor communication |
| **Work-Stealing Scheduler** | Multi-threaded actor execution |
| **JSON Runtime** | Parse, stringify, object/array manipulation |
| **HashMap Runtime** | String-keyed hash maps with SxString-safe comparison |
| **Neural/ML Runtime** | Training mode, sigmoid, gradient tape, gate registry, pruning |
| **Speculative Execution** | Lazy contexts, branch tracking, weighted results |
| **Weighted References** | GC tracking, weight thresholds, retain/release counting |
| **Observability** | Counters, gauges, histograms, span-based tracing |
| **Logging** | Structured logging with levels, console/file/JSON output |
| **Timer** | High-resolution timing (microsecond/millisecond/second) |
| **UUID** | v4 UUID generation and validation |
| **TOML** | Configuration file parsing and manipulation |
| **f64 Arithmetic** | Type-safe floating-point operations on i64 bit patterns |
| **AI/Cognitive Stubs** | Anima memory, hive mnemonic, specialist inference |

---

## Compiler Improvements

### Float Literal Codegen Fix

String constants for f64 literals now use correct module-qualified names (`@.str.MODULE.N`), preventing LLVM IR conflicts.

### Nested Closure Codegen Fix

Closure definitions are now built atomically to prevent interleaving with parent closures.

### Variable Shadowing Fix

`let` bindings evaluate init expressions before registering the new local, preventing use of uninitialized shadow variables.

### Self Method Field Access

Direct impl fields lookup for `self.field` access with fallback to all-struct search resolves struct field offsets correctly in the self-hosted compiler.

### Math Function Remapping

`cos`, `sin`, `exp`, `ln`, `sqrt`, `tanh`, `pow`, `abs` automatically redirect to f64 runtime wrappers.

### Deduplicated Declarations

All `declare` statements use `emit_stdlib_decl` to prevent LLVM IR redefinition errors when user code defines functions with the same names as stdlib.

---

## Build System Changes

### SQLite3 Dependency Removed

The `-lsqlite3` linker flag has been removed. All persistence is now handled by in-memory implementations. No external database is required.

### API Documentation System

| File | Purpose |
|------|---------|
| `sxdoc-index.sx` | API documentation index generator |
| `simplex-docs/api/lib/manifest.json` | Library API manifest |
| `search-index.json` | Documentation search index |

---

## Tool Updates

All tools updated to version 0.12.0:

| Tool | Version | Changes |
|------|---------|---------|
| **sxc** | 0.12.0 | Neural gate codegen, f64 type inference, self-hosting fixes |
| **sxpm** | 0.12.0 | - |
| **cursus** | 0.12.0 | - |
| **sxdoc** | 0.12.0 | API documentation index generation |
| **sxlsp** | 0.12.0 | - |
| **sxfmt** | 0.12.0 | Expanded to 3,173 lines |
| **sxlint** | 0.12.0 | - |

---

## Edge Hive & Nexus Updates

Both the Edge Hive framework and Nexus Protocol have been updated to 0.12.0:

- **edge-hive**: Updated for runtime compatibility with expanded actor system
- **nexus**: Updated for new runtime APIs and SxString handling

---

## Test Results

### 154/154 Tests Passing (100%)

| Test Category | Tests | Status |
|---------------|-------|--------|
| Language | 42 | PASS |
| Types | 12 | PASS |
| Basics | 6 | PASS |
| Async | 3 | PASS |
| Actors | 1 | PASS |
| Neural | 16 | PASS |
| Standard Library | 27 | PASS |
| Runtime | 8 | PASS |
| AI/Cognitive | 18 | PASS |
| Learning | 4 | PASS |
| Toolchain | 11 | PASS |
| Training | 8 | PASS |
| Observability | 1 | PASS |
| Integration | 7 | PASS |

### Test Runner Enhancement

Single-file test mode added: `./run_tests.sh f path/to/test`

### Known Limitations

| Issue | Status |
|-------|--------|
| Associated types (`type Output` in traits) not fully implemented in codegen | Planned |
| `&mut self` syntax not yet supported | Planned |
| Self-hosted compiler struct field lookup requires fallback path for `self` method access | Workaround in place |

---

## Breaking Changes

- **SQLite3 removed** - Projects that relied on the implicit SQLite3 linking will need to add `-lsqlite3` to their own linker flags if they use SQLite directly.

---

## Upgrade Guide

1. **Update version imports** (if using centralized version):
   ```simplex
   use simplex_core::version;
   // Now returns "0.12.0"
   ```

2. **Remove SQLite3 dependency** from build scripts. The runtime no longer requires it:
   ```bash
   # Before (0.11.0)
   clang build/*.o runtime/standalone_runtime.c -o myapp -lm -lsqlite3

   # After (0.12.0)
   clang build/*.o runtime/standalone_runtime.c -o myapp -lm
   ```

3. **Use new runtime features**:
   ```simplex
   // Neural gates
   neural_gate my_gate(x: f64) -> f64 { ... }

   // Dual numbers for AD
   let x = dual_variable(2.0);

   // Contract verification
   contract_check_requires(n > 0, "must be positive");
   ```

4. **Use StringBuilder for string operations**:
   ```simplex
   use simplex_core::strings;
   let sb = string_builder_new();
   ```

---

## Compatibility

| Component | Minimum Version | Maximum Version |
|-----------|-----------------|-----------------|
| LLVM | 14.0.0 | - |
| Previous Simplex | 0.8.0 | 0.12.0 |

---

## What's Next (v0.13.0)

### v0.13.0: Optimization & Maturity
- Associated types (`type Output` in traits) full codegen
- `&mut self` syntax for mutable references in methods
- Compiler optimization passes
- Performance tuning across runtime subsystems

### v1.0.0: Production Release
- All compiler features complete
- Full test suite passing
- Production-ready stability

---

## Files Changed

| File/Directory | Change |
|----------------|--------|
| `compiler/bootstrap/codegen.sx` | Neural gate codegen, f64 type inference, reference operators, variable shadowing fix |
| `compiler/bootstrap/parser.sx` | Parser improvements for new syntax |
| `compiler/bootstrap/lexer.sx` | New token types |
| `compiler/bootstrap/main.sx` | Self-hosting compiler updates |
| `compiler/bootstrap/error.sx` | Error handling updates |
| `runtime/standalone_runtime.c` | Expanded to 19,589 lines with 300+ functions |
| `simplex-core/src/strings.sx` | NEW - StringBuilder library |
| `simplex-core/src/safety.sx` | Safe memory management utilities |
| `simplex-core/src/llm.sx` | GGUF format specification |
| `runtime/slm.sx` | NEW - SLM runtime stubs |
| `sxdoc-index.sx` | NEW - API documentation index |
| `build.sh` | SQLite3 removed, build system improvements |
| `simplex-core/src/version.sx` | Version 0.12.0 |
| All tools | Version bump to 0.12.0 |

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
# sxc 0.12.0
```
