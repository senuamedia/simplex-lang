# Chapter 15: Quantum Error Mitigation

In the previous quantum tutorials, we worked with ideal simulators. Real quantum
hardware is noisy — every gate introduces errors, and measurements can flip. This
chapter teaches you how to model, mitigate, and correct quantum errors.

## Why Errors Matter

On a real quantum processor:
- Single-qubit gate error: ~0.01-0.1%
- Two-qubit gate error: ~0.5-2%
- Readout error: ~1-5%
- Qubit lifetime (T1): ~100-300 μs

A 50-gate circuit with 1% per-gate error has ~40% chance of at least one error.

## Noise Models

### Depolarizing Noise

The most common noise model. With probability p, a random Pauli error occurs:

```simplex
// Create a noise model
let noise = noise_model_new();

// Add 0.1% depolarizing noise to single-qubit gates
let depol = depolarizing_new(0.001);
noise_model_add_single_qubit(noise, depol);

// Add 1% depolarizing noise to two-qubit gates
let depol2 = depolarizing_new(0.01);
noise_model_add_two_qubit(noise, depol2);

// Add 2% readout error
noise_model_add_readout(noise, 0.02);
```

### Amplitude Damping

Models energy relaxation — the qubit spontaneously falls from |1⟩ to |0⟩:

```simplex
// T1 = 100μs, gate time = 50ns → gamma ≈ 0.0005
let amp_damp = amplitude_damping_new(0.0005);
noise_model_add_single_qubit(noise, amp_damp);
```

### Phase Damping

Models dephasing — the relative phase between |0⟩ and |1⟩ randomizes:

```simplex
// T2 = 50μs, gate time = 50ns → lambda ≈ 0.001
let phase_damp = phase_damping_new(0.001);
noise_model_add_single_qubit(noise, phase_damp);
```

## Noisy Simulation

Run a circuit with noise to see what real hardware would produce:

```simplex
// Build a simple circuit (Bell state)
let instrs = vec_new();
// H on qubit 0
vec_push(instrs, 1); vec_push(instrs, 0); vec_push(instrs, 0); vec_push(instrs, 0);
// CNOT on qubits 0,1
vec_push(instrs, 10); vec_push(instrs, 0); vec_push(instrs, 1); vec_push(instrs, 0);

// Run with noise (1000 shots)
let result = noisy_run(instrs, 2, noise, 1000);
// Without noise: |00⟩ ~50%, |11⟩ ~50%
// With noise: |00⟩ ~48%, |01⟩ ~1%, |10⟩ ~1%, |11⟩ ~50%
```

## Zero-Noise Extrapolation (ZNE)

### The Idea

1. Run the circuit at the normal noise level → get result E₁
2. Amplify the noise (gate folding) → get result E₂, E₃
3. Extrapolate back to zero noise → get E₀ (the ideal result)

### Gate Folding

To amplify noise, we "fold" gates: G → G·G†·G

This triples the noise from that gate while preserving the logical operation
(since G†·G = I, the net effect is still G).

### Implementation

```simplex
// Measure at 3 noise levels
let scale_factors = vec_new();
vec_push(scale_factors, f64_to_bits(1.0));  // normal
vec_push(scale_factors, f64_to_bits(2.0));  // 2x noise
vec_push(scale_factors, f64_to_bits(3.0));  // 3x noise

// Collect expectation values at each level
let values = vec_new();
vec_push(values, f64_to_bits(0.92));  // measured at 1x
vec_push(values, f64_to_bits(0.88));  // measured at 2x
vec_push(values, f64_to_bits(0.86));  // measured at 3x

// Extrapolate to zero noise
let zne = zne_new();
zne_set_scale_factors(zne, scale_factors);
zne_set_extrapolation(zne, EXTRAP_RICHARDSON);
let mitigated = zne_execute(zne, values);
// mitigated ≈ 1.0 (the ideal value!)
```

### Choosing an Extrapolation Method

