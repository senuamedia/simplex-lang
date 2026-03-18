# TASK-005: Dual Numbers and Forward-Mode Automatic Differentiation

**Status**: Phase 1 Complete, Phases 2-5 Planned
**Priority**: High
**Created**: 2026-01-10
**Updated**: 2026-03-16 (audited against codebase)
**Target Version**: 0.8.0

> **Audit Note (2026-03-16):**
> - Phase 1 (Core Dual Type): COMPLETE — `simplex-learning/src/dual/dual.sx` (918 lines)
>   Full arithmetic, transcendentals, ML activations (sigmoid, relu, gelu, etc.),
>   optimization helpers (minimize, newton_raphson), nth_derivative (numerical for n>1)
> - Phase 1 extras: DualTensor in `simplex-learning/src/tensor/dual_tensor.sx`,
>   Reverse-mode autograd in `simplex-learning/src/tensor/autograd.sx` (533 lines)
> - Phase 2 (multidual\<N\> for gradients): SPEC ONLY — not in compiled code
> - Phase 3 (dual2 / Hessians): SPEC ONLY — nth_derivative uses numerical approximation
> - Phase 4 (@differentiable(mode: auto)): SPEC ONLY — not implemented
> - Phase 5 (Integration with neural gates/beliefs): PARTIAL — neural gates use dual numbers
>   (`simplex-training/src/neural/gate.sx`) but full belief/safety integration pending
**Depends On**: TASK-001 (Neural IR) - Complete, TASK-004 (Real-Time Learning) - Complete

## Overview

Implement dual numbers as a native type in Simplex, enabling forward-mode automatic differentiation (AD) at the language level. This makes differentiable programming a first-class feature of the language, not a library concern.

**Key Insight**: Dual numbers provide exact derivatives with zero runtime overhead—the compiler optimizes away the abstraction entirely, producing the same assembly as hand-written derivative code.

```simplex
// Native dual number support
let x: dual = dual(3.0, 1.0);  // value=3, derivative seed=1
let y = x * x + sin(x);         // Arithmetic propagates derivatives

println(y.val);  // f(3) = 9.1411...
println(y.der);  // f'(3) = 6.9899... (exact, not numerical approximation)
```

---

## Background

### Current State of Differentiation in Simplex

Simplex v0.7.0 has Neural Gates with Gumbel-Softmax for differentiable control flow, and real-time learning with streaming optimizers. These use **reverse-mode AD** (backpropagation) internally.

However:
- Reverse-mode requires storing the computation graph (memory overhead)
- Some operations are more efficient in forward-mode
- Sensitivity analysis (Jacobians) requires forward-mode
- Safety constraint gradients benefit from forward-mode

### Forward vs Reverse Mode

| Aspect | Forward-Mode (Dual Numbers) | Reverse-Mode (Backprop) |
|--------|----------------------------|------------------------|
| Computes | Jacobian-vector product (Jv) | Vector-Jacobian product (vᵀJ) |
| Complexity | O(n) for n inputs | O(m) for m outputs |
| Memory | Constant (no graph storage) | O(computation depth) |
| Best for | Few inputs, many outputs | Many inputs, few outputs |
| Use case | Sensitivity analysis, Jacobians | Neural network training |

**Simplex Advantage**: By supporting both modes natively, the compiler can choose the optimal mode automatically based on function signature.

---

## Core Technical Specification

### 1. Dual Number Algebra

A dual number extends real numbers with an infinitesimal component:

```
d = a + bε  where ε² = 0 (nilpotent)
```

This single rule (`ε² = 0`) encodes the chain rule into arithmetic:

**Addition**:
```
(a + bε) + (c + dε) = (a + c) + (b + d)ε
```

**Multiplication**:
```
(a + bε) × (c + dε) = ac + (ad + bc)ε + bdε²
                    = ac + (ad + bc)ε    [since ε² = 0]
```

This is exactly the product rule: `(f·g)' = f·g' + f'·g`

**Division**:
```
(a + bε) / (c + dε) = (a/c) + ((bc - ad)/c²)ε
```

