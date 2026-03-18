# 19. Quantum Error Mitigation

**Version:** 0.15.0

## Overview

Quantum error mitigation addresses the noise inherent in near-term (NISQ) quantum hardware.
Without mitigation, results from real quantum processors are unreliable for practical use.
Simplex provides a layered approach: noise models for simulation, zero-noise extrapolation
for immediate error reduction, error correction codes for fault-tolerant computation, and
probabilistic error cancellation for advanced mitigation.

## Architecture

```
┌─────────────────────────────────────────────────┐
│               Application Layer                  │
│      VQE, QAOA, Grover, custom algorithms        │
├─────────────────────────────────────────────────┤
│           Mitigation Layer (v0.15.0)             │
│   ┌─────────┐ ┌─────┐ ┌─────┐ ┌──────────────┐│
│   │Noise Sim│ │ ZNE │ │ PEC │ │Error Correct. ││
│   └─────────┘ └─────┘ └─────┘ └──────────────┘│
├─────────────────────────────────────────────────┤
│           Backend Layer (v0.14.0)                │
│   LocalSimulator | Braket | IBM | Azure          │
└─────────────────────────────────────────────────┘
```

## Noise Models

### Depolarizing Channel

The most common noise model. With probability p, a random Pauli error (X, Y, or Z) is applied:

```
ρ → (1-p)ρ + (p/3)(XρX + YρY + ZρZ)
```

For two-qubit gates, the 15 non-identity two-qubit Paulis are applied with equal probability.

**Implementation**: `simplex-quantum/noise/src/depolarizing.sx`

```simplex
let channel = depolarizing_new(0.001);  // 0.1% error rate
// Apply to statevector
depolarizing_apply_single(state_re, state_im, dim, qubit, 0.001);
```

### Amplitude Damping

Models energy relaxation (T1 decay). The excited state |1⟩ spontaneously decays to |0⟩:

```
K₀ = [[1, 0], [0, √(1-γ)]]
K₁ = [[0, √γ], [0, 0]]
```

where γ = 1 - exp(-t_gate/T1).

**Implementation**: `simplex-quantum/noise/src/damping.sx`

### Phase Damping

Models dephasing (T2 decay). Coherences between |0⟩ and |1⟩ decay without energy loss:

```
K₀ = [[1, 0], [0, √(1-λ)]]
K₁ = [[0, 0], [0, √λ]]
```

where λ = 1 - exp(-t_gate/T2), with T2 ≤ 2·T1.

### Readout Error

Measurement results can be bit-flipped with probability p:

```
P(measure 0 | true state 1) = p₀₁
P(measure 1 | true state 0) = p₁₀
```

The calibration matrix enables readout error mitigation via matrix inversion.

### Custom Kraus Channels

User-defined channels specified by a set of Kraus operators {Kₖ} satisfying the
completeness relation Σ Kₖ†Kₖ = I:

```simplex
let channel = kraus_channel_new();
let k0 = kraus_matrix(1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.99, 0.0);
let k1 = kraus_matrix(0.0, 0.0, 0.14, 0.0, 0.0, 0.0, 0.0, 0.0);
kraus_add_operator(channel, k0);
kraus_add_operator(channel, k1);
let valid = kraus_validate(channel);  // checks Σ K†K = I
```

## Noisy Simulator

The noisy simulator extends the statevector simulator with noise injection:

1. Initialize state to |0...0⟩
2. For each gate in the circuit:
   a. Apply the gate unitary
   b. Apply noise channels from the noise model
3. Before measurement, apply readout error
4. Repeat for each shot

The simulator uses f64 amplitude buffers with LCG pseudo-random number generation
for stochastic noise sampling.

## Zero-Noise Extrapolation (ZNE)

### Principle

Execute the circuit at multiple noise levels and extrapolate to the zero-noise limit.
Noise is amplified by gate folding: G → G·G†·G (triples the noise contribution).

### Scale Factors

- Gate folding: scale factors 1, 3, 5, 7, ... (odd integers)
- Partial folding: non-integer scale factors by folding a fraction of gates

### Extrapolation Methods

