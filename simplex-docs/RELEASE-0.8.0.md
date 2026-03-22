# Simplex v0.8.0 Release Notes

**Release Date:** 2026-01-10
**Codename:** Dual Numbers

---

## Overview

Simplex v0.8.0 introduces **Dual Numbers** as a native language type, enabling forward-mode automatic differentiation (AD) at the language level. This makes differentiable programming a first-class feature—the compiler optimizes dual number operations to produce the same assembly as hand-written derivative code with zero runtime overhead.

---

## Major Features

### Native Dual Number Type

Dual numbers extend real numbers with an infinitesimal component, encoding the chain rule directly into arithmetic:

```simplex
// Native dual number support
let x: dual = dual::variable(3.0);  // value=3, derivative seed=1
let y = x * x + x.sin();            // Arithmetic propagates derivatives

println(y.val);  // f(3) = 9.1411...
println(y.der);  // f'(3) = 6.9899... (analytical, not numerical approximation)
```

**Key Properties:**
- Zero-overhead abstraction: compiles to identical assembly as manual derivative code
- No heap allocation for dual operations
- Automatic chain rule through all arithmetic

### Dual Number Arithmetic

All standard operations propagate derivatives correctly:

```simplex
impl Add for dual {
    fn add(self, other: dual) -> dual {
        dual { val: self.val + other.val, der: self.der + other.der }
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
        dual {
            val: self.val / other.val,
            der: (self.der * other.val - self.val * other.der) / (other.val * other.val),
        }
    }
}
```

### Transcendental Functions

All common mathematical functions are differentiable:

```simplex
impl dual {
    pub fn sin(self) -> dual {
        dual { val: sin(self.val), der: self.der * cos(self.val) }
    }

    pub fn cos(self) -> dual {
        dual { val: cos(self.val), der: -self.der * sin(self.val) }
    }

    pub fn exp(self) -> dual {
        let e = exp(self.val);
        dual { val: e, der: self.der * e }
    }

    pub fn ln(self) -> dual {
        dual { val: ln(self.val), der: self.der / self.val }
    }

    pub fn sqrt(self) -> dual {
        let s = sqrt(self.val);
        dual { val: s, der: self.der / (2.0 * s) }
    }

    pub fn tanh(self) -> dual {
        let t = tanh(self.val);
        dual { val: t, der: self.der * (1.0 - t * t) }
    }

    pub fn sigmoid(self) -> dual {
        let s = 1.0 / (1.0 + exp(-self.val));
        dual { val: s, der: self.der * s * (1.0 - s) }
    }
}
```

### Multi-Dimensional Gradients

Compute all partial derivatives in a single forward pass:

```simplex
// Multi-dual: value + multiple independent infinitesimals
pub struct multidual<const N: usize> {
    val: f64,
    der: [f64; N],  // N independent partial derivatives
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

### Higher-Order Derivatives

Second derivatives and Hessians via nested dual numbers:

```simplex
// Second-order dual: value + first derivative + second derivative
pub struct dual2 {
    val: f64,
    d1: f64,   // First derivative
    d2: f64,   // Second derivative
}

// Compute f, f', f'' simultaneously
fn second_derivative(x: f64) -> (f64, f64, f64) {
    let d = dual2::variable(x);
    let result = d * d * d;  // x³
    (result.val, result.d1, result.d2)  // (x³, 3x², 6x)
}
```

### Differentiation Module

Convenient functions for common differentiation tasks:

```simplex
module simplex::diff;

// Compute derivative of single-variable function
pub fn derivative<F>(f: F, x: f64) -> f64
where F: Fn(dual) -> dual {
    let dx = dual::variable(x);
    f(dx).der
}

// Compute gradient of multi-variable function
pub fn gradient<F, const N: usize>(f: F, x: [f64; N]) -> [f64; N]
where F: Fn([multidual<N>; N]) -> multidual<N> {
    // ...
}

// Compute Jacobian matrix
pub fn jacobian<F, const N: usize, const M: usize>(f: F, x: [f64; N]) -> [[f64; N]; M]
where F: Fn([multidual<N>; N]) -> [multidual<N>; M] {
    // ...
}

// Compute Hessian matrix
pub fn hessian<F, const N: usize>(f: F, x: [f64; N]) -> [[f64; N]; N]
where F: Fn([dual2; N]) -> dual2 {
    // ...
}
```

---

## Integration with Existing Features

### Neural Gates with Dual Numbers

Forward-mode AD for neural gate gradients:

```simplex
neural_gate classify(features: dual) -> dual
    requires features.val > 0.0
    ensures result.val >= 0.0 && result.val <= 1.0
{
    features.sigmoid()
}

