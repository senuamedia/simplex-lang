# TASK-049: Geometric Equivariant Layers

**Version:** 0.17.0
**Status:** Complete
**Priority:** P1 — High
**Depends on:** v0.16.0 release, existing tensor/layer infrastructure

## Why This Feature Is Needed

Standard neural networks are blind to the symmetries in their data. A vision model does
not know that a rotated cat is still a cat — it must learn this from millions of
augmented training examples. A molecular model does not know that rotating a molecule
does not change its properties — it must learn rotational invariance from data. A code
model does not know that renaming a variable does not change program semantics — it must
see enough examples to approximate this.

This is an enormous waste. Every parameter spent learning a known symmetry is a
parameter not available for learning actual content. For large models, the waste is
tolerable. For small specialist models where every parameter matters, it is fatal.

Equivariant neural networks solve this by building symmetries into the architecture.
If a transformation group G acts on the input data, an equivariant layer commutes with
G: `f(g·x) = g·f(x)` for all g in G. The network is *structurally incapable* of
violating the symmetry. It does not need to learn it. Research shows this dramatically
reduces parameter count, improves generalization, and increases per-sample efficiency.

The mathematical framework is group theory and representation theory. For each symmetry
group (rotations SO(3), translations R^n, permutations S_n, gauge transformations), there
is a corresponding equivariant layer design that respects that group's action. Geometric
deep learning unifies CNNs (translation equivariance), graph neural networks (permutation
equivariance), and spherical CNNs (rotation equivariance) under a single framework.

For Simplex specialists, equivariant layers mean: **don't waste parameters learning what
you already know about the domain.** A chemistry specialist with SO(3)-equivariant layers
uses every parameter for chemistry, not for relearning that rotation doesn't matter.

## Why It Adds Value

1. **Massive parameter efficiency.** Equivariant layers have fewer free parameters than
   unconstrained layers because the symmetry constrains the weight space. A rotation-
   equivariant layer may have 10x fewer parameters than a dense layer of the same
   width, with no loss in expressiveness for rotationally symmetric problems.

2. **Better generalization from less data.** The model cannot learn spurious patterns
   that violate the known symmetry. This means it generalizes better from fewer
   examples — critical for specialist domains where labeled data is scarce.

3. **Guaranteed correctness.** If the output should be invariant to some transformation,
   an equivariant network guarantees this mathematically. No amount of adversarial input
   can break the symmetry — it's in the architecture.

4. **Domain-specific inductive bias.** Each specialist domain has different symmetries.
   Chemistry: rotation, translation. Language: permutation of independent clauses.
   Time series: time translation. Simplex lets each specialist declare its symmetries
   and automatically constrains its layers.

5. **Composability.** Equivariant layers compose: if layer 1 is G-equivariant and
   layer 2 is G-equivariant, their composition is G-equivariant. Build deep equivariant
   networks by stacking equivariant layers.

## Why It Changes Systems Built With Simplex

Equivariant layers make specialists experts by construction:

- **Scientific specialists.** A materials science specialist with SE(3)-equivariant
  layers automatically respects 3D geometry. It needs 10x less training data and
  produces physically valid predictions by construction.

- **Code specialists.** Variable renaming equivariance means the specialist understands
  that `foo(x, y)` and `bar(a, b)` have the same structure. It learns programming
  patterns, not variable names.

- **Financial specialists.** Permutation equivariance over portfolio assets means the
  specialist understands that portfolio risk is independent of how assets are ordered.

- **Combinatorial optimization.** Permutation-equivariant specialists for graph problems,
  routing, scheduling — all get the symmetry for free.

## Deliverables

### Phase 1: Group Theory Foundation (~450 lines)

Location: `simplex-learning/src/equivariant/`

- **Group trait** — abstract group with identity, composition, inverse
- **Representation** — how a group acts on vector spaces (matrices representing group
  elements)
- **SO3** — 3D rotation group with Euler angles and rotation matrices
- **SE3** — 3D rigid motion (rotation + translation)
- **Sn** — symmetric group (permutations)
- **Translation** — translation group R^n

```simplex
/// Abstract group
trait Group {
    type Element;
    fn identity() -> Self::Element;
    fn compose(a: &Self::Element, b: &Self::Element) -> Self::Element;
    fn inverse(a: &Self::Element) -> Self::Element;
}

/// Group representation: how the group acts on vectors
trait Representation<G: Group> {
    fn dimension(self: &Self) -> usize;
    fn matrix(self: &Self, g: &G::Element) -> Tensor;
}

/// 3D rotation group SO(3)
struct SO3;

impl Group for SO3 {
    type Element = Tensor; // 3x3 rotation matrix
    fn identity() -> Tensor { Tensor::eye(3) }
    fn compose(a: &Tensor, b: &Tensor) -> Tensor { a.matmul(b) }
    fn inverse(a: &Tensor) -> Tensor { a.transpose() }
}

/// Permutation group S_n
struct Sn {
    n: usize,
}

impl Group for Sn {
    type Element = Vec<usize>; // permutation as mapping
    fn identity() -> Vec<usize> { (0..self.n).collect() }
    fn compose(a: &Vec<usize>, b: &Vec<usize>) -> Vec<usize> {
        b.iter().map(|&i| a[i]).collect()
    }
    fn inverse(a: &Vec<usize>) -> Vec<usize> {
        let mut inv = vec![0; a.len()];
        for (i, &j) in a.iter().enumerate() { inv[j] = i; }
        inv
    }
}
```

