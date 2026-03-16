# Simplex v0.10.0 Release Notes

**Release Date:** 2026-01-18
**Codename:** Developer Experience

---

## Overview

Simplex v0.10.0 is a **major developer tooling release** introducing comprehensive IDE support, code quality tools, and developer productivity features. This release focuses on making Simplex a joy to develop with.

---

## New Features

### Code Formatter (sxfmt)

A new code formatter ensures consistent style across Simplex codebases.

```bash
# Format a file
sxfmt file.sx

# Check formatting without modifying
sxfmt --check file.sx

# Show diff of changes
sxfmt --diff file.sx
```

**Supported Constructs:**
- Functions, closures, generics
- Enums, structs, traits, impl blocks
- Actors, specialists, hives
- Control flow: if/else, match, while, for, loop
- 4-space indentation (configurable)

### Static Linter (sxlint)

A new static analysis linter catches issues before runtime.

```bash
# Run linter
sxlint file.sx

# Enable all lints
sxlint --all file.sx

# Disable style lints
sxlint --no-style file.sx
```

**Lint Categories:**
| Category | IDs | Examples |
|----------|-----|----------|
| Unused | L001-L009 | Unused variables, imports, functions |
| Unreachable | L010-L019 | Unreachable code, dead branches |
| Suspicious | L020-L029 | Always-true conditions, self-assignment |
| Style | L030-L039 | Line length, naming conventions |
| Performance | L040-L049 | Inefficient patterns, redundant allocations |

### Benchmarking Framework

Built-in benchmarking for performance measurement.

```simplex
use bench::{Bencher, bench_main};

fn bench_sort(b: Bencher) {
    let data = generate_test_data(1000);
    b.iter(|| {
        sort(data.clone())
    });
}

bench_main!(bench_sort, bench_search, bench_insert);
```

```bash
# Run benchmarks
sxc bench file.sx

# Compare against baseline
sxc bench file.sx --baseline baseline.json

# Output JSON
sxc bench file.sx --json
```

### CPU/Memory Profiling

Runtime profiling for performance analysis.

```bash
# Profile execution
sxc run --profile file.sx

# Memory debugging
sxc run --memory-debug file.sx
```

**Profiling Output:**
- Function call counts and timing
- Memory allocation tracking
- Allocation hotspot identification
- Peak memory usage

### Code Coverage

LLVM source-based code coverage for test suites.

```bash
# Run tests with coverage
sxc test --coverage

# Generate coverage report
sxc coverage report --html
```

### Error Explanations

Detailed explanations for compiler errors.

```bash
# Explain an error code
sxc explain E0001

# List all error codes
sxc explain --list
```

**34 documented error codes** covering:
- Syntax errors (E0001-E0099)
- Type errors (E0100-E0199)
- Lifetime/borrowing errors (E0200-E0299)
- Module/import errors (E0300-E0399)
- Runtime errors (E0400-E0499)

### Incremental Compilation

Faster rebuilds by only recompiling changed files.

```bash
# Build with incremental compilation (default)
sxc build project.sx

# Force full rebuild
sxc build project.sx --force

# Disable incremental
sxc build project.sx --no-incremental

# Clean cache
sxc clean
```

**Features:**
- SHA256-based change detection
- Dependency graph tracking
- Cache persistence in `.simplex/cache/`

### Source-Level Stack Traces

Improved debugging with source locations in stack traces.

```
panic: index out of bounds
  at process_item (src/main.sx:42:15)
  at process_batch (src/main.sx:28:9)
  at main (src/main.sx:12:5)
```

**Features:**
- File, line, and column information
- Function names in traces
- Works with release builds (DWARF symbols)

### Package Registry

Publish and discover packages.

```bash
# Publish a package
sxpm publish

# Search packages
sxpm search "json parser"

# Get package info
sxpm info simplex-json
```

### Lock File Support

Reproducible builds with lock files.

```toml
# simplex.lock
[package]
name = "myapp"
version = "1.0.0"

[[dependencies]]
name = "simplex-json"
version = "0.3.2"
checksum = "sha256:abc123..."
```

### Online Playground

A self-hostable web playground for trying Simplex in your browser.

```bash
# Run locally
cd playground
pip install flask
python server.py

# Or with Docker
docker-compose up
```

**Features:**
- Monaco editor with syntax highlighting
- Real-time compilation and execution
- Example library
- Sandboxed execution environment

### Tree-sitter Grammar

Full Tree-sitter grammar for editor integration.

```bash
# Install Tree-sitter grammar
cd tree-sitter-simplex
npm install
npm run build
```

**Supports:**
- Syntax highlighting
- Code folding
- Indentation
- Text objects
- Local variable scoping

