# Simplex Programming Language

**Version 0.9.2**

Simplex is a modern systems programming language designed for AI-native applications, featuring first-class support for actors, cognitive agents, and distributed computing.

## Features

- **Actor Model**: Built-in actors with message passing for concurrent programming
- **Cognitive Agents**: First-class AI specialist agents with inference capabilities
- **Per-Hive SLM**: Each cognitive hive provisions its own shared language model
- **HiveMnemonic**: Shared consciousness across specialists within a hive
- **Neural IR**: Differentiable program execution with learnable control flow (v0.6.0)
- **Real-Time Learning**: Online training during inference without retraining (v0.7.0)
- **Dual Numbers**: Native forward-mode automatic differentiation (v0.8.0)
- **Self-Learning Annealing**: Optimization schedules that learn themselves (v0.9.0)
- **Self-Hosted**: Compiler written in Simplex itself (bootstrapped from Python)
- **LLVM Backend**: Compiles to optimized native code via LLVM IR
- **Rust-Inspired Syntax**: Familiar syntax with enums, traits, and pattern matching
- **Async/Await**: Native async support with state machine generation
- **Option/Result Types**: Safe error handling with `Option<T>` and `Result<T, E>`

## Installation

### Prerequisites

| Platform | Requirements |
|----------|-------------|
| macOS    | Xcode Command Line Tools, LLVM/Clang |
| Linux    | GCC or Clang, OpenSSL, SQLite3 |
| Windows  | MSVC or MinGW, OpenSSL, SQLite3 (coming soon) |

### Quick Install (macOS)

```bash
# Install dependencies
brew install llvm openssl sqlite3

# Clone the repository
git clone https://github.com/senuamedia/simplex-lang.git
cd simplex-lang

# Build the compiler
./build.sh
```

### Quick Install (Linux)

```bash
# Ubuntu/Debian
sudo apt-get install clang libssl-dev libsqlite3-dev

# Clone and build
git clone https://github.com/senuamedia/simplex-lang.git
cd simplex-lang
./build.sh
```

### Building from Source

The repository includes a pre-built `sxc` compiler. To rebuild from scratch:

```bash
# Run the build script (requires Python 3 and clang)
./build.sh

# Or manually:
# 1. Compile with the bootstrap compiler
python3 stage0.py hello.sx

# 2. Link with clang
clang -O2 hello.ll standalone_runtime.c -o hello -lm -lssl -lcrypto -lsqlite3
```

## Getting Started

### Hello World

Create a file `hello.sx`:

```simplex
fn main() {
    println("Hello, Simplex!");
}
```

Note: `main()` without a return type implicitly returns exit code 0. You can also use `fn main() -> i64` for explicit exit codes.

Compile and run:

```bash
# Build to executable
sxc build hello.sx

# Run directly
./hello

# Or compile and run in one step
sxc run hello.sx
```

### Variables and Types

```simplex
fn main() {
    let x: i64 = 42;
    let name: String = "Simplex";
    let pi: f64 = 3.14159;
    let active: bool = true;

    println(f"Hello {name}, x = {x}");
}
```

### Enums and Pattern Matching

```simplex
enum Option<T> {
    None,
    Some(T),
}

fn main() {
    let value: Option<i64> = Some(42);

    match value {
        Some(x) => println(f"Got value: {x}"),
        None => println("No value"),
    }
}
```

### Structs and Traits

```simplex
struct Point {
    x: i64,
    y: i64,
}

trait Printable {
    fn print(&self);
}

impl Printable for Point {
    fn print(&self) {
        println(f"Point({self.x}, {self.y})");
    }
}

fn main() {
    let p = Point { x: 10, y: 20 };
    p.print();
}
```

### Actors

```simplex
actor Counter {
    count: i64 = 0;

    receive {
        Increment => {
            self.count = self.count + 1;
        }
        GetCount => {
            self.count
        }
    }
}

fn main() -> i64 {
    let counter = spawn Counter {};
    send counter Increment;
    send counter Increment;
    ask counter GetCount
}
```

