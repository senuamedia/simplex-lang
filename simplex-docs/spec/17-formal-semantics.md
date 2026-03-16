# Formal Semantics of the Simplex Actor Model (SAM)

**Draft for Academic Review -- v0.1**
**Status**: Phase 1 Deliverable (TASK-013)

---

## 1. Introduction

This document presents the formal operational semantics of the Simplex Actor Model (SAM), the computational core of the Simplex programming language. SAM extends the classical actor model (Hewitt 1973, Agha 1986) with three primitives drawn from cognitive architecture and differentiable programming:

1. **Beliefs** -- mutable confidence values represented as dual numbers, providing both a value and its time-derivative.
2. **Belief-gated receive** -- message dispatch conditioned on belief predicates that are continuously evaluated, not merely checked at message arrival.
3. **Epistemic annealing** -- a temperature parameter governing exploration/exploitation tradeoffs, modulated by epistemic health metrics.

The central claim is that belief-gated receive with derivative patterns constitutes an irreducible primitive: it cannot be faithfully encoded in the classical actor model or in Erlang's selective receive. Section 6 provides the argument.

We use standard structural operational semantics in the style of Plotkin (1981), as systematized by Harper (2016).

---

## 2. BNF Grammar (Core Subset)

We define SAM-core, a minimal subset of Simplex sufficient to express actors, beliefs, neural gates, and belief-gated receive. Production names prefixed with `s-` denote syntactic categories.

```
s-prog    ::= s-decl*

s-decl    ::= s-actor
            | 'fn' IDENT '(' s-params ')' '->' s-type s-block

s-actor   ::= 'actor' IDENT '{' s-state* s-handler* '}'

s-state   ::= 'var' IDENT ':' s-type '=' s-expr

s-handler ::= 'receive' IDENT '(' s-params ')' s-guard? s-block
            | 'receive' IDENT '(' s-params ')' '->' s-type s-guard? s-block

s-guard   ::= '@' s-bguard

s-bguard  ::= s-bguard '&&' s-batom
            | s-bguard '||' s-batom
            | s-batom

s-batom   ::= s-bprim s-cmp s-expr
            | '(' s-bguard ')'

s-bprim   ::= 'confidence' '(' s-expr ')'
            | 'confidence' '(' s-expr ')' '.' 'derivative'

s-cmp     ::= '<' | '<=' | '>' | '>=' | '==' | '!='

s-expr    ::= IDENT
            | NUM
            | FLOAT
            | STRING
            | 'dual' '::' 'new' '(' s-expr ',' s-expr ')'
            | 'dual' '::' 'variable' '(' s-expr ')'
            | 'dual' '::' 'constant' '(' s-expr ')'
            | s-expr s-binop s-expr
            | s-expr '.' IDENT
            | s-expr '.' IDENT '(' s-args ')'
            | IDENT '(' s-args ')'
            | 'send' '(' s-expr ',' IDENT '(' s-args ')' ')'
            | 'ask' '(' s-expr ',' IDENT '(' s-args ')' ')'
            | 'spawn' IDENT '(' s-args ')'
            | 'believe' '(' s-expr ',' s-expr ')'
            | 'infer' '(' s-expr ')'
            | 'learn' '(' s-expr ',' s-expr ')'

s-binop   ::= '+' | '-' | '*' | '/' | '%'

s-block   ::= '{' s-stmt* '}'

s-stmt    ::= 'let' IDENT ':' s-type '=' s-expr
            | 'var' IDENT ':' s-type '=' s-expr
            | s-expr
            | 'if' s-expr s-block ('else' s-block)?
            | 'return' s-expr
            | 'checkpoint' '(' ')'

s-type    ::= 'i64' | 'f64' | 'bool' | 'String' | 'dual'
            | 'ActorRef' '<' IDENT '>'
            | 'Option' '<' s-type '>'

s-params  ::= (IDENT ':' s-type (',' IDENT ':' s-type)*)?

s-args    ::= (s-expr (',' s-expr)*)?
```

**Notes.**

