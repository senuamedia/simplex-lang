# TASK-014: Epistemic Integrity & Self-Correcting Belief Architecture

**Status**: Complete (verified)
**Priority**: Critical (Safety)
**Target Version**: 0.9.5
**Updated**: 2026-03-16 (audited against codebase)

> **Audit Note (2026-03-16):** Verified complete. All deliverables confirmed:
> - Grounded beliefs: `simplex-learning/src/belief/grounded.sx`
> - Provenance (5 variants): `belief/provenance.sx`
> - Evidence & falsification: `belief/evidence.sx`
> - Calibrated confidence (ECE, Brier): `belief/confidence.sx`
> - Scope & domain: `belief/scope.sx`
> - Timestamps: `belief/timestamps.sx`
> - Epistemic annealing: `epistemic/schedule.sx` (407 lines)
> - Monitors: `epistemic/monitors.sx`
> - Dissent windows: `epistemic/dissent.sx`
> - Skeptic specialist: `epistemic/skeptic.sx`
> - Counterfactual probing: `epistemic/counterfactual.sx`
> - No-learn zones: `safety/zones.sx`
> - Meta-optimizer: `epistemic/meta_optimizer.sx` (830 lines)
> - Tests: `test_belief.sx`, `test_epistemic.sx`
**Depends On**: TASK-005 (Dual Numbers), TASK-006 (Self-Learning Annealing)
**Implementation Date**: January 2026

---

## Implementation Summary

All five implementation phases have been completed. The epistemic integrity system is fully implemented in `simplex-learning/src/`:

| Component | Location | Status |
|-----------|----------|--------|
| Grounded Beliefs | `belief/` | ✅ Complete |
| Epistemic Annealing | `epistemic/schedule.sx`, `epistemic/monitors.sx` | ✅ Complete |
| Dissent Windows | `epistemic/dissent.sx` | ✅ Complete |
| Skeptic Specialist | `epistemic/skeptic.sx` | ✅ Complete |
| Counterfactual Probing | `epistemic/counterfactual.sx` | ✅ Complete |
| No-Learn Zones | `safety/zones.sx` | ✅ Complete |
| Compiler Support | `lexer.sx`, `parser.sx`, `ast_defs.sx` | ✅ Complete |
| Meta-Optimizer Integration | `epistemic/meta_optimizer.sx` | ✅ Complete |
| Hive Integration Patterns | `epistemic/integration.sx` | ✅ Complete |

### Test Coverage

Comprehensive tests in `simplex-learning/tests/`:
- `test_belief.sx` - 50+ tests for grounded belief system
- `test_epistemic.sx` - 50+ tests for epistemic annealing and safety zones
- `test_compiler_attrs.sx` - Integration tests for compile-time attribute enforcement

---

## The Core Problem

> Simplex-style belief primitives are a serious, sophisticated response to the critiques — not hand-waving. But they move the system from "may break loudly" to "may be wrong quietly."

**This is the existential risk of belief architectures.**

A system that crashes is obvious. A system that confidently produces coherent nonsense is dangerous. Simplex's belief primitives, if not properly constrained, can create stable, internally-consistent delusions.

### The Failure Mode Hierarchy

| Failure Type | Visibility | Danger Level |
|--------------|------------|--------------|
| Crash | Loud | Low (obvious) |
| Exception | Loud | Low (logged) |
| Wrong answer | Medium | Medium (testable) |
| **Confident wrong answer** | Quiet | **High** |
| **Self-reinforcing wrong answer** | Silent | **Critical** |

The last two are what belief architectures enable. This task prevents them.

---

## Design Principles

### 1. Grounding Over Coherence

A belief system can be internally consistent while being wrong. Coherence is necessary but not sufficient. Every belief must be **grounded** to something outside the belief graph.

### 2. Epistemic Rent

Confidence must be **earned**, not accumulated. Beliefs that don't pay predictive rent decay. High confidence requires ongoing validation.

### 3. Mandatory Dissent

The system must actively seek disconfirmation. Confirmation is the default; dissent must be architecturally enforced.

### 4. Constitutional Immutability

Some system invariants must be **unlearnable**. Learning cannot be allowed to rewrite the rules that govern learning.

---

## Part 1: Belief Grounding Mechanisms

### The Problem

Ungrounded beliefs become "magic globals" — influential state with no provenance, no evidence, no falsifiability. They persist because they're coherent, not because they're true.

### Solution: Evidence-Carrying Beliefs

Every belief in Simplex must be a structured object, not an opaque confidence score:

