# Release 0.16.0 — Mathematical Intelligence

**Codename:** Mathematical Intelligence
**Status:** Planned
**Theme:** Advanced mathematical AI primitives that make Simplex the definitive language
for building governed, efficient, interpretable AI systems.

## Vision

Every major advance in AI comes from better mathematics, not more compute. v0.16.0 makes
Simplex the only language where cutting-edge mathematical AI is *native* — not bolted on
through libraries, but compiled down to efficient code with formal guarantees.

**Tagline:** *AI that proves it's right, explains why, and runs on a phone.*

## Priority-Ordered Task List

### P0 — Critical (Must Ship)

These three features form the foundation of v0.16.0. They address the three biggest
problems in deployed AI: hallucination, efficiency, and scalability.

| Order | Task | Feature | Lines | Why Critical |
|-------|------|---------|-------|-------------|
| 1 | TASK-036 | Conformal Prediction | ~1,500 | Hallucination governance with mathematical guarantees — no other language has this |
| 2 | TASK-037 | Mixture of Experts | ~1,800 | 10x parameter efficiency via sparse activation — the architecture behind every frontier model |
| 3 | TASK-038 | State Space Models | ~1,800 | O(n) sequence processing for edge SLMs — linear scaling replaces quadratic transformers |

### P1 — High (Should Ship)

These three features provide the mathematical depth that differentiates Simplex from
every other AI language. They make training faster, models smaller, and reasoning causal.

| Order | Task | Feature | Lines | Why Important |
|-------|------|---------|-------|-------------|
| 4 | TASK-039 | Information Geometry | ~1,500 | 10-100x faster training convergence via natural gradient on statistical manifolds |
| 5 | TASK-040 | KAN Layers | ~1,500 | 100x smaller specialists + models that compile to symbolic formulas |
| 6 | TASK-041 | Causal Inference | ~1,650 | Root-cause hallucination prevention — SLMs that reason about cause, not correlation |

### P2 — Medium (Target Ship)

These two features complete the governance and adaptivity story. They depend on the P0/P1
foundations and provide the verification and efficiency layers.

| Order | Task | Feature | Lines | Why Valuable |
|-------|------|---------|-------|-------------|
| 7 | TASK-042 | Mechanistic Interpretability | ~1,600 | Verify what models learned — audit circuits, not just accuracy metrics |
| 8 | TASK-043 | Neural ODEs | ~1,650 | Adaptive computation depth — models that decide how much to think per input |

## Implementation Order & Dependencies

```
Phase 1 (P0 — Foundation):
  TASK-036 Conformal Prediction ─────────────────────┐
  TASK-037 Mixture of Experts ───────────────────────┤
  TASK-038 State Space Models ───────────────────────┤
                                                      │
Phase 2 (P1 — Depth):                                │
  TASK-039 Information Geometry ─────────────────────┤
  TASK-040 KAN Layers ──────────────────────────────┤
  TASK-041 Causal Inference ─────────────────────────┤
                                                      │
Phase 3 (P2 — Governance & Adaptivity):              │
  TASK-042 Mechanistic Interpretability ◄────────────┘
  TASK-043 Neural ODEs ◄─────────────────────────────┘
```

**Key dependencies:**
- TASK-042 (Interpretability) benefits from TASK-037 (MoE) and TASK-038 (SSM) being
  complete, as it needs models to interpret
- TASK-043 (Neural ODEs) benefits from TASK-039 (Information Geometry) for geometric
  ODE sensitivity analysis
- TASK-037 (MoE) and TASK-038 (SSM) can be built in parallel
- TASK-040 (KAN) integrates with TASK-037 (MoE) via KANExpert — build KAN after MoE core
- TASK-041 (Causal) integrates with TASK-036 (Conformal) via causal conformal prediction

## Total Estimated Scope

| Category | Lines |
|----------|-------|
| P0 features | ~5,100 |
| P1 features | ~4,650 |
| P2 features | ~3,250 |
| **Total** | **~13,000** |

## Competitive Position After v0.16.0

| Capability | Python/PyTorch | Rust | Mojo | Julia | **Simplex** |
|-----------|---------------|------|------|-------|-------------|
| Conformal prediction | Library (MAPIE) | None | None | None | **First-class type** |
| MoE routing | Manual code | None | None | None | **Neural gate** |
| State space models | Custom CUDA | None | None | None | **Compiled layer** |
| Natural gradient | Rare libraries | None | None | Partial | **Built-in optimizer** |
| KAN + symbolic extraction | Library (pykan) | None | None | None | **Compiles to formula** |
| Causal inference | Library (DoWhy) | None | None | None | **Language operator** |
| Mechanistic interpretability | Ad-hoc scripts | None | None | None | **Compiler annotation** |
| Neural ODEs | Library (torchdiffeq) | None | None | DiffEq.jl | **Adaptive layer** |
| Safety contracts | None | None | None | None | **Type system** |
| Cognitive hive | None | None | None | None | **Architecture** |

## What This Means for Simplex Users

After v0.16.0, a Simplex developer can:

1. **Build a specialist SLM** using MoE (sparse, efficient) with SSM layers (linear
   scaling) — runs on a phone
2. **Train it faster** with natural gradient (10x fewer steps) on KAN layers (100x fewer
   parameters)
3. **Guarantee its outputs** with conformal prediction (statistical coverage proofs)
4. **Prevent hallucination** with causal gates (interventional reasoning, not correlation)
5. **Verify what it learned** with mechanistic interpretability (circuit extraction)
6. **Optimize its compute** with Neural ODEs (adaptive depth per query)
7. **Extract its knowledge** as symbolic formulas (KAN symbolic extraction)
8. **Deploy with governance** through contracts, beliefs, and conformal bounds

No other language offers any three of these together. Simplex offers all eight.