### Async/Await

```simplex
async fn fetch_data(url: String) -> String {
    let response = http_get(url).await;
    response.body
}

async fn main() {
    let data = fetch_data("https://api.example.com").await;
    println(data);
}
```

### Cognitive Hive with Shared SLM (v0.5.0)

```simplex
// Create a hive with shared consciousness
let mnemonic = HiveMnemonic::new(100, 500, 50);  // 50% belief threshold
mnemonic.learn("Team coding standards require Result types", 0.95);

// Create shared SLM for the hive
let hive_slm = HiveSLM::new("CodeReviewHive", "simplex-cognitive-7b", mnemonic);

// Create specialists that share the SLM
let security = Specialist::new("SecurityAnalyzer", hive_slm, security_anima);
let quality = Specialist::new("QualityReviewer", hive_slm, quality_anima);
let perf = Specialist::new("PerformanceOptimizer", hive_slm, perf_anima);

// All three specialists share ONE model instance
// Findings shared via HiveMnemonic are visible to all
security.contribute_to_mnemonic("SQL injection found in process_user_input");
// quality and perf can now see this finding in their context
```

### AI Specialists with Anima

```simplex
// Create personal memory for a specialist
let anima = Anima::new(10);  // 30% belief threshold
anima.learn("I specialize in code security", 0.9, "self");
anima.desire("Find security vulnerabilities", 0.95);

specialist SecurityAnalyzer {
    model: "simplex-cognitive-7b";
    anima: anima;

    fn analyze(code: String) -> String {
        // Context from anima + hive mnemonic prepended automatically
        infer("Analyze this code for security issues: " + code)
    }
}
```

## Project Structure

```
simplex-lang/
├── compiler/
│   └── bootstrap/          # Self-hosted compiler source
│       ├── stage0.py       # Python bootstrap compiler
│       ├── lexer.sx        # Lexical analysis
│       ├── parser.sx       # Parsing
│       ├── codegen.sx      # Code generation
│       ├── main.sx         # Compiler entry point
│       ├── stdlib.sx       # Standard library
│       └── utils.sx        # Utility functions
├── runtime/
│   └── standalone_runtime.c  # C runtime library
├── tools/
│   ├── sxc.sx              # CLI compiler wrapper
│   ├── sxpm.sx             # Package manager
│   ├── cursus.sx           # Bytecode VM
│   ├── sxdoc.sx            # Documentation generator
│   └── sxlsp.sx            # Language server protocol
├── tests/                  # 156 tests across 13 categories
│   ├── language/           # Core language features (40)
│   ├── types/              # Type system tests (24)
│   ├── neural/             # Neural IR and gates (16)
│   ├── stdlib/             # Standard library (16)
│   ├── ai/                 # AI/Cognitive tests (17)
│   ├── toolchain/          # Toolchain tests (14)
│   ├── runtime/            # Runtime systems (5)
│   ├── integration/        # End-to-end tests (7)
│   ├── basics/             # Basic language (6)
│   ├── async/              # Async/await (3)
│   ├── learning/           # Automatic differentiation (3)
│   ├── actors/             # Actor model (1)
│   └── observability/      # Metrics and tracing (1)
├── simplex-docs/
│   ├── spec/               # Language specification
│   ├── tutorial/           # Learning tutorial
│   ├── testing/            # Testing documentation
│   └── guides/             # How-to guides
└── tasks/                  # Development roadmap
```

## Compiler Toolchain

| Tool | Version | Description |
|------|---------|-------------|
| **sxc** | 0.9.0 | Simplex Compiler with Neural IR and Dual Numbers |
| **sxpm** | 0.9.0 | Package Manager with SLM provisioning |
| **cursus** | 0.9.0 | Bytecode Virtual Machine |
| **sxdoc** | 0.9.0 | Documentation Generator |
| **sxlsp** | 0.9.0 | Language Server Protocol |