Files:
- `simplex-learning/src/equivariant/mod.sx` — module root
- `simplex-learning/src/equivariant/group.sx` — Group trait
- `simplex-learning/src/equivariant/representation.sx` — Representation trait
- `simplex-learning/src/equivariant/so3.sx` — SO(3) rotation group
- `simplex-learning/src/equivariant/se3.sx` — SE(3) rigid motion
- `simplex-learning/src/equivariant/permutation.sx` — Sn permutation group

### Phase 2: Equivariant Layers (~500 lines)

- **EquivariantLinear<G>** — linear layer constrained to commute with group G
  (weight matrix lives in the commutant algebra)
- **InvariantPooling<G>** — pool features to produce G-invariant output (for
  classification tasks where output should not change under G)
- **EquivariantActivation** — activation functions that preserve equivariance (norm-
  based activations, gated nonlinearities)
- **PermEquivariantLayer** — specialized layer for set/graph input (permutation
  equivariant: DeepSets / GNN backbone)

```simplex
/// Linear layer that commutes with group G: f(g·x) = g·f(x)
struct EquivariantLinear<G: Group> {
    weight: Tensor,          // constrained to commutant of G
    group: G,
    rep_in: Box<dyn Representation<G>>,
    rep_out: Box<dyn Representation<G>>,
}

impl<G: Group> EquivariantLinear<G> {
    /// Construct weight matrix constrained by group symmetry
    fn new(group: G, rep_in: Box<dyn Representation<G>>,
           rep_out: Box<dyn Representation<G>>) -> Self {
        // Weight must satisfy: W · ρ_in(g) = ρ_out(g) · W for all g in G
        // Solve for the basis of the commutant algebra
        let basis = compute_equivariant_basis(&group, &rep_in, &rep_out);
        let weight = random_combination(&basis);
        Self { weight, group, rep_in, rep_out }
    }
}

impl<G: Group> Layer for EquivariantLinear<G> {
    fn forward(self: &Self, input: Tensor) -> Tensor {
        // Standard linear, but weight is constrained to be equivariant
        self.weight.matmul(&input)
    }
}

/// Permutation-equivariant layer (DeepSets backbone)
struct PermEquivariantLayer {
    phi: Linear,    // per-element transformation
    rho: Linear,    // aggregation transformation
}

impl Layer for PermEquivariantLayer {
    fn forward(self: &Self, input: Tensor) -> Tensor {
        // DeepSets: f(X) = ρ(Σ φ(x_i)) is permutation invariant
        // For equivariance: f(X)_i = φ(x_i) + ρ(Σ_j x_j)
        let per_element = self.phi.forward(input.clone());
        let aggregated = self.rho.forward(input.sum_rows());
        per_element + aggregated.broadcast_rows(input.rows())
    }
}
```

Files:
- `simplex-learning/src/equivariant/linear.sx` — EquivariantLinear
- `simplex-learning/src/equivariant/pooling.sx` — InvariantPooling
- `simplex-learning/src/equivariant/activation.sx` — equivariant activations
- `simplex-learning/src/equivariant/perm_layer.sx` — permutation-equivariant layers

### Phase 3: Domain-Specific Equivariance (~350 lines)

- **@symmetry(SO3) annotation** — declare a specialist's symmetry group, compiler
  constrains all layers to be equivariant
- **TokenPermEquivariant** — equivariance to independent token permutations (for
  language tasks where clause order doesn't matter)
- **GraphEquivariant** — message-passing neural network with configurable node/edge
  symmetry
- **TimeTranslationEquivariant** — for time-series specialists where patterns are
  shift-invariant

Files:
- `simplex-learning/src/equivariant/annotation.sx` — @symmetry annotation
- `simplex-learning/src/equivariant/token.sx` — token permutation equivariance
- `simplex-learning/src/equivariant/graph.sx` — graph equivariance (MPNN)
- `simplex-learning/src/equivariant/temporal.sx` — time translation equivariance

### Phase 4: Tests (~350 lines)

Location: `tests/equivariant/`

- EquivariantLinear<SO3> output rotates with input (verify f(Rx) = Rf(x))
- PermEquivariantLayer output is permutation-equivariant
- InvariantPooling produces identical output for permuted inputs
- Equivariant model achieves same accuracy with 10x fewer parameters on symmetric task
- @symmetry annotation correctly constrains layer construction
- Composed equivariant layers maintain equivariance

## Success Criteria

- [ ] EquivariantLinear<SO3>: ||f(Rx) - Rf(x)|| < 1e-6 for random rotations R
- [ ] PermEquivariantLayer: f(πX) = πf(X) for all tested permutations π
- [ ] Equivariant model uses <20% parameters of unconstrained model for same accuracy
- [ ] @symmetry annotation produces compile-time error for non-equivariant operations
- [ ] Graph equivariant layer: same output regardless of node ordering
- [ ] All equivariant layers compose with dual number AD

## Estimated Scope

~1,650 lines across library code and tests.