### VS Code Extension Updates

The VS Code extension now includes:
- Full LSP integration (go to definition, hover, completion)
- Real-time error diagnostics
- Code formatting on save
- Build and run tasks
- Debugger configuration
- Keybindings (Cmd+Shift+B to build, Cmd+Shift+R to run)

---

## Compiler Improvements

### Native Library Linking Fixed

External library declarations now work correctly.

```simplex
// Now properly generates LLVM 'declare' statements
extern fn http_request_send(req: i64) -> i64;
extern fn sql_query(db: i64, query: i64) -> i64;
```

**Fixed libraries:**
- simplex-http
- simplex-sql
- simplex-json
- simplex-toml
- simplex-uuid

### Async State Machine Fix

Fixed state dispatch in async functions (exit code 240 bug).

**Root Cause:** Await resume points weren't included in state dispatch switch.

**Fix:** Collect await points during body generation, emit complete switch.

---

## Tool Updates

All tools updated to version 0.10.0:

| Tool | Version | New Features |
|------|---------|--------------|
| **sxc** | 0.10.0 | fmt, lint, bench, explain, coverage, profile, clean |
| **sxpm** | 0.10.0 | publish, search, info, lock file support |
| **cursus** | 0.10.0 | - |
| **sxdoc** | 0.10.0 | JSON-LD metadata |
| **sxlsp** | 0.10.0 | Enhanced diagnostics |
| **sxfmt** | 0.10.0 | NEW - Code formatter |
| **sxlint** | 0.10.0 | NEW - Static linter |

---

## New sxc Commands

```bash
sxc fmt <file.sx>              # Format code
sxc lint <file.sx>             # Run linter
sxc bench <file.sx>            # Run benchmarks
sxc explain <ERROR_CODE>       # Explain error
sxc test --coverage            # Test with coverage
sxc run --profile              # Profile execution
sxc run --memory-debug         # Memory debugging
sxc clean                      # Clean build cache
```

---

## Files Created

| File/Directory | Purpose |
|----------------|---------|
| `tools/sxfmt.sx` | Code formatter |
| `tools/sxlint.sx` | Static linter |
| `lib/bench.sx` | Benchmarking library |
| `lib/incremental.sh` | Incremental compilation |
| `docs/errors/*.md` | 34 error documentation files |
| `docs/errors/errors.json` | Error database |
| `playground/` | Online playground (Flask, Docker) |
| `tree-sitter-simplex/` | Tree-sitter grammar |

---

## Test Results

### Passing Tests
| Test Suite | Tests | Status |
|------------|-------|--------|
| Nexus Protocol | 12 | PASS |
| Type System | 5 | PASS |
| Buffer Safety | 1 | PASS |
| Stdlib (unit_assert) | 8 | PASS |
| Stdlib (unit_string) | 10 | PASS |
| Stdlib (unit_vec) | 10 | PASS |
| Stdlib (unit_option) | 8 | PASS |
| Stdlib (unit_result) | 8 | PASS |
| Integration (edge_cases) | 1 | PASS |

### Known Issues
| Test | Issue | Status |
|------|-------|--------|
| spec_actor_basic.sx | Compiler: undefined variable 'Get' | Blocked |
| spec_async_basic.sx | Exit code 112 (was 240) | In Progress |

---

## Breaking Changes

None. This release is fully backwards compatible with 0.9.x.

---

## Upgrade Guide

1. **Update version.sx imports** (if using centralized version):
   ```simplex
   use lib::version;
   // Now returns "0.10.0"
   ```

2. **Try new tools**:
   ```bash
   sxc fmt src/*.sx          # Format your code
   sxc lint src/*.sx         # Check for issues
   sxc test --coverage       # Measure test coverage
   ```

3. **VS Code users**: Update the Simplex extension for new features.

---

## Compatibility

| Component | Minimum Version | Maximum Version |
|-----------|-----------------|-----------------|
| LLVM | 14.0.0 | - |
| Previous Simplex | 0.8.0 | 0.11.0 |

---

## What's Next (v0.11.0)

### v0.11.0: Nexus Protocol & GPU
- Full Nexus Protocol implementation
- GPU acceleration backend
- CUDA/Metal support

### v1.0.0: Production Release
- All compiler bugs resolved
- Full test suite passing
- Production-ready stability

---

## Credits

Developed by Rod Higgins ([@senuamedia](https://github.com/senuamedia)).

---

## Installation

```bash
# Clone and build
git clone https://github.com/senuamedia/simplex-lang.git
cd simplex-lang
./build.sh

# Verify version
./sxc --version
# sxc 0.10.0
```