## Release History

### v0.9.0 (2026-01-11) - Self-Learning Annealing

**Self-Learning Annealing (TASK-006):**
- Learnable temperature schedules via meta-gradients
- Soft acceptance function using differentiable sigmoid
- Stagnation-triggered reheating with learned thresholds
- Meta-optimization framework for schedule parameter learning
- Integration with Neural Gates, Belief System, and HiveOS

**Test Suite Restructure:**
- 156 tests organized across 13 categories
- Consistent naming convention: `unit_`, `spec_`, `integ_`, `e2e_` prefixes
- New `run_tests.sh` with category and type filtering
- Categories: language, types, neural, stdlib, ai, toolchain, integration, runtime, basics, async, actors, learning, observability

**Library Architecture:**
- New `simplex-training` library for self-optimizing training pipelines
- Learnable schedules: LR, distillation, pruning, quantization, curriculum
- MetaTrainer for unified meta-optimization
- CompressionPipeline for model compression

See [RELEASE-0.9.0.md](simplex-docs/RELEASE-0.9.0.md) for details.

### v0.8.0 (2026-01-10) - Dual Numbers

**Dual Numbers (TASK-005):**
- Native `dual` type for forward-mode automatic differentiation
- Zero-overhead compilation: same assembly as hand-written derivative code
- Arithmetic with automatic chain rule propagation
- Transcendental functions: sin, cos, exp, ln, sqrt, tanh, sigmoid
- `multidual<N>` for computing N partial derivatives simultaneously
- `dual2` for second-order derivatives (Hessians)
- `diff::derivative()`, `diff::gradient()`, `diff::jacobian()`, `diff::hessian()`

**Integration:**
- Neural Gates with dual number gradients
- Belief confidence sensitivity tracking
- Safety constraint margin prediction

See [RELEASE-0.8.0.md](simplex-docs/RELEASE-0.8.0.md) for details.

### v0.7.0 (2026-01-09) - Real-Time Continuous Learning

**simplex-learning Library (TASK-004):**
- Complete real-time continuous learning framework for AI specialists
- Online training during inference without batch retraining
- Streaming optimizers (SGD, Adam, AdamW) with gradient accumulation
- Automatic gradient clipping (by norm and value)
- Safety constraints with fallback strategies
- Federated learning with 6 aggregation strategies (FedAvg, Median, TrimmedMean, etc.)
- Knowledge distillation (teacher-student, self-distillation, progressive)
- Belief conflict resolution across distributed hives
- Experience replay and memory management
- Checkpoint/restore for fault tolerance

**New Modules:**
- `simplex-learning/tensor` - Tensor operations with autograd
- `simplex-learning/optim` - Streaming optimizers and schedulers
- `simplex-learning/safety` - Constraints, bounds, and fallbacks
- `simplex-learning/distributed` - Federated learning and hive coordination
- `simplex-learning/runtime` - Online learner and metrics

See [RELEASE-0.7.0.md](simplex-docs/RELEASE-0.7.0.md) for details.

### v0.6.0 (2026-01-08) - Neural IR and Differentiable Execution

**Neural IR Backend (TASK-001/002):**
- Neural Gates with Gumbel-Softmax for differentiable control flow
- Dual compilation modes: training (differentiable) and inference (discrete)
- Contract logic for probabilistic verification (`requires`, `ensures`, `fallback`)
- Hardware-aware compilation with CPU/GPU/NPU targeting
- Structural pruning for optimized inference binaries
- Superposition memory model with weighted references
- Temperature annealing during training

**Key Features:**
- `neural_gate` keyword for learnable conditionals
- Automatic differentiation through program logic
- Graph partitioning for heterogeneous hardware
- Dead path elimination in trained models