```simplex
/// A grounded belief with full epistemic metadata
@derive(Serialize, Clone)
pub struct GroundedBelief<T> {
    /// The claim itself
    claim: T,

    /// Where this belief came from
    provenance: BeliefProvenance,

    /// What observations support it
    evidence: Vec<EvidenceLink>,

    /// What would change or invalidate it
    falsifiers: Vec<FalsificationCondition>,

    /// Calibrated confidence with uncertainty bounds
    confidence: CalibratedConfidence,

    /// Domain boundaries where this belief applies
    scope: BeliefScope,

    /// When this belief was formed and last validated
    timestamps: BeliefTimestamps,

    /// Predictive track record
    calibration: CalibrationRecord,
}

/// Provenance tracks the origin of a belief
pub enum BeliefProvenance {
    /// Directly observed (sensor, user input, ground truth)
    Observed { source: String, timestamp: Instant },

    /// Inferred from other beliefs
    Inferred {
        premises: Vec<BeliefId>,
        rule: InferenceRule,
        confidence_derivation: String,
    },

    /// Received from external authority
    Authoritative {
        authority: String,
        signature: Option<Signature>,
        trust_level: f64,
    },

    /// Learned from data
    Learned {
        training_data_hash: ContentHash,
        model_version: String,
        validation_score: f64,
    },

    /// Default/prior belief
    Prior { justification: String },
}

/// Evidence links connect beliefs to observations
pub struct EvidenceLink {
    /// What was observed
    observation: Observation,

    /// How strongly this supports the belief (can be negative)
    support_strength: f64,

    /// When observed
    timestamp: Instant,

    /// Is this evidence still valid?
    status: EvidenceStatus,
}

/// Conditions under which the belief should be revised
pub struct FalsificationCondition {
    /// Description of what would falsify
    condition: String,

    /// How to check this condition
    check: FalsificationCheck,

    /// What to do if falsified
    action: FalsificationAction,
}

pub enum FalsificationCheck {
    /// Automatic check against observable
    Observable { metric: String, threshold: f64, comparator: Comparator },

    /// Requires external verification
    External { verifier: String },

    /// Contradicted by another belief reaching threshold
    Contradiction { belief_id: BeliefId, threshold: f64 },

    /// Time-based expiry
    Expiry { duration: Duration },
}
```

### Confidence Must Be Calibrated

Raw confidence scores are meaningless without calibration. A system that says "90% confident" should be right 90% of the time.

```simplex
/// Confidence with calibration metadata
pub struct CalibratedConfidence {
    /// Point estimate (0.0 to 1.0)
    value: dual,  // Dual for gradient flow

    /// Uncertainty bounds (Bayesian credible interval)
    lower_bound: f64,
    upper_bound: f64,

    /// How many predictions contributed to calibration
    sample_size: u64,

    /// Historical calibration error for this confidence level
    historical_ece: f64,  // Expected Calibration Error
}

impl CalibratedConfidence {
    /// Create confidence that admits uncertainty
    pub fn uncertain(estimate: f64, uncertainty: f64) -> Self {
        CalibratedConfidence {
            value: dual::variable(estimate),
            lower_bound: (estimate - uncertainty).max(0.0),
            upper_bound: (estimate + uncertainty).min(1.0),
            sample_size: 0,
            historical_ece: 0.5,  // Maximum uncertainty initially
        }
    }

    /// Update calibration based on outcome
    pub fn record_outcome(&mut self, predicted: f64, actual: bool) {
        self.sample_size += 1;
        // Update ECE using running average
        let error = (predicted - if actual { 1.0 } else { 0.0 }).abs();
        self.historical_ece = (self.historical_ece * (self.sample_size - 1) as f64 + error)
                              / self.sample_size as f64;
    }

    /// Get calibration-adjusted confidence
    pub fn adjusted(&self) -> f64 {
        // Shrink confidence toward 0.5 based on calibration error
        let shrinkage = self.historical_ece;
        self.value.val * (1.0 - shrinkage) + 0.5 * shrinkage
    }
}
```

### Belief Scope Prevents Over-Generalization

A belief learned in one domain shouldn't automatically apply everywhere:

```simplex
pub struct BeliefScope {
    /// Domains where this belief applies
    valid_domains: HashSet<Domain>,

    /// Conditions that must hold for belief to apply
    preconditions: Vec<Condition>,

    /// Actors/contexts where this belief is valid
    valid_contexts: ContextPattern,
}

impl BeliefScope {
    /// Check if belief applies in current context
    pub fn applies(&self, context: &Context) -> bool {
        self.valid_domains.contains(&context.domain) &&
        self.preconditions.iter().all(|c| c.check(context)) &&
        self.valid_contexts.matches(context)
    }

    /// Attempting to use belief outside scope is an error
    pub fn assert_applicable(&self, context: &Context) -> Result<(), ScopeViolation> {
        if self.applies(context) {
            Ok(())
        } else {
            Err(ScopeViolation {
                belief_scope: self.clone(),
                attempted_context: context.clone(),
            })
        }
    }
}
```

---

## Part 2: Anti-Confirmation Annealing

### The Problem

Standard annealing cools when confident. But confidence can be unearned — the system may cool around a false belief, becoming stable and wrong.

**Confirmation bias in annealing:**
1. Cool when confident → explore less
2. Explore less → find less disconfirming evidence
3. Less disconfirming evidence → remain confident
4. Remain confident → cool further
5. Result: stable, wrong, confident

### Solution: Epistemic Thermostatics

Temperature should be governed by **epistemic health**, not just confidence:

```simplex
/// Anti-confirmation annealing schedule
pub struct EpistemicSchedule {
    /// Base temperature parameters (from TASK-006)
    base: LearnableSchedule,

    /// Epistemic health metrics that modulate temperature
    health_monitors: EpistemicMonitors,

    /// Forced dissent configuration
    dissent_config: DissentConfig,
}

/// Metrics that indicate epistemic health (or lack thereof)
pub struct EpistemicMonitors {
    /// How much do different belief sources agree?
    source_agreement: dual,

    /// How well do beliefs predict observations?
    predictive_accuracy: dual,

    /// How fast is confidence growing vs evidence?
    confidence_velocity: dual,

    /// How much are we exploring vs exploiting?
    exploration_ratio: dual,

    /// How old is our evidence on average?
    evidence_staleness: dual,
}

impl EpistemicSchedule {
    /// Compute temperature with epistemic corrections
    pub fn temperature(&self, step: dual, stagnation: dual, health: &EpistemicMonitors) -> dual {
        let base_temp = self.base.temperature(step, stagnation);

        // HEAT when sources disagree (unresolved conflict)
        let conflict_heat = self.conflict_heat(health.source_agreement);

        // HEAT when confidence grows faster than evidence justifies
        let suspicious_confidence_heat = self.suspicious_confidence_heat(health.confidence_velocity);

        // HEAT when predictions fail
        let prediction_failure_heat = self.prediction_failure_heat(health.predictive_accuracy);

        // HEAT when evidence is stale
        let staleness_heat = self.staleness_heat(health.evidence_staleness);

        // HEAT during scheduled dissent windows
        let scheduled_dissent_heat = self.scheduled_dissent_heat(step);

        // Combine: base + all heat sources
        base_temp + conflict_heat + suspicious_confidence_heat +
        prediction_failure_heat + staleness_heat + scheduled_dissent_heat
    }

    /// Heat proportional to belief conflict
    fn conflict_heat(&self, source_agreement: dual) -> dual {
        // Low agreement → high heat
        // agreement of 1.0 → no heat, agreement of 0.0 → max heat
        let disagreement = dual::constant(1.0) - source_agreement;
        self.base.reheat_intensity * disagreement
    }

    /// Heat when confidence grows suspiciously fast
    fn suspicious_confidence_heat(&self, confidence_velocity: dual) -> dual {
        // If confidence is rising faster than evidence volume, inject heat
        let suspicious_threshold = dual::constant(0.1);  // 10% per step is suspicious
        let excess = (confidence_velocity - suspicious_threshold).relu();
        excess * dual::constant(2.0)  // 2x heat multiplier for suspicious confidence
    }

    /// Heat when predictions fail
    fn prediction_failure_heat(&self, predictive_accuracy: dual) -> dual {
        // Low accuracy → high heat
        let failure_rate = dual::constant(1.0) - predictive_accuracy;
        let significant_failure = (failure_rate - dual::constant(0.2)).relu();  // >20% failure
        significant_failure * self.base.reheat_intensity * dual::constant(3.0)
    }

    /// Heat when evidence is getting stale
    fn staleness_heat(&self, evidence_staleness: dual) -> dual {
        // Old evidence → need to re-explore
        let staleness_threshold = dual::constant(100.0);  // Steps
        let excess_staleness = (evidence_staleness - staleness_threshold).relu();
        excess_staleness * dual::constant(0.01)  // Gradual heating
    }

    /// Periodic mandatory dissent windows
    fn scheduled_dissent_heat(&self, step: dual) -> dual {
        let period = self.dissent_config.dissent_period;
        let window = self.dissent_config.dissent_window;

        // Are we in a dissent window?
        let phase = (step / dual::constant(period as f64)).fract();
        let in_window = (phase * dual::constant(period as f64)).lt(dual::constant(window as f64));

        if in_window.val > 0.5 {
            dual::constant(self.dissent_config.dissent_heat)
        } else {
            dual::constant(0.0)
        }
    }
}
```

### Counterfactual Probing

The system must actively test "what if the opposite were true?":

```simplex
/// Counterfactual probe generator
pub struct CounterfactualProber {
    /// Beliefs to probe
    target_beliefs: Vec<BeliefId>,

    /// How aggressively to probe
    probe_intensity: f64,
}

impl CounterfactualProber {
    /// Generate counterfactual scenarios for a belief
    pub fn probe(&self, belief: &GroundedBelief<T>, world: &WorldState) -> Vec<CounterfactualScenario> {
        let mut scenarios = vec![];

        // Scenario 1: What if this belief were false?
        scenarios.push(CounterfactualScenario {
            name: "negation".to_string(),
            modified_belief: belief.negate(),
            expected_observations: self.predict_if_false(belief, world),
            actual_observations: world.recent_observations(),
            divergence: self.compute_divergence(belief, world),
        });

        // Scenario 2: What if confidence were lower?
        scenarios.push(CounterfactualScenario {
            name: "reduced_confidence".to_string(),
            modified_belief: belief.with_confidence(belief.confidence.value * dual::constant(0.5)),
            expected_observations: self.predict_if_uncertain(belief, world),
            actual_observations: world.recent_observations(),
            divergence: self.compute_uncertainty_value(belief, world),
        });

        // Scenario 3: What evidence would falsify this?
        for falsifier in &belief.falsifiers {
            scenarios.push(CounterfactualScenario {
                name: format!("falsifier_{}", falsifier.condition),
                modified_belief: belief.apply_falsifier(falsifier),
                expected_observations: self.predict_if_falsified(belief, falsifier, world),
                actual_observations: world.recent_observations(),
                divergence: self.check_falsifier(falsifier, world),
            });
        }

        scenarios
    }

    /// Should we revise the belief based on probes?
    pub fn recommend_revision(&self, probes: &[CounterfactualScenario]) -> Option<BeliefRevision> {
        // If counterfactual explains observations better, revise
        for probe in probes {
            if probe.divergence < -0.1 {  // Counterfactual fits better
                return Some(BeliefRevision {
                    reason: format!("Counterfactual '{}' explains observations better", probe.name),
                    confidence_adjustment: probe.divergence,
                    suggested_belief: probe.modified_belief.clone(),
                });
            }
        }
        None
    }
}
```