This is the quotient rule.

**Function Application**:
For any smooth function f:
```
f(a + bε) = f(a) + b·f'(a)·ε
```

The coefficient of ε in the result **is** the derivative.

### 2. Type Definition

```simplex
// Core dual number type
@primitive
@zero_overhead
pub struct dual {
    val: f64,  // Function value
    der: f64,  // Derivative value
}

impl dual {
    // Constructors
    pub fn new(val: f64, der: f64) -> dual {
        dual { val, der }
    }

    // Create constant (derivative = 0)
    pub fn constant(val: f64) -> dual {
        dual { val, der: 0.0 }
    }

    // Create variable (derivative = 1, seed for differentiation)
    pub fn variable(val: f64) -> dual {
        dual { val, der: 1.0 }
    }

    // Extract components
    pub fn value(self) -> f64 { self.val }
    pub fn derivative(self) -> f64 { self.der }
}
```

### 3. Arithmetic Operations

```simplex
impl Add for dual {
    fn add(self, other: dual) -> dual {
        dual {
            val: self.val + other.val,
            der: self.der + other.der,
        }
    }
}

impl Sub for dual {
    fn sub(self, other: dual) -> dual {
        dual {
            val: self.val - other.val,
            der: self.der - other.der,
        }
    }
}

impl Mul for dual {
    fn mul(self, other: dual) -> dual {
        dual {
            val: self.val * other.val,
            der: self.val * other.der + self.der * other.val,  // Product rule
        }
    }
}

impl Div for dual {
    fn div(self, other: dual) -> dual {
        let val = self.val / other.val;
        let der = (self.der * other.val - self.val * other.der) / (other.val * other.val);
        dual { val, der }  // Quotient rule
    }
}

impl Neg for dual {
    fn neg(self) -> dual {
        dual { val: -self.val, der: -self.der }
    }
}
```

### 4. Transcendental Functions

```simplex
impl dual {
    pub fn sin(self) -> dual {
        dual {
            val: sin(self.val),
            der: self.der * cos(self.val),  // d/dx sin(x) = cos(x)
        }
    }

    pub fn cos(self) -> dual {
        dual {
            val: cos(self.val),
            der: -self.der * sin(self.val),  // d/dx cos(x) = -sin(x)
        }
    }

    pub fn exp(self) -> dual {
        let e = exp(self.val);
        dual {
            val: e,
            der: self.der * e,  // d/dx exp(x) = exp(x)
        }
    }

    pub fn ln(self) -> dual {
        dual {
            val: ln(self.val),
            der: self.der / self.val,  // d/dx ln(x) = 1/x
        }
    }

    pub fn pow(self, n: dual) -> dual {
        // d/dx x^n = n*x^(n-1) * x' + x^n * ln(x) * n'
        let val = pow(self.val, n.val);
        let der = val * (n.der * ln(self.val) + n.val * self.der / self.val);
        dual { val, der }
    }

    pub fn sqrt(self) -> dual {
        let s = sqrt(self.val);
        dual {
            val: s,
            der: self.der / (2.0 * s),  // d/dx sqrt(x) = 1/(2*sqrt(x))
        }
    }

    pub fn tanh(self) -> dual {
        let t = tanh(self.val);
        dual {
            val: t,
            der: self.der * (1.0 - t * t),  // d/dx tanh(x) = 1 - tanh²(x)
        }
    }

    pub fn sigmoid(self) -> dual {
        let s = 1.0 / (1.0 + exp(-self.val));
        dual {
            val: s,
            der: self.der * s * (1.0 - s),  // d/dx σ(x) = σ(x)(1-σ(x))
        }
    }
}
```

### 5. Multi-Dual Numbers for Jacobians

To compute all partial derivatives in one pass, use multi-dimensional dual numbers:

