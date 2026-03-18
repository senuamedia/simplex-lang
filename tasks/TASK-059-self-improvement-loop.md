# TASK-059: Self-Improvement Loop

**Version:** 0.18.0
**Status:** Planned
**Priority:** P0 — Critical
**Depends on:** All other v0.18.0 specialists (TASK-054 through TASK-058)

## Why This Feature Is Needed

The individual specialists — code generation, test generation, compiler analysis,
documentation — are valuable on their own. But the breakthrough is when they work
together in a closed loop that makes each one better over time.

The Self-Improvement Loop is the orchestration system that:
1. **Code Specialist** generates new Simplex code
2. **Test Specialist** generates tests for that code
3. **Compiler Specialist** catches errors and suggests fixes
4. **Documentation Specialist** explains the results
5. Successes and failures feed back as training data for the next cycle

This is not recursive self-improvement in the AGI sense — it is a structured training
data generation pipeline where specialists produce labeled data for each other. The Code
Specialist generates code; the compiler tells us if it's correct (free label). The Test
Specialist generates tests; mutation testing tells us if they're good (free label). Each
cycle produces verified training pairs that make the next cycle better.

The loop is bounded, auditable, and human-supervised. It is engineering, not autonomy.

## Why It Adds Value

1. **Exponential training data.** The initial training data comes from the existing
   Simplex codebase (~5,000 records). The self-improvement loop generates new training
   data from the models themselves, validated by the compiler and test runner. Each
   cycle can produce thousands of additional verified training pairs.

2. **Cross-specialist learning.** When the Test Specialist catches a bug in code the
   Code Specialist generated, both learn: the Code Specialist learns not to generate
   that pattern, the Test Specialist learns that this test catches real bugs.

3. **Quality ratchet.** Each cycle's training data is verified (compiled, tested,
   explained). Low-quality outputs are filtered. The dataset quality can only improve
   over cycles.

4. **Self-play for Simplex.** Inspired by AlphaGo's self-play and AlphaEvolve's
   evolutionary approach, the specialists play different roles (proposer, verifier,
   improver) to generate training data that teaches all of them.

## Why It Changes Systems Built With Simplex

The self-improvement loop means the Simplex language ecosystem gets measurably better
with every training cycle, without human intervention in the loop:

- Each cycle, the Code Specialist generates slightly better code
- Each cycle, the Test Specialist catches slightly more bugs
- Each cycle, the Compiler Specialist explains errors slightly better
- The models improve the language tools → better tools help users write better code →
  better code becomes training data → better models

## Deliverables

### Phase 1: Improvement Cycle Orchestration (~500 lines)

Location: `simplex-training/src/loop/`

- **ImprovementCycle** — orchestrate one cycle of the self-improvement loop: generate
  code → test → validate → collect training pairs → filter quality → retrain
- **TrainingPairCollector** — collect verified (input, output, label) triples from
  specialist interactions
