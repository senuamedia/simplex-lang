# Tutorial 14: Self-Learning Annealing

Simplex v0.11.0 introduces **self-learning annealing** - temperature schedules that optimize themselves through gradient descent. This tutorial teaches you how to build systems where the cooling rate learns alongside the model.

---

## The Problem with Fixed Schedules

Traditional annealing uses fixed decay schedules:

```simplex
// Traditional: exponential decay
let temperature = initial_temp * 0.99.pow(step)
```

**Problems:**
1. **Manual tuning** - Trial and error to find good decay rates
2. **One-size-fits-all** - Same schedule for every problem
3. **No adaptation** - Can't speed up for easy parts, slow down for hard parts

---

## The Meta-Dual Number

Self-learning annealing wraps temperature as a **dual number**:

```
τ + τ̇ε
```

Where:
- **τ** is the current temperature value
- **τ̇** (tau-dot) is the sensitivity: "How does changing temperature affect learning?"

```simplex
use simplex::dual::dual

// Temperature is now a trainable dual number
var tau: dual = dual::variable(1.0)
```

---

## Inner and Outer Loop Optimization

The system performs **nested optimization**:

### Inner Loop (Standard Training)

Neural gates use the current τ to learn the task:

```simplex
fn inner_loop(model: &mut Model, data: &Batch, tau: dual) {
    // Create gate with current temperature
    let gate = Gate::gumbel(model.logits, temperature: tau)

    // Forward pass through the gate
    let output = gate.forward(data)

    // Compute loss
    let loss = compute_loss(output, data.target)

    // Standard gradient descent on model weights
    loss.backward()
    optimizer.step(&mut model.params())
}
```

### Outer Loop (Meta-Learning)

After inner iterations, the system uses **Reverse-Mode AD** to look back through training:

```simplex
fn outer_loop(tau: &mut dual, loss: f64) {
    // Extract the meta-gradient: ∂L/∂τ
    let tau_gradient = tau.der

    // Update temperature based on meta-gradient
    let meta_lr = 0.01
    *tau = dual::variable(tau.val - meta_lr * tau_gradient)

    // Keep temperature positive
    *tau = tau.max(dual::constant(0.01))
}
```

---

## The Chain Rule for Temperature

When backward pass executes, it calculates:

```
∂L/∂τ = (∂L/∂y) · (∂y/∂τ)
```

Breaking this down:

- **∂L/∂y**: How does the output affect loss? (standard gradient)
- **∂y/∂τ**: How does temperature affect the output? (temperature sensitivity)

### What ∂y/∂τ Tells Us

For Gumbel-Softmax:

```
y = exp((log(π) + g) / τ) / Σ exp((log(π) + g) / τ)
```

The derivative ∂y/∂τ encodes how the "sharpness" of the probability distribution affects error:

- **High τ**: Soft probabilities, explores many options
- **Low τ**: Sharp probabilities, commits to choices

---

## The Meta-Update Rule

Temperature updates based on the meta-gradient:

```simplex
τ_new = τ_old - η · (∂L/∂τ)
```

### Interpreting the Meta-Gradient

**Positive meta-gradient (∂L/∂τ > 0):**
- Temperature is too low
- System is hardening too fast, making mistakes
- **Action**: Slow cooling or "re-heat"

**Negative meta-gradient (∂L/∂τ < 0):**
- Temperature is too high
- Gate is too "soft" or blurry
- **Action**: Accelerate cooling to force decisions

---

## Complete Training Loop

```simplex
use simplex::dual::dual
use simplex::neural::Gate

fn train_with_self_annealing(model: &mut Model, dataset: &Dataset) {
    // Initialize temperature as trainable dual number
    var tau: dual = dual::variable(1.0)
    let meta_lr = 0.01
    let model_lr = 0.001

    for epoch in 0..100 {
        for batch in dataset.batches() {
            // ===== FORWARD PASS =====
            // Create gate using current tau
            // Dual number system tracks how tau affects 'choice'
            let gate = Gate::gumbel(model.logits(&batch.input), temperature: tau)
            let output = gate.forward(&batch.input)

            // ===== LOSS COMPUTATION =====
            let loss = mse_loss(&output, &batch.target)

            // ===== REVERSE MODE AD =====
            // Calculate standard loss gradient AND meta-gradient
            loss.backward()  // Triggers chain rule back to tau

            // ===== MODEL UPDATE (Inner Loop) =====
            optimizer.step(&mut model.params(), lr: model_lr)

            // ===== META-UPDATE (Outer Loop) =====
            // Adjust tau based on whether a change would have reduced loss
            let tau_gradient = tau.der  // Extract ε coefficient (∂L/∂τ)

            tau = dual::variable(tau.val - meta_lr * tau_gradient)

            // Ensure tau stays positive
            tau = tau.max(dual::constant(0.01))
        }

        print("Epoch {epoch}: temp = {tau.val:.4}")
    }
}
```

---

## Per-Gate Temperature

Each neural gate can have its own local temperature:

```simplex
struct AdaptiveGate {
    logits: Tensor,
    tau: dual,      // Local temperature
    meta_lr: f64,
}

impl AdaptiveGate {
    fn new(logits: Tensor) -> Self {
        AdaptiveGate {
            logits,
            tau: dual::variable(1.0),
            meta_lr: 0.01,
        }
    }

    fn forward(&mut self, input: &Tensor) -> Tensor {
        let gate = Gate::gumbel(self.logits, temperature: self.tau)
        gate.forward(input)
    }

    fn update_temperature(&mut self) {
        let gradient = self.tau.der
        self.tau = dual::variable(self.tau.val - self.meta_lr * gradient)
        self.tau = self.tau.max(dual::constant(0.01))
    }
}
```

