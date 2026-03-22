# TASK-036: Conformal Prediction & Calibrated Uncertainty

**Version:** 0.16.0
**Status:** Complete
**Priority:** P0 — Critical
**Depends on:** v0.15.0 release, existing belief/epistemic system (TASK-014)

## Why This Feature Is Needed

Every SLM hallucinates. Every LLM hallucinates. The entire industry treats this as an
unsolved problem — but it isn't. Conformal prediction is a 20-year-old statistical
framework that provides **mathematically guaranteed** prediction sets. When a conformal
predictor says "I am 95% confident the answer is in this set," that statement is
*provably correct* regardless of the model architecture, the data distribution, or the
training procedure. The only assumption is exchangeability (roughly: the future looks
like the past).

No programming language has conformal prediction as a first-class type. Python libraries
exist (MAPIE, crepes) but they are bolted onto models after the fact. In Simplex,
conformal prediction becomes part of the type system — a function that returns
`Conformal<String>` instead of `String` carries its uncertainty guarantee in the type
signature. The compiler can enforce that high-stakes decisions require conformal bounds.

## Why It Adds Value

1. **Governance by construction.** A Simplex SLM wrapped in `ConformalPredictor` cannot
   produce an answer without an accompanying confidence set. Regulators, auditors, and
   users can verify coverage guarantees without understanding the model internals.

2. **Hallucination rejection.** When the conformal set is too large (the model is
   uncertain), the system can refuse to answer rather than hallucinate. This is not
   heuristic — it is statistical law.

3. **Distribution shift detection.** Adaptive conformal methods detect when the world
   has changed and the model's calibration is degrading, triggering retraining or
   fallback before errors compound.

4. **Composability with Cognitive Hive.** Each specialist in a hive can carry its own
   conformal calibration. The hive router can use conformal set sizes to decide which
   specialist is most confident — routing by mathematical certainty, not just softmax.

5. **Integration with beliefs.** The existing epistemic belief system (confidence scores,
   revision thresholds) gains formal statistical backing. A belief's confidence becomes
   a conformal coverage guarantee, not just a learned number.

## Why It Changes Systems Built With Simplex

Systems built with conformal prediction move from "the model thinks X" to "the model
guarantees X is in this set with 95% probability." This is a categorical shift:

- **Medical SLMs** can say "the diagnosis is one of {A, B}" with proven coverage, rather
  than hallucinating a single confident-sounding wrong answer.
- **Financial SLMs** produce prediction intervals with guaranteed coverage for risk
  management, not point estimates that miss tail events.
- **Code generation SLMs** can flag when their confidence set spans multiple incompatible
  implementations, triggering human review instead of silently generating bugs.
- **Any hive-based system** gets automatic "I don't know" detection at the mathematical
  level, not the heuristic level.

## Deliverables

### Phase 1: Core Conformal Framework (~500 lines)

Location: `simplex-learning/src/conformal/`

- **NonconformityScore trait** — pluggable scoring functions for any model type
- **ConformalPredictor<T>** — wraps any predictor, calibrates on held-out data, produces
  prediction sets with guaranteed coverage
- **SplitConformal** — basic split conformal prediction (calibration/test split)
- **Coverage guarantee enforcement** — runtime assertion that empirical coverage meets
  target

```simplex
/// A nonconformity score measures how "strange" a prediction is
trait NonconformityScore<X, Y> {
    fn score(self: &Self, x: &X, y: &Y) -> f64;
}

/// Wraps any model to produce prediction sets with coverage guarantees
struct ConformalPredictor<M, S: NonconformityScore> {
    model: M,
    scorer: S,
    quantile: f64,         // calibrated threshold
    target_coverage: f64,  // e.g., 0.95
}

impl<M, S> ConformalPredictor<M, S> {
    /// Calibrate on held-out data to find the conformal quantile
    fn calibrate(mut self: &mut Self, cal_data: &[(X, Y)]) {
        let scores: Vec<f64> = cal_data.iter()
            .map(|(x, y)| self.scorer.score(x, y))
            .collect();
        // Finite-sample valid quantile (ceil((n+1)(1-alpha))/n)
        self.quantile = quantile_with_correction(&scores, self.target_coverage);
    }

    /// Produce a prediction set — all y where score(x, y) <= quantile
    fn predict_set(self: &Self, x: &X) -> PredictionSet<Y> {
        self.model.predict_candidates(x)
            .filter(|y| self.scorer.score(x, y) <= self.quantile)
            .into_prediction_set()
    }
}
```