```simplex
// Multi-dual: value + multiple independent infinitesimals
@primitive
@zero_overhead
pub struct multidual<const N: usize> {
    val: f64,
    der: [f64; N],  // N independent partial derivatives
}

impl<const N: usize> multidual<N> {
    // Create variable with derivative seed in dimension i
    pub fn variable(val: f64, i: usize) -> multidual<N> {
        var der: [f64; N] = [0.0; N];
        der[i] = 1.0;
        multidual { val, der }
    }

    // Get gradient vector
    pub fn gradient(self) -> [f64; N] {
        self.der
    }
}

// Example: compute gradient of f(x,y,z) = x²y + sin(z)
fn compute_gradient(x: f64, y: f64, z: f64) -> [f64; 3] {
    let dx = multidual::<3>::variable(x, 0);  // ∂/∂x
    let dy = multidual::<3>::variable(y, 1);  // ∂/∂y
    let dz = multidual::<3>::variable(z, 2);  // ∂/∂z

    let result = dx * dx * dy + dz.sin();
    result.gradient()  // [2xy, x², cos(z)]
}
```

### 6. Higher-Order Dual Numbers

For Hessians and higher derivatives, nest dual numbers or use polynomial extensions:

```simplex
// Second-order dual: value + first derivative + second derivative
@primitive
pub struct dual2 {
    val: f64,
    d1: f64,   // First derivative
    d2: f64,   // Second derivative
}

impl dual2 {
    pub fn variable(val: f64) -> dual2 {
        dual2 { val, d1: 1.0, d2: 0.0 }
    }
}

impl Mul for dual2 {
    fn mul(self, other: dual2) -> dual2 {
        // (a + b·ε + c·ε²) × (d + e·ε + f·ε²)
        // = ad + (ae + bd)ε + (af + be + cd)ε²  [keeping terms up to ε²]
        dual2 {
            val: self.val * other.val,
            d1: self.val * other.d1 + self.d1 * other.val,
            d2: self.val * other.d2 + 2.0 * self.d1 * other.d1 + self.d2 * other.val,
        }
    }
}

// Example: compute f, f', f'' simultaneously
fn second_derivative(x: f64) -> (f64, f64, f64) {
    let d = dual2::variable(x);
    let result = d * d * d;  // x³
    (result.val, result.d1, result.d2)  // (x³, 3x², 6x)
}
```

---

## Integration with Existing Features

### 1. Neural Gates with Dual Numbers

Forward-mode AD for neural gate gradients:

```simplex
neural_gate classify(features: dual) -> dual
    requires features.val > 0.0
    ensures result.val >= 0.0 && result.val <= 1.0
{
    // Sigmoid classification with automatic gradient
    features.sigmoid()
}

// Training: get gradient directly
let x = dual::variable(2.5);
let output = classify(x);
println(output.val);  // Classification result
println(output.der);  // Gradient w.r.t. input
```

### 2. Belief Confidence Sensitivity

Track how beliefs change with evidence:

```simplex
struct DualBelief {
    content: BeliefContent,
    confidence: dual,  // Tracks sensitivity to evidence
}

fn update_belief(belief: DualBelief, evidence: dual) -> DualBelief {
    // Bayesian update with automatic sensitivity tracking
    let prior = belief.confidence;
    let likelihood = evidence;
    let posterior = (prior * likelihood) /
                    (prior * likelihood + (dual::constant(1.0) - prior) * (dual::constant(1.0) - likelihood));

    DualBelief {
        content: belief.content,
        confidence: posterior,
        // posterior.der tells us: how does confidence change per unit change in evidence?
    }
}
```

### 3. Safety Constraints with Margin Gradients

Predict constraint violations before they happen:

```simplex
fn check_safety_margin(state: dual, constraint: Constraint) -> SafetyResult {
    let margin = compute_margin(state, constraint);

    if margin.val < 0.0 {
        SafetyResult::Violation { margin: margin.val }
    } else if margin.val < 0.1 && margin.der < -0.5 {
        // Close to boundary AND approaching rapidly
        SafetyResult::Warning {
            margin: margin.val,
            rate_of_approach: -margin.der,
            time_to_violation: margin.val / (-margin.der),
        }
    } else {
        SafetyResult::Safe { margin: margin.val }
    }
}
```

### 4. Automatic Mode Selection

