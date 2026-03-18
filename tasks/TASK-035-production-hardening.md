# TASK-035: Production Hardening

**Version:** 0.15.0
**Status:** Complete
**Priority:** Critical
**Depends on:** v0.14.0 release

## Summary

Harden the Simplex compiler and runtime for production use through comprehensive
fuzzing, property-based testing, and formal verification of critical invariants.
Goal: confidence that sxc handles malformed input safely and the runtime has no
undefined behavior.

## Deliverables

### Phase 1: Compiler Fuzzing (~600 lines)

Location: `tests/fuzz/`

- **Lexer fuzzing** — random byte streams to lexer, verify no crashes
- **Parser fuzzing** — random token streams to parser, verify no crashes
- **Codegen fuzzing** — valid ASTs with edge cases, verify valid IR output
- **Grammar-aware fuzzing** — generate syntactically plausible .sx files
- **Crash triage** — deduplicate and minimize crash inputs
- **CI integration** — run fuzz targets for N minutes per PR

```simplex
// Fuzz harness structure
fn fuzz_lexer(input: &[u8]) {
    let source = String::from_utf8_lossy(input);
    // Must not panic, must not segfault
    let _ = Lexer::new(&source).tokenize();
}
```

Files:
- `tests/fuzz/fuzz_lexer.sx` — lexer fuzz target
- `tests/fuzz/fuzz_parser.sx` — parser fuzz target
- `tests/fuzz/fuzz_codegen.sx` — codegen fuzz target
- `tests/fuzz/fuzz_grammar.sx` — grammar-aware generator
- `tests/fuzz/corpus/` — seed corpus of valid .sx files
- `scripts/run-fuzz.sh` — fuzz runner script

### Phase 2: Property-Based Testing (~800 lines)

Location: `tests/properties/`

- **Lexer properties**
  - Tokenize then concatenate tokens reproduces input (modulo whitespace)
  - Every token has a valid span within the source
  - Lexer never produces overlapping spans
- **Parser properties**
  - Parse then pretty-print then re-parse produces identical AST
  - Every AST node has a valid source span
  - Parser accepts all files in the test corpus without error
- **Codegen properties**
  - Constant folding produces same result as runtime evaluation
  - Dead code elimination preserves observable behavior
  - Generated IR passes LLVM verifier
- **Type system properties**
  - Well-typed programs don't produce type errors
  - Type inference is deterministic (same input → same types)
  - Generics monomorphize to valid specialized code

```simplex
use simplex_std::test::{property, Gen};

#[property(trials = 1000)]
fn lexer_roundtrip(source: Gen<SimplexSource>) {
    let tokens = Lexer::new(&source).tokenize();
    for token in &tokens {
        assert!(token.span.start <= token.span.end);
        assert!(token.span.end <= source.len());
    }
}
```

Files:
- `tests/properties/prop_lexer.sx` — lexer property tests
- `tests/properties/prop_parser.sx` — parser property tests
- `tests/properties/prop_codegen.sx` — codegen property tests
- `tests/properties/prop_types.sx` — type system property tests
- `tests/properties/generators.sx` — random Simplex source generators

### Phase 3: Runtime Safety Verification (~600 lines)

Location: `tests/safety/`

- **Memory safety** — verify no buffer overflows in runtime C code
  - Run all tests under AddressSanitizer
  - Run all tests under MemorySanitizer
  - Run all tests under UndefinedBehaviorSanitizer
- **Concurrency safety** — verify no data races
  - Run actor/async tests under ThreadSanitizer
- **Resource leak detection** — verify no file descriptor or memory leaks
  - Valgrind sweep on test suite
- **Stack overflow protection** — verify stack depth limits are enforced
- **Integer overflow** — verify arithmetic operations handle overflow correctly

Files:
- `tests/safety/integ_asan.sx` — AddressSanitizer integration test
- `tests/safety/integ_ubsan.sx` — UBSan integration test
- `tests/safety/integ_tsan.sx` — ThreadSanitizer integration test
- `scripts/run-sanitizers.sh` — sanitizer sweep script
- `.github/workflows/runtime-safety.yml` — extend existing CI workflow

### Phase 4: Formal Verification of Invariants (~500 lines)

Location: `tests/formal/`

- **Compiler invariants**
  - Parser produces well-formed AST (all nodes have valid types)
  - Codegen emits valid LLVM IR for every AST pattern
  - Token stream is contiguous (no gaps or overlaps in source positions)
- **Runtime invariants**
  - Stack never exceeds CVM_STACK_SIZE (1024 slots)
  - String operations maintain null-termination
  - Reference counts never go negative
  - Arena allocator never returns overlapping regions
- **Protocol invariants** (simplex-nexus)
  - Message serialization round-trips correctly
  - Belief delta encoding preserves values within epsilon
  - Vector clocks maintain causal ordering

```simplex
// Formal invariant: stack depth never exceeds limit
#[invariant]
fn stack_bounded(vm: &CVM) -> bool {
    vm.stack_pointer >= 0 && vm.stack_pointer < CVM_STACK_SIZE
}
```

Files:
- `tests/formal/invariant_compiler.sx` — compiler invariant checks
- `tests/formal/invariant_runtime.sx` — runtime invariant checks
- `tests/formal/invariant_nexus.sx` — protocol invariant checks

### Phase 5: CI Hardening (~200 lines)

- **Fuzz regression** — run fuzz corpus as regression tests in every CI run
- **Sanitizer gate** — block merge if any sanitizer reports an issue
- **Coverage tracking** — measure and report test coverage percentage
- **Benchmark gate** — warn if compilation time regresses >10%
- **Reproducibility check** — verify deterministic compilation output

Files:
- `.github/workflows/hardening.yml` — new CI workflow for hardening checks

## Tests

All deliverables above ARE tests — this task produces ~50+ new test files.

## Estimated Scope

~2700 lines across 5 phases, ~50 new test/verification files

## Success Criteria

- Fuzzer runs 1M iterations with zero crashes on lexer/parser/codegen
- All property-based tests pass with 1000+ trials each
- Zero sanitizer findings (ASan, UBSan, TSan) on full test suite
- All formal invariants verified
- CI enforces all hardening checks on every PR
- Zero regressions on existing tests
