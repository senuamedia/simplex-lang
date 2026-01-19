# Simplex Compiler Toolchain

**Version 0.11.0**

This document describes the Simplex compiler toolchain, which is **self-hosted** and compiles to native binaries via LLVM.

---

## Overview

The Simplex toolchain consists of the following components:

| Component | Binary | Version | Description |
|-----------|--------|---------|-------------|
| **sxc** | `sxc` | v0.11.0 | Simplex Compiler - compiles `.sx` source to native executables |
| **sxpm** | `sxpm` | v0.11.0 | Package manager with dependency resolution |
| **cursus** | `cursus` | v0.11.0 | Bytecode VM with garbage collection |
| **sxdoc** | `sxdoc` | v0.11.0 | Documentation generator |
| **sxlsp** | `sxlsp` | v0.11.0 | Language Server Protocol implementation |
| **sxfmt** | `sxfmt` | v0.11.0 | Code formatter with configurable styles |
| **sxlint** | `sxlint` | v0.11.0 | Static linter with extensible rules |

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
│  │   stage0.py  │ ────► │ sxc-compile  │ ────► │ sxc-compile  │             │
│  │  (bootstrap) │       │   (native)   │       │  (verified)  │             │
│  └──────────────┘       └──────────────┘       └──────────────┘             │
│         │                      │                      │                      │
│         │ compiles             │ compiles             │                      │
│         ▼                      ▼                      ▼                      │
│  codegen.sx              codegen.sx              codegen.sx                 │
│  lexer.sx                lexer.sx                (identical output)         │
│  parser.sx               parser.sx                                          │
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
├── sxc                     # Compiler wrapper script (bash)
├── sxc-compile             # Native self-hosted compiler (387KB)
├── sxpm                    # Package manager
├── cursus                  # Bytecode VM (227KB)
├── sxdoc                   # Documentation generator (225KB)
├── sxlsp                   # Language server (220KB)
├── standalone_runtime.c    # C runtime with intrinsics
├── stage0.py               # Python bootstrap (for rebuilding)
│
├── compiler/bootstrap/     # Compiler source
│   ├── lexer.sx           # Lexer
│   ├── parser.sx          # Parser
│   └── codegen.sx         # Code generator
│
└── tools/                  # Tool source
    ├── cursus.sx          # Bytecode VM source
    ├── sxdoc.sx           # Doc generator source
    └── sxlsp.sx           # LSP source
```

---

## sxc - Simplex Compiler

### Usage

```bash
sxc <command> [options] <file>

Commands:
  build <file.sx> [-o output]    Compile to native executable
  compile <file.sx>              Compile to LLVM IR only
  run <file.sx>                  Compile and run immediately
  version                        Show version
  help                           Show help

Options:
  -o <output>         Output file path
  -O                  Enable optimizations
  -v, --verbose       Verbose output
  -g, --debug         Include debug information
```

### Examples

```bash
# Compile to native executable
sxc build hello.sx -o hello
./hello

# Compile and run immediately
sxc run hello.sx

# Compile to LLVM IR only
sxc compile hello.sx
# Produces: hello.ll
```

### Compilation Process

The `sxc` wrapper script:
1. Invokes `sxc-compile` to generate LLVM IR (`.ll` file)
2. Links with `standalone_runtime.c` using clang
3. Produces a native executable

```bash
# What happens internally:
./sxc-compile hello.sx          # Generates hello.ll
clang -O2 hello.ll standalone_runtime.c -o hello -lm
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

### standalone_runtime.c

The C runtime provides:

| Category | Functions |
|----------|-----------|
| **Memory** | `malloc`, `free`, `load_i64`, `store_i64`, `load_ptr`, `store_ptr` |
| **Strings** | `string_from`, `string_concat`, `string_slice`, `string_eq`, `string_len` |
| **Vectors** | `vec_new`, `vec_push`, `vec_get`, `vec_len`, `vec_set` |
| **I/O** | `print`, `println`, `read_file`, `write_file`, `read_line` |
| **Files** | `file_exists`, `file_size`, `mkdir`, `list_dir`, `remove_file` |
| **Process** | `get_args`, `get_env`, `exit_program`, `system_call` |
| **Time** | `time_now`, `time_sleep` |
| **Network** | `http_get`, `http_post`, `tcp_connect`, `tcp_listen` |

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
git clone https://github.com/user/simplex-lang.git
cd simplex-lang

