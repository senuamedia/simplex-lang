# TASK-045: Hyperbolic Embeddings — Curved Knowledge Space

**Version:** 0.17.0
**Status:** Complete
**Priority:** P0 — Critical
**Depends on:** v0.16.0 release, existing tensor/layer infrastructure

## Why This Feature Is Needed

Every embedding in every language model today lives in flat Euclidean space. But the data
these models represent — language hierarchies, taxonomies, code ASTs, organizational
structures, concept ontologies — is fundamentally *tree-structured*. And here is the
mathematical fact that changes everything: **you cannot embed a tree into Euclidean space
without exponential distortion.**

A binary tree of depth 10 has 1,024 leaves. To represent all pairwise distances faithfully
in Euclidean space, you need roughly 1,024 dimensions. In hyperbolic space (constant
negative curvature), you need **~20 dimensions**. This is because hyperbolic space
expands exponentially with radius — exactly like a tree branches exponentially with depth.
The geometry of the space matches the geometry of the data.

This is not theoretical curiosity. Poincare embeddings (Nickel & Kiela, 2017) demonstrated
that 5-dimensional hyperbolic embeddings outperform 200-dimensional Euclidean embeddings
for representing WordNet hierarchies. Recent work (2025-2026) extends this to code
understanding (HypeCodeNet), knowledge graph reasoning (HyGGE), recommendation systems,
and multi-hop QA — all showing dramatic improvements.

For Simplex specialist SLMs, this is transformative. A medical specialist's embedding of
the ICD-10 disease hierarchy (68,000+ codes in a deep tree) would require enormous
Euclidean embeddings. In hyperbolic space, the same hierarchy fits in ~64 dimensions
with perfect hierarchical fidelity. The specialist *understands* that pneumonia is a
type of respiratory disease is a type of disease — geometrically, not memorized.

## Why It Adds Value

1. **10-50x embedding dimension reduction.** Same representational quality in a fraction
   of the dimensions. For SLMs where every parameter matters, this is massive.

2. **Native hierarchical reasoning.** Distance in hyperbolic space respects hierarchy.
   Parent-child relationships, subsumption, generalization/specialization — all encoded
   in the geometry. Models don't learn hierarchy from data; it's built into the space.

3. **Better few-shot learning.** Hyperbolic prototypical networks need fewer examples
   because the embedding space already captures structural relationships. A specialist
   seeing one example of a new disease subtype correctly places it in the hierarchy.

4. **Natural fit with beliefs.** The cognitive hive's belief system represents hierarchical
   confidence (domain → subdomain → specific claim). Hyperbolic embeddings represent this
   hierarchy natively, making belief operations (revision, aggregation) geometrically
   meaningful.

5. **Code understanding.** ASTs are trees. Hyperbolic embeddings represent code structure
   exponentially more efficiently than flat embeddings. A code specialist with hyperbolic
   embeddings understands structural similarity (these two functions have the same AST
   shape) without explicit tree comparison.

6. **Composability with existing infrastructure.** Hyperbolic operations (Mobius addition,
   exponential/logarithmic maps) are differentiable. They compose with Simplex's dual
   numbers for automatic differentiation through curved space.

## Why It Changes Systems Built With Simplex

Hyperbolic embeddings change how specialists represent knowledge:

- **Dramatically smaller specialist embeddings.** A legal specialist's embedding of
  statute hierarchies, case law precedent trees, and regulatory taxonomies fits in 1/20th
  the dimensions. Model size drops. Inference speeds up. Edge deployment becomes trivial.

- **Cross-hierarchy reasoning.** When two specialists share a hyperbolic embedding space,
  hierarchical relationships transfer. A medical specialist and a pharmaceutical specialist
  share the understanding that Drug X treats Disease Y which is a subtype of Disease
  Class Z — encoded geometrically.

- **Automatic abstraction.** Moving toward the origin in hyperbolic space = moving up the
  hierarchy = more abstract concepts. A specialist can explicitly reason at different
  levels of abstraction by varying its position in hyperbolic space.

- **Improved retrieval.** Semantic search in hyperbolic space respects hierarchy. Searching
  for "respiratory disease" returns results organized by specificity — broad categories
  near the query, specific conditions further away — without additional ranking logic.