Files:
- `simplex-learning/src/conformal/mod.sx` — module root, re-exports
- `simplex-learning/src/conformal/score.sx` — NonconformityScore trait and common scorers
- `simplex-learning/src/conformal/predictor.sx` — ConformalPredictor implementation
- `simplex-learning/src/conformal/sets.sx` — PredictionSet type with set operations

### Phase 2: Adaptive & Online Conformal (~400 lines)

- **AdaptiveConformal** — online conformal prediction that adjusts to distribution shift
  using a forgetting factor (ACI — Adaptive Conformal Inference)
- **ConformalPID** — PID-controller-based adaptive conformal for tighter sets
- **StreamingCalibration** — integrate with existing StreamingAdam/StreamingSGD for
  online learning scenarios where calibration data arrives incrementally
- **ShiftDetector** — flag when conformal set sizes are growing (distribution drift)

```simplex
/// Online conformal prediction with adaptive coverage
struct AdaptiveConformal<M, S: NonconformityScore> {
    base: ConformalPredictor<M, S>,
    learning_rate: f64,  // adaptation speed
    error_history: Vec<bool>,
}

impl<M, S> AdaptiveConformal<M, S> {
    /// Update after observing true label — adjusts quantile online
    fn update(mut self: &mut Self, x: &X, y_true: &Y) {
        let covered = self.base.predict_set(x).contains(y_true);
        self.error_history.push(!covered);
        // ACI update: raise quantile if under-covering, lower if over-covering
        let error = if covered { 0.0 } else { 1.0 };
        let target_error = 1.0 - self.base.target_coverage;
        self.base.quantile += self.learning_rate * (target_error - error);
    }
}
```

Files:
- `simplex-learning/src/conformal/adaptive.sx` — ACI and ConformalPID
- `simplex-learning/src/conformal/streaming.sx` — streaming calibration
- `simplex-learning/src/conformal/drift.sx` — distribution shift detection

### Phase 3: Language Integration (~300 lines)

- **ConformalGate** — neural gate that refuses to fire when conformal set is too large
- **@coverage(0.95) annotation** — compiler enforces conformal wrapping on annotated
  functions
- **Belief integration** — beliefs gain `conformal_bound` field linking confidence to
  statistical guarantee
- **Hive routing by conformal width** — router prefers specialist with tightest
  conformal set

```simplex
/// A neural gate that only fires when conformal certainty is sufficient
neural_gate conformal_classifier(input: Tensor) -> Class
    requires conformal_set_size(input) <= 3
    ensures coverage >= 0.95
    fallback Class::Unknown
{
    let scores = self.model.forward(input);
    let conf_set = self.conformal.predict_set(input);
    if conf_set.size() <= 3 {
        conf_set.most_likely()
    } else {
        gate_abstain()  // refuse to decide
    }
}
```

Files:
- `simplex-learning/src/conformal/gate.sx` — ConformalGate neural gate
- `simplex-learning/src/conformal/belief.sx` — belief system integration
- `simplex-learning/src/conformal/hive.sx` — hive router integration

### Phase 4: Tests (~300 lines)

Location: `tests/conformal/`

- Coverage guarantee validation on synthetic data
- Adaptive conformal tracking under distribution shift
- ConformalGate abstention behavior
- Integration with hive routing
- Stress test: coverage holds under adversarial input

## Success Criteria

- [ ] `ConformalPredictor` achieves target coverage (within ±1%) on held-out data
- [ ] `AdaptiveConformal` maintains coverage under synthetic distribution shift
- [ ] `ConformalGate` abstains when conformal set exceeds threshold
- [ ] Hive router correctly prefers specialist with tightest conformal sets
- [ ] All conformal types compose with existing tensor/dual number infrastructure
- [ ] Zero runtime overhead when conformal wrapping is not used

## Estimated Scope

~1,500 lines across library code and tests.
