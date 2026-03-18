# TASK-057: Compiler & Debugging Specialist

**Version:** 0.18.0
**Status:** Planned
**Priority:** P1 — High
**Depends on:** Core foundation model (TASK-054)

## Why This Feature Is Needed

Compiler errors are the #1 friction point for any programming language. Simplex has 34
documented error codes, but the explanations are static text. A specialist model trained
on the compiler's internals — the lexer, parser, codegen pipeline, and the patterns of
errors developers actually hit — can provide contextual, intelligent error explanations
and fix suggestions that static text never can.

This specialist understands *why* errors happen, not just *what* they are. When a user
gets a type mismatch in a neural gate's contract, the specialist explains the specific
contract constraint being violated, why the types don't align, and suggests the minimal
code change to fix it — with awareness of the surrounding code context.

It also serves as a debugging assistant: given unexpected behavior, it can trace through
the compilation pipeline (lexing → parsing → type checking → codegen) to identify where
the user's intent diverges from the compiler's interpretation.

## Why It Adds Value

1. **Intelligent error recovery.** Not "expected type X, got Y" but "your neural gate's
   requires clause expects a Tensor, but you're passing the raw f64 output of the dual
   number computation — wrap it with Tensor::from_scalar()."

2. **Compiler internals knowledge.** Trained on the actual compiler source
   (compiler/bootstrap/), the specialist understands codegen patterns, LLVM IR
   generation, and runtime FFI boundaries. It can explain why generated code fails at
   the LLVM level.

3. **Pattern-based fix suggestions.** Trained on git history of bug fixes, the specialist
   recognizes common error patterns and their fixes. It has seen how hundreds of similar
   errors were resolved.

4. **Debugging trace.** Given code that compiles but produces wrong output, the
   specialist can trace through the expected compilation pipeline and identify likely
   divergence points.

## Deliverables

### Phase 1: Error Analysis (~400 lines)

- **ErrorAnalyzer** — given a compiler error and source context, produce a structured
  analysis: root cause, affected code region, related language features
- **FixSuggester** — suggest ranked code fixes based on error analysis and git history
  patterns
- **ErrorExplainer** — generate natural language explanations of errors tailored to the
  user's apparent skill level (based on code complexity)

Files:
- `simplex-training/src/specialists/compiler/analyzer.sx`
- `simplex-training/src/specialists/compiler/fixer.sx`
- `simplex-training/src/specialists/compiler/explainer.sx`

### Phase 2: Debugging Assistant (~400 lines)

- **CompilationTracer** — trace source code through each compiler phase and identify
  where intent diverges from behavior
- **IRExplainer** — explain generated LLVM IR in terms of the original Simplex source
- **RuntimeDebugger** — given a runtime error (segfault, wrong output), trace back to
  the likely source-level cause using knowledge of the C runtime

Files:
- `simplex-training/src/specialists/compiler/tracer.sx`
- `simplex-training/src/specialists/compiler/ir_explain.sx`
- `simplex-training/src/specialists/compiler/runtime_debug.sx`

### Phase 3: Integration (~350 lines)

- **sxc integration** — enhanced error output with model-generated explanations
- **sxlsp diagnostics** — real-time error analysis as the user types
- **REPL mode** — interactive debugging conversation with the compiler specialist

Files:
- `simplex-training/src/specialists/compiler/sxc_integration.sx`
- `simplex-training/src/specialists/compiler/lsp_diagnostics.sx`
- `simplex-training/src/specialists/compiler/repl.sx`

### Phase 4: Tests (~300 lines)

- Error analyzer correctly identifies root cause for 20 common error types
- Fix suggestions compile when applied for >60% of cases
- IR explainer maps LLVM IR back to correct source locations
- Compilation tracer identifies correct divergence phase for planted errors

## Success Criteria

- [ ] Error explanations rated more helpful than static messages in blind comparison
- [ ] >60% of fix suggestions produce compiling code when applied
- [ ] Compilation tracer correctly identifies the phase where errors originate >80%
- [ ] IR explainer correctly maps IR instructions to source lines >70%
- [ ] <500ms response time for error analysis in sxlsp

## Estimated Scope

~1,450 lines across library code and tests.
