# Running Simplex Tests

**Version:** 0.16.0

## Quick Start

### Run All Tests

```bash
./tests/run_tests.sh
```

### Expected Output

```
==============================================
         Simplex Language Test Suite
==============================================
  Compiler: sxc v0.15.0 (self-hosted)

Language
    language/actors/spec_actor_basic               [spec] PASS
    language/async/spec_async_basic                 [spec] PASS
    ...

Quantum
    quantum/spec_simulator                          [spec] PASS
    quantum/spec_noise_models                       [spec] PASS
    quantum/spec_zne                                [spec] PASS
    ...

Fuzz Testing
    fuzz/fuzz_lexer                                 [test] PASS
    fuzz/fuzz_parser                                [test] PASS
    ...

==============================================
  Passed:   197
  Failed:   0
  Total:    197 (100% pass rate)
==============================================
```

## Running by Category

```bash
# Core language
./tests/run_tests.sh language
./tests/run_tests.sh types
./tests/run_tests.sh basics
./tests/run_tests.sh async
./tests/run_tests.sh actors

# AI & Learning
./tests/run_tests.sh neural
./tests/run_tests.sh ai
./tests/run_tests.sh learning
./tests/run_tests.sh training

# Libraries & Runtime
./tests/run_tests.sh stdlib
./tests/run_tests.sh runtime
./tests/run_tests.sh integration
./tests/run_tests.sh observability

# Quantum
./tests/run_tests.sh quantum

# Toolchain
./tests/run_tests.sh toolchain
./tests/run_tests.sh contracts
./tests/run_tests.sh math

# Production Hardening (v0.15.0)
./tests/run_tests.sh fuzz
./tests/run_tests.sh properties
./tests/run_tests.sh safety
./tests/run_tests.sh formal
```

## Running by Test Type

Filter tests by their naming prefix:

```bash
./tests/run_tests.sh all unit    # Only unit tests (unit_*)
./tests/run_tests.sh all spec    # Only spec tests (spec_*)
./tests/run_tests.sh all integ   # Only integration tests (integ_*)
./tests/run_tests.sh all e2e     # Only end-to-end tests (e2e_*)
```

## Combining Category and Type

```bash
./tests/run_tests.sh stdlib unit      # Only stdlib unit tests
./tests/run_tests.sh neural spec      # Only neural spec tests
./tests/run_tests.sh toolchain integ  # Only toolchain integration tests
./tests/run_tests.sh quantum spec     # Only quantum spec tests
```

## Running Individual Tests

### Single File Mode

```bash
# Run a single test file
./tests/run_tests.sh file quantum/spec_simulator.sx
./tests/run_tests.sh file stdlib/unit_hashmap.sx

# Multiple files
./tests/run_tests.sh file quantum/spec_zne.sx quantum/spec_pec.sx
```

### Using sxc Directly

```bash
# Compile and run
sxc run tests/stdlib/unit_hashmap.sx

# Compile only
sxc build tests/stdlib/unit_hashmap.sx

# Syntax check
sxc check tests/stdlib/unit_hashmap.sx
```

## Production Hardening Scripts (v0.15.0)

### Fuzzing

```bash
# Run all fuzz targets for 60 seconds each
scripts/run-fuzz.sh 60

# Run for 5 minutes each
scripts/run-fuzz.sh 300
```

### Sanitizer Sweep

```bash
# AddressSanitizer + UBSan
scripts/run-sanitizers.sh "address undefined"

# ThreadSanitizer only
scripts/run-sanitizers.sh thread
```

### Bootstrap Verification

```bash
# Three-stage compiler bootstrap check
scripts/bootstrap-verify.sh
```

## Test Output Colors

| Color | Meaning |
|-------|---------|
| GREEN | Test passed (PASS) |
| RED | Test failed (FAIL, COMPILE FAIL, LINK FAIL) |
| BLUE | Unit test `[unit]` |
| CYAN | Spec test `[spec]` |
| MAGENTA | Integration test `[integ]` |
| YELLOW | E2E test `[e2e]` / Category headers |

## Failure Types

| Result | Cause |
|--------|-------|
| `COMPILE FAIL` | Syntax error or type error in test source |
| `LINK FAIL` | LLVM IR linking error (undefined symbols, type mismatch) |
| `RUNTIME FAIL` | Runtime exception or panic |
| `FAIL` | Test assertions failed (exit code non-zero) |

## Continuous Integration

### GitHub Actions

Three CI workflows run tests:

| Workflow | File | Tests |
|----------|------|-------|
| Build | `build.yml` | Core test subset on Linux/macOS |
| Runtime Safety | `runtime-safety.yml` | ASan/UBSan/TSan matrix |
| Hardening | `hardening.yml` | Fuzz regression, property tests, formal invariants |

### Exit Codes

- `0` — all tests pass
- Non-zero — at least one test failed

## Troubleshooting

### "Unknown category"

Ensure your category is one of: `language`, `types`, `basics`, `async`, `actors`, `neural`, `stdlib`, `runtime`, `ai`, `learning`, `toolchain`, `integration`, `quantum`, `observability`, `training`, `contracts`, `math`, `fuzz`, `properties`, `safety`, `formal`.

### "COMPILE FAIL"

```bash
# Get detailed compiler error
sxc build tests/path/to/test.sx
```

### "LINK FAIL"

Usually caused by undefined functions or type mismatches in generated IR:
```bash
# Compile to .ll and try linking manually
sxc build tests/path/to/test.sx
clang -O2 tests/path/to/test.ll runtime/standalone_runtime.c -o /tmp/test.bin -lm -lssl -lcrypto
```

### Test hangs

Run with a timeout:
```bash
timeout 30 sxc run tests/path/to/test.sx
```
