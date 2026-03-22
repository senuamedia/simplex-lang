# Simplex Compiler Toolchain

**Version 0.17.0**

This document describes the Simplex compiler toolchain, which is **self-hosted** and compiles to native binaries via LLVM.

---

## Overview

The Simplex toolchain consists of the following components:

| Component | Binary | Version | Description |
|-----------|--------|---------|-------------|
| **sxc** | `sxc` | v0.17.0 | Simplex Compiler - compiles `.sx` source to native executables |
| **sxpm** | `sxpm` | v0.17.0 | Package manager with dependency resolution |
| **cursus** | `cursus` | v0.17.0 | Bytecode VM with garbage collection |
| **sxdoc** | `sxdoc` | v0.17.0 | Documentation generator |
| **sxlsp** | `sxlsp` | v0.17.0 | Language Server Protocol implementation |
| **sxfmt** | `sxfmt` | v0.17.0 | Code formatter with configurable styles |
| **sxlint** | `sxlint` | v0.17.0 | Static linter with extensible rules |

All components are written in **Simplex** and compile to native binaries.

---

## Self-Hosting Architecture

### Bootstrap Process

Simplex uses a multi-stage bootstrap similar to GCC, Go, and Rust:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          BOOTSTRAP PROCESS                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  Stage 0 (Python)        Stage 1 (Native)        Stage 2 (Self-Hosted)      │
│  ┌──────────────┐       ┌──────────────┐       ┌──────────────┐             │
│  │   stage0.py  │ ────► │  build/sxc   │ ────► │  build/sxc   │             │
│  │  (bootstrap) │       │   (native)   │       │  (verified)  │             │
│  └──────────────┘       └──────────────┘       └──────────────┘             │
│         │                      │                      │                      │
│         │ compiles             │ compiles             │                      │
│         ▼                      ▼                      ▼                      │
│  lexer.sx                lexer.sx                lexer.sx                   │
│  parser.sx               parser.sx               parser.sx                  │
│  error.sx                error.sx                error.sx                   │
│  codegen.sx              codegen.sx              codegen.sx                 │
│  main.sx                 main.sx                 (identical output)         │
│                                                                              │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ Stage 0: Python bootstrap compiler (generates LLVM IR)                │  │
│  │ Stage 1: Native compiler built by Stage 0                             │  │
│  │ Stage 2: Native compiler built by Stage 1 (verifies self-hosting)     │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Compilation Pipeline

```
Source (.sx)
     │
     ▼
┌─────────┐
│  Lexer  │  Tokenization (lexer.sx)
└────┬────┘
     │
     ▼
┌─────────┐
│ Parser  │  AST construction (parser.sx)
└────┬────┘
     │
     ▼
┌─────────┐
│ Codegen │  LLVM IR generation (codegen.sx)
└────┬────┘
     │
     ▼
┌─────────┐
│  Clang  │  Native code generation + linking
└────┬────┘
     │
     ▼
Native Binary
```

### File Structure

```
simplex-lang/
├── sxc                         # Compiler wrapper script (bash)
├── build/
│   ├── sxc                     # Native self-hosted compiler
│   ├── sxdoc                   # Documentation generator
│   ├── sxfmt                   # Code formatter
│   ├── sxlint                  # Static linter
│   └── sxlsp                   # Language server
├── runtime/
│   └── standalone_runtime.c    # C runtime with intrinsics
│
├── compiler/bootstrap/         # Compiler source
│   ├── lexer.sx               # Lexer
│   ├── parser.sx              # Parser
│   ├── codegen.sx             # Code generator
│   ├── error.sx               # Error formatting
│   ├── main.sx                # Compiler entry point
│   └── stage0.py             # Python bootstrap (for rebuilding)
│
├── tools/                      # Tool source
│   ├── cursus.sx              # Bytecode VM source
│   ├── sxc.sx                 # Compiler source
│   ├── sxdoc.sx               # Doc generator source
│   ├── sxfmt.sx               # Formatter source
│   ├── sxlint.sx              # Linter source
│   ├── sxlsp.sx               # LSP source
│   └── sxpm.sx                # Package manager source
│
├── lib/                        # Core library
│   ├── version.sx             # Version constants (single source of truth)
│   └── safety.sx              # Safety module
│
├── simplex-std/src/            # Standard library
├── simplex-learning/src/       # Real-time learning library
├── simplex-training/src/       # Training pipeline library
├── simplex-quantum/src/        # Quantum computing framework
├── simplex-edge-hive/src/      # Edge hive runtime
└── simplex-nexus/src/          # Secure connectivity
```

