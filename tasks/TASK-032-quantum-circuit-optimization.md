# TASK-032: Quantum Circuit Optimization

**Version:** 0.15.0
**Status:** Complete
**Priority:** High
**Depends on:** TASK-021 (Quantum Bridge)

## Summary

Implement circuit optimization passes in `simplex-quantum/` to reduce gate count,
circuit depth, and two-qubit gate usage. Critical for running on real hardware where
every gate introduces noise.

## Deliverables

### Phase 1: Gate Fusion (~600 lines)

Location: `simplex-quantum/optimize/src/`

- **Single-qubit gate fusion** — merge consecutive single-qubit gates into one U3 gate
- **Two-qubit gate fusion** — merge adjacent CNOT-Rz-CNOT patterns
- **Rotation merging** — combine Rz(a) followed by Rz(b) into Rz(a+b)
- **Identity elimination** — remove gate pairs that cancel (X·X = I, H·H = I)
- **Template matching** — recognize and replace known gate patterns

```simplex
use simplex_quantum::optimize::{CircuitOptimizer, FusionPass};

let optimizer = CircuitOptimizer::new()
    .add_pass(FusionPass::SingleQubit)
    .add_pass(FusionPass::RotationMerge)
    .add_pass(FusionPass::IdentityElimination);

let optimized = optimizer.optimize(&circuit);
// Gate count reduced from 47 to 23
```

Files:
- `simplex-quantum/optimize/src/fusion.sx` — gate fusion passes
- `simplex-quantum/optimize/src/templates.sx` — pattern matching templates

### Phase 2: Commutation Rules (~500 lines)

- **Gate commutation analysis** — determine which gates commute (can be reordered)
- **Commutation DAG** — build directed acyclic graph of gate dependencies
- **Gate reordering** — move commuting gates to enable further optimization
- **Cancellation via commutation** — find non-adjacent gates that cancel after reordering

```simplex
use simplex_quantum::optimize::CommutationAnalyzer;

let analyzer = CommutationAnalyzer::new(&circuit);
let dag = analyzer.build_dag();
let reordered = dag.optimize_ordering();
```

Files:
- `simplex-quantum/optimize/src/commutation.sx` — commutation rules database
- `simplex-quantum/optimize/src/dag.sx` — circuit DAG representation
- `simplex-quantum/optimize/src/reorder.sx` — gate reordering engine

### Phase 3: Depth Reduction (~500 lines)

- **Critical path analysis** — identify the longest dependency chain
- **Gate parallelization** — schedule independent gates in parallel layers
- **Qubit routing** — map logical to physical qubits minimizing SWAP insertions
- **SWAP decomposition** — break SWAPs into native gate sets (CX, Rz)
- **Layout optimization** — initial qubit placement heuristic

```simplex
use simplex_quantum::optimize::{DepthReducer, QubitRouter, Topology};

let topology = Topology::grid(4, 4);  // 16-qubit grid
let router = QubitRouter::new(&topology);
let routed = router.route(&circuit);

let reducer = DepthReducer::new();
let shallow = reducer.reduce(&routed);
// Depth reduced from 84 to 31
```

Files:
- `simplex-quantum/optimize/src/depth.sx` — depth reduction pass
- `simplex-quantum/optimize/src/routing.sx` — qubit routing and SWAP insertion
- `simplex-quantum/optimize/src/topology.sx` — hardware connectivity graphs

### Phase 4: Optimization Pipeline (~400 lines)

- **Multi-pass pipeline** — chain optimization passes with iteration
- **Cost function** — configurable objective (minimize depth, gate count, or two-qubit gates)
- **Statistics** — report optimization metrics (before/after gate counts, depth, etc.)
- **Hardware-aware optimization** — optimize for specific backend native gate sets

Files:
- `simplex-quantum/optimize/src/pipeline.sx` — optimization pipeline orchestration
- `simplex-quantum/optimize/src/cost.sx` — cost functions and metrics
- `simplex-quantum/optimize/src/native.sx` — native gate set decomposition

## Tests

Location: `tests/quantum/`

- `spec_gate_fusion.sx` — fusion correctness (unitary equivalence)
- `spec_commutation.sx` — commutation rules and DAG construction
- `spec_depth_reduction.sx` — depth reduction on benchmark circuits
- `spec_routing.sx` — qubit routing on grid/linear topologies
- `spec_optimization_pipeline.sx` — end-to-end optimization benchmarks

## Estimated Scope

~2000 lines across 4 phases, 5 new test files

## Success Criteria

- Single-qubit fusion reduces gate count by >30% on random circuits
- Commutation-based cancellation finds >10% additional reductions
- Depth reduction achieves >40% on QFT-like circuits
- Routing adds <2x overhead on grid topology
- All optimized circuits produce equivalent unitaries (verified by simulation)
- Zero regressions
