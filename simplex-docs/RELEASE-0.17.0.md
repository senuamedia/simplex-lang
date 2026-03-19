# Release 0.17.0 — Cognitive Substrate

**Codename:** Cognitive Substrate
**Status:** Planned
**Theme:** Reinvent the SLM. Stop shrinking big models — build architectures that are
natively brilliant when small.

## Vision

Every SLM today is a shrunken transformer. Same architecture, fewer parameters, worse
at everything. v0.17.0 rejects this entirely. Instead of making transformers smaller,
we build fundamentally different computational substrates designed from the ground up
for specialist intelligence at 100M-2B parameters.

The Cognitive Substrate is eight interlocking technologies that give small models
capabilities that large transformers cannot match:

- **Dynamic weights** that rewire per input (not frozen after training)
- **Curved knowledge spaces** that represent hierarchy in 1/20th the dimensions
- **Actual reasoning** via neural-symbolic hybrid (not pattern matching)
- **Exponential memory** that stores knowledge outside weights (not crammed into parameters)
- **Exponential compression** via tensor decomposition (not lossy pruning)
- **Built-in symmetry** that never wastes parameters learning known invariances
- **Lifelong learning** that never forgets (not frozen at deployment)
- **Parallel generation** that produces entire responses at once (not one token at a time)

**Tagline:** *A 500M model that outperforms a 70B transformer. Not by accident — by architecture.*

## Priority-Ordered Task List

### P0 — Critical (The Core Substrate)

These four features form the new SLM architecture. They replace the fundamental building
blocks: how the model computes (liquid), how it represents knowledge (hyperbolic), how
it reasons (neuro-symbolic), and how it remembers (Hopfield).

| Order | Task | Feature | Lines | Why Critical |
|-------|------|---------|-------|-------------|
| 1 | TASK-044 | Liquid Neural Networks | ~1,650 | Dynamic weights — 19 neurons drove a car, 1.2B matches 1.7B+ |
| 2 | TASK-045 | Hyperbolic Embeddings | ~1,700 | 10-50x embedding dimension reduction for hierarchical knowledge |
| 3 | TASK-046 | Neuro-Symbolic Engine | ~1,900 | Models that actually reason, not pattern-match — hallucination elimination |
| 4 | TASK-047 | Hopfield Associative Memory | ~1,650 | Exponential knowledge storage outside weights — small model, vast knowledge |

### P1 — High (Efficiency & Learning)

These three features make the substrate practical: exponential compression for deployment,
symmetry for parameter efficiency, and lifelong learning for continuous improvement.

| Order | Task | Feature | Lines | Why Important |
|-------|------|---------|-------|-------------|
| 5 | TASK-048 | Tensor Network Compression | ~1,700 | 10-1000x compression via mathematical decomposition, not lossy pruning |
| 6 | TASK-049 | Geometric Equivariant Layers | ~1,650 | Don't learn known symmetries — build them in, save parameters |
| 7 | TASK-050 | Sparse Distributed Memory | ~1,650 | Lifelong learning without forgetting — specialists that get smarter daily |

### P2 — Medium (Generation Paradigm)

This feature changes how specialists produce output — from sequential to parallel.

| Order | Task | Feature | Lines | Why Valuable |
|-------|------|---------|-------|-------------|
| 8 | TASK-051 | Diffusion Language Generation | ~1,600 | 10x faster parallel generation, global coherence, native editing |

## Implementation Order & Dependencies

```
Phase 1 (P0 — New SLM Foundation):
  TASK-044 Liquid Neural Networks ─────────────────┐
  TASK-045 Hyperbolic Embeddings ──────────────────┤
  TASK-046 Neuro-Symbolic Engine ──────────────────┤
  TASK-047 Hopfield Associative Memory ────────────┤
                                                    │
Phase 2 (P1 — Efficiency & Learning):              │
  TASK-048 Tensor Network Compression ◄────────────┤
  TASK-049 Geometric Equivariant Layers ◄──────────┤
  TASK-050 Sparse Distributed Memory ◄─────────────┤ (uses TASK-047)
                                                    │
Phase 3 (P2 — Generation):                         │
  TASK-051 Diffusion Language Generation ◄─────────┘
```

