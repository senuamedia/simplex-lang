# Simplex Language Documentation

**Version 0.16.0**

Simplex (Latin for "simple") is a programming language designed for the AI era. It combines the fault-tolerance of Erlang, the memory safety of Rust, the distributed computing model of Ray, and the content-addressable code of Unison into a cohesive system built for intelligent, distributed workloads.

---

## Documentation Index

### Specification

| Document | Description |
|----------|-------------|
| [Overview and Philosophy](spec/01-overview.md) | Vision, goals, and core design philosophy |
| [Architecture](spec/02-architecture.md) | System architecture and component overview |
| [Design Decisions](spec/03-design-decisions.md) | Technical decisions with rationale |
| [Language Syntax](spec/04-language-syntax.md) | Complete syntax reference |
| [Virtual Machine](spec/05-virtual-machine.md) | SVM internals, bytecode, and instruction set |
| [Swarm Computing](spec/06-swarm-computing.md) | Distributed execution model |
| [AI Integration](spec/07-ai-integration.md) | AI primitives and patterns |
| [Cost Optimization](spec/08-cost-optimization.md) | Enterprise deployment on cheap compute |
| [Cognitive Hive AI](spec/09-cognitive-hive.md) | CHAI architecture for SLM orchestration |
| [Compiler Toolchain](spec/10-compiler-toolchain.md) | sxc, spx, cursus - 100% pure Simplex |
| [Standard Library](spec/11-standard-library.md) | Complete std API reference |
| [The Anima](spec/12-anima.md) | Cognitive soul - memory, beliefs, intentions |
| [SLM Provisioning](spec/13-slm-provisioning.md) | Per-hive model architecture |
| [Neural IR](spec/14-neural-ir.md) | Differentiable execution and neural gates |
| [Real-Time Learning](spec/15-real-time-learning.md) | Online learning and adaptation |
| [Edge Hive](spec/16-edge-hive.md) | Lightweight autonomous hive for edge devices |
| [Formal Semantics](spec/17-formal-semantics.md) | Formal Semantics - SAM Model |
| [HTTP Server](spec/18-http-server.md) | HTTP server specification |
| [Quantum Error Mitigation](spec/19-quantum-error-mitigation.md) | Noise models, ZNE, PEC, error correction codes |
| [Circuit Optimization](spec/20-circuit-optimization.md) | Gate fusion, commutation, routing, native decomposition |
| [Production Hardening](spec/21-production-hardening.md) | Fuzzing, property testing, sanitizers, formal invariants |

### Tutorial

A step-by-step learning path for the Simplex language.

| Chapter | Topic | Description |
|---------|-------|-------------|
| [01](tutorial/01-variables-and-types.md) | Variables and Types | Primitives, type inference, mutability |
| [02](tutorial/02-functions.md) | Functions | Parameters, returns, closures, generics |
| [03](tutorial/03-control-flow.md) | Control Flow | Conditionals, loops, pattern matching |
| [04](tutorial/04-collections.md) | Collections | Lists, maps, sets, iteration |
| [05](tutorial/05-custom-types.md) | Custom Types | Structs, enums, traits, methods |
| [06](tutorial/06-error-handling.md) | Error Handling | Option, Result, the ? operator |
| [07](tutorial/07-actors.md) | Actors | State, messages, spawning |
| [08](tutorial/08-messages.md) | Message Passing | Send, ask, patterns |
| [09](tutorial/09-supervision.md) | Supervision | Fault tolerance, restart strategies |
| [10](tutorial/10-ai-basics.md) | AI Integration | Completion, classification, embeddings |
| [11](tutorial/11-capstone.md) | Capstone Project | Build a complete application |
| [12](tutorial/12-cognitive-hives.md) | Cognitive Hives | Building SLM swarms with CHAI |
| [13](tutorial/13-dual-numbers.md) | Dual Numbers & Forward-Mode AD | Native dual type for automatic differentiation |
| [14](tutorial/14-self-learning-annealing.md) | Self-Learning Annealing | Learnable optimization schedules via meta-gradients |
| [15](tutorial/15-quantum-error-mitigation.md) | Quantum Error Mitigation | Noise models, ZNE, error correction, circuit optimization |

Start the tutorial: [Tutorial Index](tutorial/README.md)

### Examples

| Document | Description |
|----------|-------------|
| [Document Pipeline](examples/document-pipeline.md) | Complete example: distributed document processing |