- **QualityFilter** — filter training pairs by compilation success, test pass rate,
  and novelty (don't add duplicates of existing training data)
- **CycleMetrics** — track improvement across cycles: compilation rate, test quality,
  explanation accuracy

```simplex
/// One cycle of the self-improvement loop
struct ImprovementCycle {
    code_specialist: CodeSpecialist,
    test_specialist: TestGenerator,
    compiler_specialist: CompilerSpecialist,
    doc_specialist: DocSpecialist,
    quality_filter: QualityFilter,
    collector: TrainingPairCollector,
    metrics: CycleMetrics,
}

impl ImprovementCycle {
    fn run_cycle(mut self: &mut Self, prompts: &[String]) -> CycleResult {
        let mut new_pairs = Vec::new();

        for prompt in prompts {
            // Step 1: Code Specialist generates code
            let generated_code = self.code_specialist.generate_function(prompt, None);

            // Step 2: Compiler validates
            let compile_result = compile_check(&generated_code);

            match compile_result {
                Ok(()) => {
                    // Code compiles — positive training pair for code specialist
                    new_pairs.push(TrainingPair::code_success(prompt, &generated_code));

                    // Step 3: Test Specialist generates tests
                    let tests = self.test_specialist.generate_unit_tests(
                        &parse_function(&generated_code));

                    // Step 4: Run tests — free quality labels
                    let test_results = run_tests(&tests, &generated_code);
                    for (test, passed) in tests.iter().zip(test_results.iter()) {
                        if *passed {
                            new_pairs.push(TrainingPair::test_success(
                                &generated_code, test));
                        }
                    }

                    // Step 5: Mutation testing for test quality
                    let mutations = mutate(&generated_code);
                    for (mutation, test) in mutations.iter().zip(tests.iter()) {
                        let caught = !run_test(test, mutation);
                        if caught {
                            new_pairs.push(TrainingPair::mutation_caught(
                                &generated_code, mutation, test));
                        }
                    }
                },
                Err(error) => {
                    // Code doesn't compile — learning opportunity
                    let fix = self.compiler_specialist.suggest_fix(&generated_code, &error);
                    if let Some(fixed_code) = fix {
                        if compile_check(&fixed_code).is_ok() {
                            // Fix worked — training pair for both specialists
                            new_pairs.push(TrainingPair::code_fix(
                                &generated_code, &error, &fixed_code));
                            new_pairs.push(TrainingPair::error_fix(
                                &error, &generated_code, &fixed_code));
                        }
                    }
                },
            }

            // Step 6: Documentation
            let doc = self.doc_specialist.generate_doc(&generated_code);
            if self.quality_filter.doc_quality(&doc) > 0.7 {
                new_pairs.push(TrainingPair::documentation(&generated_code, &doc));
            }
        }

        // Filter for quality and novelty
        let filtered = self.quality_filter.filter(new_pairs);

        // Track metrics
        self.metrics.record_cycle(&filtered);

        CycleResult {
            new_training_pairs: filtered,
            compilation_rate: self.metrics.compilation_rate(),
            test_quality: self.metrics.test_quality(),
            cycle_number: self.metrics.cycle_count(),
        }
    }
}
```

Files:
- `simplex-training/src/loop/cycle.sx` — ImprovementCycle
- `simplex-training/src/loop/collector.sx` — TrainingPairCollector
- `simplex-training/src/loop/filter.sx` — QualityFilter
- `simplex-training/src/loop/metrics.sx` — CycleMetrics

### Phase 2: Retraining & Convergence (~400 lines)

- **IncrementalRetrainer** — retrain specialists on accumulated new training pairs
  without forgetting original training (using EWC from v0.16.0 or SDM from v0.17.0)
- **ConvergenceDetector** — detect when improvement cycles are no longer producing
  meaningful gains (diminishing returns)
- **DifficultyEscalation** — automatically increase prompt difficulty as models improve
  (curriculum learning for self-play)
- **HumanReviewQueue** — flag uncertain or surprising results for human review before
  incorporating into training data

```simplex
/// Retrain on accumulated self-play data
struct IncrementalRetrainer {
    trainer: SubstrateTrainer,
    original_fisher: FisherMatrix,  // for EWC: don't forget original training
}

impl IncrementalRetrainer {
    fn retrain(self: &Self, model: &mut CoreModel,
               new_data: &[TrainingPair], cycle: usize) {
        // Mix new data with replay of original data (prevent forgetting)
        let mixed_dataset = Dataset::mix(
            new_data,
            self.original_replay_sample(),
            ratio: 0.7,  // 70% new, 30% replay
        );

        // Retrain with EWC regularization
        self.trainer.train_with_ewc(model, &mixed_dataset, &self.original_fisher);
    }
}

/// Detect when to stop cycling
struct ConvergenceDetector {
    history: Vec<CycleMetrics>,
    patience: usize,
    min_improvement: f64,
}

impl ConvergenceDetector {
    fn should_stop(self: &Self) -> bool {
        if self.history.len() < self.patience { return false; }
        let recent = &self.history[self.history.len() - self.patience..];
        let improvement = recent.last().unwrap().compilation_rate
            - recent.first().unwrap().compilation_rate;
        improvement < self.min_improvement
    }
}
```

Files:
- `simplex-training/src/loop/retrain.sx` — IncrementalRetrainer
- `simplex-training/src/loop/convergence.sx` — ConvergenceDetector
- `simplex-training/src/loop/difficulty.sx` — DifficultyEscalation
- `simplex-training/src/loop/review.sx` — HumanReviewQueue

### Phase 3: Prompt Generation & Diversity (~350 lines)

- **PromptGenerator** — generate diverse coding prompts from the language spec, API
  docs, tutorial examples, and feature combinations
- **FeatureCombinator** — systematically combine Simplex features into novel prompts
  (neural gate + dual number + pattern matching = novel test case)
- **WeaknessTargeter** — analyze evaluation results to identify model weaknesses and
  generate targeted prompts for those areas
- **DiversityScorer** — ensure prompt diversity across feature areas, difficulty levels,
  and code patterns

Files:
- `simplex-training/src/loop/prompts.sx` — PromptGenerator
- `simplex-training/src/loop/combinator.sx` — FeatureCombinator
- `simplex-training/src/loop/weakness.sx` — WeaknessTargeter
- `simplex-training/src/loop/diversity.sx` — DiversityScorer

### Phase 4: Tests (~300 lines)

- One improvement cycle produces >0 valid training pairs
- Quality filter rejects obviously bad pairs (non-compiling, duplicate)
- Incremental retraining doesn't degrade original benchmark scores by >5%
- Convergence detector correctly identifies plateau on synthetic metrics
- Difficulty escalation increases prompt complexity over cycles
- Metrics correctly track improvement across cycles

## Success Criteria

- [ ] Each cycle produces >100 verified training pairs
- [ ] Compilation rate improves by >5% over 5 cycles
- [ ] Test quality (mutation kill rate) improves over cycles
- [ ] Incremental retraining maintains >95% of original benchmark scores
- [ ] Convergence detector halts within 2 cycles of actual plateau
- [ ] 10 cycles run end-to-end without human intervention

## Estimated Scope

~1,550 lines across library code and tests.
