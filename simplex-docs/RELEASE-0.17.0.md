# Simplex v0.17.0 — Cloud Infrastructure & Mathematical Intelligence

**Release Date:** 2026-03-20

## Overview

v0.17.0 delivers cloud infrastructure, developer experience tools, and advanced mathematical AI primitives. This release adds ~26,000 lines of pure Simplex code providing AWS cloud services, a complete developer toolchain (REPL, test framework, benchmarking, file system), and 8 cutting-edge ML modules including State Space Models (Mamba), Mixture of Experts, Kolmogorov-Arnold Networks, Causal Inference, and Neural ODEs.

## Cloud Infrastructure (TASK-027)

- **simplex-aws** — AWS Signature V4 authentication
- **simplex-sqs** — Amazon SQS message queues
- **simplex-dynamodb** — Amazon DynamoDB NoSQL
- **simplex-kafka** — Apache Kafka event streaming (binary wire protocol)

## Developer Experience (TASK-030)

- **Test Framework** — Structured test suites with assertions, timing, groups, and reporting
- **Benchmarking** — Nanosecond-precision benchmarks with statistical analysis
- **File System** — Complete fs module: read, write, list, walk, path helpers, temp files
- **Process** — Spawn, wait, kill child processes with output capture
- **REPL** — Interactive read-eval-print loop with multi-line support and variable persistence

## Mathematical Intelligence (TASKS 036-043)

### Conformal Prediction (TASK-036)
Statistically valid prediction sets (under exchangeability assumptions) with calibration, adaptive thresholds, and conformalized quantile regression.

### Mixture of Experts (TASK-037)
Sparse routing with top-k expert selection, softmax gating, load balance loss, noisy routing for exploration, and expert collapse detection.

### State Space Models (TASK-038)
S4 layers with HiPPO initialization and Mamba selective SSM with input-dependent parameters, 1D convolution, and parallel scan.

### Information Geometry (TASK-039)
Fisher Information Matrix, natural gradients, K-FAC preconditioning, KL divergence (discrete and Gaussian), entropy, and trust regions.

### Kolmogorov-Arnold Networks (TASK-040)
B-spline basis functions, learnable edge activations, symbolic simplification, pruning, and grid refinement.

### Causal Inference (TASK-041)
Causal DAGs, d-separation (Bayes-Ball), do-calculus interventions, backdoor criterion, adjustment sets, and ATE/IPW estimation.

### Mechanistic Interpretability (TASK-042)
Activation analysis, gradient attribution, integrated gradients, saliency maps, linear probes, ablation studies, and neuron importance.

### Neural ODEs (TASK-043)
Euler/RK4/adaptive solvers, Neural ODE layers with MLP dynamics, continuous normalizing flows, and adjoint method.

### Liquid Neural Networks (TASK-044)
Dynamic weight computation with Neural Circuit Policies, closed-form continuous-time cells, and input-dependent rewiring.

### Hyperbolic Embeddings (TASK-045)
Poincare ball and Lorentz model with Mobius operations, exponential/logarithmic maps, Riemannian SGD, and hyperbolic attention.

### Neuro-Symbolic Reasoning (TASK-046)
Knowledge base with facts/rules, forward/backward chaining, unification, proof trees, and neural-symbolic bridge.

### Modern Hopfield Memory (TASK-047)
Associative memory with exponential capacity, softmax retrieval, energy-based convergence, and Hopfield-based attention layers.

### Tensor Network Compression (TASK-048)
Tensor Train decomposition, truncated SVD, Tucker decomposition, TT matrix-vector multiply, and compression ratio analysis.

### Geometric Equivariant Layers (TASK-049)
Rotation/permutation equivariance, graph neural network message passing, spherical harmonics, and invariant pooling.

### Sparse Distributed Memory (TASK-050)
Kanerva SDM with binary vector operations, Hamming distance, hyperdimensional computing (bind/bundle/permute), and online single-shot learning.

### Diffusion Language Generation (TASK-051)
Discrete diffusion with cosine noise schedule, parallel token denoising, confidence-based unmasking, and nucleus sampling.

## Breaking Changes

None. All changes are additive.

## Dependencies

No new external dependencies. All new code is pure Simplex.

## Contributors

- Rod Higgins
