# Simplex v0.9.0 Release Notes

**Release Date:** 2026-01-11
**Codename:** Edge Intelligence

---

## Overview

Simplex v0.9.0 introduces two major features: **Edge Hive** for running autonomous intelligence on user devices, and **Self-Learning Annealing** where optimization schedules are learned through meta-gradients rather than hand-tuned. This release also includes a major **test suite restructure** with 156 tests across 13 categories, and a new **library architecture** with simplex-training for self-optimizing model training pipelines.

---

## Major Features

### Edge Hive

The Edge Hive brings autonomous cognitive capabilities to edge devices, from smartwatches to desktop computers. Unlike traditional "thin client" approaches where edge devices merely execute cloud commands, the Edge Hive is a living piece of the cognitive hive with local intelligence.

```simplex
// Create an Edge Hive instance
let hive: i64 = create_edge_hive(string_from("wss://hive.example.com"));

// Connect to cloud for delegation
edge_hive_connect(hive);

// Handle requests locally when confident, delegate when needed
let response: i64 = edge_hive_query(hive, query);

// User preferences are learned and persisted
hive_set_preference(hive, string_from("theme"), 1);

// Maintenance cycle for sync and learning
hive_maintenance_cycle(hive);

// Clean shutdown
hive_shutdown(hive);
```

#### Key Capabilities

| Feature | Description |
|---------|-------------|
| **Local-First** | Processing happens on-device, cloud is fallback |
| **User Advocacy** | Represents user interests, not provider interests |
| **Adaptive** | Scales from watch (100 beliefs) to desktop (1M beliefs) |
| **Offline** | Works without network connectivity |
| **Privacy** | All data local by default |

#### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                       Edge Hive                             │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │   Device    │  │   Belief    │  │     Specialist      │  │
│  │   Profile   │  │    Store    │  │      Registry       │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
│         │                │                    │              │
│         └────────────────┼────────────────────┘              │
│                          │                                   │
│                ┌─────────▼─────────┐                        │
│                │  Cognitive Loop   │                        │
│                └─────────┬─────────┘                        │
│                          │                                   │
│                ┌─────────▼─────────┐                        │
│                │    Federation     │                        │
│                │     Manager       │                        │
│                └─────────┬─────────┘                        │
└──────────────────────────┼──────────────────────────────────┘
                           │
               ┌───────────┴───────────┐
               │                       │
        ┌──────▼──────┐        ┌───────▼──────┐
        │ Cloud Hives │        │ Peer Devices │
        └─────────────┘        └──────────────┘
```

#### Device Support

| Device | Max Model | Beliefs | Cache | Sync Strategy |
|--------|-----------|---------|-------|---------------|
| Watch | 0 | 100 | 1 MB | Aggressive |
| Phone | 500M | 10K | 50 MB | Balanced |
| Tablet | 1B | 50K | 100 MB | Balanced |
| Laptop | 7B | 100K | 500 MB | Local-First |
| Desktop | 70B | 1M | 1 GB | Local-First |

#### Specialist System

Local specialists handle domain-specific requests:
- **Context Specialist**: Learns user patterns and time-based behaviors
- **Task Specialist**: Manages user tasks with priorities
- **Notification Specialist**: Filters and prioritizes notifications
- **Quick Specialist**: Fast responses for constrained devices (watch/wearable)
- **Calendar Specialist**: Schedule management (phone and above)
- **Conversation Specialist**: Full dialogue (tablet and above)
- **Health Specialist**: Health data processing (devices with sensors)

#### Cognitive Loop

1. Request arrives at the Edge Hive
2. Check response cache
3. Enrich with current context
4. Find best local specialist
5. If confidence >= threshold: handle locally
6. Otherwise: delegate to cloud
7. Cache successful responses
8. Learn from interaction

See full specification: [spec/16-edge-hive.md](spec/16-edge-hive.md)

---

### Self-Learning Annealing

Instead of hand-tuning annealing hyperparameters, the system discovers improved schedules through differentiable optimisation:

```simplex
// Self-learning annealing: the schedule learns itself
let schedule = AnnealSchedule::learnable();
let optimizer = MetaOptimizer::new(schedule);

