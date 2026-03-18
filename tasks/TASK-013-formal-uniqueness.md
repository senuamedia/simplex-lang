# TASK-013: Formal Computational Model & Uniqueness Proof

**Status**: Not Started (conceptual document only)
**Priority**: Critical (Strategic)
**Created**: 2026-01-14
**Updated**: 2026-03-16 (audited against codebase)

> **Audit Note (2026-03-16):** 0% implemented. No formal semantics, no SAM interpreter,
> no proofs, no academic examples, no papers. This document is purely a research roadmap.
> All 6 phases (Core Formalism, Non-Encodability Proof, Reference Implementation,
> Expressiveness Examples, Paper Writing, Peer Review) remain unstarted.
**Target**: Academic Publication (POPL/OOPSLA 2027)
**Depends On**: None (foundational work)

---

## The Question

> Is Simplex actually unique, or just a clever combination of existing ideas?

**Current honest answer**: Simplex is a synthesis. It combines:
- Actor model (Erlang)
- BDI cognitive agents (SOAR/ACT-R)
- Dual numbers (Autodiff)
- Evolutionary algorithms
- CRDT synchronization

Each exists independently. **To claim uniqueness, we must prove something cannot be encoded elsewhere.**

---

## Why Uniqueness Matters (Or Doesn't)

### The Cynical View

> "Academic uniqueness is worthless. Many 'unique' languages died because they solved problems nobody had."

This is true. Uniqueness for its own sake is theoretical folly.

### The Real Question

**Does Simplex solve an important problem better than existing approaches?**

Let me be concrete about what problems belief-gated dispatch actually solves:

### Problem 1: Confidence-Driven Behavior

**Current approach (Erlang/Go/Rust):**
```erlang
% Poll beliefs, check thresholds, act
loop(State) ->
    Confidence = get_belief_confidence(State, obstacle_detected),
    if
        Confidence < 0.5 -> slow_down();
        true -> continue()
    end,
    timer:sleep(100),  % Poll every 100ms
    loop(State).
```

**Problems with current approach:**
- Polling introduces latency (up to 100ms delay)
- Rate of change requires manual history tracking
- Threshold logic scattered across codebase
- No type safety on belief access

**Simplex approach:**
```simplex
receive {
    Sensor(data) @ confidence(obstacle) < 0.5 => slow_down(),
    Sensor(data) @ confidence(obstacle).derivative < -0.1 => emergency_brake(),
    Sensor(data) => continue(),
}
```

**Why this matters:**
- Reactive, not polling (belief change triggers immediately)
- Derivative is first-class (rate of change is the signal)
- Intent is declarative (what to do when, not how to check)

### Problem 2: Adaptive Systems

**Real-world examples where derivative matters:**

| Domain | Current Value | Derivative | Action |
|--------|---------------|------------|--------|
| Autonomous vehicles | Obstacle confidence 0.7 | Falling fast (-0.2/s) | Slow down NOW |
| Trading | Signal confidence 0.6 | Rising fast (+0.3/s) | Prepare position |
| Healthcare | Sensor confidence 0.8 | Slowly degrading (-0.01/s) | Schedule maintenance |
| AI assistant | Intent confidence 0.5 | Oscillating (unstable) | Ask for clarification |

**The insight**: The derivative often matters more than the value. A confidence of 0.7 that's falling fast is more urgent than 0.5 that's stable.

**Current solutions:**
- Complex event processing (Esper, Flink) - external system, not language
- State machines with history - manual, error-prone
- Reactive streams (RxJava) - better, but no derivatives

**Why language-level matters:**
1. **Type safety**: Compiler verifies belief patterns exist
2. **Optimization**: Runtime can index belief patterns, avoid re-evaluation
3. **Guarantees**: Formal semantics enable proofs about behavior

### Problem 3: Learning That Affects Execution

**Current approach:**
```python
# Train model offline
model = train(data)
save(model, "model.pkl")

# Deploy separately
model = load("model.pkl")
while True:
    result = model.predict(input)
    # Model never learns from deployment experience
```

**The gap**: Learning is separate from execution. The system that runs is not the system that learns.