Compiler chooses forward or reverse mode based on dimensions:

```simplex
@differentiable(mode: auto)
fn neural_layer(weights: Tensor<1000, 100>, input: Tensor<100>) -> Tensor<1000> {
    // 100 inputs, 1000 outputs → forward-mode more efficient
    // Compiler automatically uses dual numbers
    matmul(weights, input).tanh()
}

@differentiable(mode: auto)
fn loss_function(predictions: Tensor<1000>, targets: Tensor<1000>) -> f64 {
    // 1000 inputs, 1 output → reverse-mode more efficient
    // Compiler automatically uses backprop
    mean_squared_error(predictions, targets)
}
```

---

## Compiler Implementation

### 1. Zero-Overhead Abstraction

The `@zero_overhead` annotation instructs the compiler to eliminate the struct wrapper:

```simplex
// Source code
let x = dual::variable(3.0);
let y = x * x;
println(y.der);

// Compiled (conceptually)
let x_val = 3.0;
let x_der = 1.0;
let y_val = x_val * x_val;
let y_der = x_val * x_der + x_der * x_val;  // = 2 * x_val * x_der = 6.0
println(y_der);

// Assembly: identical to hand-written derivative code
// No struct allocation, no method dispatch overhead
```

### 2. Type Inference for Dual Propagation

```simplex
// Automatic dual propagation through function calls
fn f(x: dual) -> dual {
    x * x + dual::constant(2.0) * x + dual::constant(1.0)
}

// Compiler infers: if input is dual, output is dual
// If input is f64, function still works (implicit conversion)
let result_dual = f(dual::variable(3.0));  // Returns dual
let result_f64 = f(3.0);                    // Returns f64 (optimized path)
```

### 3. Dual-Aware Optimization Passes

```
Source → Parse → Type Check → Dual Expansion → LLVM IR → Assembly
                                    │
                                    ▼
                           ┌─────────────────┐
                           │ Dual Expansion  │
                           │ - Inline all    │
                           │   dual ops      │
                           │ - Eliminate     │
                           │   struct alloc  │
                           │ - Fuse chains   │
                           └─────────────────┘
```

**Optimization Rules**:
1. **Struct elimination**: `dual { val, der }` becomes two SSA values
2. **Chain fusion**: `(a * b).sin()` computes both val and der without intermediate struct
3. **Dead derivative elimination**: If `.der` is never read, don't compute it
4. **Constant propagation**: `dual::constant(x).der` always equals 0, eliminate

---

## Standard Library Extensions

### 1. Differentiation Module

```simplex
module simplex::diff;

// Compute derivative of single-variable function
pub fn derivative<F>(f: F, x: f64) -> f64
where F: Fn(dual) -> dual
{
    let dx = dual::variable(x);
    f(dx).der
}

// Compute gradient of multi-variable function
pub fn gradient<F, const N: usize>(f: F, x: [f64; N]) -> [f64; N]
where F: Fn([multidual<N>; N]) -> multidual<N>
{
    let inputs: [multidual<N>; N] = array_map_indexed(x, |val, i| {
        multidual::variable(val, i)
    });
    f(inputs).gradient()
}

// Compute Jacobian matrix
pub fn jacobian<F, const N: usize, const M: usize>(
    f: F,
    x: [f64; N]
) -> [[f64; N]; M]
where F: Fn([multidual<N>; N]) -> [multidual<N>; M]
{
    let inputs: [multidual<N>; N] = array_map_indexed(x, |val, i| {
        multidual::variable(val, i)
    });
    let outputs = f(inputs);
    array_map(outputs, |out| out.gradient())
}

// Compute Hessian matrix (second derivatives)
pub fn hessian<F, const N: usize>(f: F, x: [f64; N]) -> [[f64; N]; N]
where F: Fn([dual2; N]) -> dual2
{
    // Uses nested dual numbers or hyper-dual implementation
    // ...
}
```

### 2. Integration with Tensors