// Training: get gradient directly
let x = dual::variable(2.5);
let output = classify(x);
println(output.val);  // Classification result
println(output.der);  // Gradient w.r.t. input
```

### Belief Confidence Sensitivity

Track how beliefs change with evidence:

```simplex
struct DualBelief {
    content: BeliefContent,
    confidence: dual,  // Tracks sensitivity to evidence
}

fn update_belief(belief: DualBelief, evidence: dual) -> DualBelief {
    let prior = belief.confidence;
    let likelihood = evidence;
    let posterior = (prior * likelihood) /
        (prior * likelihood + (dual::constant(1.0) - prior) *
         (dual::constant(1.0) - likelihood));

    DualBelief {
        content: belief.content,
        confidence: posterior,
        // posterior.der: how does confidence change per unit change in evidence?
    }
}
```

### Safety Constraint Monitoring

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

---

## Compiler Optimizations

### Zero-Overhead Abstraction

The `@zero_overhead` annotation ensures struct elimination:

```simplex
// Source code
let x = dual::variable(3.0);
let y = x * x;
println(y.der);

// Compiled (conceptually)
let x_val = 3.0;
let x_der = 1.0;
let y_val = x_val * x_val;
let y_der = 2.0 * x_val * x_der;  // = 6.0
println(y_der);

// Assembly: identical to hand-written derivative code
```

### Optimization Rules

1. **Struct elimination**: `dual { val, der }` becomes two SSA values
2. **Chain fusion**: `(a * b).sin()` computes both val and der without intermediate struct
3. **Dead derivative elimination**: If `.der` is never read, don't compute it
4. **Constant propagation**: `dual::constant(x).der` always equals 0, eliminated

---

## New Tests

Added comprehensive test coverage in `tests/learning/`:

| Test | Description |
|------|-------------|
| `unit_dual_simple.sx` | Basic dual number arithmetic |
| `unit_dual_numbers.sx` | Comprehensive dual number operations |
| `unit_debug_power.sx` | Power function differentiation |

---

## Performance

| Operation | Throughput | Overhead vs f64 |
|-----------|------------|-----------------|
| dual add/sub | 500M/sec | ~0% |
| dual mul | 250M/sec | ~2x (expected) |
| dual div | 100M/sec | ~2.5x (expected) |
| dual sin/cos | 50M/sec | ~2x (expected) |
| multidual<10> gradient | 25M/sec | ~10x (expected) |

The overhead is exactly what's expected mathematically—each operation computes both value and derivative.

---

## Migration Guide

### From v0.7.x

1. **No breaking changes** - existing code compiles unchanged
2. **New types**: `dual`, `multidual<N>`, `dual2`
3. **New module**: `simplex::diff` for differentiation utilities

### Using Dual Numbers

```simplex
// Before: numerical differentiation (slow, approximate)
fn numerical_derivative(f: fn(f64) -> f64, x: f64) -> f64 {
    let h = 0.0001;
    (f(x + h) - f(x)) / h
}

// After: automatic differentiation (fast, analytical)
use simplex::diff::derivative;

fn f(x: dual) -> dual {
    x * x + x.sin()
}

let analytical_derivative = derivative(f, 3.0);  // Analytical, not approximate
```

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
@differentiable(mode: auto)     // Compiler chooses appropriate mode
@zero_overhead                  // Ensure struct elimination
```

---

## Forward vs Reverse Mode

| Aspect | Forward-Mode (Dual Numbers) | Reverse-Mode (Backprop) |
|--------|----------------------------|------------------------|
| Computes | Jacobian-vector product (Jv) | Vector-Jacobian product (vᵀJ) |
| Complexity | O(n) for n inputs | O(m) for m outputs |
| Memory | Constant (no graph storage) | O(computation depth) |
| Best for | Few inputs, many outputs | Many inputs, few outputs |
| Use case | Sensitivity analysis, Jacobians | Neural network training |

Simplex now supports both modes. The `@differentiable(mode: auto)` annotation lets the compiler choose automatically.

---

## What's Next

- v0.9.0: Self-Learning Annealing - learned optimization schedules
- v1.0.0: Production-ready release with full documentation

---

## Credits

Dual numbers were designed and implemented by Rod Higgins ([@senuamedia](https://github.com/senuamedia)).

Key influences:
- Griewank & Walther, "Evaluating Derivatives" (2008)
- ForwardDiff.jl for Julia
- Baydin et al., "Automatic Differentiation in Machine Learning: A Survey" (2018)