### Guides

| Document | Description |
|----------|-------------|
| [Getting Started](guides/getting-started.md) | Quick start guide |

### Testing

| Document | Description |
|----------|-------------|
| [Testing Overview](testing/README.md) | Testing framework and coverage |
| [Framework](testing/framework.md) | Architecture and components |
| [Running Tests](testing/running-tests.md) | How to execute tests |
| [Coverage](testing/coverage.md) | Current test coverage |
| [Methods](testing/methods.md) | Testing patterns and conventions |
| [Writing Tests](testing/writing-tests.md) | Guide for writing new tests |

---

## Quick Links

- **New to Simplex?** Start with the [Tutorial](tutorial/README.md) or [Overview](spec/01-overview.md)
- **Learning the language?** Follow the [15-chapter tutorial](tutorial/README.md)
- **Want syntax reference?** Jump to [Language Syntax](spec/04-language-syntax.md)
- **Building AI agents?** See [The Anima](spec/12-anima.md) and [Cognitive Hive AI](spec/09-cognitive-hive.md)
- **Edge deployment?** Check [Edge Hive](spec/16-edge-hive.md) for device-side intelligence
- **See complete code?** Check [Examples](examples/document-pipeline.md)
- **Planning deployment?** See [Cost Optimization](spec/08-cost-optimization.md)
- **Building the VM?** Read [Virtual Machine](spec/05-virtual-machine.md)
- **Self-hosting compiler?** See [Compiler Toolchain](spec/10-compiler-toolchain.md)
- **Standard library API?** Check [Standard Library](spec/11-standard-library.md)
- **Quantum error mitigation?** See [Quantum Error Mitigation](spec/19-quantum-error-mitigation.md)
- **Circuit optimization?** See [Circuit Optimization](spec/20-circuit-optimization.md)
- **Production hardening?** See [Production Hardening](spec/21-production-hardening.md)
- **Running or writing tests?** See [Testing Documentation](testing/README.md)

---

## Design Principles

1. **AI-native**: AI operations are first-class language constructs
2. **Anima-centric**: Every AI agent has a cognitive soul (anima) with memory, beliefs, and intentions
3. **Hive-oriented**: Small language models collaborate as cognitive hives (CHAI)
4. **Distributed-first**: Programs naturally decompose across VM swarms
5. **Fault-tolerant**: Workers can die and resume transparently
6. **Cost-efficient**: Runs on the cheapest cloud compute (spot instances, ARM)
7. **Simple syntax**: Lightweight, readable code

---

## Key Features (v0.16.0)

### NEW in v0.16.0: Enterprise Libraries & Hive Intelligence

**Production-ready libraries** for data formats, databases, real-time communication, authentication, observability, and AI intelligence infrastructure.

```simplex
// Unified database connectivity
let config = db_config_new(DRIVER_POSTGRES());
db_config_set(config, string_from("host"), string_from("localhost"));
let conn = db_connect(config);
let result = db_query(conn, string_from("SELECT * FROM users"));

// JWT authentication
let token = jwt_encode(payload, secret);
let valid = jwt_verify(token, secret);

// RAG pipeline
let pipeline = rag_create(128);
rag_ingest(pipeline, document, source);
let answer = rag_query(pipeline, question, 5);
```

- **Data formats**: CSV, YAML, XML parsers in simplex-std
- **Database**: Unified `simplex-db` with PostgreSQL, MySQL, SQLite, Redis drivers
- **Real-time**: WebSocket (RFC 6455), Protocol Buffers, NATS messaging
- **Auth**: JWT (HS256), OAuth 2.0 (PKCE), dotenv
- **Observability**: Prometheus metrics, OpenTelemetry tracing
- **AI Intelligence**: VectorDB, RAG pipeline, Guardrails, Eval suite

See [RELEASE-0.16.0.md](RELEASE-0.16.0.md) for complete release notes.

### v0.15.0: Production Hardening & Quantum Maturity

**Quantum error mitigation, circuit optimization, and production hardening** complete the quantum computing framework for real hardware use.