---

## sxc - Simplex Compiler

### Usage

```bash
sxc [OPTIONS] <FILES...>
sxc build [OPTIONS] <FILES...>
sxc run <FILE>
sxc check <FILES...>
sxc repl
sxc fmt <FILES...>

Commands:
  build <file.sx> [-o output]    Compile to native executable (default)
  run <file.sx>                  Compile and run immediately
  check <files...>               Type-check without compiling
  repl                           Interactive REPL
  fmt <files...>                 Format source files
  version                        Show version
  help                           Show help

Options:
  -o <file>           Output file name
  --emit <type>       Output type: llvm-ir (default), asm, obj, exe, dylib
  -g                  Emit DWARF debug symbols for LLDB/GDB
  -v, --verbose       Verbose output
  -f, --force         Force recompilation (ignore timestamps)
  --deps              Show file dependencies
  --auto-deps         Auto-compile module dependencies
  -h, --help          Show this help
  --version           Show version
```

### Examples

```bash
# Compile to native executable
sxc build hello.sx -o hello
./hello

# Compile and run immediately
sxc run hello.sx

# Compile to LLVM IR only (default emit type)
sxc build hello.sx
# Produces: hello.ll

# Compile with debug symbols
sxc build -g hello.sx -o hello

# Auto-compile module dependencies
sxc build --auto-deps main.sx -o main
```

### Compilation Process

The `sxc` wrapper script:
1. Invokes the native compiler to generate LLVM IR (`.ll` file)
2. Links with `runtime/standalone_runtime.c` using clang
3. Produces a native executable

```bash
# What happens internally:
./build/sxc hello.sx            # Generates hello.ll
clang -O2 hello.ll runtime/standalone_runtime.c -o hello -lm
```

---

## cursus - Bytecode VM

### Usage

```bash
cursus [OPTIONS] <FILE.sxb>
cursus compile <FILE.sx> -o <FILE.sxb>

Options:
  --trace         Enable instruction tracing
  --stats         Show VM statistics
  --version       Show version
  -h, --help      Show help
```

### Features

- Stack-based bytecode interpreter
- Garbage collection
- String table management
- Call frame tracking
- Debug tracing mode

---

## sxdoc - Documentation Generator

### Usage

```bash
sxdoc [OPTIONS] <FILES...>

Options:
  --html          Generate HTML output (default)
  --markdown      Generate Markdown output
  -o <dir>        Output directory (default: ./docs)
  --version       Show version
  -h, --help      Show help
```

### Features

- Extracts `///` doc comments from source
- Generates HTML or Markdown documentation
- Supports functions, structs, enums, traits

---

## sxlsp - Language Server

### Usage

```bash
sxlsp [OPTIONS]

Options:
  --stdio         Use stdio for communication (default)
  --version       Show version
  -h, --help      Show help
```

### Features

- JSON-RPC over stdio
- Diagnostics (syntax errors)
- Hover information
- Go to definition (planned)
- Completion (planned)

---

## Runtime System

### runtime/standalone_runtime.c

The C runtime provides an extensive set of intrinsic functions:

| Category | Functions |
|----------|-----------|
| **Memory** | `malloc`, `free`, `load_i64`, `store_i64`, `load_ptr`, `store_ptr`, arena allocator (`arena_reset`, `arena_free`, `arena_used`) |
| **Memory Debug** | `memory_report`, `memory_current`, `memory_peak`, `memory_alloc_count` (enable with `-DSX_MEMORY_DEBUG=1`) |
| **Strings** | `string_from`, `string_concat`, `string_slice`, `string_eq`, `string_len`, `string_char_at`, `string_to_int`, StringBuilder API |
| **Vectors** | `vec_new`, `vec_push`, `vec_get`, `vec_len`, `vec_set`, `vec_clear`, iterators |
| **I/O** | `println`, `print_i64`, `read_file`, `write_file` |
| **Process** | `get_args`, `get_env`, `exit_program` |
| **Time** | `get_time_ms`, `get_time_us`, `get_time_ns`, `sleep_ms` |
| **HTTP** | `http_request_new`, `http_request_header`, `http_request_body`, `http_request_send`, response handling |
| **Actors** | `actor_spawn`, `actor_send`, `actor_stop`, `actor_kill`, `actor_link/unlink/monitor`, mailbox API |
| **Supervisors** | `supervisor_new`, `supervisor_add_child`, `supervisor_start/stop`, `supervisor_handle_exit` |
| **Hive/Router** | `hive_new`, `hive_add_specialist`, `hive_route`, router strategies (rule, round-robin, random, least-busy, semantic) |
| **Shared Store** | `shared_store_new`, `shared_store_put`, `shared_store_get`, `shared_store_count` |
| **Concurrency** | `thread_spawn`, `thread_join`, mutex, condvar, atomic operations |
| **Resilience** | `circuit_breaker_new/allow/success/failure`, `retry_policy_new/should_retry/next_delay` |
| **Checkpointing** | `actor_checkpoint_save/load`, `actor_spawn_from_checkpoint` |
| **Debug** | `print_stack_trace`, `panic`, `panic_at`, `dump_stack`, debug info registration |

### Intrinsic Mapping

Simplex functions are mapped to C intrinsics:

```simplex
// Simplex code:
let s = "hello"
print(s)

// Maps to C:
// intrinsic_string_new("hello")
// intrinsic_print(s)
```

---

## Building from Source

### Prerequisites

| Platform | Requirements |
|----------|-------------|
| **macOS** | Xcode Command Line Tools (includes clang), Python 3 |
| **Linux** | clang or gcc, Python 3 |
| **Windows** | Visual Studio Build Tools (includes clang-cl), Python 3 |

### Platform Support

The Simplex toolchain is **fully cross-platform**:

| Component | macOS | Linux | Windows |
|-----------|-------|-------|---------|
| `stage0.py` (bootstrap) | Yes | Yes | Yes |
| `sxc` (compiler) | Yes | Yes | Yes |
| `sxpm` (package manager) | Yes | Yes | Yes |
| `cursus` (VM) | Yes | Yes | Yes |
| `sxdoc` (docs) | Yes | Yes | Yes |
| `sxlsp` (LSP) | Yes | Yes | Yes |

The bootstrap compiler (`stage0.py`) automatically detects the platform and generates the appropriate LLVM target triple:
- **macOS**: `x86_64-apple-macosx<version>` or `aarch64-apple-macosx<version>`
- **Linux**: `x86_64-unknown-linux-gnu` or `aarch64-unknown-linux-gnu`
- **Windows**: `x86_64-pc-windows-msvc`

### Full Bootstrap

```bash
# Clone repository
git clone https://github.com/senuamedia/simplex.git
cd simplex

# Bootstrap from Python (only needed once)
# The build.sh script handles the full bootstrap process
./build.sh

# Or manually:
cd compiler/bootstrap
python3 stage0.py codegen.sx -o ../../build/sxc
cd ../..

# Build tools
./sxc build tools/sxdoc.sx -o build/sxdoc
./sxc build tools/sxlsp.sx -o build/sxlsp
./sxc build tools/sxfmt.sx -o build/sxfmt
./sxc build tools/sxlint.sx -o build/sxlint
```

