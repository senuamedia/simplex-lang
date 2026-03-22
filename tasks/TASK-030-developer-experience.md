# TASK-030: Developer Experience

**Status**: Complete
**Priority**: Medium
**Created**: 2026-03-16
**Target Version**: 0.17.0
**Depends On**: TASK-022 (v0.13.0 complete)

---

## Overview

Developer experience tools that lower the barrier to adoption. A language lives or dies by how easy it is to try, learn, and get productive. These tools complement the existing sxc, sxdoc, sxpm, sxlsp, sxfmt, and sxlint toolchain.

All implementations must be **pure Simplex** where possible.

---

## Tool 1: simplex-repl

**Location**: `tools/sxrepl.sx`
**Priority**: High — interactive exploration is the fastest way to learn a language

### Features
- Read-eval-print loop for Simplex expressions
- Multi-line input support (detect incomplete expressions)
- Variable persistence across REPL lines
- `:type expr` — show inferred type
- `:ast expr` — show parsed AST
- `:ir expr` — show generated LLVM IR
- `:load file.sx` — load and execute a file
- `:reset` — clear all state
- History (up/down arrow via readline or equivalent)
- Tab completion for keywords and variables

### Implementation Notes
- Uses the existing compiler pipeline: lexer -> parser -> codegen -> JIT execute
- JIT compilation via LLVM OrcJIT or compile-to-temp-file-and-execute
- REPL state is an accumulating set of declarations

### Success Criteria
- `let x = 42` then `x + 1` returns `43`
- Function definitions persist across lines
- `:type` shows correct type information
- History navigation works
- New test: `tests/toolchain/spec_repl.sx`

---

## Tool 2: simplex-test

**Location**: `simplex-std/src/test.sx`
**Priority**: High — structured testing framework beyond `assert_eq`

### Core API
```simplex
fn test_suite(name: String, tests: Vec<TestCase>) -> TestSuite
fn test_case(name: String, body: fn() -> Result<bool, String>) -> TestCase
fn test_run(suite: TestSuite) -> TestReport

struct TestReport {
    total: i64,
    passed: i64,
    failed: i64,
    skipped: i64,
    duration_ms: i64,
    failures: Vec<TestFailure>
}

// Assertions
fn assert_eq<T>(actual: T, expected: T, message: String) -> Result<bool, String>
fn assert_ne<T>(actual: T, expected: T, message: String) -> Result<bool, String>
fn assert_true(condition: bool, message: String) -> Result<bool, String>
fn assert_approx(actual: f64, expected: f64, epsilon: f64) -> Result<bool, String>
fn assert_err<T, E>(result: Result<T, E>) -> Result<bool, String>
fn assert_ok<T, E>(result: Result<T, E>) -> Result<bool, String>
```

### Features
- Test discovery (scan for `fn test_*` functions)
- Setup/teardown hooks per suite
- Skip annotation (`@skip`, `@skip_if(condition)`)
- Timeout per test case
- Colored output (pass=green, fail=red, skip=yellow)
- JUnit XML output for CI integration
- Parallel test execution (tests are independent actors)

### Property-Based Testing (Stretch)
```simplex
fn prop_test(name: String, generator: fn() -> T, property: fn(T) -> bool, iterations: i64) -> TestCase
```

### Success Criteria
- Test suites run with pass/fail/skip reporting
- Failures show actual vs expected with source location
- Timeout kills hung tests
- JUnit XML output is valid
- New test: `tests/toolchain/spec_test_framework.sx`

---

## Tool 3: simplex-bench

**Location**: `simplex-std/src/bench.sx`
**Priority**: Medium — mentioned as planned since v0.10.0

### Core API
```simplex
fn bench(name: String, iterations: i64, body: fn()) -> BenchResult
fn bench_compare(results: Vec<BenchResult>) -> String

struct BenchResult {
    name: String,
    iterations: i64,
    total_ns: i64,
    mean_ns: i64,
    median_ns: i64,
    min_ns: i64,
    max_ns: i64,
    std_dev_ns: i64
}
```