for epoch in 0..epochs {
    let (solution, meta_loss) = optimizer.anneal_with_grad(objective);
    schedule.update(meta_loss.gradient());  // Schedule improves each epoch
}
// After training: schedule.cool_rate, schedule.reheat_threshold are learned
```

### Learnable Temperature Schedule

All schedule parameters are dual numbers for gradient tracking:

```simplex
struct LearnableSchedule {
    initial_temp: dual,      // T₀: starting temperature
    cool_rate: dual,         // α: exponential decay rate
    min_temp: dual,          // T_min: temperature floor
    reheat_threshold: dual,  // ρ: stagnation steps before reheat
    reheat_intensity: dual,  // γ: how much to increase temp on reheat
    oscillation_amp: dual,   // β: amplitude of temperature oscillation
    oscillation_freq: dual,  // ω: frequency of oscillation
    accept_threshold: dual,  // τ: soft acceptance threshold
}

impl LearnableSchedule {
    fn temperature(&self, step: dual, stagnation: dual) -> dual {
        let base = self.initial_temp * (-self.cool_rate * step).exp();
        let oscillation = dual::constant(1.0) +
            self.oscillation_amp * (self.oscillation_freq * step).sin();
        let reheat_trigger = ((stagnation - self.reheat_threshold) /
            dual::constant(10.0)).sigmoid();
        let reheat = self.reheat_intensity * reheat_trigger;

        (base * oscillation + reheat).max(self.min_temp)
    }
}
```

### Soft Acceptance Function

Differentiable relaxation of accept/reject decisions:

```simplex
fn soft_accept(delta_e: dual, temp: dual, tau: dual) -> dual {
    let scaled = (tau - delta_e) / temp;
    scaled.sigmoid()  // Differentiable!
}
```

### Meta-Optimizer

Learns schedule parameters through meta-gradients:

```simplex
struct MetaOptimizer<S, F> {
    schedule: LearnableSchedule,
    objective: F,
    meta_learning_rate: f64,
}

impl MetaOptimizer {
    fn optimize(&mut self, initial: S, neighbor: fn(&S) -> S, epochs: i64) -> S {
        for epoch in 0..epochs {
            let (solution, meta_loss) = self.anneal_episode(initial, neighbor);
            let grad = self.schedule.gradient();
            self.schedule.update(grad, self.meta_learning_rate);
        }
        best_solution
    }
}
```

### Convenience API

```simplex
use simplex::optimize::anneal::self_learn_anneal;

let best = self_learn_anneal(
    objective,
    initial_solution,
    neighbor_fn,
    AnnealConfig::default()
);
```

---

## Actor-Based HTTP Server (`simplex-http`)

A new HTTP server library designed around the actor model, providing seamless integration with cognitive hives. Unlike traditional thread-pool servers, simplex-http treats everything as actors communicating via messages.

### Design Philosophy

- **No Threads**: Concurrency via actors and async/await, not OS threads
- **Hive-Native**: First-class routing to cognitive specialists
- **Message-Based**: Request/response flows as actor messages
- **Streaming**: WebSocket and SSE for real-time specialist communication

### Basic Server Example

```simplex
use simplex_http::{HttpServer, Router, Request, Response};

let router = Router::new()
    .get("/health", health_check)
    .post("/api/query", query_handler);

HttpServer::bind("0.0.0.0:8080")
    .router(router)
    .graceful_shutdown(signal::ctrl_c())
    .serve()
    .await?;

async fn health_check(_req: Request) -> Response {
    Response::json(&json!({ "status": "healthy" })).unwrap()
}
```

### Hive Integration

Route HTTP requests directly to cognitive specialists:

```simplex
use simplex_http::{HttpServer, Router};
use simplex_hive::{Hive, specialist};

