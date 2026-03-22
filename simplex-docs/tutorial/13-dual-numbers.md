# Tutorial 13: Dual Numbers and Automatic Differentiation

Simplex provides native **dual numbers** for forward-mode automatic differentiation. This tutorial teaches you how to compute analytical derivatives without numerical approximation.

---

## What Are Dual Numbers?

Dual numbers extend real numbers with an infinitesimal component (ε), where **ε² = 0**. A dual number has the form:

```
a + bε
```

- **a** is the value (what you'd normally compute)
- **b** is the derivative (how sensitive the output is to the input)

The key insight: when you do arithmetic with dual numbers, the chain rule happens automatically!

---

## Your First Dual Number

```simplex
use simplex::dual::dual;

fn main() {
    // Create a variable x = 3
    // The "variable" means we want to track derivatives with respect to it
    let x = dual::variable(3.0);

    // Compute x²
    let y = x * x;

    // y.val is the value: 3² = 9
    // y.der is the derivative: d(x²)/dx = 2x = 6
    print("f(3) = {y.val}");    // Output: 9
    print("f'(3) = {y.der}");   // Output: 6
}
```

**What just happened?**

When we wrote `x * x`, Simplex computed:
- Value: 3 × 3 = 9
- Derivative: (1 × 3) + (3 × 1) = 6 (using the product rule!)

---

## How the Chain Rule Works Automatically

The dual number multiplication rule encodes the product rule:

```
(a + bε) × (c + dε) = ac + (ad + bc)ε
```

Since ε² = 0, we drop that term. The result:
- Value: `ac`
- Derivative: `ad + bc` (this IS the product rule!)

### Example: Compound Expression

```simplex
let x = dual::variable(2.0);

// f(x) = x³ + 2x + 1
let f = x * x * x + dual::constant(2.0) * x + dual::constant(1.0);

print("f(2) = {f.val}");    // 8 + 4 + 1 = 13
print("f'(2) = {f.der}");   // 3x² + 2 = 12 + 2 = 14
```

---

## Constants vs Variables

**Variable**: Something we're differentiating with respect to (derivative seed = 1)

```simplex
let x = dual::variable(3.0);   // 3 + 1ε
```

**Constant**: A fixed value in our expression (derivative = 0)

```simplex
let c = dual::constant(5.0);   // 5 + 0ε
```

### Example: f(x) = 5x²

```simplex
let x = dual::variable(2.0);
let five = dual::constant(5.0);

let y = five * x * x;

print("f(2) = {y.val}");    // 5 × 4 = 20
print("f'(2) = {y.der}");   // 10x = 20
```

---

## Transcendental Functions

All common math functions work with dual numbers:

```simplex
let x = dual::variable(0.0);

let sin_x = x.sin();
print("sin(0) = {sin_x.val}");         // 0
print("d/dx sin(0) = {sin_x.der}");    // cos(0) = 1

let exp_x = x.exp();
print("exp(0) = {exp_x.val}");         // 1
print("d/dx exp(0) = {exp_x.der}");    // exp(0) = 1
```

### Available Functions

| Function | Value | Derivative |
|----------|-------|------------|
| `x.sin()` | sin(x) | cos(x) |
| `x.cos()` | cos(x) | -sin(x) |
| `x.exp()` | eˣ | eˣ |
| `x.ln()` | ln(x) | 1/x |
| `x.sqrt()` | √x | 1/(2√x) |
| `x.tanh()` | tanh(x) | 1 - tanh²(x) |
| `x.sigmoid()` | σ(x) | σ(x)(1-σ(x)) |
| `x.pow(n)` | xⁿ | nxⁿ⁻¹ |

---

## Practical Example: Sigmoid Derivative

The sigmoid function is crucial in machine learning:

```simplex
fn sigmoid_with_derivative(x: f64) -> (f64, f64) {
    let d = dual::variable(x);
    let result = d.sigmoid();
    (result.val, result.der)
}

// At x = 0
let (value, deriv) = sigmoid_with_derivative(0.0);
print("σ(0) = {value}");      // 0.5
print("σ'(0) = {deriv}");     // 0.25 (maximum slope)

// At x = 2
let (value, deriv) = sigmoid_with_derivative(2.0);
print("σ(2) = {value}");      // ~0.88
print("σ'(2) = {deriv}");     // ~0.10 (flattening out)
```

---

## Using the diff Module

For convenience, use the `diff` module:

```simplex
use simplex::diff::{derivative, gradient};

// Single-variable derivative
fn f(x: dual) -> dual {
    x * x + x.sin()
}

let df_at_3 = derivative(f, 3.0);
print("f'(3) = {df_at_3}");   // 2×3 + cos(3) ≈ 5.01
```

---

## Multi-Variable Gradients

To compute partial derivatives, use `multidual`:

```simplex
use simplex::dual::multidual;

// f(x, y) = x²y + sin(y)
fn f(x: multidual<2>, y: multidual<2>) -> multidual<2> {
    x * x * y + y.sin()
}

fn compute_gradient(x_val: f64, y_val: f64) -> [f64; 2] {
    // Mark x as variable 0, y as variable 1
    let x = multidual::<2>::variable(x_val, 0);
    let y = multidual::<2>::variable(y_val, 1);

    let result = f(x, y);
    result.gradient()
}

let grad = compute_gradient(2.0, 3.0);
print("∂f/∂x = {grad[0]}");  // 2xy = 12
print("∂f/∂y = {grad[1]}");  // x² + cos(y) ≈ 3.01
```

---

## Second Derivatives (Hessians)

Use `dual2` for second-order derivatives:

```simplex
use simplex::dual::dual2;

let x = dual2::variable(2.0);
let y = x * x * x;  // f(x) = x³

print("f(2) = {y.val}");     // 8
print("f'(2) = {y.d1}");     // 3x² = 12
print("f''(2) = {y.d2}");    // 6x = 12
```

---

## Why Not Just Use Numerical Differentiation?

Numerical differentiation (finite differences) has problems:

```simplex
// Numerical: (f(x+h) - f(x)) / h
fn numerical_derivative(f: fn(f64) -> f64, x: f64) -> f64 {
    let h = 0.0001;
    (f(x + h) - f(x)) / h
}
```

**Problems:**
1. **Truncation error**: h too large → inaccurate approximation
2. **Round-off error**: h too small → floating point noise
3. **Expensive**: Need 2 function evaluations per derivative

**Dual numbers:**
1. **Exact**: No approximation, mathematically correct
2. **Efficient**: Derivative computed alongside value
3. **Composable**: Works through arbitrary function compositions

---

## Application: Gradient Descent

```simplex
use simplex::dual::dual;

// Minimize f(x) = (x - 3)² using gradient descent
fn optimize() -> f64 {
    var x = 0.0;  // Starting point
    let lr = 0.1; // Learning rate

    for _ in 0..100 {
        let d = dual::variable(x);
        let loss = (d - dual::constant(3.0)).pow(2.0);

        // loss.der is the gradient
        x = x - lr * loss.der;
    }

    x  // Should converge to 3.0
}

let result = optimize();
print("Minimum at x = {result}");  // ~3.0
```

---

## Application: Newton's Method

Find roots using both first and second derivatives:

```simplex
use simplex::dual::dual2;

// Find root of f(x) = x² - 2 (i.e., find √2)
fn newtons_method() -> f64 {
    var x = 1.0;  // Initial guess

    for _ in 0..10 {
        let d = dual2::variable(x);
        let f = d * d - dual2::constant(2.0);

        // Newton's method: x_new = x - f(x)/f'(x)
        x = x - f.val / f.d1;
    }

    x
}

let sqrt2 = newtons_method();
print("√2 ≈ {sqrt2}");  // 1.41421356...
```

---

## Zero-Overhead Abstraction

Dual numbers compile to the same code as hand-written derivatives:

```simplex
// This code:
let x = dual::variable(3.0);
let y = x * x;
let derivative = y.der;

// Compiles to essentially:
let x_val = 3.0;
let x_der = 1.0;
let y_val = x_val * x_val;
let y_der = 2.0 * x_val * x_der;
let derivative = y_der;
```

The struct is eliminated at compile time. There's no runtime overhead compared to computing derivatives by hand.

---

## Summary

| Type | Purpose | Use Case |
|------|---------|----------|
| `dual` | First derivative of single variable | Simple derivatives |
| `multidual<N>` | Gradient (N partial derivatives) | Multi-variable functions |
| `dual2` | Second derivative | Hessians, curvature |

**Key Points:**
1. `dual::variable(x)` marks what you're differentiating with respect to
2. `dual::constant(c)` marks fixed values
3. All arithmetic and transcendental functions propagate derivatives
4. Results are analytical (not numerical approximations)
5. Zero runtime overhead vs hand-coded derivatives

---

## Exercises

1. Compute the derivative of f(x) = x·sin(x) at x = π
2. Find the gradient of f(x,y) = x·exp(y) at (1, 0)
3. Use Newton's method to find ∛8 (cube root of 8)
4. Implement gradient descent for f(x,y) = x² + y² starting from (5, 5)

---

## Next Steps

- [Tutorial 14: Self-Learning Annealing](14-self-learning-annealing.md) - Using dual numbers to learn adaptive schedules
- [Specification: Real-Time Learning](../spec/15-real-time-learning.md) - Full dual number API reference