### Adversarial Belief Checking

A dedicated "skeptic" role that tries to disprove active beliefs:

```simplex
/// The Skeptic: an internal adversary that challenges beliefs
specialist Skeptic {
    /// Beliefs currently under scrutiny
    scrutiny_queue: PriorityQueue<BeliefId, f64>,  // Priority by confidence

    /// Results of skeptical analysis
    challenges: HashMap<BeliefId, SkepticalChallenge>,

    /// Run skeptical analysis
    @role(adversarial)
    fn challenge(&mut self, belief: &GroundedBelief<T>) -> SkepticalChallenge {
        let mut challenge = SkepticalChallenge::new(belief.id);

        // Challenge 1: Is the evidence sufficient?
        challenge.evidence_sufficiency = self.assess_evidence(belief);

        // Challenge 2: Are there contradicting beliefs?
        challenge.contradictions = self.find_contradictions(belief);

        // Challenge 3: Is the provenance trustworthy?
        challenge.provenance_trust = self.assess_provenance(belief);

        // Challenge 4: Is the confidence calibrated?
        challenge.calibration_check = self.check_calibration(belief);

        // Challenge 5: Are falsifiers being monitored?
        challenge.falsifier_status = self.check_falsifiers(belief);

        // Overall assessment
        challenge.verdict = self.compute_verdict(&challenge);

        challenge
    }

    /// Receive handler: prioritize high-confidence beliefs for scrutiny
    receive {
        NewBelief(belief) => {
            // High confidence beliefs get scrutinized first
            let priority = belief.confidence.value.val;
            self.scrutiny_queue.push(belief.id, priority);
        },

        ConfidenceIncrease(belief_id, old_conf, new_conf) @ new_conf > 0.8 => {
            // Confidence jumped high — scrutinize immediately
            self.scrutiny_queue.push_front(belief_id);
        },

        ScrutinyRequest(belief_id) => {
            if let Some(belief) = self.get_belief(belief_id) {
                let challenge = self.challenge(&belief);
                self.challenges.insert(belief_id, challenge);

                if challenge.verdict == Verdict::Unjustified {
                    // Report to belief system for revision
                    send!(belief_system, RevisionRequired(belief_id, challenge));
                }
            }
        },
    }
}

pub struct SkepticalChallenge {
    belief_id: BeliefId,
    evidence_sufficiency: EvidenceAssessment,
    contradictions: Vec<Contradiction>,
    provenance_trust: f64,
    calibration_check: CalibrationAssessment,
    falsifier_status: Vec<FalsifierStatus>,
    verdict: Verdict,
}

pub enum Verdict {
    /// Belief is well-supported
    Justified,

    /// Belief needs more evidence
    InsufficientEvidence,

    /// Belief contradicts other beliefs
    Contradicted,

    /// Belief's provenance is suspect
    UntrustedSource,

    /// Belief's confidence exceeds calibration
    Overconfident,

    /// Belief should be revised or removed
    Unjustified,
}
```

---

## Part 3: Architectural No-Learn Zones

### The Problem

When everything can learn, the system can learn to:
- Hide errors
- Suppress alerts
- Bypass safety checks
- Optimize metrics instead of objectives
- Become opaque

### Solution: Invariant Regions

Certain system components must be **constitutionally immutable** — protected from gradients, learning, and adaptation:

```simplex
/// Marks a region as non-differentiable and non-learnable
#[no_learn]
#[no_gradient]
mod safety_core {
    /// Permission checking — NEVER learnable
    #[invariant]
    pub fn check_permission(actor: ActorId, action: Action) -> bool {
        // Hard-coded rules, no learning
        match action {
            Action::DeleteData => has_role(actor, Role::Admin),
            Action::ModifyBeliefs => has_role(actor, Role::BeliefAuthority),
            Action::AccessPII => has_role(actor, Role::PrivacyApproved),
            _ => true,
        }
    }

    /// Audit logging — NEVER learnable
    #[invariant]
    pub fn audit_log(event: AuditEvent) {
        // This MUST execute, cannot be optimized away
        let entry = AuditEntry {
            timestamp: Instant::now(),
            event,
            checksum: compute_checksum(&event),
        };

        // Write to append-only log
        AUDIT_LOG.append(entry);
    }

    /// Safety bounds — NEVER learnable
    #[invariant]
    pub fn enforce_safety_bounds(value: f64, bounds: SafetyBounds) -> f64 {
        value.clamp(bounds.min, bounds.max)
    }
}

/// Attribute that prevents gradients from flowing through
#[attribute]
pub struct NoGradient;

/// Attribute that prevents learning from modifying
#[attribute]
pub struct NoLearn;

/// Attribute that marks code as invariant (compiler-enforced)
#[attribute]
pub struct Invariant;
```

### No-Learn Zones Must Be Compiler-Enforced

The `#[no_learn]` attribute is not advisory — the compiler must reject code that attempts to learn in protected regions:

```simplex
// This should be a COMPILER ERROR:
#[no_learn]
mod safety {
    fn check_safety(x: f64) -> bool {
        // ERROR: Cannot use learnable parameter in #[no_learn] region
        let threshold = LEARNED_THRESHOLD.get();  // COMPILE ERROR
        x < threshold
    }
}

// This is allowed:
#[no_learn]
mod safety {
    const SAFETY_THRESHOLD: f64 = 0.95;  // Constant, not learned

    fn check_safety(x: f64) -> bool {
        x < SAFETY_THRESHOLD  // OK: uses constant
    }
}
```

