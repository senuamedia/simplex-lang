# TASK-046: Neuro-Symbolic Hybrid Reasoning Engine

**Version:** 0.17.0
**Status:** Planned
**Priority:** P0 — Critical
**Depends on:** v0.16.0 release, causal inference (TASK-041), neural gates

## Why This Feature Is Needed

Neural networks pattern-match. They do not reason. When GPT-4 "solves" a math problem,
it is pattern-matching against its training data — that is why it fails on novel problems
with the same structure but different numbers. When an SLM "reasons" about a legal case,
it is retrieving similar-sounding text — that is why it hallucinates non-existent
statutes. The fundamental limitation of pure neural AI is that correlation is not logic.

Neuro-symbolic AI is the solution the industry is converging on. 2026 is being called
"the year of neuro-symbolic AI" because regulated industries — medicine, law, finance,
autonomous systems — cannot deploy models that do not actually reason. The approach
combines two components:

- **Neural perception:** understand unstructured input (text, images, sensor data)
- **Symbolic reasoning:** apply formal logic, rules, and constraints to produce
  guaranteed-correct outputs

A neuro-symbolic specialist SLM uses its neural component to *understand the question*
and its symbolic component to *compute the answer*. A 200M-parameter neuro-symbolic
model can out-reason a 70B pure-neural model on structured tasks because it is actually
following rules, not approximating them from training data.

Simplex is uniquely positioned for this. The language already has:
- Neural gates (learned, differentiable)
- Contracts (requires, ensures, invariant)
- Causal graphs (TASK-041)
- Beliefs with epistemic grounding

What is missing is the **symbolic reasoning engine** — a formal logic system that neural
components can invoke, and that can invoke neural components. This bidirectional
integration is what makes neuro-symbolic AI work.

## Why It Adds Value

1. **Models that actually reason.** A symbolic solver produces provably correct answers
   for structured problems. The neural component handles the fuzzy parts (understanding
   intent, parsing ambiguity), the symbolic component handles the precise parts (logic,
   arithmetic, constraint satisfaction).

2. **Hallucination elimination for structured tasks.** If the answer requires following
   rules (tax calculation, medical protocol, legal compliance), the symbolic engine
   follows the rules exactly. No hallucination possible — the output is derived from
   formal logic.

3. **Dramatic size reduction.** The neural component only needs to handle perception and
   translation to symbolic form. It doesn't need to memorize rules or facts — those
   live in the symbolic knowledge base. A tiny neural component + a symbolic rule base
   outperforms a massive neural-only model.

4. **Explainability by construction.** Every symbolic inference step is a traceable
   logical derivation. The system doesn't just give an answer — it gives a proof.
   Regulatory compliance, auditing, and debugging become straightforward.

5. **Compositional generalization.** Neural models fail on novel combinations of known
   concepts. Symbolic systems compose rules freely. A neuro-symbolic specialist
   generalizes to novel combinations because the symbolic component combines known
   rules in new ways — exactly what pure neural models cannot do.

6. **Integration with existing Simplex features.** Contracts become enforceable symbolic
   constraints. Beliefs become logical propositions with formal truth values. Neural
   gates become the bridge between neural perception and symbolic reasoning.

## Why It Changes Systems Built With Simplex

Neuro-symbolic hybrid reasoning transforms specialist capabilities:

- **Medical specialists that follow protocols exactly.** The neural component understands
  the patient's symptoms; the symbolic engine matches them against clinical guidelines
  and produces the correct diagnostic pathway. No hallucinated treatments.

- **Legal specialists that cite real law.** The neural component understands the legal
  question; the symbolic engine searches the statute graph and applies formal legal
  reasoning. Every citation is real. Every inference is logically valid.

- **Code specialists that prove correctness.** The neural component understands the
  programmer's intent; the symbolic engine generates code and proves it satisfies the
  specification via formal verification.

- **Financial specialists that comply by construction.** Regulatory rules are symbolic
  constraints. The neural component parses the transaction; the symbolic engine verifies
  compliance before any output is produced.

- **Hive-level reasoning chains.** Multiple specialists chain symbolic inferences.
  Specialist A produces a logical conclusion, Specialist B takes it as a premise and
  derives the next step. The hive constructs multi-step proofs across domains.