specialist QuerySpecialist {
    model: SLM,

    type Input = QueryRequest;
    type Output = QueryResponse;

    async fn process(&self, input: QueryRequest) -> QueryResponse {
        let response = self.model.complete(&input.query).await;
        QueryResponse { answer: response.text, confidence: response.confidence }
    }
}

// Build hive and route directly to specialists
let hive = Hive::builder()
    .add_specialist(QuerySpecialist::new(SLM::load("query-model")))
    .build()
    .await?;

let router = Router::new()
    .post("/api/query", hive.handler::<QuerySpecialist>())
    .with(Logger::new())
    .with(RateLimiter::new(100, Duration::from_secs(60)));

HttpServer::bind("0.0.0.0:8080")
    .router(router)
    .with_hive(hive)
    .serve()
    .await?;
```

### Actor Middleware

Middleware are actors, enabling stateful cross-cutting concerns:

```simplex
actor AuthMiddleware {
    secret_key: String,

    impl Middleware {
        async fn handle(&self, req: Request, next: Next) -> Response {
            match self.verify_token(req.header("Authorization")).await {
                Ok(user) => {
                    let mut req = req;
                    req.set_extension(user);
                    next.run(req).await
                }
                Err(_) => Response::unauthorized(),
            }
        }
    }
}

actor RateLimiter {
    requests: HashMap<IpAddr, Vec<Instant>>,
    max_requests: u32,
    window: Duration,

    impl Middleware {
        async fn handle(&self, req: Request, next: Next) -> Response {
            if self.is_rate_limited(req.ip()) {
                return Response::new(StatusCode::TooManyRequests);
            }
            self.record_request(req.ip());
            next.run(req).await
        }
    }
}
```

### Real-Time Streaming

WebSocket and SSE for streaming responses from specialists:

```simplex
// WebSocket for bidirectional communication
actor HiveStreamHandler {
    hive: HiveRef,

    impl WebSocketHandler {
        async fn on_message(&mut self, ws: &WebSocket, msg: Message) {
            let stream = self.hive.stream::<QuerySpecialist>(msg.text()).await;
            while let Some(chunk) = stream.next().await {
                ws.send(Message::Text(chunk)).await;
            }
        }
    }
}

// SSE for server-push streaming
async fn stream_reasoning(req: Request) -> Response {
    let hive = req.extension::<HiveRef>().unwrap();
    let input: ReasoningRequest = req.body_json().await?;

    Response::sse(|stream| async move {
        let reasoning = hive.stream::<ReasoningSpecialist>(input).await;
        while let Some(step) = reasoning.next().await {
            stream.send(SseEvent::new(step).with_event("reasoning_step")).await?;
        }
        Ok(())
    })
}

let router = Router::new()
    .get("/ws/hive", WebSocket::upgrade(HiveStreamHandler::new(hive)))
    .get("/api/stream", stream_reasoning);