**Simplex approach:**
```simplex
fn handle(request: Request) -> Response {
    let response = infer(request);  // Uses current Θ

    let feedback = get_feedback(response);
    learn(response, feedback);  // Updates Θ as semantic transition

    response  // Future calls use updated Θ
}
```

**Why this matters:**
- Learning happens IN the execution model, not beside it
- Gradients flow through beliefs (because beliefs are differentiable)
- Formal semantics can reason about convergence

### The Honest Assessment

| Problem | Is it real? | Is language-level the right solution? |
|---------|-------------|--------------------------------------|
| Confidence-driven dispatch | **Yes** | Probably yes - polling is bad |
| Derivative as first-class | **Yes** | Yes - manual history is error-prone |
| Integrated learning | **Yes** | Maybe - could be runtime policy |

**Verdict**: Problems 1 and 2 justify language-level primitives. Problem 3 is less clear.

---

## What Must Be Proven

### Minimum Viable Uniqueness

To claim Simplex is unique, we need ONE of:

1. **An irreducible primitive** that cannot be encoded elsewhere
2. **An expressive separation** - programs easy in Simplex, hard elsewhere
3. **A semantic guarantee** not achievable in other models

### The Candidate: Belief-Gated Receive with Derivative Patterns

```
recv ::= receive { clause* }
clause ::= pattern @ belief_guard => expr

belief_guard ::= belief_expr cmp value
              |  belief_expr.∂ cmp value      // Derivative pattern
              |  belief_guard ∧ belief_guard
              |  belief_guard ∨ belief_guard

cmp ::= < | <= | > | >= | == | !=
```

**Key semantic properties:**

1. **Belief guards are continuously evaluated**, not just at message arrival
2. **Derivative (∂) is computed automatically** via dual number propagation
3. **Multiple clauses can race** on different belief conditions
4. **Suspension is semantic** - not busy-waiting

### Non-Encodability Argument

**Claim**: Belief-gated receive cannot be faithfully encoded in Erlang.

**Argument**:
```
1. Erlang receive guards are evaluated ONCE when a message arrives
2. Belief changes occur BETWEEN message arrivals
3. Therefore, an Erlang process cannot wake on belief change alone
4. Any encoding requires one of:
   (a) A belief-monitor process that sends messages on every change
   (b) Timeout-based polling in receive
   (c) A meta-interpreter that intercepts all dispatch
5. Each encoding changes operational semantics:
   (a) Adds messages to mailbox, changes ordering guarantees
   (b) Adds latency, changes timing semantics
   (c) Requires reflection, breaks modularity
6. Therefore, no faithful encoding exists.
```

**This is a sketch, not a proof.** Formalizing it is the work of this task.

---

## Formal Model: Simplex Abstract Machine (SAM)

### State

```
σ = ⟨A, B, H, T, Θ⟩

A : ActorId → Actor           -- Actor states
B : BeliefId → Dual           -- Belief space (value + derivative)
H : ActorId → History         -- Memory fields per actor
T : ℝ                         -- Global temperature (for annealing)
Θ : Parameters                -- Learned parameters
```

### Dual Numbers in Beliefs

```
Dual = (value: ℝ, derivative: ℝ)

Operations:
  (a, a') + (b, b') = (a + b, a' + b')
  (a, a') × (b, b') = (a × b, a × b' + a' × b)
  sin(a, a') = (sin(a), a' × cos(a))
  ...

Every belief is a Dual. Derivatives propagate automatically.
```

### Transition Rules

#### SEND: Send message to actor

```
        a₁ ∈ A    a₂ ∈ A    a₁ executes "send(a₂, m)"
        ─────────────────────────────────────────────
        σ →[send] σ' where mailbox(a₂) += m
```

#### RECV: Receive with belief guard

```
        a ∈ A
        a.code = receive { p @ g => e, ... }
        m ∈ mailbox(a)
        match(m, p) = θ             -- pattern matches with bindings θ
        eval(g, B) = true           -- belief guard satisfied NOW
        ────────────────────────────────────────────────────────
        σ →[recv] σ' where a.code = e[θ], mailbox(a) -= m
```

#### WAKE: Belief change wakes suspended receive