## Deliverables

### Phase 1: Hyperbolic Geometry Primitives (~500 lines)

Location: `simplex-learning/src/hyperbolic/`

- **PoincareBall** — the Poincare ball model of hyperbolic space with configurable
  curvature, implementing Mobius operations (addition, scalar multiplication, matrix-
  vector multiplication)
- **LorentzModel** — the hyperboloid model (numerically more stable for optimization)
  with Lorentzian inner product and distance
- **HyperbolicTensor** — tensor type living in hyperbolic space with manifold-aware
  operations
- **ExponentialMap / LogarithmicMap** — map between tangent space (Euclidean, where
  gradients live) and the manifold (hyperbolic, where embeddings live)

```simplex
/// The Poincare ball model of hyperbolic space
struct PoincareBall {
    curvature: f64,  // negative curvature (typically -1.0)
    dim: usize,
}

impl PoincareBall {
    /// Mobius addition: the "addition" operation in hyperbolic space
    fn mobius_add(self: &Self, x: &Tensor, y: &Tensor) -> Tensor {
        let c = -self.curvature;
        let x_sq = x.dot(x);
        let y_sq = y.dot(y);
        let xy = x.dot(y);
        let num = (1.0 + 2.0 * c * xy + c * y_sq) * x
                + (1.0 - c * x_sq) * y;
        let denom = 1.0 + 2.0 * c * xy + c * c * x_sq * y_sq;
        num / denom
    }

    /// Hyperbolic distance — grows logarithmically near boundary
    fn distance(self: &Self, x: &Tensor, y: &Tensor) -> Dual {
        let c = -self.curvature;
        let diff = self.mobius_add(x, &(-y));
        let norm = diff.norm();
        (2.0 / c.sqrt()) * (c.sqrt() * norm).atanh()
    }

    /// Exponential map: tangent vector at p → point on manifold
    fn exp_map(self: &Self, p: &Tensor, v: &Tensor) -> Tensor {
        let c = -self.curvature;
        let v_norm = v.norm().max(1e-10);
        let lambda_p = 2.0 / (1.0 - c * p.dot(p));
        let coeff = (c.sqrt() * lambda_p * v_norm / 2.0).tanh() / (c.sqrt() * v_norm);
        self.mobius_add(p, &(coeff * v))
    }

    /// Logarithmic map: point on manifold → tangent vector at p
    fn log_map(self: &Self, p: &Tensor, q: &Tensor) -> Tensor {
        let c = -self.curvature;
        let diff = self.mobius_add(&(-p), q);
        let diff_norm = diff.norm().max(1e-10);
        let lambda_p = 2.0 / (1.0 - c * p.dot(p));
        let coeff = (2.0 / (c.sqrt() * lambda_p)) * (c.sqrt() * diff_norm).atanh() / diff_norm;
        coeff * diff
    }

    /// Project point back onto the ball (numerical safety)
    fn project(self: &Self, x: &Tensor) -> Tensor {
        let c = -self.curvature;
        let max_norm = (1.0 / c.sqrt()) - 1e-5;
        let norm = x.norm();
        if norm > max_norm { x * (max_norm / norm) } else { x.clone() }
    }
}
```

Files:
- `simplex-learning/src/hyperbolic/mod.sx` — module root
- `simplex-learning/src/hyperbolic/poincare.sx` — PoincareBall model
- `simplex-learning/src/hyperbolic/lorentz.sx` — Lorentz hyperboloid model
- `simplex-learning/src/hyperbolic/tensor.sx` — HyperbolicTensor
- `simplex-learning/src/hyperbolic/maps.sx` — exponential/logarithmic maps

### Phase 2: Hyperbolic Neural Network Layers (~450 lines)

- **HyperbolicEmbedding** — embedding layer that places vectors in hyperbolic space,
  with Riemannian SGD optimizer for manifold-constrained updates
- **HyperbolicLinear** — linear layer operating in tangent space with gyroplane
  decision boundaries
- **HyperbolicAttention** — attention mechanism using hyperbolic distance as the
  similarity metric (hierarchy-aware attention)