# Bootstrap from Python (only needed once)
# Works on macOS, Linux, and Windows
python3 stage0.py compiler/bootstrap/codegen.sx -o sxc-compile

# Self-host verification
./sxc build compiler/bootstrap/codegen.sx -o sxc-compile-stage2

# Build tools
./sxc build tools/cursus.sx -o cursus
./sxc build tools/sxdoc.sx -o sxdoc
./sxc build tools/sxlsp.sx -o sxlsp
```

### Platform-Specific Notes

#### macOS
```bash
# Ensure Xcode CLI tools are installed
xcode-select --install

# Bootstrap
python3 stage0.py compiler/bootstrap/codegen.sx -o sxc-compile
```

#### Linux
```bash
# Install clang (Debian/Ubuntu)
sudo apt install clang python3

# Install clang (Fedora/RHEL)
sudo dnf install clang python3

# Bootstrap
python3 stage0.py compiler/bootstrap/codegen.sx -o sxc-compile
```

#### Windows
```powershell
# Install Visual Studio Build Tools with C++ workload
# Or install LLVM/Clang directly

# Bootstrap (PowerShell)
python stage0.py compiler/bootstrap/codegen.sx -o sxc-compile.exe
```

### Quick Build (already bootstrapped)

```bash
# Just build tools
./sxc build tools/cursus.sx -o cursus
./sxc build tools/sxdoc.sx -o sxdoc
./sxc build tools/sxlsp.sx -o sxlsp
```

---

## Binary Sizes

| Binary | Size | Description |
|--------|------|-------------|
| `sxc` | 6.5KB | Wrapper script |
| `sxc-compile` | 387KB | Native compiler |
| `cursus` | 227KB | Bytecode VM |
| `sxdoc` | 225KB | Doc generator |
| `sxlsp` | 220KB | Language server |

Total toolchain: ~1MB of native binaries.

---

## Test Suite

The test suite has been completely reorganized with 156 tests across 13 categories.

### Directory Structure

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
├── stdlib/             # Standard library (16 tests)
├── ai/                 # AI/Cognitive tests (17 tests)
├── toolchain/          # Compiler toolchain (14 tests)
├── runtime/            # Runtime systems (5 tests)
├── integration/        # End-to-end tests (7 tests)
├── basics/             # Basic language tests (6 tests)
├── async/              # Async/await tests (3 tests)
├── learning/           # Automatic differentiation (3 tests)
├── actors/             # Actor model tests (1 test)
└── observability/      # Metrics and tracing (1 test)
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
| 0.11.0 | 2026-01 | Cross-module function imports via `use` statements, automatic LLVM declaration generation |

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

## Benchmarking Framework (v0.10.0)

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

## Code Coverage (v0.10.0)

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

## Error Explanations (v0.10.0)

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

## Incremental Compilation (v0.10.0)

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

When compiled with debug symbols, Simplex binaries produce stack traces with source file names, line numbers, and function names, making production debugging much easier.

### Compilation

```bash
# Include debug symbols (default for non-release builds)
sxc build -g main.sx

# Release build with debug symbols
sxc build -O -g main.sx

# Release build without debug symbols (smallest binary)
sxc build -O --strip main.sx
```

### Stack Trace Output

When a panic or unhandled error occurs:

```
thread 'main' panicked at 'index out of bounds: the len is 5 but the index is 10'
stack trace:
    0: src/processor.sx:142 in process_batch
    1: src/pipeline.sx:87 in run_pipeline
    2: src/main.sx:23 in main
```

### Programmatic Stack Traces

```simplex
use std::debug::{backtrace, print_backtrace}

fn log_error(msg: String) {
    print("Error: {msg}")
    print_backtrace()
}

// Or capture for later
fn capture_context() -> Backtrace {
    backtrace()
}
```

### Binary Size Impact

| Build Type | Binary Size | Stack Traces |
|------------|-------------|--------------|
| Debug (`-g`) | +50% | Full source info |
| Release + Debug (`-O -g`) | +30% | Full source info |
| Release (`-O`) | Baseline | Function names only |
| Stripped (`-O --strip`) | -10% | Addresses only |

---

*The Simplex toolchain is self-hosted. After initial bootstrap, the Python compiler is no longer needed.*