- `s-bguard` is the belief guard grammar. It supports conjunction, disjunction, and atomic comparisons against `confidence(id)` or `confidence(id).derivative`.
- The `believe(id, value)` expression updates the global belief store.
- `infer(expr)` and `learn(loss, rate)` are stubs for neural gate integration; their semantics are given in Section 4.

---

## 3. SAM State Space

A SAM configuration is a 6-tuple:

```
    sigma = < A, M, B, S, T, Theta >
```

where:

| Component | Domain | Description |
|-----------|--------|-------------|
| **A** | ActorId -> ActorState | Map from actor identifiers to actor states |
| **M** | ActorId -> Queue(Msg) | Mailbox: ordered queue of pending messages per actor |
| **B** | BeliefId -> Dual | Global belief store mapping belief names to dual numbers |
| **S** | ActorId -> SuspendedSet | Set of suspended receives per actor |
| **T** | R+ | Global temperature (epistemic annealing) |
| **Theta** | R^n | Learned parameters (for neural gates) |

### 3.1 Actor States

An actor state is a triple:

```
    ActorState = (code : Handler*, env : Env, fields : FieldMap)
```

where `Handler*` is the sequence of receive handlers defined by the actor, `Env` is the current local environment (bindings), and `FieldMap` maps field names to values.

An actor is in one of three modes:

```
    mode(a) in { idle, processing(msg, handler), suspended }
```

- **idle**: waiting for a message that matches some handler.
- **processing(msg, handler)**: currently executing handler body for message msg.
- **suspended**: blocked on a belief guard; will resume via WAKE.

### 3.2 Dual Numbers

Every belief confidence is a dual number:

```
    Dual = (val : R, der : R)
```

Arithmetic on duals follows the standard forward-mode autodiff rules:

```
    (a, a') + (b, b')  =  (a + b, a' + b')
    (a, a') - (b, b')  =  (a - b, a' - b')
    (a, a') * (b, b')  =  (a*b, a*b' + a'*b)
    (a, a') / (b, b')  =  (a/b, (a'*b - a*b') / b^2)     [b != 0]
    sin(a, a')          =  (sin(a), a' * cos(a))
    exp(a, a')          =  (exp(a), a' * exp(a))
```

When a belief `b` is updated from value `v_old` to `v_new` over elapsed time `dt`, the derivative is:

```
    b.der  =  (v_new - v_old) / dt
```

This derivative represents the *rate of change* of confidence and is the key value tested by derivative patterns in belief guards.

### 3.3 Suspended Receives

A suspended receive record is:

```
    Susp = (actor : ActorId, msg : Msg, pat : Pattern, guard : BGuard, body : Expr)
```

The set `S(a)` holds all pending suspended receives for actor `a`. These arise when a message matches a handler's pattern but the belief guard is not satisfied. The receive is parked until the WAKE rule fires.

### 3.4 Messages

```
    Msg = (tag : MsgTag, payload : Value*)
```

Messages are tagged tuples. Mailboxes are FIFO queues. Pattern matching against a message `m` with pattern `p` either fails or produces a substitution `theta` binding pattern variables to values:

```
    match(m, p) = theta      (success)
    match(m, p) = fail        (failure)
```

---

## 4. Transition Rules

We write transitions as:

```
    sigma --[label]--> sigma'
```

The label names the rule. We use inference rule notation with premises above the bar and conclusion below.

### 4.1 SEND -- Actor sends a message

```
    a in dom(A)      a' in dom(A)
    mode(a) = processing(_, _)
    a executes send(a', Tag(v1, ..., vn))
    m = (Tag, [v1, ..., vn])
    --------------------------------------------------  [SEND]
    < A, M, B, S, T, Theta >
        --[send(a, a', m)]-->
    < A, M[a' |-> enqueue(M(a'), m)], B, S, T, Theta >
```

**Effect:** Message `m` is appended to the tail of `a'`'s mailbox. No other component changes.

### 4.2 RECV -- Receive with satisfied guard (or no guard)