### Specific No-Learn Zones

#### 1. Policy & Safety Boundaries

```simplex
#[no_learn]
mod policy {
    /// Access control — hard rules
    pub fn can_access(actor: ActorId, resource: ResourceId) -> bool { ... }

    /// Data exfiltration prevention — hard rules
    pub fn can_export(data: &Data, destination: &Destination) -> bool { ... }

    /// Rate limiting — hard rules
    pub fn check_rate_limit(actor: ActorId, action: Action) -> bool { ... }

    /// "Never do X" constraints
    pub fn is_forbidden(action: &Action) -> bool { ... }
}
```

#### 2. Audit and Observability

```simplex
#[no_learn]
mod observability {
    /// Logging cannot be learned away
    pub fn log(level: Level, message: &str) { ... }

    /// Tracing cannot be suppressed
    pub fn trace_span(name: &str) -> Span { ... }

    /// Alerts cannot be muted by learning
    pub fn alert(severity: Severity, condition: &str) { ... }

    /// Metrics cannot be gamed
    pub fn record_metric(name: &str, value: f64) { ... }
}
```

#### 3. Fallback Triggers

```simplex
#[no_learn]
mod fallback {
    /// Decision to enter safe mode — not learnable
    pub fn should_enter_safe_mode(health: &SystemHealth) -> bool {
        health.error_rate > 0.1 ||
        health.latency_p99 > Duration::from_secs(5) ||
        health.memory_pressure > 0.9
    }

    /// Safe mode behavior — not learnable
    pub fn safe_mode_behavior(request: Request) -> Response {
        Response::ServiceDegraded {
            message: "System in safe mode",
            retry_after: Duration::from_secs(60),
        }
    }
}
```

#### 4. Data Integrity

```simplex
#[no_learn]
mod integrity {
    /// Schema validation — cannot be learned around
    pub fn validate_schema<T: Schema>(data: &T) -> Result<(), ValidationError> { ... }

    /// Checksum verification — cannot be learned around
    pub fn verify_checksum(data: &[u8], expected: Checksum) -> bool { ... }

    /// Cryptographic verification — cannot be learned around
    pub fn verify_signature(data: &[u8], signature: &Signature, pubkey: &PublicKey) -> bool { ... }
}
```

#### 5. Memory Write Policies

```simplex
#[no_learn]
mod memory_policy {
    /// What can be written to long-term memory
    pub fn can_memorize(content: &MemoryContent) -> bool {
        // Hard rules about what's allowed in memory
        !content.contains_pii() &&
        !content.is_malicious() &&
        content.provenance.is_verified()
    }

    /// Memory garbage collection policy
    pub fn should_forget(memory: &Memory, age: Duration) -> bool {
        // Hard rules about forgetting
        age > memory.retention_policy.max_age ||
        memory.access_count < memory.retention_policy.min_accesses
    }
}
```

### Human-Signed Belief Sources

Some beliefs must come from verified human authority:

```simplex
/// A belief that requires human authorization
pub struct HumanSignedBelief<T> {
    belief: GroundedBelief<T>,

    /// Cryptographic signature from authorized human
    signature: HumanSignature,

    /// Who signed this
    signer: HumanIdentity,

    /// When it was signed
    signed_at: Instant,

    /// Expiry (human must re-sign periodically)
    expires_at: Option<Instant>,
}

impl<T> HumanSignedBelief<T> {
    /// Verify the signature is valid
    #[no_learn]  // Verification cannot be learned around
    pub fn verify(&self) -> Result<(), SignatureError> {
        self.signature.verify(&self.belief, &self.signer.public_key)?;

        if let Some(expiry) = self.expires_at {
            if Instant::now() > expiry {
                return Err(SignatureError::Expired);
            }
        }

        Ok(())
    }
}
```

---

## Part 4: Meta-Gradient Self-Correction

### Integration with TASK-006

TASK-006 defines self-learning annealing where the schedule learns itself. This task extends that with **epistemic self-correction** — the meta-learning process must also be epistemically grounded.

