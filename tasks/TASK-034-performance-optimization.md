# TASK-034: Performance Optimization

**Version:** 0.15.0
**Status:** Complete
**Priority:** High
**Depends on:** v0.14.0 compiler state

## Summary

Improve compiler performance and generated code quality through optimization passes,
inlining heuristics, and compile-time improvements. Target: 2x faster compilation,
measurably better runtime performance on benchmarks.

## Deliverables

### Phase 1: Compiler Performance Profiling (~100 lines)

- Profile sxc compilation of large files (combined.sx, simplex-std)
- Identify hotspots: lexer, parser, codegen, IR emission, string handling
- Establish baseline metrics for compilation time and memory usage
- Create benchmark suite in `simplex-core/src/bench.sx`

### Phase 2: Optimization Passes in Codegen (~800 lines)

Location: `compiler/bootstrap/codegen.sx`

- **Constant folding** — evaluate constant expressions at compile time (extend existing)
- **Dead code elimination** — remove unreachable blocks after branches
- **Common subexpression elimination** — reuse identical computations
- **Strength reduction** — replace expensive ops (multiply → shift for powers of 2)
- **Loop-invariant code motion** — hoist invariant computations out of loops
- **Tail call optimization** — convert tail-recursive calls to loops

```simplex
// Before optimization:
let x = 2 * 8;       // constant fold → 16
let y = x + 0;       // identity elimination → x
let z = a * 2;       // strength reduction → a << 1

// After optimization:
let x = 16;
let y = x;
let z = a << 1;
```

### Phase 3: Inlining Heuristics (~500 lines)

Location: `compiler/bootstrap/codegen.sx`

- **Small function inlining** — inline functions under a size threshold
- **Call-site analysis** — consider call frequency and context for inline decisions
- **Recursive inlining limit** — prevent infinite inlining of recursive functions
- **#[inline] attribute** — respect explicit inline hints
- **#[inline(never)] attribute** — respect explicit no-inline hints
- **Cross-module inlining** — inline functions from imported modules when IR available

Heuristic factors:
- Function body size (instruction count)
- Number of call sites
- Whether function is called in a loop
- Whether inlining enables further constant folding

### Phase 4: Compile-Time Improvements (~600 lines)

- **Incremental compilation** — skip recompiling unchanged modules
  - File hash tracking in `simplex-core/src/incremental.sh` (extend existing)
  - Module dependency graph for minimal recompilation
- **Parallel IR emission** — emit LLVM IR for independent functions concurrently
- **String interning** — deduplicate identical string constants across modules
- **Symbol table optimization** — faster lookup with hash-based symbol tables
- **Lazy parsing** — parse function bodies only when needed (for IDE/LSP)

### Phase 5: Runtime Code Quality (~500 lines)

- **Register allocation hints** — emit LLVM IR that helps the backend allocator
- **Alignment annotations** — proper alignment for struct fields and arrays
- **Alias analysis hints** — noalias annotations for function parameters
- **SIMD hints** — emit vector-friendly loops for auto-vectorization
- **Branch weight hints** — annotate likely/unlikely branches for better prediction

## Tests

- `tests/toolchain/compiler/unit_optimization.sx` — verify optimization correctness
- `tests/toolchain/compiler/unit_inlining.sx` — verify inline decisions
- `simplex-core/src/bench.sx` — compilation benchmark suite
- Extend `tests/toolchain/codegen/unit_codegen.sx` for new optimizations

## Metrics

Track before/after for:
- Compilation time (sxc on combined.sx)
- Binary size (hello world, full programs)
- Runtime performance (fibonacci, matrix multiply, string processing)
- Memory usage during compilation

## Estimated Scope

~2500 lines across 5 phases

## Success Criteria

- Compilation 2x faster on combined.sx vs v0.14.0 baseline
- Generated code 20%+ faster on benchmark suite
- Constant folding eliminates 90%+ of compile-time-known expressions
- Inlining reduces call overhead for small functions
- Zero regressions on existing tests
