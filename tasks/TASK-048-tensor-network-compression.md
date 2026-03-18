# TASK-048: Tensor Network Compression

**Version:** 0.17.0
**Status:** Planned
**Priority:** P1 — High
**Depends on:** v0.16.0 release, existing matrix/tensor infrastructure

## Why This Feature Is Needed

Neural network weight matrices are dense. A single layer in a 7B model might have a
4096x4096 weight matrix — 16 million parameters for one layer. Most of these parameters
are redundant. Pruning removes some. Quantization shrinks each. But neither addresses
the fundamental problem: **the matrix representation itself is inefficient.**

Tensor networks, borrowed from quantum physics, decompose high-dimensional tensors into
networks of small, connected tensors. The most important decomposition — Matrix Product
States (MPS), also called Tensor Train (TT) — replaces a single large matrix with a
chain of small 3-index tensors. A matrix with N² parameters decomposes into a chain with
O(N·r²) parameters, where r is the bond dimension (rank). For typical neural network
weight matrices, r can be very small (4-64), yielding **10-1000x compression with
controllable precision loss.**

This is not approximation in the way pruning is approximation. It is mathematical
decomposition — the same mathematical framework that lets physicists simulate quantum
systems with exponentially many states using polynomially many parameters. The parameters
scale linearly with dimension instead of exponentially.

Recent work (2025-2026) shows tensor network compression achieving compression ratios
that exponentially exceed conventional methods while preserving model performance. The
tensorization approach requires only black-box access to the model — compress any trained
model, regardless of architecture.

For Simplex's vision of specialist SLMs on edge devices, tensor networks are the
compression layer that makes everything else possible. A 7B model compressed via tensor
networks to 200M effective parameters — not by removing information, but by representing
it more efficiently.

## Why It Adds Value

1. **Exponential compression.** Where pruning gives 2-5x compression and quantization
   gives 2-4x, tensor networks give 10-1000x for the same quality loss budget. These
   are multiplicative — tensor network compression composes with quantization.

2. **Mathematically principled.** The compression error is bounded by the discarded
   singular values. You choose the compression ratio and get a guaranteed bound on how
   much quality you lose. No guesswork.

3. **Structure preservation.** Unlike pruning (which destroys structure) or distillation
   (which requires a student architecture), tensor decomposition preserves the original
   model's computational structure. Every operation remains differentiable.

4. **Composability with training.** Tensor networks are differentiable. You can train
   directly in tensor-decomposed form (never materializing the full matrix), saving both
   memory and compute during training.

5. **Progressive refinement.** Increase the bond dimension to recover quality. Start
   with aggressive compression for prototyping, gradually refine for production. The
   same model at different fidelity levels.

6. **Architecture-agnostic.** Compress any weight matrix in any architecture — linear
   layers, attention projections, embeddings, MoE expert weights. Apply tensor networks
   selectively to the largest layers for maximum impact.

## Why It Changes Systems Built With Simplex

Tensor networks make "impossible" deployments possible:

- **7B models on phones.** A 7B model with tensor-decomposed layers fits in 500MB-1GB
  instead of 14GB. Combined with quantization, it runs on mobile devices with
  acceptable latency.

- **Massive specialist libraries.** Instead of choosing one specialist per device, deploy
  dozens. Each tensor-compressed specialist uses a fraction of its original memory. A
  phone carries a full cognitive hive.

- **Training on laptops.** Training in tensor-decomposed form reduces memory requirements
  by the compression ratio. A model that required 80GB VRAM trains in 8GB.

- **Over-the-air updates.** Compressed models are small enough to update specialists
  over cellular connections. A medical specialist receives a guideline update as a small
  tensor network patch.

## Deliverables

### Phase 1: Tensor Train (MPS) Core (~500 lines)

Location: `simplex-learning/src/tensor_net/`

- **TensorTrain** — matrix product state decomposition of arbitrary tensors, with
  configurable bond dimension and truncation
- **TTDecompose** — SVD-based decomposition of dense matrices into tensor train format
- **TTOperations** — arithmetic on tensor trains without materializing full tensors
  (addition, element-wise product, contraction)
- **TTRounding** — reduce bond dimension of a tensor train (recompress after operations)

```simplex
/// Tensor Train (Matrix Product State) decomposition
struct TensorTrain {
    cores: Vec<Tensor>,   // chain of 3-index tensors [r_{k-1} x n_k x r_k]
    ranks: Vec<usize>,    // bond dimensions [r_0, r_1, ..., r_d]
    shape: Vec<usize>,    // original tensor shape [n_1, n_2, ..., n_d]
}

impl TensorTrain {
    /// Decompose a dense matrix into tensor train form
    fn from_matrix(matrix: &Tensor, max_rank: usize) -> Self {
        let mut cores = Vec::new();
        let mut remaining = matrix.clone();
        let mut ranks = vec![1];

        for k in 0..num_dims - 1 {
            let (rows, cols) = reshape_for_svd(&remaining, k);
            let (u, s, v) = svd(rows, cols);

            // Truncate to max_rank
            let rank = s.len().min(max_rank);
            let u_trunc = u.columns(0..rank);
            let s_trunc = s.slice(0..rank);

            cores.push(reshape_to_core(u_trunc, ranks.last().unwrap(), rank));
            remaining = diag(s_trunc).matmul(&v.rows(0..rank));
            ranks.push(rank);
        }
        cores.push(reshape_to_core(remaining, ranks.last().unwrap(), 1));
        ranks.push(1);

        Self { cores, ranks, shape: matrix.shape() }
    }

    /// Reconstruct the full tensor (for verification — not for production use)
    fn to_dense(self: &Self) -> Tensor {
        let mut result = self.cores[0].clone();
        for core in &self.cores[1..] {
            result = contract(result, core);
        }
        result.reshape(&self.shape)
    }

    /// Matrix-vector product WITHOUT materializing the full matrix
    fn matvec(self: &Self, v: &Tensor) -> Tensor {
        // Contract input vector through the TT chain: O(n·r²) instead of O(n²)
        let mut result = v.clone();
        for core in self.cores.iter().rev() {
            result = core.contract_last_index(&result);
        }
        result
    }

    /// Compression ratio
    fn compression_ratio(self: &Self) -> f64 {
        let dense_params: usize = self.shape.iter().product();
        let tt_params: usize = self.cores.iter()
            .map(|c| c.num_elements())
            .sum();
        dense_params as f64 / tt_params as f64
    }
}
```