See [RELEASE-0.6.0.md](simplex-docs/RELEASE-0.6.0.md) for details.

### v0.5.1 (2026-01-07) - Type Aliases & License Change

**License Change:**
- Changed from MIT to AGPL-3.0-or-later WITH Simplex-Runtime-Exception
- Programs written IN Simplex remain unaffected (use any license you want)
- Protects the compiler/runtime from proprietary forks without contributing back
- See [LICENSE-CHANGE-NOTICE.md](LICENSE-CHANGE-NOTICE.md) for details

**Type Alias Support:**
- New `type Name = ExistingType;` syntax for type aliases
- Type aliases resolved at compile time
- Improves code readability and maintainability

```simplex
type UserId = i64;
type Count = i64;
type Handler = fn(Request) -> Response;

fn process(id: UserId) -> Count {
    // UserId and Count are interchangeable with i64
    id + 1
}
```

**LLM Context Specification:**
- New `simplex.context.json` file for AI/LLM code generation
- Consolidated language specification in machine-readable format
- Uploaded to https://simplex.senuamedia.com/simplex.context.json

### v0.5.0 (2026-01-07) - SLM Provisioning & Cognitive Hives

**Per-Hive SLM Architecture:**
- Each cognitive hive provisions ONE shared SLM
- All specialists within a hive share the same model instance
- Memory-efficient: 10 specialists = 1 model, not 10
- Three-tier belief thresholds: 30% (Anima) → 50% (Hive) → 70% (Divine)

**HiveMnemonic (Shared Consciousness):**
- Collective memory shared across all specialists in a hive
- Episodic, semantic, and belief memory types
- Automatic context injection into inference prompts
- Cross-specialist knowledge sharing

**Built-in Models:**
- `simplex-cognitive-7b` (4.1 GB) - Full cognitive capabilities
- `simplex-cognitive-1b` (700 MB) - Lightweight alternative
- `simplex-mnemonic-embed` (134 MB) - Embedding model

**sxpm Model Commands:**
```bash
sxpm model list              # List available models
sxpm model install <name>    # Install a model
sxpm model remove <name>     # Remove a model
sxpm model info <name>       # Show model details
```

**New Standard Library Features:**
- HTTP client with TLS support
- Terminal colors, progress bars, spinners, tables
- Enhanced JSON parsing

**Testing Framework:**
- Comprehensive test coverage documentation
- End-to-end scenario tests
- AI/Cognitive component tests

### v0.4.1 (2026-01-07)

**Module System Enhancements:**
- Inline module blocks: `mod name { ... }` syntax
- Re-exports: `pub use other::Thing` for API design
- Glob re-exports: `pub use other::*` for convenience
- Prelude auto-imports: common types/functions available without imports

**Build System:**
- Build cache with mtime tracking and hash-based invalidation
- Incremental compilation support via `needs_recompile()`
- Cache persistence in `.simplex/cache/meta.json`

**Dependency Resolution:**
- Diamond dependency detection with version conflict reporting
- Full semver constraint support: `^`, `~`, `>=`, `<=`, `>`, `<`, `=`
- Enhanced lock file support with `sxpm update`

### v0.4.0 (2026-01-07)

**Phase 2: Package Ecosystem Core**
- Module system with `use`, `mod`, visibility modifiers (`pub`)
- Relative paths: `use super::sibling`, `use self::sub`
- Dependency graph with topological sort and cycle detection
- Lock file generation for reproducible builds
- Standard package structure: `src/lib.sx` vs `src/main.sx` detection

**sxpm Commands:**
- `sxpm check` - Type check without building
- `sxpm update` - Regenerate lock file
- `sxpm clean` - Remove build artifacts
- Version range support in dependencies

### sxc Commands

```bash
sxc build <file.sx> [-o output]   # Compile to native executable
sxc compile <file.sx>             # Compile to LLVM IR (.ll)
sxc run <file.sx>                 # Compile and run immediately
sxc check <file.sx>               # Syntax check only
sxc version                       # Show version info
sxc help                          # Show usage help
```