## Deliverables

### Phase 1: Symbolic Reasoning Core (~600 lines)

Location: `simplex-learning/src/symbolic/`

- **Term** — first-order logic terms (constants, variables, functions) with unification
- **Formula** — logical formulas (atoms, conjunction, disjunction, negation, quantifiers)
- **KnowledgeBase** — collection of facts and rules with forward and backward chaining
- **Unifier** — Robinson's unification algorithm for pattern matching
- **Solver** — resolution-based theorem prover for first-order logic

```simplex
/// First-order logic term
enum Term {
    Constant(String),
    Variable(String),
    Function(String, Vec<Term>),
}

/// Logical formula
enum Formula {
    Atom(String, Vec<Term>),           // predicate(args)
    Not(Box<Formula>),
    And(Box<Formula>, Box<Formula>),
    Or(Box<Formula>, Box<Formula>),
    Implies(Box<Formula>, Box<Formula>),
    ForAll(String, Box<Formula>),
    Exists(String, Box<Formula>),
}

/// A rule: head :- body (Prolog-style)
struct Rule {
    head: Formula,
    body: Vec<Formula>,
    confidence: f64,      // optional probabilistic weight
    provenance: String,   // where this rule came from
}

/// Knowledge base with inference
struct KnowledgeBase {
    facts: Vec<Formula>,
    rules: Vec<Rule>,
}

impl KnowledgeBase {
    /// Forward chaining: derive all consequences of current facts
    fn forward_chain(self: &Self) -> Vec<Formula> {
        let mut derived = self.facts.clone();
        loop {
            let new_facts = self.rules.iter()
                .filter_map(|rule| {
                    let bindings = self.match_body(&rule.body, &derived);
                    bindings.map(|b| rule.head.substitute(&b))
                })
                .filter(|f| !derived.contains(f))
                .collect::<Vec<_>>();
            if new_facts.is_empty() { break; }
            derived.extend(new_facts);
        }
        derived
    }

    /// Backward chaining: prove a goal by finding supporting rules
    fn backward_chain(self: &Self, goal: &Formula) -> Option<Proof> {
        // Direct fact match
        if let Some(bindings) = self.unify_with_facts(goal) {
            return Some(Proof::Fact(goal.clone(), bindings));
        }
        // Try each rule whose head unifies with goal
        for rule in &self.rules {
            if let Some(bindings) = unify(&rule.head, goal) {
                let subgoals: Vec<Formula> = rule.body.iter()
                    .map(|b| b.substitute(&bindings))
                    .collect();
                let subproofs: Option<Vec<Proof>> = subgoals.iter()
                    .map(|sg| self.backward_chain(sg))
                    .collect();
                if let Some(proofs) = subproofs {
                    return Some(Proof::Derivation(rule.clone(), proofs));
                }
            }
        }
        None
    }

    /// Query with proof: returns answer AND the logical derivation
    fn query(self: &Self, goal: &Formula) -> QueryResult {
        match self.backward_chain(goal) {
            Some(proof) => QueryResult::Proved(proof),
            None => QueryResult::Unknown,  // not proved (open world)
        }
    }
}
```

Files:
- `simplex-learning/src/symbolic/mod.sx` — module root
- `simplex-learning/src/symbolic/term.sx` — Term and unification
- `simplex-learning/src/symbolic/formula.sx` — Formula types
- `simplex-learning/src/symbolic/kb.sx` — KnowledgeBase
- `simplex-learning/src/symbolic/solver.sx` — forward/backward chaining, resolution
- `simplex-learning/src/symbolic/proof.sx` — Proof tree representation

### Phase 2: Neural-Symbolic Bridge (~500 lines)

- **NeuralParser** — neural component that translates unstructured input (text, data)
  into symbolic terms and formulas
- **SymbolicGrounding** — maps neural embeddings to symbolic constants via nearest-
  neighbor in embedding space
- **NeuralSymbolicGate** — neural gate that decides whether to use neural or symbolic
  reasoning for each sub-problem
- **DifferentiableLogic** — soft logic operators (fuzzy AND, OR, NOT) that allow
  gradient flow through symbolic reasoning during training