| Method | Formula | Best for |
|--------|---------|----------|
| Linear | y = a + bx, evaluate at x=0 | Low noise, linear decay |
| Polynomial | Lagrange interpolation at x=0 | Moderate noise |
| Richardson | Weighted combination via Vandermonde | Systematic errors |
| Exponential | A·exp(-Bx) + C, ratio method | Exponential decay |

### Accuracy

ZNE typically reduces error by 50-90% compared to unmitigated results, depending on
circuit depth and noise characteristics.

## Error Correction Codes

### Bit-Flip Code [[3,1,1]]

The simplest quantum error correction code. Encodes one logical qubit into three physical qubits:

```
|0⟩_L = |000⟩
|1⟩_L = |111⟩
```

Syndrome extraction uses two ancilla qubits to detect which (if any) qubit was flipped.

### Shor Code [[9,1,3]]

Corrects any single-qubit error (X, Y, or Z) by combining bit-flip and phase-flip protection:

- 3 blocks of 3 qubits for bit-flip protection within blocks
- Phase-flip protection across blocks using Hadamard basis
- 8 syndrome bits for complete error identification

### Steane Code [[7,1,3]]

A CSS (Calderbank-Shor-Steane) code based on the classical Hamming [7,4,3] code:

- 3 X stabilizers detect Z errors
- 3 Z stabilizers detect X errors
- Transversal gates enable fault-tolerant operations

### Surface Code [[d²,1,d]]

The leading candidate for fault-tolerant quantum computing:

- Distance d code uses d² data qubits and approximately d²-1 ancilla qubits
- Threshold error rate ≈ 1% for depolarizing noise
- Logical error rate scales as (p/p_threshold)^((d+1)/2)

### Minimum-Weight Perfect Matching (MWPM) Decoder

For surface codes, the MWPM decoder finds the most likely error given the syndrome:

1. Build a graph where nodes are syndrome defects
2. Edge weights represent the probability of the corresponding error chain
3. Find the minimum-weight perfect matching
4. Apply the correction corresponding to the matching

## Probabilistic Error Cancellation (PEC)

### Principle

Decompose the ideal (noiseless) gate as a linear combination of implementable noisy operations:

```
G_ideal = Σ qᵢ · Nᵢ
```

where qᵢ are quasi-probabilities (can be negative) and Nᵢ are noisy operations.

### Sampling Overhead

The sampling overhead γ = Σ|qᵢ| determines how many samples are needed.
For n gates with error rate p: γ ≈ (1 + 2p)ⁿ. This grows exponentially,
limiting PEC to moderate-depth circuits.

### Variance

PEC variance = γ²/N where N is the number of samples. More overhead requires
more samples for the same precision.

## Module Structure

```
simplex-quantum/noise/
├── Modulus.toml
└── src/
    ├── mod.sx           NoiseModel, channel types
    ├── depolarizing.sx  Depolarizing channels
    ├── damping.sx       Amplitude and phase damping
    ├── readout.sx       Measurement error model
    ├── kraus.sx         Custom Kraus operators
    └── simulator.sx     Noisy simulation engine

simplex-quantum/mitigation/
├── Modulus.toml
└── src/
    ├── mod.sx           MitigationResult, strategy types
    ├── zne.sx           Zero-noise extrapolation
    ├── scaling.sx       Noise amplification (gate folding)
    ├── extrapolation.sx Polynomial/exponential fitting
    ├── pec.sx           Probabilistic error cancellation
    └── tomography.sx    Gate set characterization

simplex-quantum/ecc/
├── Modulus.toml
└── src/
    ├── mod.sx           Code types, info
    ├── repetition.sx    Bit-flip, phase-flip codes
    ├── shor.sx          9-qubit Shor code
    ├── steane.sx        7-qubit Steane code
    ├── surface.sx       Rotated surface code
    ├── syndrome.sx      Syndrome extraction
    └── decoder.sx       MWPM decoder
```

## References

- Temme, K., Bravyi, S., & Gambetta, J. M. (2017). Error mitigation for short-depth quantum circuits. Physical Review Letters, 119(18), 180509.
- Li, Y., & Benjamin, S. C. (2017). Efficient variational quantum simulator incorporating active error minimization. Physical Review X, 7(2), 021050.
- Fowler, A. G., Mariantoni, M., Martinis, J. M., & Cleland, A. N. (2012). Surface codes: Towards practical large-scale quantum computation. Physical Review A, 86(3), 032324.