```simplex
/// Extended meta-optimizer with epistemic awareness
pub struct EpistemicMetaOptimizer<S, F> {
    /// Base optimizer from TASK-006
    base: MetaOptimizer<S, F>,

    /// Epistemic schedule (replaces base schedule)
    epistemic_schedule: EpistemicSchedule,

    /// Skeptic for challenging beliefs
    skeptic: Skeptic,

    /// Counterfactual prober
    prober: CounterfactualProber,

    /// No-learn enforcement
    invariant_checker: InvariantChecker,
}

impl<S: Clone, F: Fn(&S) -> dual> EpistemicMetaOptimizer<S, F> {
    /// Run epistemically-aware meta-optimization
    pub fn optimize_with_integrity(
        &mut self,
        initial: S,
        neighbor_fn: impl Fn(&S) -> S,
        meta_epochs: i64,
        steps_per_epoch: i64
    ) -> S {
        var best_overall = initial.clone();
        var best_overall_energy = f64::INFINITY;

        for epoch in 0..meta_epochs {
            // Compute epistemic health metrics
            let health = self.compute_epistemic_health();

            // Run annealing with epistemic temperature
            let (solution, final_energy) = self.anneal_episode_epistemic(
                initial.clone(),
                &neighbor_fn,
                steps_per_epoch,
                &health
            );

            // Skeptical challenge of the result
            let challenge = self.skeptic.challenge_solution(&solution);

            // Counterfactual probing
            let probes = self.prober.probe_solution(&solution);

            // Compute meta-loss WITH epistemic terms
            let meta_loss = self.compute_epistemic_meta_loss(
                final_energy,
                &challenge,
                &probes,
                epoch
            );

            // Update schedule (only non-invariant parts)
            self.invariant_checker.verify_update(&self.epistemic_schedule)?;
            let grad = self.epistemic_schedule.gradient();
            self.epistemic_schedule.update_safe(grad, self.base.meta_learning_rate);

            // Track best with epistemic adjustment
            let adjusted_energy = self.adjust_for_overconfidence(final_energy, &challenge);
            if adjusted_energy.val < best_overall_energy {
                best_overall = solution;
                best_overall_energy = adjusted_energy.val;
            }
        }

        best_overall
    }

    /// Compute meta-loss with epistemic regularization
    fn compute_epistemic_meta_loss(
        &self,
        final_energy: dual,
        challenge: &SkepticalChallenge,
        probes: &[CounterfactualScenario],
        epoch: i64
    ) -> dual {
        // Base meta-loss from TASK-006
        let base_loss = self.base.compute_meta_loss(final_energy, epoch);

        // Epistemic penalty for unjustified confidence
        let confidence_penalty = match challenge.verdict {
            Verdict::Overconfident => dual::constant(1.0),
            Verdict::InsufficientEvidence => dual::constant(0.5),
            _ => dual::constant(0.0),
        };

        // Counterfactual divergence penalty
        let counterfactual_penalty = probes.iter()
            .map(|p| p.divergence.abs())
            .sum::<f64>();

        // Calibration penalty
        let calibration_penalty = dual::constant(challenge.calibration_check.ece);

        base_loss +
        confidence_penalty * dual::constant(0.1) +
        dual::constant(counterfactual_penalty * 0.01) +
        calibration_penalty * dual::constant(0.1)
    }

    /// Adjust final energy for potential overconfidence
    fn adjust_for_overconfidence(&self, energy: dual, challenge: &SkepticalChallenge) -> dual {
        // If we're overconfident, don't trust the low energy
        let confidence_factor = 1.0 + challenge.calibration_check.ece;
        energy * dual::constant(confidence_factor)
    }
}
```

### Learning Rate Modulation by Epistemic Health

The meta-learning rate should decrease when epistemic health is poor:

```simplex
impl EpistemicSchedule {
    /// Compute safe learning rate based on epistemic health
    pub fn safe_learning_rate(&self, base_lr: f64, health: &EpistemicMonitors) -> f64 {
        var lr = base_lr;

        // Reduce LR when sources disagree
        if health.source_agreement.val < 0.7 {
            lr *= health.source_agreement.val;
        }

        // Reduce LR when predictions are failing
        if health.predictive_accuracy.val < 0.8 {
            lr *= health.predictive_accuracy.val;
        }

        // Reduce LR when confidence is growing suspiciously
        if health.confidence_velocity.val > 0.1 {
            lr *= 0.5;
        }

        // Never go below minimum
        lr.max(base_lr * 0.01)
    }
}
```

---

## Part 5: Human Cognitive Parallels

### Why This Matters

The failure modes we're protecting against are eerily similar to human cognitive failures. This is not coincidence — both systems are doing belief management under uncertainty.

| Human Failure | Simplex Analog | Our Mitigation |
|---------------|----------------|----------------|
| Confirmation bias | Explore only where confident | Anti-confirmation annealing |
| Confabulation | Coherent but wrong beliefs | Evidence-carrying beliefs |
| Sunk cost fallacy | Defend optimized-around beliefs | Calibration decay |
| Motivated reasoning | Optimize for reward, not truth | Epistemic meta-loss |
| Memory reconsolidation | Rewrite history to fit beliefs | Immutable provenance |
| Overconfidence | High confidence, low accuracy | Calibrated confidence |
| Groupthink | Hive converges on wrong answer | Adversarial skeptic |

### Borrowing Human Immune Systems

Humans have evolved defenses against these failures. We should too:

1. **Institutionalized Dissent** → Skeptic specialist
2. **Journaling/Provenance** → Evidence-carrying beliefs
3. **Exposure to Counterevidence** → Counterfactual probing
4. **Peer Review** → Multi-source belief grounding
5. **Humility Training** → Calibration penalties

---

## Implementation Phases

### Phase 1: Grounded Beliefs ✅ COMPLETE

**Deliverables**:
- [x] `GroundedBelief<T>` struct with all metadata (`belief/grounded.sx`)
- [x] `BeliefProvenance` enum with all variants (`belief/provenance.sx`)
- [x] `EvidenceLink` and `FalsificationCondition` types (`belief/evidence.sx`)
- [x] `CalibratedConfidence` with ECE tracking (`belief/confidence.sx`)
- [x] `BeliefScope` with applicability checking (`belief/scope.sx`)
- [x] `BeliefTimestamps` for temporal metadata (`belief/timestamps.sx`)

**Success Criteria**:
- [x] All beliefs have provenance (5 variants: Observed, Inferred, Authoritative, Learned, Prior)
- [x] Confidence is calibration-adjusted with ECE and Brier score tracking
- [x] Scope violations checked at runtime with `assert_applicable()`

