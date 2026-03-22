# TASK-041: Causal Inference Primitives

**Version:** 0.16.0
**Status:** Complete
**Priority:** P1 — High
**Depends on:** v0.15.0 release, belief/epistemic system (TASK-014)

## Why This Feature Is Needed

LLMs hallucinate because they learn correlations, not causation. A model trained on
medical data learns that "patients who receive Drug X tend to recover" — but it cannot
distinguish whether Drug X *causes* recovery or whether doctors prescribe Drug X to
patients who are already recovering. This is Simpson's Paradox and it is everywhere in
real-world data. Every LLM is vulnerable to it.

Judea Pearl's do-calculus provides the mathematical framework to reason about causation.
The `do(X=x)` operator represents an *intervention* — actively setting X to value x —
as opposed to *observation* — passively seeing that X equals x. The difference is
everything:

- P(Recovery | Drug=1) = "what is recovery rate among patients who took the drug?"
  (observational — confounded by selection bias)
- P(Recovery | do(Drug=1)) = "what would the recovery rate be if we *forced* everyone
  to take the drug?" (interventional — the actual causal effect)

No programming language has causal inference as a first-class primitive. Python libraries
(DoWhy, CausalML) exist but they are disconnected from model training. In Simplex,
causal reasoning is built into the type system — a `CausalGraph` is a type, `do()` is
an operator, and neural gates can be required to produce causal (not correlational)
evidence for their decisions.

## Why It Adds Value

1. **Root-cause hallucination prevention.** Conformal prediction tells you *when* the
   model is uncertain. Causal inference tells you *why* — and prevents the model from
   acting on spurious correlations in the first place.

2. **Interventional reasoning.** Specialist SLMs that can answer "what would happen if
   we changed X?" instead of just "what usually happens when X is high?" This is the
   difference between a recommendation engine and an advisor.

3. **Counterfactual generation.** `Counterfactual<T>` lets SLMs reason about "what would
   have happened if..." — essential for explanation, debugging, and fairness auditing.

4. **Belief system upgrade.** The existing epistemic belief system tracks confidence and
   evidence. Causal beliefs add directionality: "A causes B with confidence 0.8" is
   fundamentally more useful than "A correlates with B with confidence 0.8." The hive's
   belief state becomes a causal model of the world.

5. **Fairness and bias detection.** Causal graphs make it possible to formally identify
   whether a model's decision is based on a protected attribute (directly or through a
   proxy). The compiler can enforce that decisions do not causally depend on prohibited
   features.

6. **Composability with neural gates.** A `CausalGate` only fires when the evidence is
   causal, not correlational. This prevents neural gates from learning spurious shortcuts
   — the most common failure mode in production ML.

## Why It Changes Systems Built With Simplex

Causal inference changes what AI systems can *claim* about their outputs:

- **Medical SLMs** that recommend treatments based on causal evidence, not observational
  correlation. The specialist can say "Drug X causes 30% improvement" with the causal
  graph as proof, not "Drug X is associated with 30% improvement."

- **Financial SLMs** that identify causal drivers of market movements, not just
  correlated indicators. A hive specialist that says "Fed policy *caused* this yield
  shift" can back it with an interventional calculation.

- **Fairness-auditable AI.** A hiring specialist can be verified to not causally depend
  on gender, race, or age — even through proxy variables. The causal graph makes the
  information flow explicit and auditable.

- **Self-correcting hives.** When a specialist's causal model is contradicted by new
  evidence, the belief system can identify *which causal link broke* and update
  precisely, rather than retraining the entire model.

- **Explanation generation.** Instead of "the model predicted X because of features A
  and B," the system can say "A caused B which caused X, and if A had been different,
  X would have been Y." Counterfactual explanations are legally required in some
  jurisdictions (EU AI Act).

## Deliverables

### Phase 1: Causal Graph Framework (~500 lines)

Location: `simplex-learning/src/causal/`

- **CausalGraph** — directed acyclic graph representing causal relationships, with
  methods for d-separation, backdoor criterion, and frontdoor criterion
- **CausalVariable** — typed variable in a causal graph with optional observed/latent
  status
- **Intervention** — the `do()` operator: set a variable to a value and propagate
  through the graph