```
        a ∈ A    a.suspended = (p @ g => e, ...)
        B' ≠ B                      -- beliefs changed
        eval(g, B') = true          -- guard NOW satisfied
        m ∈ mailbox(a)
        match(m, p) = θ
        ────────────────────────────────────────────────────────
        σ →[wake] σ' where a.code = e[θ], a.suspended = ∅
```

**This is the key rule.** WAKE has no analog in Erlang.

#### BELIEF: Update belief (affects all suspended receives)

```
        a ∈ A    a executes "believe(id, v)"
        old = B(id)
        new = (v, (v - old.value) / Δt)    -- Derivative from change
        ────────────────────────────────────────────────────────
        σ →[belief] σ' where B' = B[id ↦ new]

        -- All actors with suspended receives on id are candidates for WAKE
```

#### LEARN: Gradient update

```
        a ∈ A    a executes "learn(loss)"
        ∇ = gradient(loss, Θ, B)           -- Gradient through beliefs
        Θ' = Θ - α × ∇
        ─────────────────────────────────────────────────────────
        σ →[learn] σ' where Θ' replaces Θ
```

---

## Research Roadmap

### Phase 1: Core Formalism (2 months)

- [ ] Define syntax formally (BNF grammar)
- [ ] Define SAM state space precisely
- [ ] Define all transition rules
- [ ] Prove basic properties (determinism where expected, progress)
- [ ] Write 10 pages of formal definitions

**Deliverable**: Technical report on SAM semantics

### Phase 2: Non-Encodability Proof (2 months)

- [ ] Define "faithful encoding" precisely
- [ ] Define Erlang operational semantics (or cite existing)
- [ ] Prove WAKE rule has no Erlang analog
- [ ] Prove any encoding changes observational equivalence
- [ ] Consider objections (could Erlang + library do it?)

**Deliverable**: Proof of separation from actor model

### Phase 3: Reference Implementation (2 months)

- [ ] Implement minimal SAM interpreter (~1000 lines)
- [ ] Not the full Simplex toolchain - just the semantics
- [ ] Deterministic where semantics say so
- [ ] Test cases for each transition rule
- [ ] Demonstrate belief-gated receive

**Deliverable**: Reference interpreter with test suite

### Phase 4: Expressiveness Examples (1 month)

- [ ] 5 programs natural in Simplex, awkward elsewhere
- [ ] Show Erlang encoding for each, highlight semantic gap
- [ ] Show Prolog encoding, highlight gap
- [ ] Show probabilistic language encoding, highlight gap
- [ ] Quantify code size / complexity difference

**Deliverable**: Example corpus with comparative analysis

### Phase 5: Paper Writing (2 months)

- [ ] Write POPL/OOPSLA submission
- [ ] Introduction: The problem of adaptive systems
- [ ] Model: SAM formal semantics
- [ ] Uniqueness: Non-encodability proof
- [ ] Implementation: Reference interpreter
- [ ] Evaluation: Expressiveness examples
- [ ] Related work: Comprehensive comparison
- [ ] Conclusion: What this enables

**Deliverable**: Submitted paper

### Phase 6: Peer Review & Revision (3 months)

- [ ] Submit to workshop first (AGERE, REBLS, ML4PL)
- [ ] Incorporate feedback
- [ ] Submit to main venue (POPL 2027 or OOPSLA 2027)
- [ ] Handle reviews
- [ ] Camera-ready

**Deliverable**: Accepted publication

---

## Comparison Matrix

The paper must honestly compare against:

| System | Beliefs | Derivatives | Learning | Actors | Belief-Gated Recv |
|--------|---------|-------------|----------|--------|-------------------|
| Erlang | No | No | No | Yes | No |
| Pony | No | No | No | Yes | No |
| SOAR | Yes | No | Chunking | No | No |
| ACT-R | Yes | No | Subsymbolic | No | No |
| Prolog | Yes (as facts) | No | No | No | No |
| Church/Pyro | Yes (as distributions) | No | Inference | No | No |
| JAX | No | Yes | Yes | No | No |
| Stan | Yes (as parameters) | Yes | Yes | No | No |
| **Simplex** | Yes | Yes | Yes | Yes | Yes |

**The claim**: No existing system has all five. The combination enables programs not expressible elsewhere without meta-interpretation.