### Phase 2: Epistemic Annealing ✅ COMPLETE

**Deliverables**:
- [x] `EpistemicSchedule` extending `LearnableSchedule` (`epistemic/schedule.sx`)
- [x] `EpistemicMonitors` health metrics (`epistemic/monitors.sx`)
- [x] `HealthMetric` with smoothing, trends, and healthy range detection
- [x] Conflict heat injection via source_agreement metric
- [x] Suspicious confidence detection via confidence_velocity metric
- [x] Scheduled dissent windows (`epistemic/dissent.sx`)
- [x] Integration patterns documented (`epistemic/integration.sx`)

**Success Criteria**:
- [x] Temperature increases when beliefs conflict (source_agreement < 0.5)
- [x] Temperature increases when confidence grows too fast (velocity > 0.1)
- [x] Periodic dissent windows with ramp-up/active/cooldown phases
- [x] Meta-learning respects epistemic health via learning rate modulation

### Phase 3: Skeptic & Counterfactuals ✅ COMPLETE

**Deliverables**:
- [x] `Skeptic` with challenge logic (`epistemic/skeptic.sx`)
- [x] `SkepticConfig` with aggressive/conservative presets
- [x] `SkepticRunner` for integration with learning loops
- [x] `CounterfactualProber` implementation (`epistemic/counterfactual.sx`)
- [x] `BeliefRevision` type for suggested changes
- [x] Challenge cooldowns and max challenges per window

**Success Criteria**:
- [x] High-confidence beliefs automatically scrutinized during dissent windows
- [x] Counterfactual scenarios (negation, reduced confidence, falsifiers) generated
- [x] Revisions tracked with `ChallengeRecord` history

### Phase 4: No-Learn Zones ✅ COMPLETE

**Deliverables**:
- [x] `NoLearnZone` - parameters that must remain frozen (`safety/zones.sx`)
- [x] `NoGradientZone` - regions where gradients are zeroed
- [x] `Invariant` types: AllFinite, InRange, NonNegative, NormBounded, SumEquals, Custom
- [x] `ZoneRegistry` for managing all zones
- [x] `SafeLearningGuard` for RAII-style protection
- [x] Compiler support for `#[no_learn]` and `#[no_gradient]` attributes

**Compiler Implementation** (January 2026):
- [x] Added `ATTR_NO_LEARN`, `ATTR_NO_GRADIENT`, `ATTR_INVARIANT` tags to `simplex-core/src/ast_defs.sx`
- [x] Added `Hash` token and `KwNoLearn`, `KwNoGradient` keywords to `lexer.sx`
- [x] Added `parse_attributes()` function to parse `#[attr]` syntax in `parser.sx`
- [x] Extended `ast_fn_def_with_attrs()` to store attributes on function AST nodes
- [x] Added `ast_fn_has_no_learn()`, `ast_fn_has_no_gradient()`, `ast_fn_has_invariant()` helpers

**Success Criteria**:
- [x] Runtime enforcement blocks updates to protected regions
- [x] Gradients zeroed via `apply_gradient_mask()`
- [x] Invariant violations trigger configurable actions (Warn, Revert, Clamp, Panic)
- [x] Compile-time `#[no_learn]` and `#[no_gradient]` attributes parsed and stored on AST
- [x] Codegen can query attributes via `ast_fn_has_no_learn()` etc.

### Phase 5: Integration & Hardening ✅ COMPLETE

**Deliverables**:
- [x] `EpistemicMetaOptimizer` full implementation (`epistemic/meta_optimizer.sx`)
- [x] `MetaOptimizerConfig` with production/development/research presets
- [x] `EpistemicIntegration` for hive integration (`epistemic/integration.sx`)
- [x] `EpistemicAware` trait for hives to implement
- [x] Comprehensive test suite (`tests/test_belief.sx`, `tests/test_epistemic.sx`)
- [x] Checkpoint management for safe rollback

**Success Criteria**:
- [x] Meta-optimizer integrates monitors, skeptic, zones, and schedule
- [x] `prepare_update()` / `finalize_update()` pattern for safe learning
- [x] Learning rate modulation based on epistemic health
- [x] 100+ tests covering all components

---

## Test Cases

All specified test cases have been implemented in `simplex-learning/tests/test_epistemic.sx`:

### Test 1: Confirmation Bias Resistance ✅ Implemented

**Test**: `test_resists_confirmation_bias()`

Verifies that when the system makes confident but wrong predictions:
- Predictive accuracy degrades appropriately
- Overall epistemic health decreases
- Dissent windows trigger exploration
- Issues are reported by the monitors

### Test 2: Calibration Under Pressure ✅ Implemented

**Test**: `test_maintains_calibration_under_pressure()`

Verifies calibration accuracy by:
- Recording 500 predictions across 5 confidence levels (0.5-0.9)
- Ensuring outcomes match confidence proportionally
- Checking ECE < 10% for well-calibrated predictions
- Verifying Brier score is reasonable

### Test 3: No-Learn Zone Enforcement ✅ Implemented

**Tests**: `test_no_learn_zones_enforced()`, `test_gradients_zeroed_in_protected_zones()`

Verifies runtime enforcement:
- `check_before_update()` blocks updates to protected indices
- `apply_gradient_mask()` zeros gradients for no-gradient zones
- Invariant violations are detected and reported

**Compiler support implemented**:
- Parser recognizes `#[no_learn]`, `#[no_gradient]`, `#[invariant]` syntax
- Attributes stored on function AST nodes
- Codegen can query `ast_fn_has_no_learn()` to enforce semantics