- **AdjustmentSet** — automatically identify valid adjustment sets for causal effect
  estimation

```simplex
/// A directed acyclic graph representing causal relationships
struct CausalGraph {
    variables: Vec<CausalVariable>,
    edges: Vec<(usize, usize)>,  // directed edges: cause → effect
    latent: HashSet<usize>,       // unobserved confounders
}

struct CausalVariable {
    name: String,
    observed: bool,
    distribution: Option<Distribution>,
}

impl CausalGraph {
    /// Check if X and Y are d-separated given Z (conditional independence)
    fn d_separated(self: &Self, x: usize, y: usize, z: &HashSet<usize>) -> bool {
        // Bayes-Ball algorithm
        bayes_ball(self, x, y, z)
    }

    /// Find the backdoor adjustment set for estimating X → Y
    fn backdoor_adjustment(self: &Self, x: usize, y: usize) -> Option<HashSet<usize>> {
        // Find minimal set Z such that:
        // 1. No descendant of X is in Z
        // 2. Z blocks all backdoor paths from X to Y
        find_minimal_backdoor(self, x, y)
    }

    /// Compute interventional distribution P(Y | do(X=x))
    fn do_calculus(self: &Self, target: usize, intervention: Intervention) -> Distribution {
        let adjustment = self.backdoor_adjustment(intervention.variable, target);
        match adjustment {
            Some(z) => self.adjust(target, intervention, z),
            None => {
                // Try frontdoor criterion
                let frontdoor = self.frontdoor_adjustment(intervention.variable, target);
                match frontdoor {
                    Some(m) => self.frontdoor_adjust(target, intervention, m),
                    None => panic!("Causal effect not identifiable from observed data"),
                }
            }
        }
    }
}

/// The do() operator: intervene on a variable
struct Intervention {
    variable: usize,
    value: f64,
}

/// Perform an intervention on a causal graph
fn do_intervention(graph: &CausalGraph, intervention: Intervention) -> CausalGraph {
    // Remove all incoming edges to the intervened variable
    // Set its value to the intervention value
    let mut modified = graph.clone();
    modified.edges.retain(|(_, to)| *to != intervention.variable);
    modified.variables[intervention.variable].distribution =
        Some(Distribution::Constant(intervention.value));
    modified
}
```

Files:
- `simplex-learning/src/causal/mod.sx` — module root
- `simplex-learning/src/causal/graph.sx` — CausalGraph and graph algorithms
- `simplex-learning/src/causal/variable.sx` — CausalVariable and distributions
- `simplex-learning/src/causal/intervention.sx` — Intervention and do() operator
- `simplex-learning/src/causal/adjustment.sx` — backdoor/frontdoor adjustment sets

### Phase 2: Causal Effect Estimation (~400 lines)

- **CausalEstimator** — estimate causal effects from observational data using the
  identified adjustment sets
- **InversePropensityWeighting** — IPW estimator for causal effects
- **DoublyRobust** — doubly robust estimator (consistent if *either* outcome model or
  propensity model is correct)
- **InstrumentalVariable** — IV estimation when no valid adjustment set exists but an
  instrument is available
- **CausalForest** — heterogeneous treatment effect estimation (effect varies by subgroup)

```simplex
/// Estimate causal effects from data using the identified causal graph
trait CausalEstimator {
    fn estimate_ate(
        self: &Self,
        data: &DataFrame,
        treatment: &str,
        outcome: &str,
        graph: &CausalGraph,
    ) -> CausalEffect;
}

/// The estimated causal effect with uncertainty
struct CausalEffect {
    estimate: f64,
    confidence_interval: (f64, f64),
    p_value: f64,
    method: EstimationMethod,
}

/// Doubly robust estimator: consistent if either model is correct
struct DoublyRobust {
    outcome_model: Box<dyn Predictor>,
    propensity_model: Box<dyn Predictor>,
}

impl CausalEstimator for DoublyRobust {
    fn estimate_ate(
        self: &Self,
        data: &DataFrame,
        treatment: &str,
        outcome: &str,
        graph: &CausalGraph,
    ) -> CausalEffect {
        let adjustment = graph.backdoor_adjustment(
            graph.variable_index(treatment),
            graph.variable_index(outcome),
        ).expect("Cannot identify causal effect");

        // DR estimator: combines outcome regression and IPW
        let propensity = self.propensity_model.predict(data, treatment);
        let outcome_pred = self.outcome_model.predict(data, outcome);
        doubly_robust_estimate(data, propensity, outcome_pred, treatment, outcome)
    }
}
```

