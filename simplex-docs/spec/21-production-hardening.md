# 21. Production Hardening

**Version:** 0.15.0

## Overview

Production hardening ensures the Simplex compiler and runtime handle all inputs safely,
produce correct results, and maintain critical invariants. This specification covers
fuzzing, property-based testing, sanitizer integration, and formal invariant verification.

## Testing Pyramid

```
     ┌───────────────────┐
     │   Formal Proofs   │  invariant_*.sx
     │   (3 test files)  │
     ├───────────────────┤
     │  Property-Based   │  prop_*.sx
     │   (4 test files)  │
     ├───────────────────┤
     │  Safety/Sanitizer │  integ_*.sx
     │   (3 test files)  │
     ├───────────────────┤
     │      Fuzzing      │  fuzz_*.sx
     │   (4 test files)  │
     ├───────────────────┤
     │  Specification    │  spec_*.sx (existing)
     │   (130+ tests)    │
     ├───────────────────┤
     │    Unit Tests     │  unit_*.sx (existing)
     │   (50+ tests)     │
     └───────────────────┘
```

## Compiler Fuzzing

### Strategy

Fuzz testing feeds random or semi-random inputs to the compiler to discover crashes,
hangs, or unexpected behavior. The compiler must handle any input gracefully — either
producing correct output or a clean error message.

### Fuzz Targets

| Target | Input | Invariant |
|--------|-------|-----------|
| `fuzz_lexer` | Random strings | Tokenizer never crashes |
| `fuzz_parser` | Malformed programs | Parser produces AST or error |
| `fuzz_codegen` | Edge-case programs | Codegen produces valid IR or error |
| `fuzz_grammar` | Random valid programs | All stages complete without crash |

### Grammar-Aware Fuzzing

The grammar-aware fuzzer generates syntactically plausible Simplex programs using
a context-free grammar with random choices. This produces more meaningful test
cases than pure random bytes.

Random element generators:
- `gen_identifier(seed)` — valid Simplex identifiers
- `gen_integer(seed)` — integer literals in valid range
- `gen_type(seed)` — randomly selected type (i64, f64, bool, String)
- `gen_expr(seed, depth)` — expressions with depth limiting
- `gen_function(seed)` — complete function definitions

PRNG: Linear Congruential Generator (LCG):
```
next = (seed × 1103515245 + 12345) mod 2^31
```

### CI Integration

Fuzz targets run in CI on every PR:
- Duration: 30 seconds per target in CI, configurable locally
- Any crash fails the PR
- Crash inputs are minimized and added to the regression corpus

## Property-Based Testing

### Lexer Properties

| Property | Description |
|----------|-------------|
| Span validity | Every token span is within source bounds |
| Non-overlapping | Token spans don't overlap |
| Deterministic | Lexing twice produces same result |
| Keyword identity | Keywords tokenize to correct token type |
| Roundtrip | Integer/string literals preserve their values |

### Parser Properties

| Property | Description |
|----------|-------------|
| Corpus acceptance | All test suite files parse without error |
| Precedence | `a + b * c` groups multiplication first |
| Associativity | `a - b - c` is left-associative |
| Parameter count | Functions accept 0-N parameters |

### Codegen Properties

| Property | Description |
|----------|-------------|
| Arithmetic | Compiled `2 + 3` evaluates to `5` |
| Control flow | `if true { a } else { b }` evaluates to `a` |
| Loops | `for i in 0..n { count++ }` produces `n` |
| Function calls | Return values match specification |

### Type System Properties

| Property | Description |
|----------|-------------|
| Integer closure | Integer + integer → integer |
| Float closure | Float + float → float |
| Vec roundtrip | push then get returns pushed value |
| Generic consistency | Same function works with multiple types |

## Runtime Safety Verification

### AddressSanitizer (ASan)

Detects memory safety violations:
- Buffer overflows (stack and heap)
- Use-after-free
- Double-free
- Memory leaks (with `detect_leaks=1`)

Compile flags: `-fsanitize=address -fno-omit-frame-pointer -O1 -g`

### UndefinedBehaviorSanitizer (UBSan)

Detects undefined behavior:
- Signed integer overflow
- Null pointer dereference
- Misaligned pointer access
- Division by zero
- Shift by negative or too-large amount

Compile flags: `-fsanitize=undefined`

### ThreadSanitizer (TSan)

Detects data races in concurrent code:
- Unsynchronized access to shared memory
- Lock ordering violations
- Race conditions in actor message passing

Compile flags: `-fsanitize=thread`

### Integration

```bash
# Run full sanitizer sweep
scripts/run-sanitizers.sh "address undefined"

# Environment variables
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
TSAN_OPTIONS=halt_on_error=1
```

## Formal Invariant Verification

### Compiler Invariants

| Invariant | Description |
|-----------|-------------|
| Token type range | All token types are within defined range [0, MAX_TOKEN] |
| AST well-formedness | Every AST node has a valid type tag |
| Error code format | All error codes follow E#### pattern (4 digits) |
| String termination | All string constants are null-terminated |
| Scope depth | Scope depth is non-negative and bounded |

### Runtime Invariants

| Invariant | Description |
|-----------|-------------|
| Vec length ≥ 0 | Vector length is never negative |
| Push increments | Vec push increases length by exactly 1 |
| Get-after-push | `vec_get(v, len-1)` after `vec_push(v, x)` returns `x` |
| String length | `string_len` matches actual content length |
| Malloc non-null | `malloc(n)` returns non-zero for n > 0 |
| Additive identity | `a + 0 = a` for all integers a |
| Multiplicative identity | `a * 1 = a` for all integers a |
| Multiplicative zero | `a * 0 = 0` for all integers a |

### Protocol Invariants (Nexus)

| Invariant | Description |
|-----------|-------------|
| Serialization roundtrip | encode(decode(x)) = x for integers |
| Vec serialization | Serialize then deserialize preserves elements |
| Comparison transitivity | If a < b and b < c then a < c |

## CI Workflow

### hardening.yml

Runs on push to main/dev and all PRs:

```yaml
jobs:
  fuzz-regression:     # Run fuzz corpus as regression
  sanitizer-sweep:     # ASan + UBSan matrix
  property-tests:      # Property-based test suite
  formal-invariants:   # Formal invariant checks
```

All jobs must pass for PR to merge.

## Test File Locations

```
tests/
├── fuzz/
│   ├── fuzz_lexer.sx
│   ├── fuzz_parser.sx
│   ├── fuzz_codegen.sx
│   └── fuzz_grammar.sx
├── properties/
│   ├── prop_lexer.sx
│   ├── prop_parser.sx
│   ├── prop_codegen.sx
│   └── prop_types.sx
├── safety/
│   ├── integ_asan.sx
│   ├── integ_ubsan.sx
│   └── integ_tsan.sx
└── formal/
    ├── invariant_compiler.sx
    ├── invariant_runtime.sx
    └── invariant_nexus.sx
```

## Success Criteria

- Zero crashes from fuzz testing (1M+ iterations)
- All property-based tests pass with 1000+ trials
- Zero sanitizer findings on full test suite
- All formal invariants verified
- CI enforces all checks on every PR
- Zero regressions on existing 188 tests