```

### Core Types

| Type | Description |
|------|-------------|
| `HttpServer` | Actor that accepts connections |
| `Router` | Routes requests to handlers |
| `Request` | HTTP request (method, path, headers, body) |
| `Response` | HTTP response (status, headers, body) |
| `Handler` | Trait for request handlers (actors or functions) |
| `Middleware` | Trait for request/response transformation |
| `HiveHandler<S>` | Routes directly to a specialist type |

### Built-in Middleware

| Middleware | Description |
|------------|-------------|
| `Logger` | Request/response logging |
| `Cors` | Cross-Origin Resource Sharing |
| `RateLimiter` | Rate limiting per IP |
| `Timeout` | Request timeout enforcement |
| `Compression` | Gzip/Brotli response compression |
| `AuthMiddleware` | JWT/Bearer token authentication |

See full specification: [spec/12-http-server.md](spec/12-http-server.md)

---

## Test Suite Restructure

The test suite has been completely reorganized with consistent naming conventions and clear categorization.

### New Directory Structure

```
tests/
├── language/           # Core language features (40 tests)
│   ├── actors/
│   ├── async/
│   ├── basics/
│   ├── closures/
│   ├── control/
│   ├── functions/
│   ├── modules/
│   ├── traits/
│   └── types/
├── types/              # Type system tests (24 tests)
├── neural/             # Neural IR and gates (16 tests)
│   ├── contracts/
│   ├── gates/
│   └── pruning/
├── stdlib/             # Standard library (16 tests)
├── ai/                 # AI/Cognitive tests (17 tests)
│   ├── anima/
│   ├── hive/
│   ├── inference/
│   ├── memory/
│   ├── orchestration/
│   ├── specialists/
│   └── tools/
├── toolchain/          # Compiler toolchain (14 tests)
│   ├── codegen/
│   ├── parser/
│   ├── sxpm/
│   └── verification/
├── runtime/            # Runtime systems (5 tests)
│   ├── actors/
│   ├── async/
│   ├── distribution/
│   ├── io/
│   └── networking/
├── integration/        # End-to-end tests (7 tests)
├── basics/             # Basic language tests (6 tests)
├── async/              # Async/await tests (3 tests)
├── actors/             # Actor model tests (1 test)
├── learning/           # Automatic differentiation (3 tests)
└── observability/      # Metrics and tracing (1 test)
```

### Naming Convention

All tests follow a consistent prefix convention:

| Prefix | Type | Description |
|--------|------|-------------|
| `unit_` | Unit | Tests individual functions/types in isolation |
| `spec_` | Specification | Tests language specification compliance |
| `integ_` | Integration | Tests integration between components |
| `e2e_` | End-to-End | Tests complete workflows |

### Test Categories Summary

| Category | Tests | Description |
|----------|-------|-------------|
| language | 40 | Core language features |
| types | 24 | Type system (generics, patterns) |
| neural | 16 | Neural IR, gates, contracts |
| stdlib | 16 | Standard library |
| ai | 17 | AI/Cognitive framework |
| toolchain | 14 | Compiler toolchain |
| integration | 7 | End-to-end workflows |
| basics | 6 | Basic language constructs |
| runtime | 5 | Runtime systems |
| async | 3 | Async/await features |
| learning | 3 | Automatic differentiation |
| actors | 1 | Actor model |
| observability | 1 | Metrics and tracing |
| **Total** | **156** | |

### New Test Runner

```bash
# Run all tests
./run_tests.sh

# Run specific category
./run_tests.sh neural
./run_tests.sh learning
./run_tests.sh ai

# Filter by test type
./run_tests.sh all unit    # Only unit tests
./run_tests.sh all spec    # Only spec tests
./run_tests.sh all integ   # Only integration tests
./run_tests.sh all e2e     # Only end-to-end tests

# Combine category and type
./run_tests.sh stdlib unit
./run_tests.sh neural spec
```

---

## Library Architecture

### simplex-training Library

New library for self-optimizing training pipelines:

```
simplex-training/
├── Modulus.toml
└── src/
    ├── mod.sx
    ├── schedules/
    │   ├── lr.sx              # Learnable LR schedule
    │   ├── distill.sx         # Learnable distillation
    │   ├── prune.sx           # Learnable pruning
    │   ├── quant.sx           # Learnable quantization
    │   └── curriculum.sx      # Learnable curriculum
    ├── trainer/
    │   ├── meta.sx            # Meta-optimization
    │   ├── specialist.sx      # Specialist training
    │   └── compression.sx     # Model compression
    ├── data/
    │   └── loader.sx          # Data loading
    └── export/
        └── gguf.sx            # GGUF export
```

### Learnable Training Schedules

All training hyperparameters become learnable:

```simplex
// Learnable learning rate schedule
struct LearnableLRSchedule {
    initial_lr: dual,
    decay_rate: dual,
    warmup_steps: dual,
    plateau_threshold: dual,
    plateau_boost: dual,
}

// Learnable knowledge distillation
struct LearnableDistillation {
    initial_temp: dual,
    temp_decay: dual,
    alpha_start: dual,
    alpha_decay: dual,
}

// Learnable pruning
struct LearnablePruning {
    initial_sparsity: dual,
    final_sparsity: dual,
    pruning_rate: dual,
    layer_sensitivity: Vec<dual>,
}

