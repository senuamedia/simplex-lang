# Release 0.18.0 — Self-Forging Intelligence

**Codename:** Self-Forging Intelligence
**Status:** Planned
**Theme:** Build the foundational models. Prove the Cognitive Substrate works.
Create a language that improves itself.

## Vision

v0.16.0 gave Simplex advanced mathematical intelligence. v0.17.0 reinvented what an SLM
is with the Cognitive Substrate. v0.18.0 turns the key: **build real models on the
substrate, trained in Simplex, that make Simplex itself better.**

This is not an abstract exercise. The deliverables are concrete:
- A **Core Foundation Model** that understands Simplex as deeply as a native speaker
  understands their language
- **Four Specialist Models** that write code, generate tests, debug errors, and produce
  documentation — all for Simplex
- A **Self-Improvement Loop** where specialists generate verified training data for each
  other, getting measurably better every cycle
- **Toolchain integration** so every Simplex developer benefits immediately: smarter
  completions in sxlsp, better errors in sxc, auto-generated tests and docs

The self-improvement loop is the endgame: the Code Specialist writes Simplex code → the
Test Specialist validates it → the Compiler Specialist catches errors → failures become
training data → next cycle is better. Simplex becomes a language that forges its own
intelligence.

**Tagline:** *A language that teaches itself.*

## Priority-Ordered Task List

### P0 — Critical (Foundation + Flagship + Loop)

The training infrastructure, the core model, the flagship specialist, and the
self-improvement loop. These four are the minimum viable proof.

| Order | Task | Feature | Lines | Why Critical |
|-------|------|---------|-------|-------------|
| 1 | TASK-052 | Training Data Pipeline | ~1,700 | No data = no models. Must be first. |
| 2 | TASK-053 | Model Training Framework | ~1,800 | The training loop using Cognitive Substrate. Validates v0.17.0. |
| 3 | TASK-054 | Core Foundation Model | ~1,600 | The base model. All specialists inherit from this. |
| 4 | TASK-055 | Code Generation Specialist | ~1,650 | Flagship proof: does the substrate produce a good code model? |
| 5 | TASK-059 | Self-Improvement Loop | ~1,550 | The flywheel: specialists improve each other every cycle. |

### P1 — High (Remaining Specialists)

The specialists that complete the hive and enable the full self-improvement loop.

| Order | Task | Feature | Lines | Why Important |
|-------|------|---------|-------|-------------|
| 6 | TASK-056 | Test Generation Specialist | ~1,550 | Validates code specialist output. Feeds the loop. |
| 7 | TASK-057 | Compiler & Debugging Specialist | ~1,450 | Error analysis and fix suggestions. Catches loop failures. |
| 8 | TASK-058 | Documentation Specialist | ~1,400 | Makes the language accessible. Documents loop outputs. |

## Implementation Order & Dependencies

```
Phase 1 (Infrastructure):
  TASK-052 Training Data Pipeline ──────────────────────┐
  TASK-053 Model Training Framework ────────────────────┤
                                                         │
Phase 2 (Core Model):                                   │
  TASK-054 Core Foundation Model ◄──────────────────────┘
                                   │
Phase 3 (Specialists):             │
  TASK-055 Code Generation ◄───────┤
  TASK-056 Test Generation ◄───────┤
  TASK-057 Compiler/Debug ◄────────┤
  TASK-058 Documentation ◄─────────┘
                   │
Phase 4 (Loop):    │
  TASK-059 Self-Improvement Loop ◄──── (needs all specialists)
```

**Key dependencies:**
- TASK-052 and TASK-053 are independent and can be built in parallel
- TASK-054 depends on both TASK-052 (data) and TASK-053 (training framework)
- All specialists (TASK-055 through TASK-058) depend on TASK-054 (core model)
- Specialists can be built in parallel after the core model exists
- TASK-059 requires at minimum TASK-055 and TASK-056 to be functional; benefits from
  all four specialists

## Total Estimated Scope