---

## Risk Assessment

### Risk 1: Encodability Proof Fails

**Scenario**: Someone shows belief-gated receive CAN be encoded faithfully in Erlang + library.

**Mitigation**:
- Define "faithful" precisely enough that trivial encodings fail
- Focus on DERIVATIVE patterns - harder to encode
- Be prepared to weaken claim to "more natural" rather than "unique"

### Risk 2: Nobody Cares

**Scenario**: Reviewers say "this is niche, who needs belief-gated dispatch?"

**Mitigation**:
- Lead with real-world examples (autonomous systems, trading, healthcare)
- Show actual systems that would benefit
- Quantify the polling-vs-reactive latency difference

### Risk 3: Formalism Too Complex

**Scenario**: Reviewers reject because SAM is too complicated to understand.

**Mitigation**:
- Start with SIMPLE version (no learning, no annealing)
- Add features incrementally
- Prioritize clarity over completeness

### Risk 4: Implementation Doesn't Match Semantics

**Scenario**: Reference interpreter has bugs that contradict formal model.

**Mitigation**:
- Property-based testing against semantics
- Keep interpreter minimal
- Formal verification if time permits (but probably not)

---

## Success Criteria

### Minimum Success

- [ ] Formal semantics document (20+ pages)
- [ ] Reference interpreter passes all tests
- [ ] Workshop paper accepted

### Full Success

- [ ] Non-encodability proof accepted by reviewers
- [ ] POPL or OOPSLA publication
- [ ] Other researchers cite/build on the work

### Stretch Success

- [ ] Simplex model influences other language designs
- [ ] "Belief-gated dispatch" becomes recognized term
- [ ] Follow-up papers extend the model

---

## The Honest Bottom Line

**Is this worth doing?**

| If uniqueness is proven... | Value |
|---------------------------|-------|
| Academic credibility | High - publishable, citable |
| Practical impact | Medium - enables new program patterns |
| Marketing | High - "provably unique" is powerful |
| Technical foundation | High - formalism catches design bugs |

| If uniqueness is NOT proven... | Value |
|-------------------------------|-------|
| Academic credibility | Low - "just a synthesis" |
| Practical impact | Still medium - synthesis is useful |
| Marketing | Low - competitors can claim equivalence |
| Technical foundation | Still high - formalism still valuable |

**My recommendation**: The formalism work is valuable regardless of uniqueness outcome. It forces precision, catches bugs, and enables optimization. The uniqueness proof is bonus.

**The real question**: Is belief-gated dispatch solving a problem worth solving?

If autonomous systems, adaptive AI, and real-time decision-making are the future, then yes. These systems NEED to respond to confidence changes and derivatives. Current solutions are polling-based hacks.

If the future is batch processing and request-response APIs, then no. Traditional languages are fine.

**I believe the future is adaptive systems.** That's why this work matters.

---

## Related Tasks

- **TASK-012**: Nexus Protocol - will need formal message semantics
- **TASK-005**: Dual Numbers - mathematical foundation
- **TASK-009**: Edge Hive - example use case for formalism

---

## References

### Programming Language Theory
- Pierce, B. (2002). "Types and Programming Languages"
- Harper, R. (2016). "Practical Foundations for Programming Languages"
- Plotkin, G. (1981). "A Structural Approach to Operational Semantics"

### Actor Model
- Hewitt, C. (1973). "A Universal Modular Actor Formalism"
- Agha, G. (1986). "Actors: A Model of Concurrent Computation"
- Armstrong, J. (2003). "Making reliable distributed systems..."

### Cognitive Architectures
- Laird, J. (2012). "The SOAR Cognitive Architecture"
- Anderson, J. (2007). "How Can the Human Mind Occur in the Physical Universe?"
- Bratman, M. (1987). "Intention, Plans, and Practical Reason"

### Probabilistic Programming
- Goodman, N. (2008). "Church: a language for generative models"
- Carpenter, B. (2017). "Stan: A Probabilistic Programming Language"

### Differentiable Programming
- Baydin, A. (2018). "Automatic Differentiation in Machine Learning: A Survey"
- Innes, M. (2019). "A Differentiable Programming System to Bridge ML and Scientific Computing"