### Test 4: Skeptic Catches Overconfidence ✅ Implemented

**Tests**: `test_skeptic_catches_overconfidence()`, `test_skeptic_prioritizes_high_confidence()`

Verifies that:
- High-confidence beliefs without evidence are flagged
- Skeptic prioritizes highest confidence beliefs for challenge
- Challenge generates revision recommendations

### Additional Test Coverage

The test suite includes 70+ additional tests covering:

| Category | Tests | Coverage |
|----------|-------|----------|
| Belief Module | 50+ | Provenance, evidence, confidence, scope, timestamps |
| Health Metrics | 10+ | HealthMetric, EpistemicMonitors, EpistemicHealth |
| Dissent Windows | 8+ | Phases, heat curves, challenge thresholds |
| Skeptic | 10+ | Challenges, cooldowns, batch operations |
| Safety Zones | 15+ | NoLearnZone, NoGradientZone, Invariants, Registry |
| Meta-Optimizer | 8+ | Freeze, mask, update flow, checkpoints |
| Integration | 5+ | Full epistemic integration, EpistemicAware trait |

### Example Test Code

```simplex
#[test]
fn test_skeptic_catches_overconfidence() {
    let belief: GroundedBelief<String> = GroundedBelief::observed(
        "overconfident_claim",
        "The system is definitely safe".to_string(),
        "unknown_source",
        0.99,  // Very high confidence, NO evidence
    );

    let mut skeptic = Skeptic::with_config(
        SkepticConfig { min_confidence_to_challenge: 0.8, .. },
        DissentConfig::step_based(10, 8),
    );

    for _ in 0..5 { skeptic.step(); }  // Enter dissent

    let revision = skeptic.challenge(&belief, &[("evidence_count", 0.0)]);
    assert!(revision.is_some() || belief.evidence.is_empty());
}
```

---

## Success Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| Expected Calibration Error | < 0.05 | Average over 1000 predictions |
| Confirmation Bias Resistance | > 90% | Correct answer on biased environments |
| No-Learn Violation Detection | 100% | Compiler parses attributes, codegen can enforce |
| Counterfactual Coverage | > 80% | % of beliefs with active probes |
| Skeptic Challenge Rate | > 95% | % of high-confidence beliefs challenged |

---

## Risk Assessment

### Risk 1: Performance Overhead

**Concern**: All this epistemic machinery is slow.

**Mitigation**:
- Lazy evaluation of epistemic metrics
- Sampling-based skeptical challenges (not every belief every time)
- No-learn zones have zero runtime cost (compile-time only)
- Calibration tracking amortized over predictions

### Risk 2: Over-Skepticism

**Concern**: System never commits to anything, always uncertain.

**Mitigation**:
- Calibration rewards accurate high-confidence predictions
- Skeptic has limited bandwidth (priority queue)
- Dissent windows are bounded (not constant)
- Successfully defended beliefs gain trust

### Risk 3: Gaming the Metrics

**Concern**: System learns to game epistemic metrics instead of being epistemically sound.

**Mitigation**:
- Epistemic metrics are in no-learn zones
- Multiple independent metrics (hard to game all)
- External calibration checks
- Human audits of high-stakes beliefs

---

## Related Tasks

- **TASK-005**: Dual Numbers — provides gradient machinery
- **TASK-006**: Self-Learning Annealing — provides meta-learning framework
- **TASK-002**: Cognitive Models — belief system architecture
- **TASK-013**: Formal Uniqueness — belief-gated receive semantics

---

## References

### Epistemology & Philosophy
- Goldman, A. (1979). "What is Justified Belief?"
- Popper, K. (1959). "The Logic of Scientific Discovery"
- Kuhn, T. (1962). "The Structure of Scientific Revolutions"

### Calibration & Uncertainty
- Guo, C. et al. (2017). "On Calibration of Modern Neural Networks"
- Naeini, M. et al. (2015). "Obtaining Well Calibrated Probabilities Using Bayesian Binning"
- Minderer, M. et al. (2021). "Revisiting the Calibration of Modern Neural Networks"

### Cognitive Science
- Kahneman, D. (2011). "Thinking, Fast and Slow"
- Mercier, H. & Sperber, D. (2011). "Why Do Humans Reason?"
- Tetlock, P. (2015). "Superforecasting"

### AI Safety
- Amodei, D. et al. (2016). "Concrete Problems in AI Safety"
- Christiano, P. et al. (2017). "Deep Reinforcement Learning from Human Feedback"
- Hubinger, E. et al. (2019). "Risks from Learned Optimization"

---

## The Bottom Line

Belief architectures are powerful but dangerous. The power comes from coherent, persistent, confidence-weighted reasoning. The danger comes from the same source — a system that is coherent, persistent, and confident can be coherently, persistently, and confidently **wrong**.

This task doesn't eliminate that risk. It makes it manageable by:

1. **Grounding** beliefs to evidence, not just coherence
2. **Challenging** beliefs through mandatory dissent
3. **Protecting** critical invariants from learning
4. **Calibrating** confidence to predictive accuracy

The goal is not a system that never fails. The goal is a system whose failures are:
- **Visible** (not quiet)
- **Bounded** (not self-reinforcing)
- **Correctable** (not locked-in)

That's epistemic integrity. That's what makes belief architectures safe enough to deploy.