```
    a in dom(A)      mode(a) = idle
    h = receive Tag(x1, ..., xn) @ g => e    in handlers(A(a))
    m = head(M(a))
    match(m, Tag(x1, ..., xn)) = theta
    eval(g, B) = true                          [guard satisfied]
    --------------------------------------------------  [RECV]
    < A, M, B, S, T, Theta >
        --[recv(a, m)]-->
    < A[a |-> (A(a) with mode=processing(m, h), env=theta)],
      M[a |-> dequeue(M(a))],
      B, S, T, Theta >
```

After this transition, actor `a` is in mode `processing(m, h)` and the handler body `e` executes under substitution `theta`. If no guard is present (`g` is absent), `eval(g, B)` is trivially `true`.

**Handler selection.** When multiple handlers match, the first handler in declaration order whose pattern matches *and* whose guard is satisfied is selected. This is deterministic.

### 4.3 SUSPEND -- Receive with unsatisfied guard

```
    a in dom(A)      mode(a) = idle
    h = receive Tag(x1, ..., xn) @ g => e    in handlers(A(a))
    m = head(M(a))
    match(m, Tag(x1, ..., xn)) = theta
    eval(g, B) = false                         [guard NOT satisfied]
    no other handler in handlers(A(a)) matches m with satisfied guard
    --------------------------------------------------  [SUSPEND]
    < A, M, B, S, T, Theta >
        --[suspend(a, m, g)]-->
    < A[a |-> (A(a) with mode=suspended)],
      M,
      B,
      S[a |-> S(a) ∪ {(a, m, Tag(x1,...,xn), g, e)}],
      T, Theta >
```

**Effect:** The message remains in the mailbox. The actor enters `suspended` mode. The guard `g`, together with the matched message and handler body, is recorded in `S(a)`. The message is *not* dequeued -- it will be consumed when WAKE fires.

**Critical distinction from Erlang:** In Erlang, if no clause matches a message, the message remains in the mailbox and the process tries the next message. There is no notion of "suspended on a guard that may become true later." An Erlang process can only re-evaluate guards when a *new* message arrives or a timeout expires.

### 4.4 WAKE -- Belief change resumes suspended receive

```
    a in dom(A)      mode(a) = suspended
    (a, m, p, g, e) in S(a)
    B' != B                                    [beliefs have changed]
    eval(g, B') = true                         [guard NOW satisfied]
    match(m, p) = theta                        [pattern still matches]
    --------------------------------------------------  [WAKE]
    < A, M, B', S, T, Theta >
        --[wake(a, m)]-->
    < A[a |-> (A(a) with mode=processing(m, h), env=theta)],
      M[a |-> dequeue_specific(M(a), m)],
      B',
      S[a |-> S(a) \ {(a, m, p, g, e)}],
      T, Theta >
```

**WAKE is the central primitive of SAM.** It fires when:

1. An actor is suspended (not idle, not processing).
2. The global belief store has changed (due to a BELIEF-UPDATE by any actor).
3. The suspended guard, re-evaluated against the *new* beliefs, now returns true.

**Trigger mechanism.** WAKE is not polled. It is triggered by the BELIEF-UPDATE rule (Section 4.5). After any belief update, the runtime checks all suspended receives whose guards reference the changed belief. This is reactive, not periodic.

**Derivative awareness.** Because beliefs are dual numbers, the guard `confidence("x").derivative < -0.1` tests the *current rate of change*, not a historical value. The derivative is updated at each BELIEF-UPDATE. A guard may fire because the value crossed a threshold, or because the derivative crossed a threshold -- both are first-class.

### 4.5 BELIEF-UPDATE -- Actor updates a belief

```
    a in dom(A)      mode(a) = processing(_, _)
    a executes believe(id, v_new)
    b_old = B(id)                              [may be undefined; then (0, 0)]
    dt = now() - last_update_time(id)
    der = (v_new - b_old.val) / max(dt, epsilon)
    b_new = (clamp(v_new, 0, 1), der)
    W = { (a', m, p, g, e) in S(a') |
            references(g, id) and eval(g, B[id |-> b_new]) = true }
    --------------------------------------------------  [BELIEF-UPDATE]
    < A, M, B, S, T, Theta >
        --[belief(a, id, v_new)]-->
    < A, M, B[id |-> b_new], S, T, Theta >

    -- followed by WAKE for each element of W (nondeterministic order)
```

