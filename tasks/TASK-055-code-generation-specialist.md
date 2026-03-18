# TASK-055: Code Generation Specialist

**Version:** 0.18.0
**Status:** Planned
**Priority:** P0 — Critical
**Depends on:** Core foundation model (TASK-054)

## Why This Feature Is Needed

The Code Generation Specialist is the flagship proof that the Cognitive Substrate works.
It takes the core foundation model and fine-tunes it for one purpose: writing correct,
idiomatic Simplex code. This specialist is integrated directly into sxlsp, providing
intelligent code completion, function generation, and refactoring suggestions that
understand Simplex at a level no general-purpose code model can match.

General code models (Copilot, Cursor, Codeium) know Simplex as "some language they
maybe saw a few files of." They cannot generate neural gates, cognitive hive
configurations, dual number operations, or contract annotations — because these concepts
exist only in Simplex.

The Code Generation Specialist knows *nothing but* Simplex. Every parameter is dedicated
to understanding Simplex syntax, semantics, patterns, and idioms. It is the most capable
Simplex code generator possible at its parameter count — because it wastes zero capacity
on Python, JavaScript, or any other language.

## Why It Adds Value

1. **First-class Simplex intelligence.** The specialist understands neural gates,
   contracts, cognitive hive patterns, dual numbers, belief systems — all the features
   that make Simplex unique. No general model can match this.

2. **Integrated into the toolchain.** Not a separate service — integrated directly into
   sxlsp for real-time code completion, into sxc for fix suggestions, and into sxpm
   for scaffolding new projects.

3. **Correctness through hybrid reasoning.** Uses the neuro-symbolic engine to verify
   generated code against the Simplex spec before presenting it. Suggestions that
   violate the type system or contract logic are filtered out.

4. **Learns from usage.** Via SDM (TASK-050), the specialist accumulates knowledge from
   each coding session. Patterns the user writes frequently become patterns the
   specialist suggests. Personal adaptation without retraining.

5. **Proof by construction.** If this specialist — a 200M Cognitive Substrate model —
   outperforms a 3B general code model on Simplex tasks, the architecture is validated.

## Deliverables

### Phase 1: Fine-Tuning Pipeline (~400 lines)

- **CodeSpecialistTrainer** — fine-tune the core model for code generation tasks:
  completion, generation from description, refactoring, and fill-in-the-middle
- **CodeSpecialistConfig** — specialist-specific configuration: completion window size,
  generation temperature, max output length
- **SpecialistAdapter** — LoRA adapters on the core model for parameter-efficient
  specialist creation (don't copy all core weights)

Files:
- `simplex-training/src/specialists/code/trainer.sx`
- `simplex-training/src/specialists/code/config.sx`
- `simplex-training/src/specialists/code/adapter.sx`

### Phase 2: Generation Modes (~500 lines)

- **CodeCompletion** — given a cursor position in a file, generate the most likely
  continuation; context-aware using open file, imports, and project structure
- **FunctionGeneration** — given a function signature and/or docstring, generate the
  implementation
- **FillInTheMiddle** — given prefix and suffix, generate the middle (using diffusion
  generation from TASK-051 where appropriate)
- **Refactoring** — given code and a refactoring instruction, generate the refactored
  version
- **ContractInference** — given a function body, infer appropriate requires/ensures
  contracts
- **VerifiedGeneration** — generate code, then verify via symbolic engine that it
  satisfies type constraints and contracts before presenting

```simplex
/// Code generation specialist
struct CodeSpecialist {
    core: CoreModel,          // base understanding
    adapter: LoRAAdapter,     // specialist fine-tuning
    verifier: SymbolicVerifier, // check generated code
    sdm: NeuralSDM,           // personal learning from usage
}

impl CodeSpecialist {
    /// Complete code at cursor position
    fn complete(self: &Self, context: &CodeContext) -> Vec<Completion> {
        let input = context.to_prompt();

        // Retrieve relevant patterns from personal memory
        let memory_context = self.sdm.forward(&self.encode(&input));

        // Generate candidates
        let candidates = self.generate_candidates(&input, &memory_context, 5);

        // Verify each candidate
        candidates.into_iter()
            .filter(|c| self.verifier.check_syntax(c))
            .filter(|c| self.verifier.check_types(c, &context))
            .map(|c| Completion {
                text: c,
                confidence: self.score(&c, &context),
            })
            .sorted_by_confidence()
            .take(3)
            .collect()
    }

    /// Generate function from signature
    fn generate_function(self: &Self, signature: &str, docstring: Option<&str>) -> String {
        let prompt = match docstring {
            Some(doc) => format!("/// {}\n{}", doc, signature),
            None => signature.to_string(),
        };

        let generated = self.generate(&prompt);

        // Verify contracts are satisfiable
        if let Some(contracts) = extract_contracts(&generated) {
            if !self.verifier.check_contracts_satisfiable(&contracts) {
                // Regenerate with contract guidance
                return self.generate_with_constraint(&prompt, &contracts);
            }
        }

        generated
    }

    /// Learn from user's coding session
    fn learn_from_session(mut self: &mut Self, edits: &[Edit]) {
        for edit in edits {
            // Store patterns the user writes into personal SDM
            let address = self.encode(&edit.context);
            let pattern = self.encode(&edit.new_code);
            self.sdm.learn(&address, &pattern);
        }
    }
}
```

Files:
- `simplex-training/src/specialists/code/completion.sx`
- `simplex-training/src/specialists/code/generation.sx`
- `simplex-training/src/specialists/code/fill_middle.sx`
- `simplex-training/src/specialists/code/refactor.sx`
- `simplex-training/src/specialists/code/contracts.sx`
- `simplex-training/src/specialists/code/verified.sx`

### Phase 3: Toolchain Integration (~400 lines)

- **LSPCodeProvider** — integration with sxlsp for real-time completions
- **CompilerSuggestions** — integration with sxc for error fix suggestions
- **ProjectScaffold** — integration with sxpm for generating new project boilerplate

Files:
- `simplex-training/src/specialists/code/lsp.sx`
- `simplex-training/src/specialists/code/compiler.sx`
- `simplex-training/src/specialists/code/scaffold.sx`

### Phase 4: Tests & Benchmarks (~350 lines)

- Code completion accuracy on held-out Simplex files
- Function generation compilation rate
- Refactoring preserves behavior (before/after produce same test results)
- Verified generation rejects syntactically or type-invalid suggestions
- SDM learning improves completion accuracy over simulated coding session
- Comparison: CodeSpecialist vs general code model on Simplex tasks

## Success Criteria

- [ ] >70% of completions compile on first attempt
- [ ] >50% of generated functions pass associated test suites
- [ ] Verified generation filters out >95% of type-invalid suggestions
- [ ] SDM learning improves completion accuracy by >10% over 100-edit session
- [ ] Outperforms 3B general code model on Simplex-specific benchmark
- [ ] Integrated into sxlsp with <200ms completion latency

## Estimated Scope

~1,650 lines across library code and tests.