### Platform-Specific Notes

#### macOS
```bash
# Ensure Xcode CLI tools are installed
xcode-select --install

# Bootstrap
./build.sh
```

#### Linux
```bash
# Install clang (Debian/Ubuntu)
sudo apt install clang python3

# Install clang (Fedora/RHEL)
sudo dnf install clang python3

# Bootstrap
./build.sh
```

#### Windows
```powershell
# Install Visual Studio Build Tools with C++ workload
# Or install LLVM/Clang directly

# Bootstrap (PowerShell)
cd compiler\bootstrap
python stage0.py codegen.sx -o ..\..\build\sxc.exe
```

### Quick Build (already bootstrapped)

```bash
# Just build tools
./sxc build tools/sxdoc.sx -o build/sxdoc
./sxc build tools/sxlsp.sx -o build/sxlsp
./sxc build tools/sxfmt.sx -o build/sxfmt
./sxc build tools/sxlint.sx -o build/sxlint
```

---

## Binary Sizes

| Binary | Description |
|--------|-------------|
| `sxc` | Wrapper script (bash) |
| `build/sxc` | Native compiler |
| `build/sxdoc` | Doc generator |
| `build/sxfmt` | Code formatter |
| `build/sxlint` | Static linter |
| `build/sxlsp` | Language server |

**Note:** Binary sizes vary by platform. The toolchain compiles to compact native binaries via LLVM.

---

## Test Suite

The test suite covers language features, standard library, AI/cognitive systems, runtime, and toolchain across multiple categories.

### Directory Structure

```
tests/
├── language/           # Core language features
│   ├── actors/         # Actor model tests
│   ├── async/          # Async/await tests
│   ├── basics/         # Basic language constructs (enum, match, for, closures, try)
│   ├── closures/       # Closure-specific tests
│   ├── control/        # Control flow (if-let, match binding/patterns)
│   ├── functions/      # Function features (closures, turbofish, generics)
│   ├── modules/        # Module system and imports
│   ├── traits/         # Trait system (impl trait, trait ref, self ref)
│   └── types/          # Type system (generics, option/result, turbofish)
├── types/              # Additional type system tests
├── neural/             # Neural IR and gates
│   ├── contracts/      # Neural gate contracts and static analysis
│   ├── gates/          # Gate inference, training, gradients, hardware
│   └── pruning/        # Structural pruning
├── stdlib/             # Standard library unit and integration tests
├── ai/                 # AI/Cognitive tests
│   ├── anima/          # Anima integration
│   ├── hive/           # Hive mnemonic and per-hive SLM
│   ├── inference/      # Memory-augmented inference
│   ├── memory/         # Cognitive memory and BDI
│   ├── orchestration/  # Cognitive orchestration
│   ├── specialists/    # Specialist unit tests
│   └── tools/          # AI tool tests
├── training/           # Training pipeline tests (annealing, attention, LoRA, etc.)
├── toolchain/          # Compiler toolchain integration
├── runtime/            # Runtime systems (async, I/O, memory, networking)
├── integration/        # End-to-end workflow tests
├── basics/             # Basic language tests
├── async/              # Top-level async tests
├── learning/           # Automatic differentiation (dual numbers)
└── observability/      # Metrics and tracing
```

### Naming Convention

| Prefix | Type | Description |
|--------|------|-------------|
| `unit_` | Unit | Tests individual functions/types in isolation |
| `spec_` | Specification | Tests language specification compliance |
| `integ_` | Integration | Tests integration between components |
| `e2e_` | End-to-End | Tests complete workflows |

### Running Tests