```simplex
module simplex::tensor::diff;

// Dual-valued tensors for batched differentiation
pub struct DualTensor<const ROWS: usize, const COLS: usize> {
    val: Tensor<ROWS, COLS>,
    der: Tensor<ROWS, COLS>,
}

impl<const R: usize, const C: usize> DualTensor<R, C> {
    pub fn matmul<const K: usize>(self, other: DualTensor<C, K>) -> DualTensor<R, K> {
        DualTensor {
            val: self.val.matmul(other.val),
            der: self.val.matmul(other.der) + self.der.matmul(other.val),
        }
    }

    pub fn elementwise_mul(self, other: DualTensor<R, C>) -> DualTensor<R, C> {
        DualTensor {
            val: self.val * other.val,
            der: self.val * other.der + self.der * other.val,
        }
    }
}
```

---

## HiveOS Integration

### 1. Specialist Sensitivity Analysis

```simplex
// In HiveOS: track how specialist outputs depend on inputs
specialist Classifier {
    model: "simplex-cognitive-8b",

    @differentiable(mode: forward)
    fn classify(input: DualTensor) -> DualTensor {
        let features = self.encode(input);
        let logits = self.head(features);
        logits.softmax()
        // Output includes Jacobian of predictions w.r.t. inputs
        // Useful for: explainability, adversarial detection, input sensitivity
    }
}
```

### 2. Belief Graph with Gradient Flow

```simplex
// Beliefs track sensitivity to evidence sources
struct GradientBelief {
    id: BeliefId,
    content: BeliefContent,
    confidence: multidual<MAX_EVIDENCE_SOURCES>,
    // confidence.der[i] = how much does this belief depend on evidence source i?
}

fn propagate_with_gradients(graph: BeliefGraph) -> BeliefGraph {
    // Forward-mode propagation gives us:
    // 1. Updated confidences
    // 2. Sensitivity matrix: which evidence affects which beliefs
    // This enables: "What evidence would change this belief most?"
}
```

### 3. Safety Constraint Monitoring

```simplex
// Kernel safety with predictive margins
fn kernel_safety_check(state: SystemState) -> SafetyReport {
    let dual_state = state.to_dual();  // Lift to dual numbers

    var critical_margins: Vec<MarginReport> = vec_new();

    for constraint in safety_constraints {
        let margin = evaluate_constraint(dual_state, constraint);

        if margin.val < CRITICAL_THRESHOLD || margin.der < CRITICAL_RATE {
            vec_push(critical_margins, MarginReport {
                constraint: constraint.id,
                current_margin: margin.val,
                rate_of_change: margin.der,
                predicted_violation: if margin.der < 0.0 {
                    Some(margin.val / (-margin.der))
                } else { None },
            });
        }
    }

    SafetyReport { margins: critical_margins }
}
```

---

## Implementation Phases

### Phase 1: Core Dual Type (Feb 2026)

**Deliverables**:
1. `dual` struct definition with `val` and `der` fields
2. Arithmetic operators: `+`, `-`, `*`, `/`, unary `-`
3. Basic math functions: `sin`, `cos`, `exp`, `ln`, `sqrt`, `pow`
4. Constructors: `dual::new`, `dual::constant`, `dual::variable`
5. Zero-overhead compilation (struct elimination pass)

**Success Criteria**:
- [ ] `dual` operations compile to same assembly as manual derivative code
- [ ] All arithmetic satisfies dual number algebra laws
- [ ] Transcendental functions produce exact derivatives
- [ ] No heap allocation for dual operations
- [ ] Benchmark: dual ops within 5% of scalar ops

### Phase 2: Multi-Dual and Gradients (Feb 2026)

**Deliverables**:
1. `multidual<N>` for computing N partial derivatives simultaneously
2. `gradient()` function for multi-variable functions
3. `jacobian()` function for vector-valued functions
4. SIMD optimization for multi-dual arithmetic

**Success Criteria**:
- [ ] Gradient of 100-variable function in single forward pass
- [ ] Jacobian computation matches finite difference (but exact)
- [ ] SIMD utilization >80% for multi-dual ops
- [ ] Memory usage O(N) for N-dimensional gradient

### Phase 3: Higher-Order Derivatives (Mar 2026)