```simplex
use simplex_quantum::noise::{noise_model_new, depolarizing_new};
use simplex_quantum::mitigation::{zne_new, zne_execute};
use simplex_quantum::circuit_opt::{pipeline_default, pipeline_run};

// Build a noise model for realistic simulation
let noise = noise_model_new();
noise_model_add_single_qubit(noise, depolarizing_new(0.001));

// Optimize circuit for hardware
let pipe = pipeline_default();
let optimized = pipeline_run(pipe, circuit, n_qubits, 10);

// Mitigate remaining noise via zero-noise extrapolation
let zne = zne_new();
let mitigated = zne_execute(zne, noisy_values);
```

- **Noise models**: Depolarizing, amplitude/phase damping, readout error, custom Kraus
- **Error mitigation**: ZNE (4 methods), PEC with Monte Carlo sampling
- **Error correction**: Bit-flip, Shor [[9,1,3]], Steane [[7,1,3]], surface code (d=3)
- **Circuit optimization**: Gate fusion, commutation cancellation, qubit routing
- **Native decomposition**: IBM, Clifford+T, CX+Rz gate sets
- **Compiler**: Collection iteration, keyword methods, where clauses, nested generics
- **Hardening**: Fuzzing, property tests, ASan/UBSan/TSan, formal invariants, CI gates

See [RELEASE-0.15.0.md](RELEASE-0.15.0.md) for complete release notes.

### v0.14.0: Quantum Bridge

**Hybrid classical-quantum computation** built on the mathematical foundations from v0.13.0 (complex numbers, matrices, eigendecomposition).

```simplex
use simplex_quantum::{Qubit, Circuit, Hadamard, CNOT, measure};
use simplex_quantum::backend::{LocalSimulator};

// Build a Bell state circuit
let circuit = Circuit::new(2);
circuit.add(Hadamard::on(0));
circuit.add(CNOT::on(0, 1));

// Execute on local simulator
let backend = LocalSimulator::new();
let result = backend.run(circuit, shots: 1000);
// Measures |00> ~50%, |11> ~50% -- entangled qubits
```

- **Core quantum types**: Qubit, Circuit, Gate, Measurement with statevector simulation
- **Gate library**: Hadamard, Pauli-X/Y/Z, CNOT, Phase, T-gate, Toffoli, and custom unitaries
- **Backend abstraction**: Amazon Braket, IBM Quantum, Azure Quantum, local simulator
- **Variational algorithms**: VQE, QAOA with parameter-shift gradient estimation
- **Cost-aware dispatch**: Automatic routing to cheapest backend with budget tracking
- **Quantum optimization**: MaxCut, molecular simulation, Grover search, hybrid annealing
- **Ecosystem integration**: Quantum neural gates, epistemic quantum beliefs, gradient bridge

See [RELEASE-0.14.0.md](RELEASE-0.14.0.md) for complete release notes.

### v0.9.0: Edge Hive

**Run intelligence on every device**: The Edge Hive brings autonomous cognitive capabilities to edge devices, from smartwatches to desktops.

```simplex
// Create an Edge Hive on this device
let hive = create_edge_hive("wss://hive.example.com")

// Connect to cloud for delegation
edge_hive_connect(hive)

// Handle requests locally when confident, delegate when needed
let response = edge_hive_query(hive, query)

// User preferences are learned and persisted
hive_set_preference(hive, "theme", 1)
```

- **Local-first**: Processing happens on-device, cloud is a fallback
- **User advocacy**: Represents user interests, not provider interests
- **Adaptive**: Automatically scales from watch to desktop
- **Offline-capable**: Works without network connectivity
- **Privacy-first**: All data local by default

See [Edge Hive Specification](spec/16-edge-hive.md) for details.

### v0.9.0: Self-Learning Annealing

**The headline feature**: Optimization schedules that learn themselves through meta-gradients.

```simplex
use simplex::optimize::anneal::{self_learn_anneal, AnnealConfig};

// Self-learning annealing: the schedule learns itself
let schedule = AnnealSchedule::learnable();
let optimizer = MetaOptimizer::new(schedule);

for epoch in 0..epochs {
    let (solution, meta_loss) = optimizer.anneal_with_grad(objective);
    schedule.update(meta_loss.gradient());  // Schedule improves each epoch
}
// After training: schedule.cool_rate, schedule.reheat_threshold are optimal
```

- **Learnable schedules**: Temperature, cooling rate, reheating all learned via meta-gradients
- **Test restructure**: 156 tests across 13 categories with consistent naming
- **simplex-training**: New library for self-optimizing training pipelines
- **llama.cpp integration**: High-performance inference via native bindings in `simplex-inference`