```bash
# Run all tests
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
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 0.1.0 | 2024-12 | Initial Python bootstrap |
| 0.2.0 | 2024-12 | Self-hosted compiler (Stage 1) |
| 0.3.0 | 2025-01 | Native binary compilation |
| 0.3.1 | 2025-01 | Fixed lookup_variant bug, all tools compiled |
| 0.8.0 | 2026-01 | Native dual numbers for automatic differentiation |
| 0.9.0 | 2026-01 | Self-learning annealing, test suite restructure, llama.cpp integration |
| 0.10.0 | 2026-01 | sxfmt, sxlint, benchmarking, coverage, error explanations, incremental compilation, source-level stack traces |
| 0.11.0 | 2026-02 | Module system with `use` imports |
| 0.12.0 | 2026-03 | Cross-module function imports, automatic LLVM declaration generation, expanded runtime (actors, supervisors, hive, resilience) |
| 0.13.0 | 2026-03 | Completion & Foundations: 5 compiler bug fixes, complex numbers, matrix algebra, dual number extensions, HTTP client, JSON parser, training pipeline |
| 0.14.0 | 2026-03 | Quantum Bridge: quantum computing primitives, backend abstraction, variational algorithms, cost-aware dispatch, quantum-enhanced optimization |
| 0.17.0 | 2026-03 | Cloud infrastructure, developer tools (REPL, test framework), mathematical intelligence modules |

---

## sxfmt - Code Formatter (v0.10.0)

### Usage

```bash
sxfmt [OPTIONS] <FILES...>

Options:
  --check         Check if files are formatted (exit 1 if not)
  --write, -w     Write formatted output back to files
  --config <file> Use custom configuration file
  --stdin         Read from stdin, write to stdout
  --version       Show version
  -h, --help      Show help
```

### Features

- Deterministic formatting for consistent code style
- Configurable via `.sxfmt.toml` or `simplex.toml`
- Preserves semantics while normalizing whitespace and style
- Integration with sxlsp for format-on-save

### Configuration

Create a `.sxfmt.toml` file:

```toml
# .sxfmt.toml
indent_width = 4
max_line_width = 100
trailing_commas = "always"  # "always", "never", "multiline"
brace_style = "same_line"   # "same_line", "next_line"
blank_lines_between_items = 1
sort_imports = true
```

### Examples

```bash
# Format a single file
sxfmt -w main.sx

# Check formatting in CI
sxfmt --check src/**/*.sx

# Format all project files
sxfmt -w .
```

---

## sxlint - Static Linter (v0.10.0)

### Usage

```bash
sxlint [OPTIONS] <FILES...>

Options:
  --fix           Automatically fix issues where possible
  --config <file> Use custom configuration file
  --rule <name>   Run only specified rule
  --format <fmt>  Output format: text, json, sarif (default: text)
  --version       Show version
  -h, --help      Show help
```

### Features

- Extensible rule system with severity levels
- Auto-fix support for common issues
- IDE integration via sxlsp
- Custom rule authoring in Simplex

### Built-in Rules

| Rule | Severity | Description | Auto-fix |
|------|----------|-------------|----------|
| `unused-variable` | Warning | Detects unused variables | Yes |
| `unused-import` | Warning | Detects unused imports | Yes |
| `dead-code` | Warning | Detects unreachable code | No |
| `implicit-return` | Info | Suggests explicit returns | Yes |
| `shadow-variable` | Warning | Detects variable shadowing | No |
| `mutable-capture` | Warning | Warns on mutable closure captures | No |
| `actor-blocking` | Error | Detects blocking calls in actors | No |
| `checkpoint-required` | Warning | Suggests checkpoints for long operations | No |
| `unsafe-unwrap` | Warning | Warns on unwrap without error handling | No |
| `deprecated-api` | Warning | Flags use of deprecated APIs | No |

### Configuration

Create a `.sxlint.toml` file:

```toml
# .sxlint.toml
[rules]
unused-variable = "warn"
unused-import = "warn"
dead-code = "warn"
implicit-return = "off"
actor-blocking = "error"

[rules.mutable-capture]
severity = "warn"
allow_in_tests = true

# Ignore patterns
[ignore]
paths = ["tests/fixtures/**", "generated/**"]
```

### Examples

```bash
# Lint all source files
sxlint src/