---

## Escaping Local Minima

Self-learning annealing can trigger "re-heating" when stuck:

```simplex
fn adaptive_train(model: &mut Model, data: &Dataset) {
    var tau: dual = dual::variable(1.0)
    var prev_loss = f64::MAX
    var stuck_count = 0

    for step in 0..10000 {
        let loss = train_step(model, data, tau)

        // Detect if stuck
        if (loss - prev_loss).abs() < 0.0001 {
            stuck_count += 1
        } else {
            stuck_count = 0
        }

        // Re-heat if stuck for too long
        if stuck_count > 50 {
            tau = dual::variable(tau.val * 1.5)  // Increase temperature
            stuck_count = 0
            print("Re-heating to escape local minimum")
        }

        // Normal meta-update
        tau = dual::variable(tau.val - 0.01 * tau.der)
        tau = tau.max(dual::constant(0.01))

        prev_loss = loss
    }
}
```

---

## Neurosymbolic Transition

Self-learning annealing enables automatic transition from **Neural** (fuzzy) to **Symbolic** (discrete):

```simplex
fn train_to_symbolic(model: &mut Model, data: &Dataset) -> SymbolicProgram {
    var tau: dual = dual::variable(1.0)
    let freeze_threshold = 0.05

    loop {
        train_epoch(model, data, &mut tau)

        // Check if temperature has cooled enough
        if tau.val < freeze_threshold {
            print("Temperature frozen - extracting symbolic program")
            break
        }
    }

    // Extract discrete logic from frozen gates
    model.extract_symbolic()
}
```

### What Happens During Transition

1. **Early training (τ ≈ 1.0)**: Gates are soft, exploring many paths
2. **Mid training (τ ≈ 0.3)**: Gates becoming sharper, committing to paths
3. **Late training (τ < 0.1)**: Gates nearly discrete, ready for extraction
4. **Frozen (τ < 0.05)**: Extract as symbolic program with zero overhead

---

## Comparison: Fixed vs Self-Learning

| Component | Traditional Annealing | Simplex Self-Learning |
|-----------|----------------------|----------------------|
| Decay Rate | Fixed (e.g., 0.99^t) | Dynamic (learned via ε) |
| Adaptability | None (same for every problem) | High (fast for easy, slow for hard) |
| Tuning | Manual (trial & error) | Automated (gradient descent on τ) |
| Local Minima | Often stuck | Can re-heat to escape |
| Per-Gate Control | No | Yes (each gate learns its own τ) |

---

## Why Self-Learning Annealing is Powerful

| Feature | Benefit |
|---------|---------|
| **Optimal Hardening** | Each gate cools at its ideal speed |
| **Avoiding Local Minima** | Meta-gradient can trigger re-heating |
| **Automatic Logic Synthesis** | System discovers when search is over |
| **No Manual Tuning** | Schedule optimizes itself |
| **Neurosymbolic Bridge** | Smooth transition from neural to discrete |

---

## Practical Example: Learning a Classifier

```simplex
use simplex::dual::dual
use simplex::neural::{Gate, Linear}

struct LearnableClassifier {
    layer1: Linear,
    layer2: Linear,
    gate_tau: dual,
}

impl LearnableClassifier {
    fn new(input_dim: usize, hidden_dim: usize, output_dim: usize) -> Self {
        LearnableClassifier {
            layer1: Linear::new(input_dim, hidden_dim),
            layer2: Linear::new(hidden_dim, output_dim),
            gate_tau: dual::variable(1.0),
        }
    }

    fn forward(&self, x: &Tensor) -> Tensor {
        let h = self.layer1.forward(x).relu()
        let logits = self.layer2.forward(&h)

        // Gumbel-softmax with learnable temperature
        let gate = Gate::gumbel(logits, temperature: self.gate_tau)
        gate.sample()
    }

    fn train_step(&mut self, x: &Tensor, target: &Tensor, lr: f64, meta_lr: f64) {
        let output = self.forward(x)
        let loss = cross_entropy(&output, target)

        loss.backward()

        // Update model weights
        self.layer1.update(lr)
        self.layer2.update(lr)

        // Update temperature (meta-learning)
        let tau_grad = self.gate_tau.der
        self.gate_tau = dual::variable(self.gate_tau.val - meta_lr * tau_grad)
        self.gate_tau = self.gate_tau.max(dual::constant(0.01))
    }
}
```

---

## Summary

| Concept | Description |
|---------|-------------|
| Meta-Dual Number | τ + τ̇ε tracks temperature sensitivity |
| Inner Loop | Standard training with current temperature |
| Outer Loop | Meta-gradient updates temperature schedule |
| Chain Rule | ∂L/∂τ = (∂L/∂y) · (∂y/∂τ) |
| Meta-Update | τ_new = τ_old - η · (∂L/∂τ) |
| Re-heating | Positive gradient → slow cooling or increase τ |
| Freezing | When τ < threshold, extract symbolic program |

**Key Insight**: By making temperature a dual number, the system learns *how to learn* - optimizing not just the model weights, but the exploration/exploitation tradeoff itself.

---

## Exercises

1. Implement a simple gate with self-learning temperature and observe how τ changes during training
2. Create a multi-gate network where each gate has its own τ - do they converge at different rates?
3. Add re-heating logic and test on a problem with local minima
4. Extract a symbolic program from a fully-cooled network

---

## Next Steps

- [Specification: Neural IR](../spec/14-neural-ir.md) - Full Neural IR reference
- [Specification: Real-Time Learning](../spec/15-real-time-learning.md) - Online adaptation
- [Tutorial 13: Dual Numbers](13-dual-numbers.md) - Foundation for meta-gradients
