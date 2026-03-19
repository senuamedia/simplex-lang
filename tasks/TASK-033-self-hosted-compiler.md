# TASK-033: Self-Hosted Compiler Completion

**Version:** 0.15.0
**Status:** Complete
**Priority:** Critical
**Depends on:** v0.14.0 compiler state

## Summary

Close the remaining gaps in the self-hosted Simplex compiler (`compiler/bootstrap/`).
The compiler currently bootstraps via Python stage0 → LLVM IR → native binary. The goal
is to reach full self-hosting where sxc can compile itself without Python assistance.

## Current State

The compiler handles:
- Lexer, parser, codegen, error reporting, main driver
- Structs, enums, traits, impl blocks, generics (partial)
- Functions, closures, match expressions, control flow
- String handling, format strings, arrays
- Module system (use declarations, mod)
- Const declarations, type aliases
- Attributes (#[cfg], #[derive])

### Known Gaps (to audit and close)

### Phase 1: Audit & Gap Analysis (~200 lines)

- Run sxc on its own source files and catalog every parse/codegen failure
- Categorize gaps: syntax not supported, codegen missing, type system limitation
- Prioritize by frequency of occurrence in compiler source

### Phase 2: Parser Completions (~600 lines)

Location: `compiler/bootstrap/parser.sx`

Likely gaps based on compiler source patterns:
- **Associated types** — `type Output = T` in trait impls
- **Trait objects** — `dyn Trait` in type positions
- **Where clauses** — complex bounds on generic functions
- **Lifetime annotations** — `'a` syntax in references
- **Nested generics** — `Vec<Option<T>>` multi-level type params
- **Pattern matching completeness** — nested patterns, guard expressions
- **Const generics** — `[T; N]` where N is a const parameter
- **Slice patterns** — `[first, rest @ ..]`

### Phase 3: Codegen Completions (~800 lines)

Location: `compiler/bootstrap/codegen.sx`

- **Generic monomorphization** — generate specialized code for each type instantiation
- **Trait dispatch** — vtable generation for dynamic dispatch
- **Enum discriminant layout** — proper tagged union representation
- **Closure capture** — environment capture and closure struct generation
- **Drop/destructor calls** — automatic cleanup at scope exit
- **Operator overloading** — emit trait method calls for +, -, *, etc.
- **Iterator protocol** — for-in loop desugaring to iterator trait calls
- **String interpolation codegen** — f"..." format string lowering

### Phase 4: Type System Gaps (~500 lines)

Location: `compiler/bootstrap/` (new file or extend parser/codegen)

- **Type inference improvements** — better inference for closures and iterators
- **Coercion** — automatic &T to &dyn Trait, T to &T where needed
- **Method resolution** — handle impl blocks, trait methods, and auto-deref
- **Recursive types** — Box<Self> and similar self-referential types
- **Exhaustiveness checking** — verify match expressions cover all cases

### Phase 5: Bootstrap Loop (~200 lines)

- **Stage 1**: Compile sxc with Python-bootstrapped sxc (existing)
- **Stage 2**: Compile sxc with Stage 1 sxc
- **Stage 3**: Verify Stage 1 and Stage 2 produce identical output
- Add CI job to verify bootstrap reproducibility

Files:
- `scripts/bootstrap-verify.sh` — three-stage bootstrap verification script

## Tests

- `tests/toolchain/compiler/unit_self_host.sx` — compile small .sx files and verify output
- `tests/toolchain/compiler/integ_bootstrap.sx` — stage 2 bootstrap test
- Extend existing parser/codegen unit tests for new features

## Estimated Scope

~2300 lines across 5 phases

## Success Criteria

- sxc can compile all 5 compiler source files (lexer, parser, error, codegen, main)
- Stage 2 binary produces identical IR to Stage 1 for a test corpus
- Bootstrap verification passes in CI
- Zero regressions on existing 173 tests