# Auto-fix issues
sxlint --fix src/

# Run specific rule
sxlint --rule unused-import src/

# Output for CI integration
sxlint --format sarif src/ > lint-results.sarif
```

---

## Benchmarking Framework (v0.10.0) -- Planned

### Usage

```bash
sxc bench [OPTIONS] <FILE>

Options:
  --filter <pattern>  Run only benchmarks matching pattern
  --iterations <n>    Number of iterations (default: auto-determined)
  --warmup <n>        Warmup iterations (default: 3)
  --output <file>     Save results to JSON file
  --compare <file>    Compare against previous results
  --version           Show version
  -h, --help          Show help
```

### Writing Benchmarks

Benchmarks are defined using the `bench` attribute:

```simplex
use std::bench::{Bencher, black_box}

#[bench]
fn bench_vector_push(b: &Bencher) {
    b.iter(|| {
        let mut v: Vec<i64> = Vec::new()
        for i in 0..1000 {
            v.push(black_box(i))
        }
        v
    })
}

#[bench]
fn bench_map_insert(b: &Bencher) {
    b.iter(|| {
        let mut m: Map<String, i64> = Map::new()
        for i in 0..1000 {
            m.insert(format("key_{i}"), i)
        }
        m
    })
}

#[bench]
fn bench_with_setup(b: &Bencher) {
    // Setup runs once before benchmarking
    let data = generate_test_data(10000)

    b.iter(|| {
        process(black_box(&data))
    })
}
```

### Benchmark Output

```
Running 3 benchmarks

bench_vector_push    ... bench:    12,345 ns/iter (+/- 234)
bench_map_insert     ... bench:    45,678 ns/iter (+/- 1,234)
bench_with_setup     ... bench:   123,456 ns/iter (+/- 5,678)

test result: ok. 3 passed; 0 failed; 0 ignored
```

### Comparing Results

```bash
# Save baseline
sxc bench benchmarks.sx --output baseline.json

# Compare against baseline
sxc bench benchmarks.sx --compare baseline.json
```

Output with comparison:

```
bench_vector_push    ... bench:    11,234 ns/iter (+/- 210) [-9.0%]
bench_map_insert     ... bench:    47,890 ns/iter (+/- 1,100) [+4.8%]
bench_with_setup     ... bench:   121,000 ns/iter (+/- 5,200) [-2.0%]
```

---

## Code Coverage (v0.10.0) -- Planned

### Usage

```bash
sxc test --coverage [OPTIONS] <FILES...>

Options:
  --coverage              Enable coverage collection
  --coverage-report <fmt> Report format: text, html, lcov, json (default: text)
  --coverage-dir <dir>    Output directory for coverage reports (default: coverage/)
  --min-coverage <pct>    Fail if coverage below threshold
```

### Features

- Line, branch, and function coverage tracking
- HTML reports with source highlighting
- LCOV output for CI integration
- Coverage thresholds for quality gates

### Examples

```bash
# Run tests with coverage
sxc test --coverage tests/

# Generate HTML report
sxc test --coverage --coverage-report html tests/

# Require minimum coverage
sxc test --coverage --min-coverage 80 tests/

# Generate LCOV for CI tools
sxc test --coverage --coverage-report lcov tests/
```

### Coverage Report (Text)

```
Coverage Report
===============

File                        Lines    Branches    Functions
----------------------------------------------------------
src/main.sx                 85.2%    78.3%       92.0%
src/parser.sx               91.4%    84.1%       100.0%
src/codegen.sx              73.2%    68.9%       87.5%
src/runtime.sx              88.7%    81.2%       95.0%
----------------------------------------------------------
Total                       84.6%    78.1%       93.6%
```

---

## Error Explanations (v0.10.0) -- Planned

### Usage

```bash
sxc explain <ERROR_CODE>

Examples:
  sxc explain E0001
  sxc explain E0042
