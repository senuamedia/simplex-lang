# TASK-038: State Space Models (Mamba/S4 Architecture)

**Version:** 0.16.0
**Status:** Complete
**Priority:** P0 — Critical
**Depends on:** v0.15.0 release, existing tensor/layer infrastructure

## Why This Feature Is Needed

Transformers have a fundamental scaling problem: attention is O(n²) in sequence length.
A 4096-token context costs 16 million attention computations. An 8192-token context costs
67 million. This quadratic scaling is why long-context models require enormous GPU memory
and why running transformers on edge devices with limited context is painful.

State Space Models (SSMs) solve this. The S4 (Structured State Spaces for Sequences)
family — and its breakthrough successor Mamba — achieve O(n) linear scaling by modeling
sequences as continuous dynamical systems discretized for computation. Mamba matches
transformer quality on language modeling benchmarks while running 3-5x faster at
inference, with memory that scales linearly instead of quadratically.

For Simplex's goal of building small, fast, intelligent specialist SLMs that run on edge
devices, SSMs are not optional — they are the architecture. A Mamba-based specialist SLM
can process 10x longer contexts on the same hardware, or the same context in 1/5 the
memory.

No compiled language has SSM layers as first-class primitives. Building Mamba in Python
requires custom CUDA kernels (the `mamba-ssm` package). In Simplex, `StateSpaceLayer`
compiles to efficient parallel scans natively, and `SelectiveSSM` (Mamba) compiles to
SIMD-vectorized selective scans without writing a single line of CUDA.

## Why It Adds Value

1. **Linear-time sequence processing.** O(n) instead of O(n²). For a 4096-token
   specialist SLM, this means 4096x less computation in the sequence modeling layer
   compared to full attention.

2. **Constant memory at inference.** SSMs maintain a fixed-size hidden state. Unlike
   transformers (which store a growing KV-cache), an SSM specialist uses the same
   memory whether processing token 10 or token 10,000.

3. **Native long-context support.** Specialist SLMs that can process entire documents,
   full codebases, or long conversation histories without context window limitations —
   critical for research specialists, code analysis, and document understanding.

4. **Efficient parallel training.** Despite being recurrent at inference, SSMs train
   in parallel via the parallel scan algorithm. Training speed matches transformers.

5. **Hybrid architectures.** The best models (Jamba, Zamba) mix SSM and attention layers.
   Simplex should support `HybridBlock` that uses SSM for most layers and attention for
   a few, getting the best of both worlds.

6. **Hardware efficiency.** The parallel scan at the core of SSMs maps directly to SIMD
   and GPU warp-level operations. The compiler can target these patterns for maximum
   throughput.

## Why It Changes Systems Built With Simplex

SSMs change what specialist SLMs can do on constrained hardware:

- **Edge SLMs with long context.** A Mamba-based specialist on a phone can process an
  entire research paper (8000+ tokens) in the same memory a transformer uses for 512
  tokens. Domain specialists become useful for real documents, not just short queries.

- **Real-time streaming.** SSMs naturally process one token at a time with constant
  memory — perfect for live audio transcription, real-time code analysis, or streaming
  sensor data. The specialist doesn't need to see the whole sequence at once.

- **Cognitive Hive with deep context.** Each specialist in a hive can maintain a deep
  understanding of its entire conversation history without KV-cache explosion. The hive's
  shared memory becomes a continuous state, not a sliding window.

- **Combined with MoE.** An MoE model with SSM experts instead of transformer experts
  gets both sparsity (from MoE) and linear scaling (from SSM). This is the architecture
  for running 70B-quality on a laptop.

## Deliverables

### Phase 1: Core State Space Infrastructure (~500 lines)

Location: `simplex-learning/src/ssm/`

- **StateSpaceLayer** — the fundamental S4 layer: HiPPO matrix initialization,
  discretization (bilinear/ZOH), and parallel scan
- **ParallelScan** — efficient associative scan primitive, the computational backbone
  of all SSMs
- **HiPPO** — Matrices that optimally compress continuous signals into fixed-size state
  (HiPPO-LegS, HiPPO-LagT)
- **Discretization** — convert continuous-time SSM to discrete-time (ZOH, bilinear,
  Euler)

```simplex
/// A State Space Model layer: x'(t) = Ax(t) + Bu(t), y(t) = Cx(t) + Du(t)
struct StateSpaceLayer {
    A: Tensor,  // state matrix (N x N), initialized via HiPPO
    B: Tensor,  // input matrix (N x 1)
    C: Tensor,  // output matrix (1 x N)
    D: Tensor,  // feedthrough (1 x 1)
    dt: Dual,   // discretization step (learnable)
    state_dim: usize,
}

impl StateSpaceLayer {
    fn new(state_dim: usize, method: HiPPOInit) -> Self {
        let A = hippo_matrix(state_dim, method);
        let B = Tensor::randn([state_dim, 1]);
        let C = Tensor::randn([1, state_dim]);
        let D = Tensor::zeros([1, 1]);
        let dt = Dual::new(0.001, 1.0);  // learnable via dual number
        Self { A, B, C, D, dt, state_dim }
    }

    /// Training: parallel scan over entire sequence (O(n log n))
    fn forward_parallel(self: &Self, input: Tensor) -> Tensor {
        let (A_bar, B_bar) = discretize(self.A, self.B, self.dt);
        parallel_scan(A_bar, B_bar, self.C, self.D, input)
    }

    /// Inference: recurrent step with constant memory (O(1) per token)
    fn step(self: &Self, input: Tensor, state: &mut Tensor) -> Tensor {
        let (A_bar, B_bar) = discretize(self.A, self.B, self.dt);
        *state = A_bar.matmul(state) + B_bar.matmul(&input);
        self.C.matmul(state) + self.D.matmul(&input)
    }
}
```