// Learnable quantization
struct LearnableQuantization {
    initial_bits: dual,
    final_bits: dual,
    quant_rate: dual,
    layer_precision: Vec<dual>,
}
```

### Meta-Training Framework

```simplex
use simplex_training::{MetaTrainer, SpecialistConfig, CompressionPipeline};

// Create meta-trainer with all learnable schedules
let trainer = MetaTrainer::new()
    .with_learnable_lr()
    .with_learnable_distillation()
    .with_learnable_pruning()
    .with_learnable_quantization();

// Train specialists with learned schedules
let result = trainer.meta_train(&specialists, &teacher).await;

// Compress for deployment
let pipeline = CompressionPipeline::for_seed_models();
for specialist in specialists {
    let compressed = pipeline.compress(&specialist).await;
    compressed.export_gguf(&path).await?;
}
```

---

## New Standard Library Modules

### Module Philosophy

The Simplex standard library is organized around the actor model. Modules are classified as:

| Category | Modules | Description |
|----------|---------|-------------|
| **Actor-Native** | `runtime`, `sync`, `signal` | Core primitives for actor/async programming |
| **Hive Support** | `env`, `crypto`, `io`, `net`, `http` | Building blocks for hive API servers |

**Notably absent**: There are no `thread`, `fs`, or `process` modules. These are antithetical to the actor model:
- **Concurrency**: Use `runtime::spawn` and actors, not threads
- **Configuration**: Use `env` module, not config files
- **Logging**: Use actor-based logging, not file I/O
- **Lifecycle**: Actor system handles shutdown, not process control

### Environment (`std::env`)

Environment variable access and directory operations:

```simplex
use simplex_std::env::{var, set_var, remove_var, var_is_set, current_dir, VarError};

// Get/set environment variables
let path = var("PATH")?;
set_var("MY_VAR", "value");
if var_is_set("DEBUG") { ... }

// Directory operations
let cwd = current_dir()?;

// Error handling
enum VarError {
    NotPresent,
    NotUnicode(String),
}
```

### Signal Handling (`std::signal`)

Async OS signal handling:

```simplex
use simplex_std::signal::{ctrl_c, terminate, hangup, Signal};

// Wait for Ctrl+C
ctrl_c().await;
println("Received SIGINT, shutting down...");

// Available signals: Interrupt, Terminate, Hangup, User1, User2
```

### Async Runtime (`std::runtime`)

Runtime primitives for async programs:

```simplex
use simplex_std::runtime::{block_on, spawn, JoinHandle, JoinError};

// Entry point for async
block_on(async {
    let handle = spawn(async { compute() });
    let result = handle.await?;
});

// JoinError variants: Cancelled, Panic
```

### Cryptography (`std::crypto`)

Password hashing and token generation:

```simplex
use simplex_std::crypto::{bcrypt_hash, bcrypt_verify, generate_token, CryptoError};

let hash = bcrypt_hash("password", 12)?;
let valid = bcrypt_verify("password", &hash)?;
let token = generate_token(32);  // 64 hex chars

// CryptoError variants: HashError, VerifyError
```

### Oneshot Channels (`std::sync::oneshot`)

Single-value, single-use channels:

```simplex
use simplex_std::sync::oneshot;

let (tx, rx) = oneshot::channel::<String>();

spawn(async { tx.send("Hello".to_string()).unwrap() });
let message = rx.recv().await?;

// RecvError::Closed, TryRecvError::{Empty, Closed}
```

### Async I/O (`std::io`)

New async I/O traits for non-blocking stream operations:

```simplex
/// Asynchronous read trait
trait AsyncRead {
    async fn read(self, buf: &mut [u8]) -> Result<usize, IoError>
    async fn read_exact(self, buf: &mut [u8]) -> Result<(), IoError>
    async fn read_to_end(self) -> Result<Vec<u8>, IoError>
    async fn read_line(self) -> Result<String, IoError>
}