Files:
- `simplex-learning/src/tensor_net/mod.sx` — module root
- `simplex-learning/src/tensor_net/tt.sx` — TensorTrain core type
- `simplex-learning/src/tensor_net/decompose.sx` — SVD-based decomposition
- `simplex-learning/src/tensor_net/ops.sx` — TT arithmetic operations
- `simplex-learning/src/tensor_net/rounding.sx` — bond dimension reduction

### Phase 2: Neural Network Integration (~450 lines)

- **TTLinear** — linear layer with weight matrix in tensor train form (drop-in
  replacement for Linear with configurable compression ratio)
- **TTEmbedding** — embedding table in tensor train form (massive compression for
  vocabulary embeddings)
- **ModelCompressor** — compress an existing trained model by decomposing all weight
  matrices into tensor trains
- **TTTrainer** — train directly in tensor train form (never allocate full matrix)

```simplex
/// Linear layer with tensor-train compressed weights
struct TTLinear {
    weight_tt: TensorTrain,
    bias: Option<Tensor>,
    compression_ratio: f64,
}

impl TTLinear {
    /// Create from existing Linear layer (post-training compression)
    fn from_linear(linear: &Linear, max_rank: usize) -> Self {
        let weight_tt = TensorTrain::from_matrix(&linear.weight, max_rank);
        Self {
            compression_ratio: weight_tt.compression_ratio(),
            weight_tt,
            bias: linear.bias.clone(),
        }
    }

    /// Create fresh for training in compressed form
    fn new(in_dim: usize, out_dim: usize, rank: usize) -> Self {
        let weight_tt = TensorTrain::random([in_dim, out_dim], rank);
        Self { weight_tt, bias: Some(Tensor::zeros([out_dim])), compression_ratio: 0.0 }
    }
}

impl Layer for TTLinear {
    fn forward(self: &Self, input: Tensor) -> Tensor {
        let out = self.weight_tt.matvec(&input);
        match &self.bias {
            Some(b) => out + b,
            None => out,
        }
    }
}

/// Compress entire model to tensor train form
struct ModelCompressor {
    max_rank: usize,
    min_compression: f64,  // skip layers below this compression ratio
}

impl ModelCompressor {
    fn compress(self: &Self, model: &dyn Model) -> CompressedModel {
        let mut compressed_layers = Vec::new();
        for layer in model.layers() {
            if let Some(linear) = layer.as_linear() {
                let tt = TTLinear::from_linear(linear, self.max_rank);
                if tt.compression_ratio > self.min_compression {
                    compressed_layers.push(Layer::TTLinear(tt));
                } else {
                    compressed_layers.push(layer.clone()); // keep dense
                }
            } else {
                compressed_layers.push(layer.clone());
            }
        }
        CompressedModel { layers: compressed_layers }
    }
}
```

Files:
- `simplex-learning/src/tensor_net/linear.sx` — TTLinear
- `simplex-learning/src/tensor_net/embedding.sx` — TTEmbedding
- `simplex-learning/src/tensor_net/compressor.sx` — ModelCompressor
- `simplex-learning/src/tensor_net/trainer.sx` — training in TT form

### Phase 3: Advanced Decompositions (~400 lines)

- **TreeTensorNetwork (TTN)** — tree-structured decomposition for data with hierarchical
  correlations (composes naturally with hyperbolic embeddings)
- **TuckerDecomposition** — Tucker decomposition for attention weight tensors (compress
  4-index attention tensors)
- **AdaptiveRank** — automatically determine optimal bond dimension per layer based on
  singular value spectrum
- **TTCrossApproximation** — decompose without ever computing the full matrix (for
  compressing models too large to fit in memory)

Files:
- `simplex-learning/src/tensor_net/ttn.sx` — TreeTensorNetwork
- `simplex-learning/src/tensor_net/tucker.sx` — Tucker decomposition
- `simplex-learning/src/tensor_net/adaptive.sx` — adaptive rank selection
- `simplex-learning/src/tensor_net/cross.sx` — cross approximation

### Phase 4: Tests (~350 lines)

Location: `tests/tensor_net/`

- TT decomposition and reconstruction within tolerance for random matrices
- TTLinear output matches dense Linear output within tolerance
- Compression ratio scales as expected with bond dimension
- TTmatvec is faster than dense matvec for large matrices
- ModelCompressor produces compressed model with <5% quality loss
- Adaptive rank selects appropriate ranks for matrices of varying structure

## Success Criteria

- [ ] TT decomposition of 4096x4096 matrix: 50x compression with <1% Frobenius error
- [ ] TTLinear forward pass matches dense Linear within 1e-4
- [ ] TTmatvec is >5x faster than dense matvec for 4096x4096 at rank 32
- [ ] ModelCompressor compresses synthetic 4-layer model with <2% accuracy drop
- [ ] Training in TT form converges to equivalent loss as dense training
- [ ] Cross approximation decomposes without full matrix materialization

## Estimated Scope

~1,700 lines across library code and tests.