```

### Features

- Detailed explanations for all compiler errors
- Code examples showing the problem and fix
- Links to relevant documentation
- Searchable error database

### Example

```bash
$ sxc explain E0015

Error E0015: Borrow of moved value
==================================

This error occurs when you try to use a value after it has been moved
to another location.

Example of incorrect code:

    let s = String::from("hello")
    let t = s                    // s is moved here
    print(s)                     // ERROR: s was moved

The value `s` was moved to `t` on line 2. After a move, the original
variable is no longer valid.

To fix this, you can:

1. Clone the value instead of moving it:

    let s = String::from("hello")
    let t = s.clone()            // s is cloned, not moved
    print(s)                     // OK: s is still valid

2. Use a reference instead:

    let s = String::from("hello")
    let t = &s                   // borrow s, don't move
    print(s)                     // OK: s is still valid

See also:
- Ownership: https://simplex-lang.org/book/ownership
- Borrowing: https://simplex-lang.org/book/borrowing
```

---

## Incremental Compilation (v0.10.0) -- Planned

### Overview

Incremental compilation tracks dependencies between source files and only recompiles what has changed, dramatically reducing rebuild times for large projects.

### How It Works

1. **Dependency Graph**: The compiler builds a graph of which files depend on which
2. **Content Hashing**: Source files are hashed to detect changes
3. **Cached Artifacts**: Intermediate compilation results are cached
4. **Selective Rebuild**: Only affected files are recompiled

### Usage

Incremental compilation is **enabled by default**. To disable:

```bash
sxc build --no-incremental main.sx
```

### Cache Location

| Platform | Path |
|----------|------|
| macOS | `~/Library/Caches/simplex/incremental/` |
| Linux | `~/.cache/simplex/incremental/` |
| Windows | `%LOCALAPPDATA%\simplex\incremental\` |

### Cache Management

```bash
# Clear incremental cache
sxc cache clear

# Show cache statistics
sxc cache stats

# Prune old cache entries
sxc cache prune --older-than 7d
```

### Performance

| Project Size | Full Build | Incremental (1 file changed) |
|--------------|------------|------------------------------|
| Small (10 files) | 2.1s | 0.4s |
| Medium (100 files) | 12.3s | 0.8s |
| Large (1000 files) | 45.6s | 1.2s |

---

## Source-Level Stack Traces (v0.10.0)

### Overview

When compiled with debug symbols (`-g` flag), Simplex binaries produce stack traces with source file names, line numbers, and function names. The runtime uses DWARF debug info (via `addr2line`) and a registered debug info table.

### Compilation

```bash
# Include debug symbols
sxc build -g main.sx -o main
```

### Stack Trace Output

When a panic or unhandled error occurs, the runtime calls `intrinsic_print_stack_trace()`:

```
thread 'main' panicked at 'index out of bounds'
stack trace:
    0: src/processor.sx:142 in process_batch
    1: src/pipeline.sx:87 in run_pipeline
    2: src/main.sx:23 in main
```

### Runtime Intrinsics

The C runtime provides:
- `intrinsic_print_stack_trace()` -- prints the current stack trace
- `intrinsic_panic(message)` -- prints message and stack trace, then exits
- `intrinsic_panic_at(message, file, line)` -- panic with source location
- `intrinsic_dump_stack()` -- debug stack dump
- `sx_register_debug_info(func_start, func_end, name, file, line)` -- registers debug info for a function

**Note:** A higher-level programmatic API (`std::debug::backtrace`) for capturing and inspecting stack traces is planned.

### Binary Size Impact

| Build Type | Binary Size | Stack Traces |
|------------|-------------|--------------|
| Debug (`-g`) | +50% | Full source info |
| Release + Debug (`-O -g`) | +30% | Full source info |
| Release (`-O`) | Baseline | Function names only |
| Stripped (`-O --strip`) | -10% | Addresses only |

---

*The Simplex toolchain is self-hosted. After initial bootstrap, the Python compiler is no longer needed.*