**Effect:** The belief `id` is updated to the new value with a computed derivative. The set `W` identifies all suspended receives whose guards now evaluate to true. Each element of `W` triggers a WAKE transition. The order of WAKE transitions among different actors is nondeterministic; within a single actor, at most one WAKE fires (since the actor can only be in one mode).

**Derivative computation.** The derivative `der` is the finite-difference approximation of the rate of change. Because beliefs are dual numbers, further arithmetic on beliefs (e.g., `strength()` which multiplies confidence by evidence weight) propagates derivatives via the chain rule automatically.

### 4.6 BELIEF-GUARD Evaluation

Guard evaluation is defined recursively over the `s-bguard` grammar:

```
    eval(confidence(id) cmp k, B)
        = B(id).val  [cmp]  k

    eval(confidence(id).derivative cmp k, B)
        = B(id).der  [cmp]  k

    eval(g1 && g2, B)
        = eval(g1, B) and eval(g2, B)

    eval(g1 || g2, B)
        = eval(g1, B) or eval(g2, B)
```

where `[cmp]` is the standard comparison operator and `k` is a numeric literal or expression evaluated in the current environment.

### 4.7 ANNEAL -- Temperature transition

```
    a in dom(A)      mode(a) = processing(_, _)
    a executes an action requiring stochastic choice
    T_old = T
    health = epistemic_health(B, S, A)
    T_new = schedule(T_old, health)
    --------------------------------------------------  [ANNEAL]
    < A, M, B, S, T_old, Theta >
        --[anneal]-->
    < A, M, B, S, T_new, Theta >
```

The temperature `T` governs stochastic transitions in neural gates and learning. The schedule function incorporates epistemic health signals:

```
    schedule(T, health) = base_cool(T)
                        + conflict_heat(health.source_agreement)
                        + suspicious_heat(health.confidence_velocity)
                        + failure_heat(health.predictive_accuracy)
                        + staleness_heat(health.evidence_staleness)
```

Each heat term is non-negative and represents a reheat signal. When beliefs conflict, predictions fail, confidence grows suspiciously, or evidence is stale, temperature increases to force exploration.

The acceptance probability for a stochastic transition with energy difference `delta_E` at temperature `T` is:

```
    P(accept) = sigmoid(-delta_E / T)
              = exp(-delta_E / T) / (1 + exp(-delta_E / T))
```

When `T` is represented as a dual number, the derivative `dP/dT` propagates through the acceptance function, enabling meta-gradient computation over the schedule itself.

### 4.8 LEARN -- Gradient update of parameters

```
    a in dom(A)      mode(a) = processing(_, _)
    a executes learn(loss, alpha)
    grad = gradient(loss, Theta, B)
    Theta' = Theta - alpha * grad
    --------------------------------------------------  [LEARN]
    < A, M, B, S, T, Theta >
        --[learn(a, loss)]-->
    < A, M, B, S, T, Theta' >
```

**Effect:** Learned parameters are updated via gradient descent. Because beliefs are dual numbers, the gradient computation can flow through belief values, enabling learning that is aware of the belief state. This is a semantic transition -- the learned parameters are part of the global configuration, so learning by one actor affects future inference by all actors.

---

## 5. The WAKE Rule -- Detailed Analysis

### 5.1 Formal Statement

We restate WAKE with full precision. Let:

- `sigma = < A, M, B, S, T, Theta >` be a SAM configuration.
- `a` be an actor with `mode(a) = suspended`.
- `s = (a, m, p, g, e) in S(a)` be a suspended receive.
- `sigma'` be a configuration identical to `sigma` except `B' != B` (some belief changed).
- `eval(g, B) = false` (the guard was false before the belief change).
- `eval(g, B') = true` (the guard is true after the belief change).

Then:

```
    sigma' --[wake(a, m)]--> sigma''
```

where `sigma''` is `sigma'` with:
- `mode(a) = processing(m, h)` with `h` the handler from which `s` originated
- `env(a) = match(m, p)` (the substitution from pattern matching)
- `M(a) = dequeue_specific(M(a), m)`
- `S(a) = S(a) \ {s}`