Files:
- `simplex-learning/src/ssm/mod.sx` — module root
- `simplex-learning/src/ssm/layer.sx` — StateSpaceLayer
- `simplex-learning/src/ssm/scan.sx` — ParallelScan primitive
- `simplex-learning/src/ssm/hippo.sx` — HiPPO matrix initialization
- `simplex-learning/src/ssm/discretize.sx` — discretization methods

### Phase 2: Selective SSM / Mamba (~500 lines)

- **SelectiveSSM** — Mamba's key innovation: B, C, and dt become *input-dependent*
  (selection mechanism), allowing the model to selectively remember or forget information
- **SelectiveScan** — hardware-efficient selective scan (fused kernel)
- **MambaBlock** — complete Mamba block: linear projection → conv1d → SelectiveSSM →
  linear projection, with residual and normalization
- **MambaStack** — stack of MambaBlocks forming a complete language model backbone

```simplex
/// Mamba's selective state space — input-dependent dynamics
struct SelectiveSSM {
    // These are now functions of the input, not fixed parameters
    s_B: Linear,   // input → B(x)
    s_C: Linear,   // input → C(x)
    s_dt: Linear,  // input → dt(x)
    A_log: Tensor,  // log-parameterized for stability
    D: Tensor,
    state_dim: usize,
}

impl SelectiveSSM {
    fn forward(self: &Self, input: Tensor) -> Tensor {
        // Selection: parameters depend on input content
        let B = self.s_B.forward(input.clone());
        let C = self.s_C.forward(input.clone());
        let dt = softplus(self.s_dt.forward(input.clone()));
        let A = -self.A_log.exp();

        // Selective scan — content-aware state transitions
        selective_scan(A, B, C, self.D, dt, input)
    }
}

/// Complete Mamba block — drop-in replacement for transformer block
struct MambaBlock {
    norm: RMSNorm,
    in_proj: Linear,    // expand to 2 * d_inner
    conv1d: Conv1d,     // local context
    ssm: SelectiveSSM,  // global context via state space
    out_proj: Linear,   // project back to d_model
}
```

Files:
- `simplex-learning/src/ssm/selective.sx` — SelectiveSSM (Mamba core)
- `simplex-learning/src/ssm/selective_scan.sx` — efficient selective scan
- `simplex-learning/src/ssm/mamba_block.sx` — MambaBlock
- `simplex-learning/src/ssm/mamba_stack.sx` — MambaStack model

### Phase 3: Hybrid Architectures (~400 lines)

- **HybridBlock** — interleave SSM and attention layers (Jamba-style). Use attention
  every Nth layer for global information mixing, SSM for the rest
- **SSMAttentionRouter** — neural gate that decides whether to use SSM or attention per
  layer based on input characteristics
- **SlidingWindowSSM** — combine SSM state with local sliding window attention for best
  of both worlds
- **Integration with MoE** — MoE where some experts are SSM-based and others are
  attention-based

```simplex
/// Hybrid block: SSM for most processing, attention for global mixing
struct HybridBlock {
    ssm_layers: Vec<MambaBlock>,
    attn_layers: Vec<MultiHeadAttention>,
    attn_interval: usize,  // attention every N layers
}

impl HybridBlock {
    fn forward(self: &Self, input: Tensor) -> Tensor {
        let mut x = input;
        for (i, ssm) in self.ssm_layers.iter().enumerate() {
            x = ssm.forward(x);
            if (i + 1) % self.attn_interval == 0 {
                let attn_idx = i / self.attn_interval;
                x = self.attn_layers[attn_idx].forward(x, x, x);
            }
        }
        x
    }
}
```

Files:
- `simplex-learning/src/ssm/hybrid.sx` — HybridBlock
- `simplex-learning/src/ssm/router.sx` — SSM/attention routing gate
- `simplex-learning/src/ssm/moe_ssm.sx` — MoE with SSM experts

### Phase 4: Tests (~400 lines)

Location: `tests/ssm/`

- StateSpaceLayer learns to copy/shift sequences (basic SSM validation)
- SelectiveSSM selectively remembers relevant tokens (selection mechanism test)
- MambaBlock matches expected output shapes and gradient flow
- Hybrid model trains on synthetic language modeling task
- Inference recurrence matches parallel scan output (numerical equivalence)
- Memory usage scales linearly with sequence length

## Success Criteria

- [ ] StateSpaceLayer parallel scan matches recurrent step output (within 1e-5)
- [ ] MambaBlock is a drop-in replacement for transformer blocks
- [ ] Hybrid model trains without divergence on synthetic data
- [ ] Memory usage at inference is O(1) per token (constant state size)
- [ ] Training throughput with parallel scan within 2x of equivalent transformer
- [ ] 4096-token sequence processes in <50% memory vs transformer equivalent

## Estimated Scope

~1,800 lines across library code and tests.