### Features
- Warmup iterations (excluded from measurement)
- Statistical analysis (mean, median, std dev, min, max)
- Comparison between benchmarks
- Memory allocation tracking (integrate with existing `intrinsic_memory_*`)
- Output formats: human-readable table, JSON, CSV

### Success Criteria
- Benchmark produces stable, repeatable results
- Statistical output is correct
- Memory tracking reports allocations per iteration
- New test: `tests/toolchain/spec_bench.sx`

---

## Tool 4: simplex-fs (Enhanced File System)

**Location**: `simplex-std/src/fs.sx`
**Priority**: Medium — the runtime has basic I/O, but needs higher-level operations

### Core API
```simplex
fn fs_read(path: String) -> Result<String, FsError>
fn fs_write(path: String, content: String) -> Result<bool, FsError>
fn fs_append(path: String, content: String) -> Result<bool, FsError>
fn fs_exists(path: String) -> bool
fn fs_delete(path: String) -> Result<bool, FsError>
fn fs_mkdir(path: String) -> Result<bool, FsError>
fn fs_rmdir(path: String) -> Result<bool, FsError>
fn fs_list(path: String) -> Result<Vec<FsEntry>, FsError>
fn fs_walk(path: String, pattern: String) -> Result<Vec<String>, FsError>
fn fs_copy(src: String, dst: String) -> Result<bool, FsError>
fn fs_move(src: String, dst: String) -> Result<bool, FsError>
fn fs_stat(path: String) -> Result<FsStat, FsError>
fn fs_temp_file() -> Result<String, FsError>
fn fs_temp_dir() -> Result<String, FsError>

struct FsEntry {
    name: String,
    path: String,
    is_dir: bool,
    size: i64,
    modified: i64
}

struct FsStat {
    size: i64,
    created: i64,
    modified: i64,
    is_dir: bool,
    is_file: bool
}
```

### Features
- Recursive directory walking with glob patterns
- Temp file/directory creation with auto-cleanup
- File metadata (size, timestamps)
- Path manipulation (join, parent, extension, stem)
- File watching (inotify/kqueue — stretch goal)

### Success Criteria
- Read/write/append/delete files correctly
- Directory creation and recursive listing
- Glob-based file walking
- Temp files are unique and writable
- New test: `tests/stdlib/spec_fs.sx`

---

## Tool 5: simplex-process

**Location**: `simplex-std/src/process.sx`
**Priority**: Low — spawn and manage child processes

### Core API
```simplex
fn process_run(cmd: String, args: Vec<String>) -> Result<ProcessOutput, ProcessError>
fn process_spawn(cmd: String, args: Vec<String>) -> Result<ProcessHandle, ProcessError>
fn process_wait(handle: ProcessHandle) -> Result<ProcessOutput, ProcessError>
fn process_kill(handle: ProcessHandle) -> Result<bool, ProcessError>

struct ProcessOutput {
    exit_code: i64,
    stdout: String,
    stderr: String
}
```

### Success Criteria
- Run command and capture output
- Non-zero exit code returns error
- Spawn async process and wait later
- Kill terminates running process
- New test: `tests/stdlib/spec_process.sx`

---

## Dependency Graph

```
TASK-022 (v0.13.0)
    |
    +--> simplex-repl (needs compiler pipeline)
    +--> simplex-test (independent)
    +--> simplex-bench (independent, uses existing intrinsics)
    +--> simplex-fs (C runtime file operations)
    +--> simplex-process (C runtime process operations)
```

All five are independent and can be built in parallel.

---

## Estimated Line Counts

| Tool | Est. Lines |
|------|-----------|
| simplex-repl | ~1,200-1,600 |
| simplex-test | ~800-1,000 |
| simplex-bench | ~600-800 |
| simplex-fs | ~600-800 |
| simplex-process | ~400-500 |
| **Total** | **~3,600-4,700** |