```simplex
/// Bridge between neural perception and symbolic reasoning
struct NeuralSymbolicBridge {
    parser: NeuralParser,        // text → symbolic terms
    grounder: SymbolicGrounding, // embeddings → constants
    kb: KnowledgeBase,           // symbolic knowledge
    gate: NeuralSymbolicGate,    // decides neural vs symbolic per query
}

impl NeuralSymbolicBridge {
    fn reason(self: &Self, input: &str) -> ReasoningResult {
        // Step 1: Neural perception — understand the input
        let parsed = self.parser.parse(input);

        // Step 2: Gate decision — symbolic or neural reasoning?
        let mode = self.gate.decide(&parsed);

        match mode {
            Mode::Symbolic => {
                // Ground neural output to symbolic terms
                let goal = self.grounder.to_formula(&parsed);
                // Prove via symbolic reasoning
                let result = self.kb.query(&goal);
                ReasoningResult::from_proof(result)
            },
            Mode::Neural => {
                // Pure neural for fuzzy/creative tasks
                ReasoningResult::from_neural(self.neural_reason(&parsed))
            },
            Mode::Hybrid => {
                // Neural generates candidates, symbolic verifies
                let candidates = self.neural_generate_candidates(&parsed);
                let verified = candidates.into_iter()
                    .filter(|c| self.kb.is_consistent(c))
                    .collect();
                ReasoningResult::from_verified(verified)
            },
        }
    }
}

/// Decides whether to route to neural or symbolic reasoning
neural_gate reasoning_router(input: ParsedInput) -> ReasoningMode
    requires input.is_valid()
    ensures result != ReasoningMode::Symbolic || input.is_formalizable()
    fallback ReasoningMode::Neural
{
    let structure_score = self.structure_detector.forward(input.embedding());
    if structure_score > 0.8 { ReasoningMode::Symbolic }
    else if structure_score > 0.4 { ReasoningMode::Hybrid }
    else { ReasoningMode::Neural }
}
```

Files:
- `simplex-learning/src/symbolic/parser.sx` — NeuralParser
- `simplex-learning/src/symbolic/grounding.sx` — SymbolicGrounding
- `simplex-learning/src/symbolic/bridge.sx` — NeuralSymbolicBridge
- `simplex-learning/src/symbolic/gate.sx` — NeuralSymbolicGate
- `simplex-learning/src/symbolic/differentiable.sx` — soft logic operators

### Phase 3: Constraint Satisfaction & Planning (~400 lines)

- **ConstraintSolver** — generic constraint satisfaction problem (CSP) solver with
  arc consistency and backtracking search
- **SymbolicPlanner** — STRIPS-style automated planner for multi-step reasoning
- **TypeChecker** — symbolic type inference and checking for neural outputs (verify
  that the neural component produced well-typed symbolic terms)
- **ProofCertificate** — exportable proof certificate that a third party can verify

Files:
- `simplex-learning/src/symbolic/constraints.sx` — ConstraintSolver
- `simplex-learning/src/symbolic/planner.sx` — SymbolicPlanner
- `simplex-learning/src/symbolic/typecheck.sx` — output type verification
- `simplex-learning/src/symbolic/certificate.sx` — exportable proof certificates

### Phase 4: Tests (~400 lines)

Location: `tests/symbolic/`

- Unification correctly binds variables in complex terms
- Forward chaining derives all consequences of a fact set
- Backward chaining proves goals with multi-step derivations
- NeuralSymbolicGate correctly routes structured vs unstructured queries
- Symbolic solver handles transitive reasoning chains
- Proof certificates are verifiable by independent checker
- Hybrid mode: neural generates, symbolic verifies

## Success Criteria

- [ ] KnowledgeBase correctly answers Prolog-style family relationship queries
- [ ] Forward chaining derives all transitive closures in <100ms for 1000-fact KB
- [ ] Backward chaining produces verifiable proofs for multi-step deductions
- [ ] NeuralSymbolicGate routes >90% of structured queries to symbolic engine
- [ ] Hybrid mode rejects neural outputs that violate symbolic constraints
- [ ] Proof certificates verify correctly in independent checker

## Estimated Scope

~1,900 lines across library code and tests.