### 5.2 Causal Chain

The causal chain for WAKE is:

```
    Actor a_j executes believe(id, v)
        --> BELIEF-UPDATE fires, B changes
            --> Runtime scans S for guards referencing id
                --> For each (a_i, m, p, g, e) where eval(g, B') = true:
                    --> WAKE(a_i, m) fires
                        --> a_i transitions from suspended to processing
```

This is a *cross-actor causal chain*: actor `a_j`'s belief update causes actor `a_i`'s receive to resume. There is no message sent from `a_j` to `a_i`. The communication is mediated entirely through the shared belief store.

### 5.3 Derivative-Triggered WAKE

A particularly important case is when the guard tests the derivative:

```
    receive SensorData(d) @ confidence("obstacle").derivative < -0.1 => {
        emergency_brake()
    }
```

Here the guard fires not when the confidence *value* crosses a threshold, but when the *rate of change* crosses a threshold. This means:

- Confidence might be 0.8 (high) and stable -- guard does not fire.
- Confidence might be 0.8 (high) but dropping at -0.15/s -- guard fires.
- Confidence might be 0.3 (low) but stable -- guard does not fire.

The derivative provides trajectory information that the value alone cannot capture. This is essential for reactive systems that must anticipate state changes rather than merely react to them.

### 5.4 Formal Properties of WAKE

**Property 1 (Reactivity).** If belief `id` is updated and there exists a suspended receive with guard `g` such that `eval(g, B_old) = false` and `eval(g, B_new) = true`, then WAKE fires in the same macro-step as the BELIEF-UPDATE. There is no delay, no polling interval, and no intermediate state where the guard is true but the actor remains suspended.

**Property 2 (Atomicity).** WAKE and BELIEF-UPDATE together form an atomic macro-step. No other transition on actor `a` can interleave between the belief change and the wake. This prevents race conditions where a guard flickers between true and false.

**Property 3 (Deterministic Selection).** For a given actor with multiple suspended receives, if multiple guards become true simultaneously, the one corresponding to the earliest handler in declaration order is selected. This is deterministic.

**Property 4 (No Spurious Wake).** An actor is woken if and only if its guard transitions from false to true. If the guard was already true (which should not happen, since the actor should have matched earlier), or remains false, no WAKE fires.

---

## 6. Comparison with the Classical Actor Model

### 6.1 Classical Actor Model (Agha 1986)

In the classical actor model, an actor configuration is:

```
    sigma_classic = < A_c, M_c >
```

where `A_c` maps actor identifiers to behaviors and `M_c` maps actor identifiers to message queues. Transitions are:

```
    SEND_c:   Actor sends message to another actor's mailbox.
    RECV_c:   Actor selects a message from its mailbox matching a pattern.
    CREATE_c: Actor creates a new actor.
```

In Erlang's formulation, `RECV_c` supports *selective receive*: patterns with guards are tried against all messages in the mailbox, and the first matching message is dequeued. Guards may include arbitrary expressions, but they are evaluated only when a candidate message is tested.

### 6.2 Why WAKE Cannot Be Encoded as Selective Receive

**Claim.** There is no encoding `E` from SAM to the classical actor model such that for all SAM programs `P`:

```
    observable_behavior(P) = observable_behavior(E(P))
```

where observable behavior includes the set of messages sent and their causal ordering.

**Argument.** We show that any encoding must alter observable behavior by considering the following SAM program:

```
    actor Sensor {
        receive Reading(v: f64) {
            believe("obstacle", v)
        }
    }

    actor Controller {
        receive Command(c: String) @ confidence("obstacle").derivative < -0.1 => {
            send(actuator, EmergencyBrake())
        }
    }
```

In SAM, the following execution is possible:

1. `Controller` receives `Command("go")`, but `confidence("obstacle").derivative = 0`. Guard is false. Actor suspends.
2. `Sensor` receives `Reading(0.8)`. Belief updates: `obstacle = (0.8, 0.0)`.
3. `Sensor` receives `Reading(0.3)`. Belief updates: `obstacle = (0.3, -0.5)`.
4. WAKE fires on `Controller` because derivative `-0.5 < -0.1`.
5. `Controller` sends `EmergencyBrake()` to `actuator`.

