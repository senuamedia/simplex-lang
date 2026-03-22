# TASK-040: Kolmogorov-Arnold Networks (KANs)

**Version:** 0.16.0
**Status:** Complete
**Priority:** P1 — High
**Depends on:** v0.15.0 release, dual numbers (TASK-005), existing layer infrastructure

## Why This Feature Is Needed

Standard neural networks (MLPs) use fixed activation functions (ReLU, GELU, sigmoid) on
nodes. The Kolmogorov-Arnold representation theorem proves that any continuous multivariate
function can be represented as a composition of continuous univariate functions and
addition. KANs exploit this by placing *learnable activation functions* on edges instead
of fixed activations on nodes.

The result is dramatic: KANs achieve the same accuracy as MLPs with 10-100x fewer
parameters. A KAN with [2, 5, 1] architecture (2 inputs, 5 hidden, 1 output) can learn
functions that require a [2, 100, 100, 1] MLP. For small specialist models, this
compression is transformative.

But the real breakthrough is what happens *after* training. Because KAN activations are
B-splines (piecewise polynomials), a trained KAN can be symbolically simplified. The
learned function can be extracted as a closed-form mathematical formula. The model
literally compiles away into an equation. No other neural architecture can do this.

For Simplex's vision of compiled AI, KANs are the ultimate expression: train a neural
network, extract the formula, compile it to native code. Zero inference cost. Zero model
size. Pure math.

## Why It Adds Value

1. **100x parameter reduction.** KANs need dramatically fewer parameters for the same
   function approximation quality. A specialist SLM's feedforward layers replaced with
   KAN layers shrink by orders of magnitude.

2. **Symbolic extraction.** After training, the learned function can be extracted as a
   mathematical formula. This means:
   - The model becomes *interpretable* — you can read what it learned
   - The formula can be *verified* — formal methods apply to extracted equations
   - The formula *compiles to native code* — no neural network at inference time