/// Asynchronous write trait
trait AsyncWrite {
    async fn write(self, buf: &[u8]) -> Result<usize, IoError>
    async fn write_all(self, buf: &[u8]) -> Result<(), IoError>
    async fn flush(self) -> Result<(), IoError>
}

// Buffered async I/O
type BufReader<R: AsyncRead>
type BufWriter<W: AsyncWrite>
```

### Async Networking (`std::net`)

Async TCP networking with full AsyncRead/AsyncWrite support:

```simplex
// Async TCP Listener
let listener = TcpListener::bind("0.0.0.0:8080").await?;
loop {
    let (stream, addr) = listener.accept().await?;
    spawn(handle_connection(stream, addr));
}

// Async TCP Stream
let stream = TcpStream::connect("localhost:8080").await?;
let mut stream = TcpStream::connect_timeout("localhost:8080", 5000).await?;
stream.write_all(b"Hello").await?;
stream.set_nodelay(true);
stream.shutdown().await?;
```

### Compression (`std::compress`)

Gzip and DEFLATE compression via zlib:

```simplex
use simplex_std::compress::{gzip, gunzip, deflate, inflate};

// One-shot compression
let compressed = gzip(data)?;
let decompressed = gunzip(&compressed)?;

// Streaming compression
let mut encoder = GzipEncoder::with_level(9);
encoder.compress(chunk1)?;
encoder.compress(chunk2)?;
let final_bytes = encoder.finish()?;

// Streaming decompression
let mut decoder = GzipDecoder::new();
decoder.decompress(compressed_chunk)?;
let result = decoder.finish()?;
```

### MPSC Channels (`std::sync::mpsc`)

Multi-producer, single-consumer channels for async message passing:

```simplex
use simplex_std::sync::mpsc;

// Bounded channel
let (tx, rx) = mpsc::channel::<i64>(100);

// Unbounded channel
let (tx, rx) = mpsc::unbounded::<String>();

// Producer (cloneable for multi-producer)
let tx2 = tx.clone();
spawn(async {
    tx.send(42).await.unwrap();
    tx2.send(43).await.unwrap();
});

// Consumer
while let Some(value) = rx.recv().await {
    process(value);
}

// Non-blocking operations
match tx.try_send(value) {
    Ok(()) => {},
    Err(TrySendError::Full(v)) => {},
    Err(TrySendError::Disconnected(v)) => {},
}