| Category | Lines |
|----------|-------|
| Infrastructure (P0) | ~3,500 |
| Core + Code Specialist (P0) | ~3,250 |
| Self-Improvement Loop (P0) | ~1,550 |
| Remaining Specialists (P1) | ~4,400 |
| **Total** | **~12,700** |

## The Self-Improvement Loop

```
    ┌──────────────────────────────────────────────┐
    │              Prompt Generator                  │
    │  (feature combinator, weakness targeter)       │
    └──────────────┬───────────────────────────────┘
                   │ prompts
                   ▼
    ┌──────────────────────────────────────────────┐
    │           Code Specialist                      │
    │  (generates Simplex code)                      │
    └──────────────┬───────────────────────────────┘
                   │ generated code
                   ▼
    ┌──────────────────────────────────────────────┐
    │           Compiler                             │
    │  (compile check — free binary label)           │
    └────────┬─────────────────┬───────────────────┘
             │ compiles         │ error
             ▼                  ▼
    ┌────────────────┐  ┌──────────────────────────┐
    │ Test Specialist │  │ Compiler Specialist       │
    │ (generate tests)│  │ (suggest fix)             │
    └───────┬────────┘  └──────────┬───────────────┘
            │ tests                 │ fix
            ▼                       ▼
    ┌────────────────┐  ┌──────────────────────────┐
    │ Run Tests       │  │ Re-compile               │
    │ + Mutation Test  │  │ (validate fix worked)    │
    └───────┬────────┘  └──────────┬───────────────┘
            │ results               │ results
            ▼                       ▼
    ┌──────────────────────────────────────────────┐
    │         Training Pair Collector                │
    │  (verified (input, output, label) triples)     │
    └──────────────┬───────────────────────────────┘
                   │ filtered pairs
                   ▼
    ┌──────────────────────────────────────────────┐
    │         Quality Filter & Dedup                 │
    └──────────────┬───────────────────────────────┘
                   │ high-quality pairs
                   ▼
    ┌──────────────────────────────────────────────┐
    │         Incremental Retrainer                  │
    │  (retrain specialists with EWC/SDM)            │
    └──────────────┬───────────────────────────────┘
                   │ improved specialists
                   ▼
              Next Cycle
```

Every arrow produces training data. The compiler's pass/fail is a free label. The test
runner's pass/fail is a free label. Mutation testing scores are free labels. The loop
produces verified training data without human annotation.

## Model Sizes

| Model | Parameters | Purpose | Hardware Target |
|-------|-----------|---------|----------------|
| simplex-core-micro | 50M | Development/testing | Any CPU |
| simplex-core-base | 200M | Standard development | 8GB RAM |
| simplex-core-large | 500M | Maximum quality | 16GB RAM / GPU |

Each specialist adds ~10-30M parameters via LoRA adapters on top of the core model.
Total ecosystem: ~250M (base) to ~650M (large) for the complete specialist hive.

## What This Means for Simplex Users

After v0.18.0:

1. **sxlsp** provides intelligent completions that understand neural gates, contracts,
   dual numbers, and every Simplex-specific feature — not generic code suggestions
2. **sxc** provides contextual error explanations with fix suggestions that actually
   compile when applied
3. **Running `sxpm test-gen`** generates a complete test suite for any Simplex file
4. **Running `sxpm doc-gen`** generates accurate documentation for any Simplex module
5. The specialist models improve every month through the self-improvement loop,
   without requiring developer action

## Validation Criteria

v0.18.0 succeeds if:

- [ ] simplex-core-base (200M) outperforms a 3B general code model on Simplex-specific
  benchmarks (CompilationBench, TestPassBench, SpecAdherenceBench)
- [ ] The self-improvement loop produces measurable quality improvements over 5+ cycles
- [ ] >70% of Code Specialist completions compile on first attempt
- [ ] >60% of Test Specialist tests catch planted bugs via mutation testing
- [ ] >60% of Compiler Specialist fix suggestions produce compiling code
- [ ] All models train end-to-end in pure Simplex (no Python, no PyTorch)

If these criteria are met, the Cognitive Substrate is validated, and Simplex becomes the
first programming language with a self-improving AI ecosystem built entirely in itself.