**Key observation.** Between steps 1 and 4, no message is sent to `Controller`. The wake is triggered by a belief change caused by messages to a *different* actor (`Sensor`). Controller's behavior depends on global belief state, not on its own mailbox.

**Encoding attempts:**

**(a) Belief-monitor process.** Encode beliefs as a monitor actor that sends notification messages on every belief change. Controller receives these notifications and re-evaluates its guard.

*Problem:* This adds messages to Controller's mailbox that do not exist in the original program. The mailbox ordering changes. If Controller has other pending messages, the interleaving of belief-notification messages with application messages alters the observable message ordering. Furthermore, the monitor must send a message on *every* belief update, even when no guard would fire, creating O(updates * suspended) overhead that changes timing behavior.

**(b) Timeout-based polling.** Controller uses a receive timeout to periodically poll beliefs.

*Problem:* Introduces latency proportional to the polling interval. In SAM, WAKE fires in the same macro-step as the belief change. With polling at interval `delta`, the response latency is in `[0, delta]`. For any fixed `delta > 0`, there exist programs where the SAM version sends `EmergencyBrake()` before the polling version does, and this timing difference may cause the actuator to receive messages in a different order.

**(c) Meta-interpreter.** Implement a custom scheduler that intercepts all message dispatch and checks belief guards.

*Problem:* Requires all actors to communicate through the meta-interpreter, breaking the location transparency and modularity of the actor model. Actors can no longer be independently deployed or reasoned about. The meta-interpreter becomes a single point of failure and a serialization bottleneck, fundamentally altering the concurrent semantics.

**Conclusion.** Each encoding strategy alters at least one of: (i) the set of messages in mailboxes, (ii) the timing of transitions, (iii) the modularity of actor composition. Therefore, no faithful encoding of WAKE exists in the classical actor model.

### 6.3 The Source of Non-Encodability

The fundamental issue is that SAM has a *shared mutable belief store* that serves as an implicit communication channel between actors. Classical actors communicate exclusively via messages. SAM actors communicate via two channels:

1. **Explicit**: messages in mailboxes (as in classical actors).
2. **Implicit**: the belief store `B`, which is read by guards and written by `believe()`.

The WAKE rule couples these channels: an explicit message arrival at one actor can cause an implicit belief change that wakes a different actor's suspended receive. This cross-actor, cross-channel coupling has no analog in the classical actor model.