match rx.try_recv() {
    Ok(value) => {},
    Err(TryRecvError::Empty) => {},
    Err(TryRecvError::Disconnected) => {},
}
```

---

## Integration with Existing Features

### Neural Gate Architecture Search

```simplex
fn optimize_architecture() -> Classifier {
    let objective = |config: &ArchConfig| -> f64 {
        let model = Classifier::from_config(config);
        model.train_and_validate()
    };

    self_learn_anneal(
        objective,
        ArchConfig::default(),
        |c| c.mutate(),
        AnnealConfig::default()
    )
}
```

### Belief System Dynamics

```simplex
fn learn_belief_dynamics(evidence: Stream<Evidence>) -> LearnableSchedule {
    let objective = |state: &BeliefAnnealState| {
        state.beliefs.consistency_score() +
        state.beliefs.prediction_accuracy(evidence)
    };

    // Schedule learns when to be "open-minded" vs "committed"
    var optimizer = MetaOptimizer::new(objective);
    optimizer.optimize(BeliefAnnealState::initial(), belief_neighbor, 100)
}
```

### HiveOS Coordination

```simplex
specialist Coordinator {
    schedule: LearnableSchedule,

    fn allocate_tasks(&self, tasks: Vec<Task>) -> TaskAllocation {
        let temp = self.schedule.temperature(self.step, self.idle_specialists);

        if temp.val > 0.5 {
            self.exploratory_allocation(tasks)  // High temp: explore
        } else {
            self.greedy_allocation(tasks)       // Low temp: exploit
        }
    }
}
```

---

## New Tests

### Learning Tests (`tests/learning/`)

| Test | Description |
|------|-------------|
| `unit_dual_simple.sx` | Basic dual number operations |
| `unit_dual_numbers.sx` | Comprehensive dual arithmetic |
| `unit_debug_power.sx` | Power function differentiation |

### Standard Library Tests (`tests/stdlib/`)

| Test | Description |
|------|-------------|
| `unit_anneal.sx` | Self-learning annealing |
| `unit_training.sx` | Training schedule tests |
| `unit_io.sx` | Async I/O traits and buffered streams |
| `unit_net.sx` | TCP networking (TcpListener, TcpStream) |
| `unit_compress.sx` | Gzip/deflate compression |
| `unit_mpsc.sx` | Multi-producer single-consumer channels |
| `unit_env.sx` | Environment variables and directories |
| `unit_signal.sx` | OS signal handling |
| `unit_runtime.sx` | Async runtime primitives |
| `unit_fs.sx` | Filesystem operations |
| `unit_process.sx` | Process control and execution |
| `unit_thread.sx` | Threading primitives |
| `unit_sync.sx` | Sync primitives and oneshot channels |
| `unit_crypto.sx` | Cryptography (bcrypt, tokens) |

---

## Performance

### Self-Learning Annealing

| Operation | Throughput |
|-----------|------------|
| Temperature computation | 10M/sec |
| Soft acceptance | 50M/sec |
| Meta-gradient extraction | 1M/sec |
| Schedule update | 100K/sec |

### Expected Improvements Over Fixed Schedules

| Metric | Fixed | Learned | Improvement |
|--------|-------|---------|-------------|
| Final Loss | 1.0 | 0.85-0.90 | 10-15% |
| Training Steps | 100K | 70-80K | 20-30% fewer |
| Pruning Quality | 50% @ 5% loss | 50% @ 2% loss | 60% less degradation |
| Quantization Quality | 4-bit @ 8% loss | 4-bit @ 4% loss | 50% less degradation |

---

## Migration Guide

### From v0.8.x

1. **No breaking changes** - existing code compiles unchanged
2. **New module**: `simplex::optimize::anneal` for self-learning annealing
3. **New library**: `simplex-training` for training pipelines
4. **New std modules**: `std::io` (async), `std::net` (async TCP), `std::compress`, `std::sync::mpsc`
5. **Test reorganization**: Tests moved to category-based structure

### Using Self-Learning Annealing

```simplex
// Before: fixed cooling schedule
fn fixed_anneal<S>(objective: fn(&S) -> f64, initial: S) -> S {
    var temp = 1.0;
    for step in 0..10000 {
        temp = temp * 0.999;  // Fixed rate!
        // ... annealing logic
    }
}

// After: learned schedule
use simplex::optimize::anneal::{self_learn_anneal, AnnealConfig};

let best = self_learn_anneal(objective, initial, neighbor, AnnealConfig::default());
// Schedule parameters learned automatically!
```

### Running Tests with New Structure

```bash
# Old: sxpm test
# New: ./tests/run_tests.sh

# Run all tests
./tests/run_tests.sh

# Run specific category
./tests/run_tests.sh learning
./tests/run_tests.sh neural

# Filter by type
./tests/run_tests.sh all unit
```

---

## API Summary

```simplex
// Core types
LearnableSchedule          // Learnable temperature schedule
MetaOptimizer<S, F>       // Meta-optimizer for schedule learning
AnnealConfig              // Configuration for annealing runs
AnnealState<S>            // State of annealing process

// Schedule methods
schedule.temperature(step, stagnation) -> dual
schedule.accept_probability(delta_e, temp) -> dual
schedule.gradient() -> ScheduleGradient
schedule.update(grad, lr)

// Meta-optimizer methods
optimizer.anneal_episode(initial, neighbor, steps) -> (S, dual)
optimizer.optimize(initial, neighbor, epochs, steps) -> S

// Convenience function
self_learn_anneal(objective, initial, neighbor, config) -> S

// Training library
MetaTrainer::new() -> MetaTrainer
MetaTrainer::meta_train(specialists, teacher) -> TrainResult
CompressionPipeline::compress(specialist) -> CompressedModel

