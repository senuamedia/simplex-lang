# 20. Quantum Circuit Optimization

**Version:** 0.15.0

## Overview

Circuit optimization reduces gate count, circuit depth, and two-qubit gate usage.
On real quantum hardware, every gate introduces noise, so fewer gates directly
translates to better results. Simplex provides a multi-pass optimization pipeline
with hardware-aware routing and native gate decomposition.

## Optimization Pipeline

```
Input Circuit
    │
    ▼
┌──────────────────┐
│ Identity Elim.   │  H·H=I, X·X=I, CNOT·CNOT=I
├──────────────────┤
│ Rotation Merge   │  Rz(a)·Rz(b) → Rz(a+b)
├──────────────────┤
│ Commutation      │  Cancel non-adjacent gates
│ Cancellation     │  via commutation analysis
├──────────────────┤
│ Single-Qubit     │  ZYZ decomposition of
│ Fusion           │  consecutive single-qubit gates
├──────────────────┤
│ Depth Reduction  │  ASAP/ALAP scheduling
├──────────────────┤
│ Qubit Routing    │  SWAP insertion for
│                  │  hardware connectivity
├──────────────────┤
│ Native Gate      │  Decompose to target
│ Decomposition    │  hardware gate set
└──────────────────┘
    │
    ▼
Optimized Circuit
```

## Gate Fusion

### Identity Elimination

Removes consecutive self-inverse gate pairs on the same qubit:

| Pattern | Result | Savings |
|---------|--------|---------|
| H·H | I | 2 gates |
| X·X | I | 2 gates |
| Y·Y | I | 2 gates |
| Z·Z | I | 2 gates |
| CNOT·CNOT (same qubits) | I | 2 gates |
| SWAP·SWAP (same qubits) | I | 2 gates |

### Rotation Merging

Consecutive rotation gates on the same qubit merge by adding angles:

```
Rz(a) · Rz(b) → Rz(a + b)
Rx(a) · Rx(b) → Rx(a + b)
Ry(a) · Ry(b) → Ry(a + b)
```

If the merged angle ≈ 0 (mod 2π), the gate is eliminated entirely.

### Single-Qubit Fusion

Any sequence of single-qubit gates on the same qubit can be fused into a single
U3 gate via ZYZ decomposition:

```
U = Rz(α) · Ry(β) · Rz(γ)
```

The 2×2 unitary matrices are multiplied, then the ZYZ angles are extracted.

## Commutation Analysis

### Commutation Rules

Two gates **commute** if applying them in either order gives the same result.

| Rule | Example |
|------|---------|
| Disjoint qubits | Any gate on q0 commutes with any gate on q1 |
| Diagonal gates | Z, S, T, Rz all commute with each other |
| CNOT + Z on target | CNOT(0,1) commutes with Z(1) |
| Same gate | Any gate commutes with itself |

### Non-Adjacent Cancellation

When two gates are inverses of each other but separated by intervening gates,
they can still be cancelled if all intervening gates commute with them:

```
Z(q0), X(q1), Z(q0)
       ↑ commutes with Z(q0) since disjoint qubits
Result: X(q1)
```

### Circuit DAG

The compiler builds a directed acyclic graph (DAG) of gate dependencies:
- **Nodes** = gates
- **Edges** = non-commuting dependencies
- **Topological sort** → valid gate ordering
- **Critical path** = circuit depth

## Depth Reduction

### ASAP Scheduling

Assign each gate to the earliest possible time step:

```
Layer 0: H(q0), H(q1), H(q2)     ← all independent
Layer 1: CNOT(q0, q1)              ← depends on H(q0), H(q1)
Layer 2: CNOT(q1, q2)              ← depends on CNOT(q0,q1)
```

### ALAP Scheduling

Assign each gate to the latest possible time step. Useful for reducing
the time qubits spend idle (exposed to decoherence).

### Parallelization

Independent gates on disjoint qubits execute in the same time step.
Four independent H gates → depth 1 instead of depth 4.

## Qubit Routing

### Hardware Topologies

Real quantum hardware has limited connectivity:

```
Linear: 0—1—2—3—4

Grid:   0—1—2—3
        |  |  |  |
        4—5—6—7
        |  |  |  |
        8—9—10—11

Heavy-hex (IBM): degree-3 graph, 127 qubits
```

### SWAP Insertion

When a two-qubit gate targets non-adjacent qubits, SWAP gates are inserted
to bring the qubits together:

```
SWAP(q0, q1) = CNOT(q0,q1) · CNOT(q1,q0) · CNOT(q0,q1)
```

Cost: 3 CNOT gates per SWAP.

### Routing Algorithm

1. Maintain a logical→physical qubit mapping
2. For each two-qubit gate:
   a. Check if target qubits are adjacent
   b. If not, find shortest path (BFS)
   c. Insert SWAPs along the path
   d. Update the qubit mapping
3. Track total routing overhead

### Initial Layout

Heuristic placement assigns frequently-interacting logical qubits
to adjacent physical qubits, minimizing total routing overhead.

## Native Gate Decomposition

Hardware backends support specific native gate sets:

| Gate Set | Native Gates | Hardware |
|----------|-------------|----------|
| Clifford+T | H, S, T, CNOT | Universal (theoretical) |
| CX+Rz | CNOT, Rz, Rx | Superconducting (general) |
| IBM | CNOT, Rz, SX, X | IBM Quantum (Eagle, Heron) |

### Decomposition Rules

```
H = Rz(π) · SX · Rz(π)          (3 IBM native gates)
S = Rz(π/2)                      (1 native gate)
T = Rz(π/4)                      (1 native gate)
SWAP = CX · CX · CX              (3 native gates)
CZ = H(target) · CX · H(target)  (5 native gates for IBM)
```

## Cost Functions

Three metrics for circuit quality:

| Metric | Weight | Rationale |
|--------|--------|-----------|
| Gate count | 1.0 | Total operations |
| Circuit depth | 2.0 | Decoherence exposure |
| Two-qubit gates | 10.0 | Highest error rate |

Weighted cost: `C = w₁·gates + w₂·depth + w₃·two_qubit_gates`

## Convergence

The pipeline iterates passes until the gate count stabilizes:

```
Pass 1: 100 gates → 72 gates
Pass 2: 72 gates → 58 gates
Pass 3: 58 gates → 55 gates
Pass 4: 55 gates → 55 gates  ← converged
```

Maximum iterations configurable (default: 10).

## Module Structure

```
simplex-quantum/circuit-opt/
├── Modulus.toml
└── src/
    ├── mod.sx          Optimizer pipeline, pass types
    ├── fusion.sx       Identity elim, rotation merge, fusion
    ├── commutation.sx  Commutation rules, cancellation
    ├── dag.sx          Circuit DAG representation
    ├── reorder.sx      Gate reordering strategies
    ├── depth.sx        Depth reduction, layer assignment
    ├── routing.sx      SWAP insertion, qubit mapping
    ├── topology.sx     Hardware connectivity graphs
    ├── pipeline.sx     Multi-pass orchestration
    ├── cost.sx         Cost functions and metrics
    └── native.sx       Native gate decomposition
```