One might object that shared memory between actors can be simulated via message passing. This is true in general, but the WAKE rule requires the simulation to be *reactive* (no delay), *non-intrusive* (no extra messages in the application mailbox), and *modular* (actors need not know about each other's guards). Satisfying all three simultaneously is what the above argument shows to be impossible.

### 6.4 Relationship to Other Models

| Model | Shared state? | Reactive guards? | Derivative patterns? |
|-------|---------------|-------------------|----------------------|
| Classical Actors (Agha) | No | No | No |
| Erlang (Armstrong) | ETS (opt-in) | No (guards at msg arrival) | No |
| Pony | No (capabilities) | No | No |
| SOAR | Yes (working memory) | Yes (productions) | No |
| ACT-R | Yes (buffers) | Yes (productions) | No |
| **SAM** | Yes (beliefs) | Yes (WAKE) | Yes (dual numbers) |

SAM is closest to cognitive architectures like SOAR, which also have production rules that fire when working memory changes. The key differences are:

- SOAR productions are global; SAM guards are local to an actor's receive handlers.
- SOAR has no derivatives; SAM's dual-number beliefs provide automatic rate-of-change tracking.
- SOAR is single-threaded; SAM actors are concurrent.

SAM can be viewed as a concurrent, differentiable production system embedded in the actor model.

---

## 7. Properties (Proof Sketches)

### 7.1 Progress

**Theorem (Progress).** If `sigma` is a well-formed SAM configuration and `sigma` is not a final state, then there exists `sigma'` such that `sigma --> sigma'`.

*Sketch.* A configuration is non-final if some actor is not idle or some mailbox is non-empty. If an actor is in `processing` mode, it can take an internal step (SEND, BELIEF-UPDATE, LEARN, or complete processing). If an actor is `idle` with a non-empty mailbox, either RECV or SUSPEND applies. If an actor is `suspended`, it awaits a WAKE, which will be triggered by some future BELIEF-UPDATE (or the system is deadlocked on beliefs that will never change, which is a genuine deadlock -- analogous to an Erlang process waiting for a message that will never arrive).

### 7.2 Deterministic Handler Selection

**Theorem (Deterministic Selection).** For a given actor `a`, mailbox `M(a)`, and belief store `B`, the handler selected by RECV is deterministic.

*Proof.* Handlers are ordered by declaration. The message at the head of the mailbox is tested against each handler in order. The first handler whose pattern matches and whose guard is satisfied is selected. Since pattern matching and guard evaluation are deterministic functions, the selection is deterministic.

### 7.3 Belief Monotonicity of WAKE

**Theorem (No Spurious Wake).** Actor `a` transitions from `suspended` to `processing` via WAKE only if there exists a belief `id` such that:

1. `id` was updated (B changed).
2. `a` has a suspended receive with guard `g` referencing `id`.
3. `eval(g, B_old) = false` and `eval(g, B_new) = true`.

*Proof.* Follows directly from the premises of the WAKE rule. The runtime only checks guards that reference the updated belief (via the `references(g, id)` predicate in BELIEF-UPDATE), and only fires WAKE when the guard transitions from false to true.

---

## 8. Notation Summary

| Symbol | Meaning |
|--------|---------|
| `sigma` | SAM configuration `< A, M, B, S, T, Theta >` |
| `A` | Actor state map |
| `M` | Mailbox map |
| `B` | Belief store (BeliefId -> Dual) |
| `S` | Suspended receive map |
| `T` | Global temperature |
| `Theta` | Learned parameters |
| `(v, d)` or `Dual` | Dual number with value `v` and derivative `d` |
| `eval(g, B)` | Evaluate belief guard `g` against belief store `B` |
| `match(m, p)` | Pattern match message `m` against pattern `p` |
| `mode(a)` | Current mode of actor `a` (idle, processing, suspended) |
| `sigma --[l]--> sigma'` | Transition from `sigma` to `sigma'` with label `l` |
| `enqueue(q, m)` | Append message `m` to queue `q` |
| `dequeue(q)` | Remove head of queue `q` |
| `B[id |-> v]` | Belief store `B` updated with `id` mapped to `v` |

---

## 9. Future Work

This document establishes the core operational semantics. Remaining phases (per TASK-013):

- **Phase 2**: Formalize the non-encodability argument of Section 6 as a full separation proof, defining "faithful encoding" rigorously via a notion of weak bisimulation.
- **Phase 3**: Build a reference SAM interpreter (~1000 lines) that mechanizes these rules.
- **Phase 4**: Develop expressiveness examples showing programs natural in SAM that require meta-interpretation in Erlang.
- **Phase 5**: Write the POPL/OOPSLA submission combining all phases.

---

## References

- Agha, G. (1986). *Actors: A Model of Concurrent Computation in Distributed Systems.* MIT Press.
- Armstrong, J. (2003). *Making Reliable Distributed Systems in the Presence of Software Errors.* PhD Thesis, KTH.
- Baydin, A. G. et al. (2018). Automatic Differentiation in Machine Learning: A Survey. *JMLR* 18(153):1--43.
- Bratman, M. (1987). *Intention, Plans, and Practical Reason.* Harvard University Press.
- Harper, R. (2016). *Practical Foundations for Programming Languages.* Cambridge University Press, 2nd edition.
- Hewitt, C., Bishop, P., and Steiger, R. (1973). A Universal Modular ACTOR Formalism for Artificial Intelligence. *IJCAI*.
- Laird, J. (2012). *The Soar Cognitive Architecture.* MIT Press.
- Plotkin, G. (1981). A Structural Approach to Operational Semantics. Technical Report DAIMI FN-19, Aarhus University.