**Key dependencies:**
- TASK-044 (Liquid) builds on TASK-043 (Neural ODEs from v0.16.0)
- TASK-050 (SDM) complements TASK-047 (Hopfield) — SDM for long-term, Hopfield for fast
- TASK-048 (Tensor Networks) applies to all layer types including liquid and equivariant
- TASK-046 (Neuro-Symbolic) builds on TASK-041 (Causal Inference from v0.16.0)
- TASK-051 (Diffusion) integrates with TASK-046 for contract-checked refinement

## Total Estimated Scope

| Category | Lines |
|----------|-------|
| P0 features | ~6,900 |
| P1 features | ~5,000 |
| P2 features | ~1,600 |
| **Total** | **~13,500** |

## The Cognitive Substrate Architecture

```
┌─────────────────────────────────────────────────────┐
│                  Simplex SLM v2.0                    │
│                                                      │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────┐ │
│  │   Liquid     │  │  Hyperbolic  │  │  Geometric │ │
│  │   Dynamics   │  │  Embeddings  │  │  Equivar.  │ │
│  │  (adaptive   │  │ (hierarchical│  │ (symmetry  │ │
│  │   weights)   │  │  knowledge)  │  │  built-in) │ │
│  └──────┬───────┘  └──────┬───────┘  └─────┬──────┘ │
│         │                 │                 │        │
│  ┌──────▼─────────────────▼─────────────────▼──────┐ │
│  │         Tensor Network Core Layers               │ │
│  │    (exponentially compressed parameters)         │ │
│  └──────┬───────────────────────────────────┬──────┘ │
│         │                                   │        │
│  ┌──────▼───────┐              ┌────────────▼──────┐ │
│  │   Hopfield   │              │  Sparse Distrib.  │ │
│  │   Associative│              │  Memory (SDM)     │ │
│  │   Memory     │              │  (lifelong learn) │ │
│  │  (knowledge) │              │                   │ │
│  └──────┬───────┘              └────────────┬──────┘ │
│         │                                   │        │
│  ┌──────▼───────────────────────────────────▼──────┐ │
│  │        Neuro-Symbolic Reasoning Engine           │ │
│  │   (neural perception + symbolic logic)           │ │
│  └──────────────────┬──────────────────────────────┘ │
│                     │                                │
│  ┌──────────────────▼──────────────────────────────┐ │
│  │        Diffusion Generation Head                 │ │
│  │   (parallel token generation + refinement)       │ │
│  └─────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────┘
```

## How the Substrate Differs from Existing SLMs

| Property | Transformer SLM | Simplex Cognitive Substrate |
|----------|----------------|----------------------------|
| Weights | Fixed after training | Dynamic per input (liquid) |
| Knowledge space | Flat Euclidean | Curved hyperbolic (10-50x smaller) |
| Reasoning | Pattern matching | Actual logic (neuro-symbolic) |
| Knowledge storage | Crammed into weights | External Hopfield memory (exponential) |
| Compression | Pruning + quantization (2-10x) | Tensor networks (10-1000x) |
| Symmetry handling | Learned from data (wasteful) | Built into architecture (free) |
| Post-deployment learning | Frozen or fragile fine-tuning | Lifelong via SDM (no forgetting) |
| Generation | One token at a time | All tokens in parallel (diffusion) |
| Interpretability | Black box | ODE dynamics + symbolic proofs |
| Parameters needed | Billions | Millions (same capability) |

## What This Means for Simplex Users

After v0.17.0, building a specialist SLM in Simplex means:

1. **Define the substrate:** liquid dynamics for adaptive computation, hyperbolic
   embeddings for the domain's hierarchy, equivariant layers for known symmetries
2. **Compress via tensor networks:** 10-1000x parameter reduction from day one
3. **Attach Hopfield memory:** vast knowledge storage without parameter cost
4. **Add symbolic reasoning:** actual logic for structured tasks, neural for fuzzy tasks
5. **Enable lifelong learning:** SDM for continuous improvement without retraining
6. **Choose generation mode:** diffusion for document-level tasks, autoregressive for
   streaming

A medical specialist built this way: 200M liquid parameters + 64-dim hyperbolic
embeddings for ICD-10 + Hopfield memory of clinical guidelines + symbolic reasoning for
diagnostic protocols + SDM for learning from each patient interaction + diffusion
generation for complete reports.

That 200M model outperforms a 70B transformer for medical tasks. Not because it's
cleverer — because its architecture matches its purpose.