Files:
- `simplex-learning/src/causal/estimator.sx` — CausalEstimator trait and implementations
- `simplex-learning/src/causal/ipw.sx` — Inverse Propensity Weighting
- `simplex-learning/src/causal/doubly_robust.sx` — Doubly Robust estimator
- `simplex-learning/src/causal/iv.sx` — Instrumental Variable estimation

### Phase 3: Counterfactuals & Belief Integration (~400 lines)

- **Counterfactual<T>** — "what would Y have been if X had been x?" using the three-step
  counterfactual procedure (abduction, action, prediction)
- **CausalBelief** — extends existing belief system with causal directionality and
  interventional semantics
- **CausalGate** — neural gate that requires causal (not correlational) evidence
- **FairnessAuditor** — check whether a model's decision causally depends on protected
  attributes

```simplex
/// Counterfactual reasoning: what would have happened if...
struct Counterfactual<T> {
    factual_outcome: T,
    counterfactual_outcome: T,
    intervention: Intervention,
    probability: f64,
}

impl CausalGraph {
    /// Three-step counterfactual: abduction → action → prediction
    fn counterfactual(
        self: &Self,
        evidence: &HashMap<String, f64>,  // what we observed
        intervention: Intervention,        // what we change
        target: usize,                     // what we want to know
    ) -> Counterfactual<f64> {
        // Step 1: Abduction — infer exogenous variables from evidence
        let exogenous = self.abduct(evidence);
        // Step 2: Action — apply intervention
        let modified = do_intervention(self, intervention);
        // Step 3: Prediction — compute target under modified graph with inferred exogenous
        let cf_outcome = modified.predict(target, &exogenous);
        Counterfactual {
            factual_outcome: evidence[&self.variables[target].name],
            counterfactual_outcome: cf_outcome,
            intervention,
            probability: self.counterfactual_probability(&exogenous),
        }
    }
}

/// A neural gate that requires causal evidence, not just correlation
neural_gate causal_recommender(patient: PatientData, graph: &CausalGraph) -> Treatment
    requires graph.is_identifiable("treatment", "outcome")
    ensures result.causal_effect.p_value < 0.05
    fallback Treatment::Refer_to_specialist
{
    let effect = graph.do_calculus(
        graph.variable_index("outcome"),
        Intervention { variable: graph.variable_index("treatment"), value: 1.0 },
    );
    if effect.mean() > 0.3 {
        Treatment::Prescribe
    } else {
        Treatment::Monitor
    }
}
```

Files:
- `simplex-learning/src/causal/counterfactual.sx` — Counterfactual type and computation
- `simplex-learning/src/causal/belief.sx` — CausalBelief integration
- `simplex-learning/src/causal/gate.sx` — CausalGate neural gate
- `simplex-learning/src/causal/fairness.sx` — FairnessAuditor

### Phase 4: Tests (~350 lines)

Location: `tests/causal/`

- d-separation correctly identifies conditional independencies
- Backdoor adjustment recovers true causal effect on Simpson's Paradox dataset
- do() operator correctly removes confounding
- Counterfactual computation matches analytical solution on simple graph
- CausalGate refuses to fire on correlational-only evidence
- FairnessAuditor detects proxy discrimination through causal paths

## Success Criteria

- [ ] CausalGraph correctly computes d-separation for standard graph structures
- [ ] Backdoor adjustment recovers true ATE within 10% on synthetic confounded data
- [ ] Counterfactual "what if" answers match analytical solutions on 3-variable graphs
- [ ] CausalGate distinguishes causal from correlational evidence in synthetic setting
- [ ] FairnessAuditor identifies all causal paths from protected attribute to decision
- [ ] Integration with existing belief system — beliefs carry causal directionality

## Estimated Scope

~1,650 lines across library code and tests.
