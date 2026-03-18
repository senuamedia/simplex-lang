# TASK-058: Documentation & Explanation Specialist

**Version:** 0.18.0
**Status:** Planned
**Priority:** P1 — High
**Depends on:** Core foundation model (TASK-054)

## Why This Feature Is Needed

Documentation is the interface between a language and its community. Simplex is a complex
language with unique features — neural gates, cognitive hives, dual numbers, belief
systems, conformal prediction, causal inference — that no developer has seen before.
Without excellent documentation, these features are inaccessible.

The Documentation Specialist generates documentation, tutorials, and explanations that
help developers understand and use Simplex features. It is trained on the existing docs,
tutorials, and spec, understanding both the features themselves and how to explain them
clearly.

## Why It Adds Value

1. **Always-current docs.** When code changes, the specialist regenerates affected
   documentation. No more stale docs.

2. **Contextual explanations.** Hover over a neural gate in sxlsp and get an explanation
   tailored to the specific gate's contracts, not a generic neural gate description.

3. **Tutorial generation.** Given a feature area, generate a step-by-step tutorial with
   working code examples.

4. **Cross-referencing.** The specialist understands how features relate and can explain
   connections: "This neural gate uses dual numbers (v0.8.0) for gradient computation
   through the Gumbel-Softmax (v0.6.0) during training mode."

## Deliverables

### Phase 1: Documentation Generation (~450 lines)

- **DocGenerator** — generate documentation for functions, structs, traits, and modules
  from source code
- **TutorialGenerator** — generate step-by-step tutorials for feature areas
- **ExampleGenerator** — generate working code examples for API documentation
- **ChangelogGenerator** — generate changelog entries from code diffs

Files:
- `simplex-training/src/specialists/docs/generator.sx`
- `simplex-training/src/specialists/docs/tutorial.sx`
- `simplex-training/src/specialists/docs/examples.sx`
- `simplex-training/src/specialists/docs/changelog.sx`

### Phase 2: Explanation Engine (~400 lines)

- **ConceptExplainer** — explain Simplex concepts at different expertise levels
- **CodeNarrator** — given a code block, generate a line-by-line explanation
- **FeatureLinker** — identify and explain connections between features
- **FAQGenerator** — anticipate and answer common questions about code patterns

Files:
- `simplex-training/src/specialists/docs/concept.sx`
- `simplex-training/src/specialists/docs/narrator.sx`
- `simplex-training/src/specialists/docs/linker.sx`
- `simplex-training/src/specialists/docs/faq.sx`

### Phase 3: Toolchain Integration (~300 lines)

- **sxdoc enhancement** — auto-generate docs for undocumented functions
- **sxlsp hover** — contextual explanations on hover
- **Interactive tutorial mode** — guided learning within the IDE

Files:
- `simplex-training/src/specialists/docs/sxdoc.sx`
- `simplex-training/src/specialists/docs/hover.sx`
- `simplex-training/src/specialists/docs/interactive.sx`

### Phase 4: Tests (~250 lines)

- Generated docs contain all function parameters and return types
- Generated examples compile and run
- Concept explanations are factually consistent with the language spec
- Tutorial steps produce working code at each stage

## Success Criteria

- [ ] Generated docs cover all public API functions with accurate type signatures
- [ ] >90% of generated code examples compile
- [ ] Concept explanations pass fact-checking against language spec
- [ ] Tutorial steps produce progressively building, working programs
- [ ] <300ms hover explanation latency in sxlsp

## Estimated Scope

~1,400 lines across library code and tests.