- **RiemannianOptimizer** — SGD/Adam adapted for Riemannian manifolds (gradient
  computed in tangent space, step taken via exponential map)

```simplex
/// Embedding layer in hyperbolic space
struct HyperbolicEmbedding {
    weights: Tensor,       // embeddings on the Poincare ball
    manifold: PoincareBall,
}

impl HyperbolicEmbedding {
    fn forward(self: &Self, indices: &[usize]) -> Vec<Tensor> {
        indices.iter()
            .map(|i| self.manifold.project(&self.weights.row(*i)))
            .collect()
    }
}

/// Riemannian Adam optimizer for hyperbolic parameters
struct RiemannianAdam {
    manifold: PoincareBall,
    lr: f64,
    beta1: f64,
    beta2: f64,
    m: Vec<Tensor>,  // first moment (in tangent space)
    v: Vec<Tensor>,  // second moment (in tangent space)
}

impl Optimizer for RiemannianAdam {
    fn step(mut self: &mut Self, params: &mut Vec<Tensor>, grads: &[Tensor]) {
        for (i, (param, grad)) in params.iter_mut().zip(grads).enumerate() {
            // Riemannian gradient: scale Euclidean gradient by inverse metric
            let rgrad = self.manifold.riemannian_gradient(param, grad);

            // Update moments in tangent space
            self.m[i] = self.beta1 * &self.m[i] + (1.0 - self.beta1) * &rgrad;
            self.v[i] = self.beta2 * &self.v[i] + (1.0 - self.beta2) * &rgrad.elem_mul(&rgrad);

            let m_hat = &self.m[i] / (1.0 - self.beta1);
            let v_hat = &self.v[i] / (1.0 - self.beta2);

            let direction = m_hat / (v_hat.sqrt() + 1e-8);
            // Step via exponential map (stays on manifold)
            *param = self.manifold.exp_map(param, &(-self.lr * direction));
        }
    }
}
```

Files:
- `simplex-learning/src/hyperbolic/embedding.sx` — HyperbolicEmbedding
- `simplex-learning/src/hyperbolic/linear.sx` — HyperbolicLinear
- `simplex-learning/src/hyperbolic/attention.sx` — HyperbolicAttention
- `simplex-learning/src/hyperbolic/optimizer.sx` — RiemannianAdam

### Phase 3: Integration with Cognitive Substrate (~400 lines)

- **HyperbolicBeliefSpace** — beliefs organized in hyperbolic space where depth =
  specificity, enabling geometric belief revision
- **HyperbolicHiveMemory** — shared hive memory with hierarchy-aware retrieval
- **HyperbolicCodeEmbedding** — AST-aware code embeddings in hyperbolic space
- **CurvatureLearning** — learn the optimal curvature for each specialist's domain

Files:
- `simplex-learning/src/hyperbolic/belief.sx` — HyperbolicBeliefSpace
- `simplex-learning/src/hyperbolic/memory.sx` — HyperbolicHiveMemory
- `simplex-learning/src/hyperbolic/code.sx` — code-specific embeddings
- `simplex-learning/src/hyperbolic/curvature.sx` — learnable curvature

### Phase 4: Tests (~350 lines)

Location: `tests/hyperbolic/`

- Poincare distance satisfies triangle inequality
- Exponential/logarithmic maps are inverse of each other
- HyperbolicEmbedding recovers synthetic tree hierarchy
- Riemannian optimizer stays on manifold (all points inside ball)
- 32-dim hyperbolic embedding outperforms 256-dim Euclidean on synthetic hierarchy
- HyperbolicAttention attends to hierarchically relevant tokens

## Success Criteria

- [ ] Poincare operations numerically stable (no NaN/Inf for 10K random operations)
- [ ] Tree embedding: 32-dim hyperbolic achieves lower distortion than 256-dim Euclidean
- [ ] Riemannian optimizer: all parameters remain on manifold throughout training
- [ ] HyperbolicAttention correctly upweights hierarchically related tokens
- [ ] Curvature learning converges to different values for different data structures
- [ ] Integration with dual numbers for automatic differentiation through manifold ops

## Estimated Scope

~1,700 lines across library code and tests.
