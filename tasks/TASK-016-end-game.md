# TASK-016: End Game - Millennium Prize Validation

**Status**: Not Started (blocked on TASK-015)
**Priority**: Ultimate (This Is Why Simplex Exists)
**Target Version**: 1.0.0 (The Proof)
**Codebase**: `/Users/rod/code/codex` (Clean slate - purpose-built)
**Updated**: 2026-03-16 (audited against codebase)

> **Audit Note (2026-03-16):** 0% implemented. No Codex codebase exists at `/Users/rod/code/codex`.
> No deliverables present: no git commit protocol, no EC2 deployment, no web UI,
> no artifact generator, no seed corpus loader, no proof verification engine,
> no public audit trail. Blocked on TASK-015 (Simplex-Core SLM).
**Depends On**:
- TASK-015 (Simplex-Core SLM) - Native cognitive persistence
- TASK-014 (Belief Epistemics) - ✅ Complete
- TASK-006 (Self-Learning Annealing) - ✅ Complete
- TASK-007 (Pure Simplex Training) - Training pipeline
- Phase 4 (The Anima) - Cognitive soul architecture

> **This is the break point.** If Simplex can solve a Millennium Prize Problem through continuous self-improvement, it validates everything - the language, the runtime, the cognitive architecture, the vision. If it works, it changes everything about how we think about computation and AI.

---

## The Thesis

Simplex is designed to be a **continuously self-improving cognitive system**. Not a tool that humans use. Not an assistant that answers questions. A system that:

1. **Runs continuously** - Days, weeks, months, years
2. **Improves its own code** - Rewrites algorithms, heuristics, proof strategies
3. **Evolves its own models** - SLMs that get better at mathematical reasoning
4. **Generates its own training data** - Creates conjectures, proof attempts, counterexamples
5. **Scales with compute** - More hardware = more exploration = faster convergence