| Method | Points Needed | Best When |
|--------|:------------:|-----------|
| Linear | 2 | Noise decays linearly |
| Polynomial | N+1 for degree N | Moderate noise |
| Richardson | N | Systematic errors |
| Exponential | 3 | Exponential decay (most circuits) |

## Error Correction Codes

For fault-tolerant quantum computing, we encode logical qubits into multiple physical qubits.

### 3-Qubit Bit-Flip Code

Protects against X errors (bit flips):

```
|0⟩_L = |000⟩   (logical zero)
|1⟩_L = |111⟩   (logical one)
```

If one qubit flips, majority vote identifies and corrects it:
- |000⟩ → correct (no error)
- |001⟩ → error on qubit 2, correct to |000⟩
- |010⟩ → error on qubit 1, correct to |000⟩
- |100⟩ → error on qubit 0, correct to |000⟩

```simplex
// Encode: CNOT(0,1), CNOT(0,2)
let encode = bitflip_encode_instructions();

// Syndrome extraction with 2 ancilla qubits
let syndrome_circuit = bitflip_syndrome_instructions();

// Look up correction from syndrome
let correction = bitflip_correction(syndrome_bits);
// Returns: -1 (no error) or qubit number to correct
```

### Surface Code

The leading candidate for practical quantum error correction:
- Uses a 2D grid of qubits
- Distance d code can correct ⌊(d-1)/2⌋ errors
- Below threshold (~1%), increasing distance exponentially suppresses errors

```simplex
let code = surface_code_new(3);  // distance-3 surface code
let n_data = surface_data_qubits(code);     // 9 data qubits
let n_ancilla = surface_ancilla_qubits(code); // 8 ancilla qubits
// Total: 17 physical qubits for 1 logical qubit
```

## Circuit Optimization

Fewer gates = less noise. Always optimize before running on hardware.

```simplex
// Build optimized circuit
let pipe = pipeline_default();
let optimized = pipeline_run(pipe, instructions, n_qubits, 10);

// Check improvement
let before = cost_gate_count(instructions);
let after = cost_gate_count(optimized);
// Typically 30-50% reduction
```

## Putting It All Together

A typical workflow for running on real quantum hardware:

```simplex
// 1. Build your circuit
let circuit = build_vqe_circuit(params);

// 2. Optimize for target hardware
let topo = topology_grid(4, 4);
let optimized = pipeline_run(pipeline_aggressive(), circuit, 16, 10);
let routed = route_circuit(optimized, 16, topo);
let native = decompose_to_native(routed, GATESET_IBM);

// 3. Apply error mitigation
let zne = zne_new();
zne_set_extrapolation(zne, EXTRAP_EXPONENTIAL);
let mitigated_result = zne_execute(zne, measured_values);

// 4. Interpret results
let energy = mitigation_value(mitigated_result);
let uncertainty = mitigation_error_bar(mitigated_result);
```

## Summary

| Technique | Overhead | Effectiveness | Use Case |
|-----------|----------|---------------|----------|
| Circuit optimization | None | 30-50% fewer gates | Always |
| ZNE | 3-5x circuit executions | 50-90% error reduction | Shallow circuits |
| PEC | Exponential samples | Near-exact | Short circuits |
| Bit-flip code | 3x qubits | Corrects 1 X error | Educational |
| Surface code | O(d²) qubits | Exponential suppression | Fault-tolerant |

## Exercises

1. Create a noise model with 0.5% depolarizing and 1% readout error. Run a Bell
   state circuit and compare the noisy results to the ideal.

2. Implement ZNE with linear extrapolation on a 4-qubit circuit. How many shots
   do you need for the mitigated result to be within 1% of the ideal?

3. Encode a qubit using the bit-flip code. Introduce a single bit-flip error on
   each qubit in turn, verify the syndrome identifies the correct qubit each time.

## Next Steps

- Explore the [Quantum Error Mitigation Specification](../spec/19-quantum-error-mitigation.md)
- Read about [Circuit Optimization](../spec/20-circuit-optimization.md) passes
- See the [API documentation](../api/simplex-quantum-noise/manifest.json) for the full API