See [RELEASE-0.9.0.md](RELEASE-0.9.0.md) for complete release notes.

### v0.8.0: Dual Numbers and Forward-Mode AD

Native `dual` type for forward-mode automatic differentiation with zero overhead:

```simplex
let x: dual = dual::variable(3.0)
let y = x * x + x.sin()

print(y.val)  // f(3) = 9.1411...
print(y.der)  // f'(3) = 6.9899... (exact, not numerical)
```

- **Zero overhead**: Compiles to same assembly as hand-written derivatives
- **Full math support**: sin, cos, exp, ln, sqrt, tanh, sigmoid all differentiable
- **Gradients**: `multidual<N>` for computing N partial derivatives at once

See [RELEASE-0.8.0.md](RELEASE-0.8.0.md) for complete release notes.

### v0.7.0: Real-Time Continuous Learning

AI specialists learn and adapt during runtime without batch retraining:

```simplex
use simplex_learning::{OnlineLearner, StreamingAdam, SafeFallback};

let learner = OnlineLearner::new(model_params)
    .optimizer(StreamingAdam::new(0.001))
    .fallback(SafeFallback::with_default(safe_output));

for (input, feedback) in interactions {
    let output = learner.forward(&input);
    learner.learn(&feedback);  // Adapts in real-time
}
```

See [RELEASE-0.7.0.md](RELEASE-0.7.0.md) for complete release notes.

### v0.6.0: Neural IR and Differentiable Execution

Neural Gates transform control flow into learnable operations:

```simplex
neural_gate should_retry(confidence: f64) -> bool
    requires confidence > 0.5
    fallback => conservative_path()
{
    confidence > 0.7  // Threshold learned during training
}
```

- **Neural Gates**: Gumbel-Softmax for differentiable branches
- **Dual compilation**: Training mode (differentiable) vs inference mode (discrete)
- **Contract logic**: `requires`, `ensures`, `fallback` for probabilistic verification
- **Hardware targeting**: `@cpu`, `@gpu`, `@npu` annotations

See [RELEASE-0.6.0.md](RELEASE-0.6.0.md) for complete release notes.

### v0.5.0: Per-Hive SLM Architecture

Each hive provisions ONE shared SLM that all its specialists use:

```
┌─────────────────────────────────────────┐
│            HIVE SLM (4.1 GB)            │
│         One model per hive              │
└────────────────┬────────────────────────┘
                 │
    ┌────────────┼────────────┐
    ▼            ▼            ▼
 Analyst      Coder      Reviewer
  Anima       Anima       Anima
    │            │            │
    └────────────┴────────────┘
          HiveMnemonic
       (Shared consciousness)
```

- **Per-hive SLM**: 10 specialists share 1 model (not 10)
- **HiveMnemonic**: Shared consciousness across specialists
- **Memory-augmented inference**: Anima + Mnemonic context flows to SLM
- **Built-in models**: cognitive-7b (4.1GB), cognitive-1b (700MB), mnemonic-embed (134MB)

See [RELEASE-0.5.0.md](RELEASE-0.5.0.md) for complete release notes.

### Language Core
- Static typing with inference
- Pattern matching with guards
- Generics and traits
- Closures and lambdas
- Async/await
- Option and Result types
- Module system with `use`, `mod`, `pub`

### Actor System
- Message passing actors
- Supervision trees
- Fault tolerance ("let it crash")
- Checkpointing and recovery

### AI/Cognitive (The Anima)
- `anima` keyword for cognitive agents
- Episodic, semantic, procedural, and working memory
- Belief system with revision (30% threshold for individual, 50% for hive)
- BDI (Beliefs-Desires-Intentions) architecture
- Goal-directed memory recall
- Memory persistence and sharing
- Integration with HiveMnemonic for shared consciousness

### AI Specialists & Hives
- Per-hive SLM sharing (one model per hive)
- HiveMnemonic for collective memory
- Multi-provider support (Anthropic, OpenAI, Ollama, local SLMs)
- Streaming responses
- Tool/function calling
- Multi-actor orchestration (pipeline, parallel, consensus)
- Semantic routing

### Neural IR (v0.6.0)
- `neural_gate` keyword for learnable control flow
- Gumbel-Softmax differentiable branches
- Dual compilation modes (training/inference)
- Contract logic with `requires`, `ensures`, `fallback`
- Hardware targeting (`@cpu`, `@gpu`, `@npu`)
- Structural pruning for optimized inference