**The Millennium Prize Problems are the ultimate test.** They are:
- Objectively verifiable (a proof is correct or it isn't)
- Unsolved by humans (success is undeniable)
- Require sustained deep reasoning (not pattern matching)
- Worth $1M each (real stakes)

If Simplex can do this, it proves the architecture is sound. If it can't, we learn where the limits are.

---

## The Codex System

**Location**: `/Users/rod/code/codex`
**Purpose**: The execution engine for End Game - built from scratch for this specific mission.

### Repositories

| Repository | Purpose | Contents |
|------------|---------|----------|
| `senuamedia/codex-log` | Codex engine + audit trail | Source code, HTML artifacts, evolution log |
| `senuamedia/simplex-codex` | Simplex language modifications | Compiler, runtime, stdlib changes by Codex |

Codex is not a coding assistant. It is the **mathematical reasoning engine** that will:
1. Run continuously, exploring the problem space
2. Generate mathematical artifacts (notes, theories, formulas, proofs)
3. Document all self-improvement in real-time
4. Produce a public, verifiable audit trail
5. **Modify its own source code** (codex-log)
6. **Modify the Simplex language** (simplex-codex) when new capabilities are needed

---

## Git Commit Protocol

### Commit Message Format

All commits MUST follow this format:

```
Codex log: <summary>

<detailed description>

Codex is built with Simplex
```

**Rules:**
- Prefix: Always `Codex log:` (never any other attribution)
- Summary: Brief description of what changed
- Description: Detailed explanation (can be multi-paragraph)
- Footer: Always ends with `Codex is built with Simplex`
- **No references** to Claude, Claude Code, AI assistants, or any external tools

### Example Commits

```
Codex log: Generated conjecture C-00042 regarding circuit lower bounds

Explored relationship between circuit depth and P/NP separation.
Generated new conjecture based on analysis of Theorem T-00023.

Evidence:
- Numerical verification up to n=10^6
- Consistent with known barriers (relativization, natural proofs)

Confidence: 0.73
Status: Under investigation

Codex is built with Simplex
```

```
Codex log: Self-modified proof search heuristic v003

Previous heuristic (v002) showed diminishing returns on depth-first exploration.
Analysis of 10,000 proof attempts revealed suboptimal branching factor.

Changes:
- Increased breadth-first component from 0.3 to 0.45
- Added backtracking threshold based on confidence decay
- Performance improvement: +12% successful lemma proofs

Codex is built with Simplex
```

### GitHub Integration

Codex uses a long-lived Personal Access Token (PAT) to autonomously push updates to both repositories:

```
Codex Repository:   github.com/senuamedia/codex-log
Simplex Repository: github.com/senuamedia/simplex-codex
Branch: main (both)
Token: Stored in .codex/config.toml (gitignored)
```

The PAT requires:
- `Contents` permission (read/write) - for pushing commits
- `Metadata` permission (read) - for repository access
- Scope: Classic PAT with `repo` scope (for cross-org access)
- Expiration: No expiration (for continuous autonomous operation)

---

## EC2 Deployment Architecture

**Instance**: `i-0fdc9f1576ed6fc11` (reused from TASK-015)
**Region**: ap-southeast-2
**Simplex**: Already built and deployed

For true self-improvement, both codebases must be deployed on the EC2 instance:

```
┌─────────────────────────────────────────────────────────────────────┐
│  EC2: i-0fdc9f1576ed6fc11                                           │
│                                                                     │
│  ┌─────────────────────┐         ┌─────────────────────┐           │
│  │  /opt/simplex/      │◄───────►│  /opt/codex/        │           │
│  │  (simplex-codex)    │ modify  │  (codex-log)        │           │
│  │                     │         │                     │           │
│  │  - Compiler (sxc)   │         │  - Engine           │           │
│  │  - Runtime          │         │  - Specialists      │           │
│  │  - Stdlib           │         │  - Evolution        │           │
│  │  - Learning libs    │         │  - Output           │           │
│  └──────────┬──────────┘         └──────────┬──────────┘           │
│             │                               │                       │
│             │ compile                       │ execute               │
│             ▼                               ▼                       │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                   Running Codex Process                      │   │
│  │                                                              │   │
│  │  1. Reason about mathematics                                 │   │
│  │  2. Identify capability gaps                                 │   │
│  │  3. Modify code:                                             │   │
│  │     - /opt/codex/* → push to codex-log                      │   │
│  │     - /opt/simplex/* → push to simplex-codex                │   │
│  │  4. Recompile with new Simplex if modified                  │   │
│  │  5. Hot-reload or restart with improved capabilities        │   │
│  │  6. Continue operation                                       │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                              │                                      │
│                              ▼                                      │
│                     Push to GitHub                                  │
│              (codex-log AND simplex-codex)                         │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Self-Modification Workflow

1. **Capability Gap Identified**
   - Codex discovers it needs a feature (e.g., new proof tactic, better memory management)

2. **Determine Scope**
   - If Codex-level: modify `/opt/codex/src/*.sx`
   - If Simplex-level: modify `/opt/simplex/lib/*.sx` or compiler

3. **Implement Change**
   - Write new code with full documentation
   - Include reasoning trace in commit message

4. **Validate Change**
   - Run tests (if Simplex-level)
   - Verify no regressions

5. **Commit and Push**
   - Codex-log: `Codex log: <change>`
   - Simplex-codex: `Codex log: <change>`

6. **Recompile if Needed**
   - If Simplex modified: `sxc build /opt/codex`
   - Reload or restart process

7. **Continue with Enhanced Capabilities**

---

## Live Web UI

Codex serves a real-time web interface showing its progress. Two viewing options:

### Option 1: Live UI (Real-time Streaming)

Served directly from EC2 via HTTP/SSE:

```
┌─────────────────────────────────────────────────────────────────────┐
│  EC2: i-0fdc9f1576ed6fc11                                           │
│                                                                     │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────────────┐   │
│  │   Codex     │────►│  Web Server │────►│  Browser            │   │
│  │  Engine     │     │  :443       │     │                     │   │
│  │             │     │             │     │  - Dashboard        │   │
│  │  Generates: │     │  Serves:    │     │  - Live timeline    │   │
│  │  - HTML     │     │  - Static   │     │  - Artifact browser │   │
│  │  - Events   │     │  - SSE      │     │  - Git log viewer   │   │
│  └─────────────┘     └─────────────┘     │  - Belief graph     │   │
│                                          └─────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

**Features:**
- **SSE Stream** (`/api/events`) - Real-time event feed
- **Dashboard** (`/`) - Current status, metrics, progress
- **Timeline** (`/timeline`) - Chronological activity with live updates
- **Artifacts** (`/artifacts`) - Browse conjectures, theorems, proofs
- **Evolution** (`/evolution`) - Code/model/dataset changes
- **Git Log** (`/git`) - Recent commits to both repos

### Option 2: GitHub (Permanent Record)

All artifacts and commits are pushed to GitHub for:
- Permanent, immutable record
- Browse history via GitHub UI
- View diffs and evolution
- Third-party verification

| Repository | URL |
|------------|-----|
| Codex + Audit Trail | https://github.com/senuamedia/codex-log |
| Simplex Modifications | https://github.com/senuamedia/simplex-codex |

### Web Server Architecture

```simplex
// Codex serves its own web UI using simplex-http
fn start_web_server(port: u16) {
    let server = HttpServer::new()
        .bind(("0.0.0.0", port))
        .route("/", get(dashboard))
        .route("/api/events", get(sse_stream))
        .route("/timeline", get(timeline))
        .route("/timeline/:date", get(timeline_day))
        .route("/artifacts/conjectures", get(conjectures))
        .route("/artifacts/conjectures/:id", get(conjecture_detail))
        .route("/artifacts/theorems", get(theorems))
        .route("/evolution/code", get(code_evolution))
        .route("/evolution/models", get(model_evolution))
        .route("/git", get(git_log))
        .route("/api/beliefs.json", get(beliefs_json))
        .route("/api/metrics.json", get(metrics_json))
        .static_files("/static", "./output/static");

    server.run();
}

// SSE stream for real-time updates
fn sse_stream(req: Request) -> Response {
    Response::sse(|tx| {
        loop {
            if let Some(event) = event_queue.recv() {
                tx.send(SseEvent {
                    event: event.type,
                    data: event.to_json(),
                    id: event.id,
                });
            }
        }
    })
}
```

### Real-time Event Types

| Event | Description | Example |
|-------|-------------|---------|
| `conjecture` | New conjecture generated | `{id: "C-00042", confidence: 0.73}` |
| `proof_attempt` | Proof attempt started/ended | `{conjecture: "C-00042", status: "in_progress"}` |
| `theorem` | Conjecture proven | `{id: "T-00015", from_conjecture: "C-00042"}` |
| `counterexample` | Conjecture disproven | `{conjecture: "C-00039", counterexample: {...}}` |
| `evolution` | Code/model/dataset change | `{type: "code", version: "v003"}` |
| `checkpoint` | System checkpoint saved | `{epoch: 1000, beliefs: 5432}` |
| `git_push` | Changes pushed to GitHub | `{repo: "codex-log", commits: 3}` |

---

## The Harness: Public Audit Trail

### Core Requirement

Every action, every thought, every improvement must be **publicly documented** with **cryptographic timestamps**. This proves:
- The work was done by the system (not humans)
- When discoveries were made
- How the system evolved
- The complete chain of reasoning

### HTML5 Public Output

Codex generates a **live, browsable website** documenting the entire journey:

```
codex-output/
├── index.html                    # Dashboard: current status, progress metrics
├── timeline/
│   ├── index.html               # Chronological view of all activity
│   ├── 2025-01-17/
│   │   ├── index.html           # Day's summary
│   │   ├── 00-00-00-conjecture-001.html
│   │   ├── 00-15-32-proof-attempt-001.html
│   │   ├── 01-42-18-insight-discovered.html
│   │   └── ...
│   └── ...
├── artifacts/
│   ├── conjectures/             # All generated conjectures
│   │   ├── C-00001.html         # Conjecture with status, confidence, evidence
│   │   ├── C-00002.html
│   │   └── index.html           # Conjecture browser
│   ├── theorems/                # Proven results
│   │   ├── T-00001.html         # Theorem with full proof
│   │   └── index.html
│   ├── lemmas/                  # Supporting lemmas
│   ├── counterexamples/         # Disproven conjectures
│   ├── techniques/              # Discovered proof techniques
│   └── dead-ends/               # Failed approaches (valuable!)
├── evolution/
│   ├── code/
│   │   ├── index.html           # Code evolution timeline
│   │   ├── v001/                # Version snapshots
│   │   ├── v002/
│   │   └── diffs/               # What changed and why
│   ├── models/
│   │   ├── index.html           # Model evolution timeline
│   │   ├── checkpoint-001/      # Model checkpoints with metrics
│   │   └── training-logs/       # Training runs
│   └── datasets/
│       ├── index.html           # Dataset evolution
│       ├── generated/           # Self-generated examples
│       └── curation-log/        # What was added/removed and why
├── beliefs/
│   ├── index.html               # Current belief state
│   ├── knowledge-graph.html     # Visual belief network
│   └── confidence-calibration.html
├── metrics/
│   ├── index.html               # Performance dashboard
│   ├── compute-usage.html       # Resource utilization
│   ├── progress-rate.html       # Improvement over time
│   └── capability-tests.html    # Benchmark results
└── api/
    ├── beliefs.json             # Machine-readable belief dump
    ├── timeline.json            # Event stream
    └── metrics.json             # Current metrics
```

### Artifact Format

Each mathematical artifact is a self-contained HTML document:

```html
<!DOCTYPE html>
<html>
<head>
    <title>Conjecture C-00042: [Title]</title>
    <meta name="timestamp" content="2025-01-17T14:32:18.847Z">
    <meta name="hash" content="sha256:a1b2c3d4...">
    <meta name="confidence" content="0.73">
    <meta name="status" content="under_investigation">
</head>
<body>
    <header>
        <h1>Conjecture C-00042</h1>
        <div class="metadata">
            <span class="timestamp">2025-01-17 14:32:18 UTC</span>
            <span class="confidence">Confidence: 73%</span>
            <span class="status">Under Investigation</span>
        </div>
    </header>

    <section class="statement">
        <h2>Statement</h2>
        <div class="formal">
            <!-- LaTeX/MathML rendering -->
            ∀ P ∈ NP: ∃ polynomial p such that...
        </div>
        <div class="informal">
            <!-- Plain English explanation -->
            For every problem in NP, there exists a polynomial-time verifier...
        </div>
    </section>

    <section class="provenance">
        <h2>How This Was Generated</h2>
        <ul>
            <li>Generated by: ConjectureGenerator v12</li>
            <li>Seed beliefs: [links to supporting beliefs]</li>
            <li>Generation method: Analogical reasoning from T-00023</li>
        </ul>
    </section>

    <section class="evidence">
        <h2>Evidence</h2>
        <h3>Supporting</h3>
        <ul>
            <li><a href="...">Lemma L-00018</a> (confidence: 0.91)</li>
            <li><a href="...">Numerical verification up to n=10^6</a></li>
        </ul>
        <h3>Against</h3>
        <ul>
            <li>No counterexamples found (searched: 10^9 cases)</li>
        </ul>
    </section>

    <section class="proof-attempts">
        <h2>Proof Attempts</h2>
        <ul>
            <li><a href="...">Attempt 1</a>: Direct construction - FAILED (stuck at step 7)</li>
            <li><a href="...">Attempt 2</a>: Contradiction - IN PROGRESS</li>
        </ul>
    </section>

    <section class="connections">
        <h2>Related</h2>
        <ul>
            <li><a href="...">Implies: C-00056</a></li>
            <li><a href="...">Implied by: T-00012</a></li>
            <li><a href="...">Similar to: C-00039</a></li>
        </ul>
    </section>

    <footer>
        <div class="verification">
            SHA-256: a1b2c3d4e5f6...
            Timestamped: 2025-01-17T14:32:18.847Z
        </div>
    </footer>
</body>
</html>
```

### Timestamp Verification

Every artifact includes:
1. **ISO 8601 timestamp** - When it was created
2. **SHA-256 hash** - Content integrity
3. **Optional**: Blockchain anchor or trusted timestamping service (RFC 3161)

This creates an **unforgeable audit trail** proving when discoveries were made.

---

## Self-Improvement Documentation

### Code Evolution

When Codex modifies its own code:

```
evolution/code/
├── v001/
│   ├── source/                  # Full source snapshot
│   ├── metadata.json            # Version info, metrics
│   └── README.html              # What this version does
├── v002/
│   ├── source/
│   ├── metadata.json
│   ├── README.html
│   └── diff-from-v001.html      # What changed
└── changes/
    ├── change-001.html
    │   ├── What changed
    │   ├── Why (reasoning trace)
    │   ├── Expected improvement
    │   ├── Actual improvement (measured)
    │   └── Diff
    └── ...
```

### Model Evolution

When Codex retrains or fine-tunes its models:

```
evolution/models/
├── checkpoint-001/
│   ├── weights/                 # Model weights (or reference)
│   ├── config.json              # Architecture, hyperparameters
│   ├── training-log.html        # Loss curves, metrics
│   ├── capability-eval.html     # Before/after benchmarks
│   └── README.html              # What this training run achieved
└── training-runs/
    ├── run-001/
    │   ├── dataset-used.html    # What data was used
    │   ├── objective.html       # What we were optimizing for
    │   ├── results.html         # What happened
    │   └── decision.html        # Keep/discard and why
    └── ...
```

### Dataset Evolution

When Codex generates or curates training data:

```
evolution/datasets/
├── seed/                        # Initial mathematical corpus
│   ├── axioms.json              # Foundational truths (confidence: 1.0)
│   ├── theorems.json            # Proven results (confidence: 1.0)
│   └── sources.html             # Where this came from
├── generated/
│   ├── batch-001/
│   │   ├── examples.json        # Generated examples
│   │   ├── generation-method.html
│   │   └── quality-assessment.html
│   └── ...
└── curation-log/
    ├── addition-001.html        # Added examples and why
    ├── removal-001.html         # Removed examples and why
    └── rebalancing-001.html     # Dataset rebalancing decisions
```

---

## Seed Datasets

### Mathematical Truths (100% Confidence)

The system starts with human mathematical knowledge as seed beliefs:

```simplex
// Axioms - foundational, cannot be proven, confidence = 1.0
belief ZFC_Axioms {
    axiom_of_extensionality: "∀A ∀B (∀x (x∈A ↔ x∈B) → A=B)",
    axiom_of_pairing: "∀a ∀b ∃c ∀x (x∈c ↔ x=a ∨ x=b)",
    // ... all ZFC axioms
    confidence: 1.0,
    provenance: Axiomatic,
}

// Proven theorems - established results, confidence = 1.0
belief FundamentalTheoremOfArithmetic {
    statement: "Every integer > 1 is either prime or a unique product of primes",
    confidence: 1.0,
    provenance: Proven {
        proof_reference: "Euclid, Elements, Book VII",
        verified_by: ["mathematical_community"],
    },
}

// Complexity theory foundations
belief PClass {
    definition: "Problems solvable in polynomial time by a DTM",
    confidence: 1.0,
    provenance: Definition,
}

belief NPClass {
    definition: "Problems verifiable in polynomial time by a DTM",
    confidence: 1.0,
    provenance: Definition,
}

belief P_subset_NP {
    statement: "P ⊆ NP",
    confidence: 1.0,
    provenance: Proven {
        proof: "Trivial: polynomial-time solver is also polynomial-time verifier",
    },
}

// The target - unknown confidence
belief P_equals_NP {
    statement: "P = NP",
    confidence: 0.5,  // Unknown - this is what we're trying to determine
    provenance: Conjecture,
    status: OpenProblem,
}
```

### Corpus Structure

```
seed-data/
├── foundations/
│   ├── logic/
│   │   ├── propositional.json
│   │   ├── first-order.json
│   │   └── higher-order.json
│   ├── set-theory/
│   │   ├── zfc-axioms.json
│   │   └── ordinals-cardinals.json
│   └── number-theory/
│       ├── peano-axioms.json
│       └── fundamental-theorems.json
├── complexity-theory/
│   ├── classes/
│   │   ├── p.json
│   │   ├── np.json
│   │   ├── co-np.json
│   │   ├── pspace.json
│   │   └── exp.json
│   ├── reductions/
│   │   ├── polynomial-reductions.json
│   │   └── np-complete-problems.json
│   ├── barriers/
│   │   ├── relativization.json
│   │   ├── natural-proofs.json
│   │   └── algebrization.json
│   └── techniques/
│       ├── diagonalization.json
│       ├── padding.json
│       └── simulation.json
├── proof-techniques/
│   ├── direct-proof.json
│   ├── contradiction.json
│   ├── induction.json
│   ├── construction.json
│   └── probabilistic.json
└── literature/
    ├── papers/                  # Key papers in machine-readable format
    ├── textbooks/               # Standard references
    └── surveys/                 # State-of-the-art surveys
```

---

## What's Blocking End Game

### Current Blockers (Must Resolve)

| Blocker | Description | Dependency | Status |
|---------|-------------|------------|--------|
| **Simplex 0.10.0** | Core language features needed | Phase 1-4 | In Progress |
| **simplex-core SLM** | Native cognitive model | TASK-015 | Design Phase |
| **Formal math types** | Type system for proofs | New requirement | Not Started |
| **Proof verification** | Validate claimed proofs | New requirement | Not Started |
| **HTML5 generator** | Public output system | New requirement | Not Started |
| **Seed corpus** | Mathematical knowledge base | New requirement | Not Started |

### Technical Requirements

1. **Language Features Needed**:
   - Full belief system with epistemic metadata (TASK-014 ✅)
   - Self-learning annealing (TASK-006 ✅)
   - Actor/Hive system for parallel exploration
   - Self-modification capabilities (safe code evolution)

2. **Toolchain Requirements**:
   - Compiler that can compile Simplex from Simplex
   - Hot-reload for code evolution
   - Model training pipeline (TASK-007)

3. **New Components to Build**:
   - Formal mathematics representation
   - Proof verification engine
   - Conjecture generator
   - HTML5 artifact generator
   - Timestamp/audit system

---

## The Millennium Prize Problems

Seven problems, six remain unsolved:

| Problem | Domain | Status | Simplex Fit |
|---------|--------|--------|-------------|
| **P vs NP** | Computational Complexity | Unsolved | ⭐⭐⭐⭐⭐ Perfect |
| **Riemann Hypothesis** | Number Theory | Unsolved | ⭐⭐⭐⭐ Strong |
| **Yang-Mills Existence** | Mathematical Physics | Unsolved | ⭐⭐⭐ Moderate |
| **Navier-Stokes Existence** | PDEs/Fluid Dynamics | Unsolved | ⭐⭐⭐ Moderate |
| **Hodge Conjecture** | Algebraic Geometry | Unsolved | ⭐⭐ Lower |
| **Birch & Swinnerton-Dyer** | Number Theory | Unsolved | ⭐⭐⭐⭐ Strong |
| ~~Poincaré Conjecture~~ | Topology | ✅ Solved | N/A |

### Recommended Target: P vs NP

**Why P vs NP is ideal:**
1. Computational self-reference - Simplex reasoning about computation
2. Discrete structure - Amenable to systematic exploration
3. Rich literature - Abundant training signal
4. Clear barriers - We know what approaches don't work
5. Intermediate value - Even partial progress advances the field

---

## Architecture: Codex Structure

```
codex/
├── src/
│   ├── main.sx                  # Entry point - continuous operation loop
│   ├── config.sx                # Configuration and scaling
│   │
│   ├── core/
│   │   ├── beliefs.sx           # Belief system integration
│   │   ├── memory.sx            # Knowledge graph storage
│   │   └── persistence.sx       # Checkpoint/restore
│   │
│   ├── math/
│   │   ├── types.sx             # Formal mathematical types
│   │   ├── expressions.sx       # Expression representation
│   │   ├── proofs.sx            # Proof objects
│   │   └── verification.sx      # Proof checker
│   │
│   ├── specialists/
│   │   ├── conjecture.sx        # Conjecture generator
│   │   ├── prover.sx            # Proof search
│   │   ├── counterexample.sx    # Counterexample hunter
│   │   ├── critic.sx            # Skeptic/validator
│   │   └── curator.sx           # Dataset curator
│   │
│   ├── evolution/
│   │   ├── code.sx              # Code self-modification
│   │   ├── model.sx             # Model training loop
│   │   └── dataset.sx           # Dataset generation
│   │
│   ├── output/
│   │   ├── html.sx              # HTML5 generator
│   │   ├── timeline.sx          # Event logging
│   │   ├── artifacts.sx         # Artifact management
│   │   └── timestamp.sx         # Cryptographic timestamps
│   │
│   └── hive.sx                  # Hive orchestration
│
├── seed-data/                   # Mathematical corpus
│   ├── foundations/
│   ├── complexity-theory/
│   └── literature/
│
├── output/                      # Generated HTML5 (public)
│   ├── index.html
│   ├── timeline/
│   ├── artifacts/
│   └── evolution/
│
├── checkpoints/                 # System state snapshots
│
├── Modulus.toml                 # Project manifest
└── README.md
```

---

## Validation Criteria

### Ultimate Success: Proof Accepted

1. Valid mathematical proof - logically correct
2. Peer reviewed by mathematical community
3. Published in major journal
4. Clay Institute acceptance

### Intermediate Success

| Metric | Description | Target |
|--------|-------------|--------|
| **Novel lemmas** | New proven results | > 10 |
| **New barriers** | Understanding of problem structure | > 3 |
| **Technique transfer** | Methods applied to other problems | > 5 |
| **Self-improvement** | Measurable capability increase | Positive trend |
| **Public engagement** | Community interest | Active discussion |

### The Public Experiment

If you choose to share:
- Live website showing real-time progress
- Complete audit trail from day one
- Transparent methodology
- Reproducible results
- Community can verify claims

---

## Development Phases

### Phase 1: Foundation (Current)
- [ ] Clear codex codebase ✅
- [ ] Define architecture
- [ ] Implement core belief system integration
- [ ] Create HTML5 output generator
- [ ] Build seed corpus loader

### Phase 2: Specialists
- [ ] Conjecture generator
- [ ] Proof search engine
- [ ] Counterexample hunter
- [ ] Verification system

### Phase 3: Self-Improvement Loop
- [ ] Code evolution framework
- [ ] Model training integration
- [ ] Dataset genesis

### Phase 4: Hardening
- [ ] Continuous operation (weeks of uptime)
- [ ] Checkpoint/restore
- [ ] Monitoring and alerting

### Phase 5: Launch
- [ ] Seed with mathematical corpus
- [ ] Begin continuous operation
- [ ] Publish public output
- [ ] Monitor and iterate

---

## The Stakes

This is not a research project. This is not an experiment. This is **validation of a vision**:

> **Can a system built on actors, epistemic beliefs, and continuous self-improvement solve problems that humans cannot?**

If yes: Simplex becomes the foundation for a new kind of intelligence.

If no: We learn exactly why, and what would be needed instead.

Either way, we advance human knowledge. The public audit trail ensures the attempt itself has value regardless of outcome.

**This is the end game.**

---

## References

- [Clay Mathematics Institute - Millennium Prize Problems](https://www.claymath.org/millennium-problems)
- [P vs NP Problem - Official Description](https://www.claymath.org/millennium-problems/p-vs-np-problem)
- TASK-014: Belief Epistemics
- TASK-015: Simplex-Core SLM
- TASK-006: Self-Learning Annealing
- Phase 4: The Anima