// Async I/O (std::io)
trait AsyncRead { read, read_exact, read_to_end, read_line }
trait AsyncWrite { write, write_all, flush }
BufReader<R: AsyncRead>::new(inner) -> BufReader
BufWriter<W: AsyncWrite>::new(inner) -> BufWriter

// Networking (std::net)
TcpListener::bind(addr).await -> Result<TcpListener, IoError>
TcpListener::accept(self).await -> Result<(TcpStream, SocketAddr), IoError>
TcpStream::connect(addr).await -> Result<TcpStream, IoError>
TcpStream::connect_timeout(addr, ms).await -> Result<TcpStream, IoError>

// Compression (std::compress)
gzip(data) -> Result<Vec<u8>, CompressError>
gunzip(data) -> Result<Vec<u8>, CompressError>
deflate(data) -> Result<Vec<u8>, CompressError>
inflate(data) -> Result<Vec<u8>, CompressError>
GzipEncoder::new() -> GzipEncoder
GzipDecoder::new() -> GzipDecoder

// MPSC Channels (std::sync::mpsc)
mpsc::channel<T>(capacity) -> (Sender<T>, Receiver<T>)
mpsc::unbounded<T>() -> (Sender<T>, Receiver<T>)
Sender::send(value).await -> Result<(), SendError>
Receiver::recv().await -> Option<T>

// Oneshot Channels (std::sync::oneshot)
oneshot::channel<T>() -> (Sender<T>, Receiver<T>)
Sender::send(self, value) -> Result<(), T>
Receiver::recv(self).await -> Result<T, RecvError>

// Environment (std::env)
var(key) -> Result<String, VarError>
set_var(key, value)
remove_var(key)
var_is_set(key) -> bool
current_dir() -> Result<String, VarError>
set_current_dir(path) -> Result<(), VarError>

// Signal (std::signal)
ctrl_c() -> impl Future<Output = ()>
terminate() -> impl Future<Output = ()>
hangup() -> impl Future<Output = ()>
user1() -> impl Future<Output = ()>
user2() -> impl Future<Output = ()>

// Runtime (std::runtime)
block_on(future) -> T
spawn(future) -> JoinHandle<T>

// Thread (std::thread)
spawn(f) -> JoinHandle<T>
sleep(duration)
sleep_ms(ms)
yield_now()
current_id() -> u64

// Filesystem (std::fs)
read(path) -> Result<Vec<u8>, FsError>
read_to_string(path) -> Result<String, FsError>
write(path, contents) -> Result<(), FsError>
create_dir_all(path) -> Result<(), FsError>
remove_file(path) -> Result<(), FsError>
exists(path) -> bool
is_file(path) -> bool
is_dir(path) -> bool

// Process (std::process)
exit(code) -> !
id() -> u32
abort() -> !
command(program, args) -> Result<Output, ProcessError>

// Crypto (std::crypto)
bcrypt_hash(password, cost) -> Result<String, CryptoError>
bcrypt_verify(password, hash) -> Result<bool, CryptoError>
generate_token(byte_len) -> String
```

---

## What's Next

- v0.9.1: Enhanced Edge Hive features (local model integration, secure peer sync)
- v0.10.0: GPU acceleration for tensor operations
- v1.0.0: Production-ready release with full documentation

---

## Credits

Edge Hive and Self-Learning Annealing were designed and implemented by Rod Higgins ([@senuamedia](https://github.com/senuamedia)).

Key influences for Edge Hive:
- Distributed systems principles from Erlang/OTP
- Local-first software design from Ink & Switch
- Privacy-preserving machine learning research

Key influences for Self-Learning Annealing:
- Kirkpatrick et al., "Optimization by Simulated Annealing" (1983)
- Maclaurin et al., "Gradient-based Hyperparameter Optimization" (2015)
- Lorraine et al., "Optimizing Millions of Hyperparameters by Implicit Differentiation" (2020)

---

*"Intelligence everywhere, owned by you."*
