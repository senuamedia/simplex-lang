# TASK-031: Quantum Error Mitigation

**Version:** 0.15.0
**Status:** Complete
**Priority:** High
**Depends on:** TASK-021 (Quantum Bridge)

## Summary

Implement quantum error mitigation techniques in `simplex-quantum/` to handle the noise
inherent in near-term (NISQ) quantum hardware. Without error mitigation, results from
real quantum processors are unreliable for practical use.

## Deliverables

### Phase 1: Noise Models (~800 lines)

Location: `simplex-quantum/noise/`

- **Depolarizing noise** — symmetric single-qubit and two-qubit depolarizing channels
- **Amplitude damping** — models energy relaxation (T1 decay)
- **Phase damping** — models dephasing (T2 decay)
- **Readout error** — measurement bit-flip probabilities
- **Custom noise model** — user-defined Kraus operator channels
- **Noisy simulator** — extend statevector simulator with noise injection after each gate

```simplex
use simplex_quantum::noise::{NoiseModel, DepolarizingNoise, AmplitudeDamping};

let noise = NoiseModel::new()
    .add_single_qubit(DepolarizingNoise::new(0.001))
    .add_two_qubit(DepolarizingNoise::new(0.01))
    .add_readout_error(0.02);

let result = noisy_simulator.run(&circuit, &noise, shots: 1000);
```

Files:
- `simplex-quantum/noise/src/mod.sx` — NoiseModel, NoiseChannel trait
- `simplex-quantum/noise/src/depolarizing.sx` — depolarizing channels
- `simplex-quantum/noise/src/damping.sx` — amplitude and phase damping
- `simplex-quantum/noise/src/readout.sx` — measurement error model
- `simplex-quantum/noise/src/kraus.sx` — Kraus operator representation
- `simplex-quantum/noise/src/simulator.sx` — noisy simulation engine

### Phase 2: Zero-Noise Extrapolation (~600 lines)

Location: `simplex-quantum/mitigation/`

- **Richardson extrapolation** — polynomial extrapolation to zero-noise limit
- **Linear extrapolation** — simple two-point extrapolation
- **Exponential extrapolation** — for exponentially decaying expectation values
- **Noise scaling** — pulse stretching and gate folding to amplify noise
- **Automatic ZNE** — wrap any circuit execution with transparent error mitigation

```simplex
use simplex_quantum::mitigation::{ZNE, NoiseScaling, Extrapolation};

let zne = ZNE::new()
    .scaling(NoiseScaling::GateFolding)
    .scale_factors([1.0, 2.0, 3.0])
    .extrapolation(Extrapolation::Richardson);

let mitigated = zne.execute(&circuit, &backend);
```

Files:
- `simplex-quantum/mitigation/src/mod.sx` — MitigationStrategy trait
- `simplex-quantum/mitigation/src/zne.sx` — zero-noise extrapolation engine
- `simplex-quantum/mitigation/src/scaling.sx` — noise amplification methods
- `simplex-quantum/mitigation/src/extrapolation.sx` — polynomial/exponential fits

### Phase 3: Error Correction Codes (~1000 lines)

Location: `simplex-quantum/ecc/`

- **Bit-flip code** — 3-qubit repetition code (pedagogical)
- **Phase-flip code** — 3-qubit phase error correction
- **Shor code** — 9-qubit code correcting arbitrary single-qubit errors
- **Steane code** — 7-qubit CSS code
- **Surface code (simplified)** — distance-3 rotated surface code
- **Syndrome extraction** — ancilla-based error detection
- **Decoder** — minimum-weight perfect matching for surface codes

```simplex
use simplex_quantum::ecc::{SurfaceCode, SyndromeDecoder};

let code = SurfaceCode::new(distance: 3);
let logical_circuit = code.encode(&circuit);
let syndrome = code.extract_syndrome(&measurement);
let correction = SyndromeDecoder::mwpm(&syndrome);
```

Files:
- `simplex-quantum/ecc/src/mod.sx` — ErrorCorrectionCode trait
- `simplex-quantum/ecc/src/repetition.sx` — bit-flip and phase-flip codes
- `simplex-quantum/ecc/src/shor.sx` — 9-qubit Shor code
- `simplex-quantum/ecc/src/steane.sx` — 7-qubit Steane code
- `simplex-quantum/ecc/src/surface.sx` — rotated surface code
- `simplex-quantum/ecc/src/syndrome.sx` — syndrome extraction circuits
- `simplex-quantum/ecc/src/decoder.sx` — MWPM decoder

### Phase 4: Probabilistic Error Cancellation (~400 lines)

- **Quasi-probability decomposition** — represent ideal operation as linear combination of noisy operations
- **Monte Carlo sampling** — sample from quasi-probability distribution
- **Gate set tomography integration** — characterize noise for PEC calibration

Files:
- `simplex-quantum/mitigation/src/pec.sx` — probabilistic error cancellation
- `simplex-quantum/mitigation/src/tomography.sx` — gate set characterization

## Tests

Location: `tests/quantum/`

- `spec_noise_models.sx` — noise channel correctness
- `spec_zne.sx` — extrapolation accuracy on known circuits
- `spec_error_correction.sx` — encode/decode round-trip fidelity
- `spec_pec.sx` — PEC convergence tests

## Estimated Scope

~2800 lines across 4 phases, 8 new test files

## Success Criteria

- Noisy simulator matches analytical noise predictions within 1%
- ZNE reduces error by >50% on 4-qubit VQE test circuit
- Surface code (d=3) corrects all single-qubit errors
- All tests pass, zero regressions on existing 173 tests