**Deliverables**:
1. `dual2` for second derivatives (Hessian computation)
2. `dualN<K>` for K-th order derivatives
3. `hessian()` function
4. Taylor series expansion up to order K

**Success Criteria**:
- [ ] Hessian of 50-variable function computed correctly
- [ ] Taylor expansion matches numerical evaluation
- [ ] No exponential blowup in computation

### Phase 4: Automatic Mode Selection (Mar 2026)

**Deliverables**:
1. `@differentiable(mode: auto)` annotation
2. Compiler analysis of input/output dimensions
3. Automatic selection between forward (dual) and reverse (backprop) mode
4. Hybrid mode for mixed-dimension functions

**Success Criteria**:
- [ ] Compiler chooses optimal mode for 90%+ of functions
- [ ] Manual override available when needed
- [ ] No performance regression vs explicit mode selection

### Phase 5: Integration and Optimization (Apr 2026)

**Deliverables**:
1. Integration with Neural Gates
2. Integration with Belief System
3. Integration with Safety Manager
4. Dead derivative elimination optimization
5. Dual-aware constant folding

**Success Criteria**:
- [ ] Neural gate training uses dual numbers where beneficial
- [ ] Belief sensitivity analysis functional
- [ ] Safety margin gradients computed in real-time
- [ ] Overall compile time increase <10%

---

## Technical Dependencies

- LLVM for code generation and struct elimination
- SIMD intrinsics for multi-dual operations
- Existing Neural IR infrastructure (TASK-001)
- Real-time learning infrastructure (TASK-004)

## Research References

- Griewank & Walther, "Evaluating Derivatives" (2008) - Comprehensive AD textbook
- SciML Book, Ch 8: Forward-Mode AD via High Dimensional Algebras
- Revels et al., "Forward-Mode AD in Julia" (ForwardDiff.jl)
- Baydin et al., "Automatic Differentiation in Machine Learning: A Survey" (2018)
- Fike & Alonso, "Automatic Differentiation Through the Use of Hyper-Dual Numbers" (2011)

## Resources

- TASK-001: Neural IR specification
- TASK-004: Real-time learning specification
- Spec: simplex-docs/spec/14-neural-ir.md
- Spec: simplex-docs/spec/15-real-time-learning.md

---

## API Summary

```simplex
// Core types
dual                          // First-order dual number
multidual<N>                  // Multi-dimensional dual for gradients
dual2                         // Second-order dual for Hessians

// Constructors
dual::new(val, der)           // Explicit construction
dual::constant(val)           // Constant (der = 0)
dual::variable(val)           // Variable (der = 1)
multidual<N>::variable(val, i) // Variable with seed in dimension i

// Accessors
d.val                         // Function value
d.der                         // Derivative value
md.gradient()                 // Gradient vector [∂f/∂x₁, ..., ∂f/∂xₙ]

// Library functions
diff::derivative(f, x)        // Compute f'(x)
diff::gradient(f, x)          // Compute ∇f(x)
diff::jacobian(f, x)          // Compute J_f(x)
diff::hessian(f, x)           // Compute H_f(x)

// Annotations
@differentiable(mode: forward)  // Force forward-mode AD
@differentiable(mode: reverse)  // Force reverse-mode AD
@differentiable(mode: auto)     // Compiler chooses optimal mode
@zero_overhead                  // Ensure struct elimination
```

---

## Notes

Dual numbers represent a fundamental shift in how Simplex handles differentiation. Rather than treating AD as a library feature (like PyTorch/JAX), Simplex makes it a **language-level primitive**. The compiler understands dual numbers and optimizes them away completely.

This has profound implications:
1. **Performance**: Zero overhead means dual numbers can be used everywhere
2. **Composability**: Any function can be differentiated without modification
3. **Safety**: Gradients through safety constraints enable predictive violation detection
4. **Explainability**: Input sensitivity analysis becomes trivial

Combined with Neural Gates (TASK-001) and Real-Time Learning (TASK-004), dual numbers complete the differentiable programming story in Simplex.