3. **Scientific discovery.** KANs trained on physical data rediscover known physical
   laws (conservation of energy, Maxwell's equations). Specialist SLMs for science
   domains can learn and output actual equations, not just token predictions.

4. **Grid refinement.** KANs can be progressively refined by increasing B-spline grid
   resolution. Train coarse first (fast, stable), then refine (accurate). This is
   natural curriculum learning at the architecture level.

5. **Integration with dual numbers.** B-splines are piecewise polynomials — naturally
   differentiable. Simplex's dual number infrastructure computes exact derivatives
   through KAN layers with zero overhead, unlike the numerical approximations used in
   Python KAN implementations.

6. **Composability with neural gates.** A neural gate backed by a KAN can be trained
   soft, annealed to hard, and then *extracted to a symbolic decision function*. The
   gate becomes a compiled if-else tree derived from learned mathematics.

## Why It Changes Systems Built With Simplex

KANs fundamentally change what "deploying a model" means:

- **Models that become formulas.** A medical specialist SLM's diagnostic pathway, after
  training, extracts to: `risk = 0.3*log(age) + 0.7*sigmoid(biomarker_a - 2.1)`. This
  formula is verifiable, auditable, and runs in nanoseconds. No GPU required.

- **Infinitely small deployment.** A KAN-based specialist's learned functions compile
  to a few kilobytes of native arithmetic. Deploy on microcontrollers, embedded systems,
  anywhere. The "model" is just compiled math.

- **Interpretable AI by default.** Every KAN layer can be visualized as a set of learned
  curves. Regulators can inspect exactly what each specialist learned. No black box.

- **Scientific computing specialists.** Hive specialists for physics, chemistry, and
  engineering don't just predict — they discover equations. A materials science specialist
  could learn the stress-strain relationship for a new material as a symbolic formula.

- **Neural gate simplification.** After training, neural gates backed by KANs simplify
  to symbolic decision trees. The entire trained hive can partially compile away into
  native branching logic + small neural components for genuinely fuzzy decisions.

## Deliverables

### Phase 1: Core KAN Infrastructure (~500 lines)

Location: `simplex-learning/src/kan/`

- **BSpline** — B-spline basis functions with configurable order and grid
- **KANLayer** — a layer where each edge has a learnable B-spline activation function
- **KANNetwork** — composition of KAN layers with residual connections
- **GridExtension** — refine B-spline grid resolution after initial training

```simplex
/// A B-spline activation function on a single edge
struct BSplineActivation {
    coefficients: Vec<Dual>,  // learnable B-spline coefficients
    grid: Vec<f64>,           // knot positions
    order: usize,             // B-spline order (typically 3 = cubic)
}

impl BSplineActivation {
    fn new(grid_size: usize, order: usize) -> Self {
        let grid = uniform_grid(-1.0, 1.0, grid_size);
        let coefficients = vec![Dual::new(0.0, 0.0); grid_size + order];
        Self { coefficients, grid, order }
    }

    /// Evaluate the B-spline at input x
    fn forward(self: &Self, x: Dual) -> Dual {
        let bases = bspline_basis(x, &self.grid, self.order);
        let mut result = Dual::zero();
        for (coeff, basis) in self.coefficients.iter().zip(bases.iter()) {
            result = result + *coeff * *basis;
        }
        result
    }

    /// Extend grid resolution for progressive refinement
    fn refine_grid(mut self: &mut Self, new_grid_size: usize) {
        let new_grid = uniform_grid(-1.0, 1.0, new_grid_size);
        // Interpolate existing coefficients onto finer grid
        self.coefficients = interpolate_coefficients(
            &self.coefficients, &self.grid, &new_grid, self.order
        );
        self.grid = new_grid;
    }
}

/// A KAN layer: learnable activations on edges, summation on nodes
struct KANLayer {
    activations: Vec<Vec<BSplineActivation>>,  // [out_dim][in_dim]
    in_dim: usize,
    out_dim: usize,
}

impl Layer for KANLayer {
    fn forward(self: &Self, input: Tensor) -> Tensor {
        let mut output = Tensor::zeros([self.out_dim]);
        for j in 0..self.out_dim {
            for i in 0..self.in_dim {
                // Each edge applies its learned activation, then sum at node
                output[j] = output[j] + self.activations[j][i].forward(input[i]);
            }
        }
        output
    }
}
```

Files:
- `simplex-learning/src/kan/mod.sx` — module root
- `simplex-learning/src/kan/bspline.sx` — BSplineActivation and basis functions
- `simplex-learning/src/kan/layer.sx` — KANLayer implementation
- `simplex-learning/src/kan/network.sx` — KANNetwork with residual connections
- `simplex-learning/src/kan/grid.sx` — grid extension and refinement

### Phase 2: Symbolic Extraction (~400 lines)

- **SymbolicExtractor** — analyze trained B-splines and fit symbolic functions (sin, cos,
  exp, log, polynomial, etc.)
- **FormulaTree** — abstract syntax tree for extracted mathematical formulas
- **FormulaSimplifier** — algebraic simplification of extracted formulas (combine terms,
  reduce redundancy)
- **FormulaCompiler** — compile extracted formula to native Simplex code (the model
  becomes a function)

```simplex
/// Attempt to extract a symbolic formula from a trained KAN
struct SymbolicExtractor {
    candidates: Vec<SymbolicFunction>,  // sin, cos, exp, log, x^n, etc.
    tolerance: f64,                      // R² threshold for accepting a fit
}

enum SymbolicFunction {
    Identity,           // f(x) = x
    Power(f64),         // f(x) = x^n
    Exp,                // f(x) = e^x
    Log,                // f(x) = ln(x)
    Sin,                // f(x) = sin(x)
    Cos,                // f(x) = cos(x)
    Sigmoid,            // f(x) = 1/(1+e^{-x})
    Tanh,               // f(x) = tanh(x)
    Composite(Box<SymbolicFunction>, Box<SymbolicFunction>),
}

/// Extracted formula as a tree
enum FormulaTree {
    Constant(f64),
    Variable(String),
    Add(Box<FormulaTree>, Box<FormulaTree>),
    Mul(Box<FormulaTree>, Box<FormulaTree>),
    Apply(SymbolicFunction, Box<FormulaTree>),
}

impl SymbolicExtractor {
    /// Try to replace each B-spline with a symbolic function
    fn extract(self: &Self, kan: &KANNetwork) -> Option<FormulaTree> {
        let mut formula = FormulaTree::zero();
        for layer in &kan.layers {
            for (j, row) in layer.activations.iter().enumerate() {
                for (i, activation) in row.iter().enumerate() {
                    if let Some(sym) = self.fit_symbolic(activation) {
                        formula = formula.compose(sym, i, j);
                    } else {
                        return None;  // cannot fully symbolize
                    }
                }
            }
        }
        Some(formula.simplify())
    }
}
```

Files:
- `simplex-learning/src/kan/symbolic.sx` — SymbolicExtractor
- `simplex-learning/src/kan/formula.sx` — FormulaTree and simplification
- `simplex-learning/src/kan/compile.sx` — FormulaCompiler (formula → Simplex code)

### Phase 3: Integration with Existing Systems (~300 lines)

- **KANGate** — neural gate backed by KAN, extractable to symbolic decision function
  after training
- **KANExpert** — MoE expert using KAN layers (ultra-compact experts)
- **HybridKANMLP** — replace only some MLP layers with KAN (for layers where symbolic
  extraction is most valuable)
- **@symbolic_extractable annotation** — compiler flag that triggers symbolic extraction
  after training and replaces the KAN with compiled formula

```simplex
/// Neural gate that can be extracted to a symbolic formula after training
@symbolic_extractable
neural_gate risk_assessment(features: Tensor) -> RiskLevel
    requires features.len() == 5
    ensures result.confidence >= 0.8
    fallback RiskLevel::Unknown
{
    // During training: KAN learns nonlinear risk function
    // After extraction: becomes compiled formula
    let score = self.kan.forward(features);
    if score > 0.7 { RiskLevel::High }
    else if score > 0.3 { RiskLevel::Medium }
    else { RiskLevel::Low }
}
// Post-extraction: risk_assessment compiles to:
// score = 0.3*ln(feature[0]) + 0.7*sigmoid(feature[2] - 2.1) + 0.1*feature[4]^2
// if score > 0.7 { High } else if score > 0.3 { Medium } else { Low }
```

Files:
- `simplex-learning/src/kan/gate.sx` — KANGate with symbolic extraction
- `simplex-learning/src/kan/expert.sx` — KANExpert for MoE integration
- `simplex-learning/src/kan/hybrid.sx` — hybrid KAN+MLP architectures

### Phase 4: Tests (~300 lines)

Location: `tests/kan/`

- KAN learns f(x,y) = sin(x) + cos(y) and extracts exact formula
- Grid refinement improves approximation quality
- Symbolic extraction succeeds for polynomial, trigonometric, and exponential functions
- KANLayer gradient flow via dual numbers matches numerical gradients
- KANGate trains soft and extracts to compiled decision function
- Parameter count comparison: KAN vs MLP for equivalent accuracy

## Success Criteria

- [ ] KAN [2,5,1] learns f(x,y) = x² + y² with <1% error using <50 parameters
- [ ] Symbolic extraction recovers sin, cos, exp, log from trained B-splines (R² > 0.99)
- [ ] FormulaCompiler produces valid Simplex code from extracted formulas
- [ ] Grid refinement reduces approximation error by >50% per refinement step
- [ ] KANGate symbolic extraction produces equivalent output to trained gate (within 1e-4)
- [ ] Dual number gradients through B-splines are exact (no numerical approximation)

## Estimated Scope

~1,500 lines across library code and tests.