### Real-Time Learning (v0.7.0)
- `simplex-learning` library for online adaptation
- Tensor operations with automatic differentiation
- Streaming optimizers (SGD, Adam, AdamW)
- Safety constraints with fallback strategies
- Federated learning (FedAvg, Median, TrimmedMean, etc.)
- Knowledge distillation (teacher-student, self-distillation)
- Belief conflict resolution across hives
- Experience replay and checkpointing

### Dual Numbers (v0.8.0)
- Native `dual` type for forward-mode automatic differentiation
- Zero-overhead compilation (same assembly as hand-written derivatives)
- Transcendental functions: sin, cos, exp, ln, sqrt, tanh, sigmoid
- `multidual<N>` for computing gradients (N partial derivatives)
- `dual2` for second-order derivatives (Hessians)
- `diff::derivative()`, `diff::gradient()`, `diff::jacobian()`, `diff::hessian()`

### Self-Learning Annealing (v0.9.0)
- Learnable temperature schedules via meta-gradients
- Soft acceptance function using differentiable sigmoid
- Stagnation-triggered reheating with learned thresholds
- Meta-optimization for schedule parameter learning
- `simplex-training` library for self-optimizing pipelines
- Learnable LR, distillation, pruning, and quantization schedules

### Edge Hive (v0.9.0)
- Lightweight autonomous hive for edge devices (watch to desktop)
- Local-first processing with cloud delegation fallback
- BeliefStore with CRDT-based sync across devices
- Specialist system for domain-specific local handling
- Adaptive footprint based on device capabilities
- Privacy-first design with local data by default
- Offline operation as a first-class feature

### High-Performance Inference (v0.9.0)
- Native llama.cpp integration via `simplex-inference`
- Continuous batching for throughput optimization
- Prompt caching for repeated context
- Response caching for deterministic queries
- GPU offloading (CUDA, Metal) support
- Smart routing based on query complexity

### Quantum Computing Bridge (v0.14.0)
- `simplex-quantum` core: Qubit, Circuit, Gate, Measurement with statevector simulation
- Gate library: Hadamard, Pauli-X/Y/Z, CNOT, Phase, T-gate, Toffoli, custom unitaries
- Backend abstraction: Amazon Braket, IBM Quantum, Azure Quantum, local simulator
- Variational algorithms: VQE, QAOA, parameter-shift gradient estimation
- Cost-aware task dispatcher with budget enforcement and per-backend tracking
- Quantum optimization: MaxCut (QAOA), molecular simulation (VQE), Grover search, hybrid annealing
- Ecosystem bridges: quantum neural gates, epistemic quantum beliefs, gradient bridge to dual numbers

### Quantum Maturity (v0.15.0)
- Noise models: depolarizing, amplitude/phase damping, readout error, custom Kraus channels
- Noisy simulator with per-gate noise injection and stochastic sampling
- Zero-noise extrapolation (ZNE): Richardson, linear, polynomial, exponential
- Probabilistic error cancellation (PEC) with Monte Carlo sampling
- Error correction codes: bit-flip, phase-flip, Shor [[9,1,3]], Steane [[7,1,3]], surface code (d=3)
- MWPM syndrome decoder for surface codes
- Circuit optimization: identity elimination, rotation merging, single-qubit fusion
- Commutation analysis and non-adjacent gate cancellation
- Qubit routing for linear, grid, all-to-all topologies with SWAP insertion
- Native gate set decomposition (IBM, Clifford+T, CX+Rz)
- Multi-pass optimization pipeline with convergence detection

### Production Hardening (v0.15.0)
- Compiler fuzzing: lexer, parser, codegen, grammar-aware fuzz harnesses
- Property-based testing for lexer, parser, codegen, and type system
- Runtime safety: AddressSanitizer, UBSan, ThreadSanitizer integration
- Formal invariant verification for compiler, runtime, and protocol
- CI hardening workflow with fuzz regression and sanitizer gates

### Observability
- Metrics (counter, gauge, histogram)
- Distributed tracing
- Structured logging
- Prometheus export

### Toolchain
- `sxc` - Native compiler (LLVM backend)
- `sxpm` - Package manager with model provisioning
  - **NEW**: `sxpm model list/install/remove/info` for SLM management