### sxpm Commands

```bash
sxpm new <name>           # Create a new package
sxpm init                 # Initialize package in current directory
sxpm build                # Build the current package
sxpm run                  # Build and run the current package
sxpm test                 # Run tests
sxpm add <package>        # Add a dependency
sxpm remove <package>     # Remove a dependency
sxpm install              # Install dependencies
sxpm model list           # List available models
sxpm model install <name> # Install a model
```

## Language Features

### Implemented
- Functions, closures, and generics
- Structs, enums, and traits
- Pattern matching with guards
- Option and Result types
- Async/await with state machines
- Actor model with supervision
- f-strings for formatting
- Reference types (&T, *T)
- Turbofish syntax (::<T>)
- Anima cognitive agents
- HiveMnemonic shared consciousness
- Per-hive SLM provisioning

### In Development
- Full trait bounds and where clauses
- Module system improvements
- GPU acceleration for SLMs
- Distributed hive clustering

## Running Tests

```bash
# Run all tests (156 tests across 13 categories)
./tests/run_tests.sh

# Run specific category
./tests/run_tests.sh neural
./tests/run_tests.sh learning
./tests/run_tests.sh ai

# Filter by test type
./tests/run_tests.sh all unit    # Only unit tests
./tests/run_tests.sh all spec    # Only spec tests
./tests/run_tests.sh all integ   # Only integration tests
./tests/run_tests.sh all e2e     # Only end-to-end tests

# Combine category and type
./tests/run_tests.sh stdlib unit
./tests/run_tests.sh neural spec

# Run a specific test directly
sxc run tests/learning/unit_dual_numbers.sx
```

## Documentation

- [Language Specification](simplex-docs/spec/)
- [Tutorial](simplex-docs/tutorial/)
- [Testing Documentation](simplex-docs/testing/)
- [Getting Started Guide](simplex-docs/guides/getting-started.md)
- [Release Notes v0.9.0](simplex-docs/RELEASE-0.9.0.md) - Self-Learning Annealing
- [Release Notes v0.8.0](simplex-docs/RELEASE-0.8.0.md) - Dual Numbers
- [Release Notes v0.7.0](simplex-docs/RELEASE-0.7.0.md) - Real-Time Learning
- [Release Notes v0.6.0](simplex-docs/RELEASE-0.6.0.md) - Neural IR
- [Release Notes v0.5.0](simplex-docs/RELEASE-0.5.0.md) - SLM Provisioning

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

As of version 0.5.1, Simplex is licensed under **AGPL-3.0-or-later WITH Simplex-Runtime-Exception**.

**What this means:**
- **Programs you write IN Simplex** can use any license you want (MIT, Apache, proprietary, etc.)
- **The Simplex compiler/runtime** is AGPL-3.0 - modifications must be shared if distributed or run as a network service

Versions 0.5.0 and earlier remain under the MIT License.

See [LICENSE](LICENSE), [RUNTIME-EXCEPTION](RUNTIME-EXCEPTION), and [LICENSE-CHANGE-NOTICE.md](LICENSE-CHANGE-NOTICE.md) for details.

## Credits

Simplex was created and developed by **Rod Higgins** ([@senuamedia](https://github.com/senuamedia)).

Key contributions include:
- Language design and specification
- Self-hosted compiler implementation
- Actor model and cognitive agent architecture
- "Anima" cognitive memory system concept
- Per-hive SLM provisioning architecture
- HiveMnemonic shared consciousness design

If you use Simplex in your project, please maintain the copyright notices in the source files.

## Links

- **Repository**: https://github.com/senuamedia/simplex-lang
- **Issues**: https://github.com/senuamedia/simplex-lang/issues
- **Documentation**: https://simplex-lang.org (coming soon)