- `sxdoc` - Documentation generator
- `sxlsp` - Language server
- `cursus` - Build system

### Cross-Platform Support (v0.9.0)
The entire toolchain is **fully cross-platform**:

| Platform | Bootstrap | Compiler | Tools |
|----------|-----------|----------|-------|
| **macOS** (Intel/ARM) | Yes | Yes | Yes |
| **Linux** (x86_64/ARM64) | Yes | Yes | Yes |
| **Windows** (x86_64) | Yes | Yes | Yes |

- `stage0.py` automatically detects the platform and generates appropriate target triples
- All tools use platform-appropriate path separators and commands
- See [Compiler Toolchain](spec/10-compiler-toolchain.md) for build instructions

---

## Influences

Simplex draws from:

- **Erlang/OTP**: Actor model, supervision trees, "let it crash"
- **Rust**: Ownership-based memory management, zero-cost abstractions
- **Unison**: Content-addressed code, seamless distribution
- **Ray**: Distributed computing primitives, AI/ML optimization
- **Go**: Simple syntax, fast compilation
- **CHAI**: Cognitive Hive AI architecture for SLM orchestration

---

## Module Terminology

Simplex uses the following terminology for its module system:

| Term | Definition |
|------|------------|
| **modulus** | A single compilation unit (like Rust's crate) |
| **moduli** | Plural of modulus - multiple compilation units |
| **module** | A namespace within a modulus (like Rust's mod) |

---

## Document History

| Version | Date | Changes |
|---------|------|---------|
| 0.1.0 | 2024-12 | Initial draft specification |
| 0.2.0 | 2024-12 | Added Cognitive Hive AI (CHAI) specification |
| 0.3.0 | 2025-01 | Self-hosted native compiler via LLVM |
| 0.3.1 | 2025-01 | Complete toolchain: sxc, cursus, sxdoc, sxlsp all compiled |
| 0.3.5 | 2025-01 | Added sxpm package manager, Phase 1 stdlib (HashMap, HashSet, String ops) |
| 0.4.0 | 2026-01 | The Anima: cognitive memory, BDI, tool calling, observability, multi-actor orchestration |
| 0.4.1 | 2026-01 | sxpm enhancements: inline modules, pub use re-exports, diamond dependency detection, build cache, lock files |
| 0.5.0 | 2026-01-07 | SLM Provisioning: model commands, HTTP client, terminal colors, enhanced test framework |
| 0.5.1 | 2026-01-07 | Type aliases, AGPL-3.0 license change, LLM context specification |
| 0.6.0 | 2026-01-08 | Neural IR: neural gates, Gumbel-Softmax, dual compilation, hardware targeting |
| 0.7.0 | 2026-01-09 | Real-Time Learning: simplex-learning library, streaming optimizers, federated learning |
| 0.8.0 | 2026-01-10 | Dual Numbers: native forward-mode AD, zero-overhead compilation, multidual for gradients |
| 0.9.0 | 2026-01-11 | Self-Learning Annealing, Edge Hive, test restructure (156 tests), simplex-training library |
| 0.9.9 | 2026-01-18 | Runtime stability, 175 ABI fixes, device ID collision fix |
| 0.10.0 | 2026-01-18 | Developer Experience: sxfmt, sxlint, benchmarking, profiling, coverage, playground, Tree-sitter |
| 0.12.0 | 2026-01-19 | Module System: Cross-module function imports via `use`, automatic LLVM declaration generation |
| 0.13.0 | 2026-03-16 | Completion & Foundations: compiler stability, complex numbers, matrices, dual number extensions, training pipeline, HTTP client, JSON parser |
| 0.14.0 | 2026-03-17 | Quantum Bridge: quantum computing primitives, backend abstraction, variational algorithms, cost-aware dispatch, quantum-enhanced optimization |
| 0.15.0 | 2026-03-19 | Production Hardening & Quantum Maturity: noise models, ZNE, PEC, error correction codes, circuit optimization, qubit routing, compiler improvements, fuzzing, property testing, sanitizers, formal invariants |
| 0.16.0 | 2026-03-19 | Production Hardening & Quantum Maturity: noise models, ZNE, PEC, error correction codes, circuit optimization, qubit routing, compiler improvements, fuzzing, property testing, sanitizers, formal invariants |

---

*Simplex is a work in progress. This specification is subject to change.*
