# TASK-019: Compiler Bugs & Developer Tools for 0.10.0

**Status:** Partial (~25%) — compiler bugs remain, sxfmt complete
**Priority:** Critical
**Target Release:** 0.10.0
**Created:** 2026-01-18
**Updated:** 2026-03-16 (audited against codebase)

> **Audit Note (2026-03-16):**
> - Compiler bugs: NOT FIXED — vec_set redefinition, undefined 'Get', async exit 240, segfaults on belief/epistemic modules, missing FFI declarations
> - sxfmt (code formatter): COMPLETE — `tools/sxfmt.sx` (3173 lines)
> - LSP: STUBBED — `tools/sxlsp.sx` (1576 lines), diagnostics partial, definition/hover/completion not working
> - DWARF debug: PARTIAL — slots allocated, metadata not emitted
> - Error codes: NOT IMPLEMENTED
> - These compiler bugs remain release blockers for 0.10.0+

---

## Overview

This task documents:
1. **Compiler bugs** discovered during 0.9.9 testing (NOT runtime issues)
2. **Developer tools** required for language adoption
3. **Priority matrix** for 0.10.0 release

**Goal:** Make Simplex usable by developers with proper debugging, IDE support, and quality tooling.

**Important Distinction:**
- **0.9.9 (TASK-018)**: Fixed 175 C runtime function signatures (void→int64_t) - COMPLETE
- **0.10.0 (This Task)**: Fix compiler bugs + add developer tools - IN PROGRESS

---

# PART 1: COMPILER BUGS

## Bug Category 1: LLVM IR Generation Errors

### Bug 1.1: `vec_set` Function Redefinition

**Severity:** Critical
**Affected Tests:** 5 edge-hive tests

**Symptom:**
```
simplex-edge-hive/tests/test_adaptation.ll:2245:12: error: invalid redefinition of function 'vec_set'
```

**Affected Files:**
- `test_adaptation.sx`
- `test_hardening.sx`
- `test_hive.sx`
- `test_model.sx`
- `test_security.sx`

**Root Cause:**
The compiler generates duplicate function definitions for `vec_set` when multiple modules are compiled together.

**Location:** `compiler/bootstrap/codegen.sx` - function emission logic

**Fix Required:**
- Track emitted functions to prevent duplicates
- Use `declare` for external functions, `define` only once

---

### Bug 1.2: Undefined Variable 'Get'

**Severity:** High
**Affected Tests:** `spec_actor_basic.sx`

**Symptom:**
```
tests/actors/spec_actor_basic.ll:370:24: error: use of undefined value '%local.Get'
```

**Root Cause:**
The compiler fails to properly resolve message patterns in actor receive blocks.

**Location:** `compiler/bootstrap/codegen.sx` - actor message pattern codegen

**Fix Required:**
- Ensure message pattern variables are allocated before use
- Check pattern matching codegen for actor `receive` blocks

---

### Bug 1.3: Async Runtime Exit Code 240

**Severity:** Medium
**Affected Tests:** `spec_async_basic.sx`

**Symptom:**
```
Exit code 240 (no output)
```

**Root Cause:**
Async state machine generation produces code that crashes.

**Location:** `compiler/bootstrap/codegen.sx` - async/await codegen

**Fix Required:**
- Audit async state machine codegen
- Check stack frame management in async contexts

---

## Bug Category 2: Compiler Crashes (Segfaults)

### Bug 2.1: Segfault on `test_beliefs.sx`

**Severity:** Critical

**Symptom:**
```
./sxc: line 108: 95835 Segmentation fault: 11 "$COMPILER" "$SOURCE"
```

**Root Cause:**
Null pointer dereference or stack overflow in belief parsing.

**Location:** `compiler/bootstrap/parser.sx` or `codegen.sx`

---

### Bug 2.2: Segfault on `test_epistemic.sx`

**Severity:** Critical

Same as Bug 2.1 - compiler crashes on epistemic module code.

---

## Bug Category 3: Native Library Linking

### Bug 3.1: Missing Native Function Declarations

**Severity:** High
**Affected Tests:** All 5 library tests

**Symptom:**
```
error: use of undefined value '@http_request_body'
error: use of undefined value '@sql_begin'
error: use of undefined value '@json_array'
error: use of undefined value '@toml_free'
error: use of undefined value '@uuid_is_nil'
```

**Root Cause:**
Compiler doesn't emit `declare` statements for extern functions.

**Fix Required:**
- Emit LLVM `declare` statements for all extern functions
- Update build process to link native libraries

---

## Bug Category 4: Upstream Blockers

| Bug | Description | Status |
|-----|-------------|--------|
| 4.1 | Mnemonic block parsing | Blocked |
| 4.2 | Hive constructor generation | Blocked |
| 4.3 | Hive runtime intrinsics | Blocked |
| 4.4 | FFI infinite recursion stubs | Blocked |
| 4.5 | SLM native bindings | Blocked |

---

# PART 2: DEVELOPER TOOLS

## Section A: Debugging (CRITICAL)

### Tool A.1: DWARF 5 Debug Symbols

**Priority:** CRITICAL - Release Blocker
**Status:** Not Implemented

**Description:**
Emit standard DWARF debugging information that maps machine code to source lines. Required for ANY debugging capability.

**Requirements:**
- Emit `.debug_info` section with compilation units
- Emit `.debug_line` section with line number mappings
- Emit `.debug_abbrev` for type abbreviations
- Support DWARF 5 format (smaller binaries, faster linking)

**Implementation:**
- Add `-g` flag to `sxc` compiler
- Generate LLVM debug metadata via `!dbg` annotations
- Pass through to LLVM backend for DWARF emission

**Files to Modify:**
- `compiler/bootstrap/codegen.sx` - Add debug metadata generation
- `sxc` - Add `-g` flag handling

---

### Tool A.2: Source-Level Stack Traces

**Priority:** CRITICAL - Release Blocker
**Status:** Partial (addresses only)

**Description:**
On panics/crashes, print human-readable stack traces with function names, file paths, and line numbers.

**Current State:**
```
PANIC: index out of bounds
  at 0x10004a2f0
  at 0x10004b180
```

**Required State:**
```
PANIC: index out of bounds
  at vec_get (simplex-std/src/collections.sx:142)
  at process_items (src/main.sx:87)
  at main (src/main.sx:12)
```

**Implementation:**
- Parse DWARF info at runtime for symbol resolution
- Or embed symbol table in binary
- Use `libbacktrace` or similar for stack unwinding

**Files to Modify:**
- `runtime/standalone_runtime.c` - Add stack trace printing
- `compiler/bootstrap/codegen.sx` - Embed debug info

---

### Tool A.3: Variable Inspection Support

**Priority:** CRITICAL - Release Blocker
**Status:** Not Implemented

**Description:**
Debug info must include variable names, types, and scopes so debuggers (GDB/LLDB) can inspect variables.

**Requirements:**
- Emit `DW_TAG_variable` entries for local/global variables
- Include type information (`DW_TAG_base_type`, `DW_TAG_structure_type`)
- Track variable scope (function, block level)

---

### Tool A.4: Debugger Integration Documentation

**Priority:** High
**Status:** Not Started

**Description:**
Create documentation showing how to debug Simplex programs with:
- LLDB (macOS/Linux)
- GDB (Linux)
- CodeLLDB (VS Code)

**Deliverables:**
- `docs/debugging.md` - Debugging guide
- `.vscode/launch.json` template for debugging
- Example debugging session walkthrough

---

## Section B: IDE Support / Language Server (CRITICAL)

### Tool B.1: Basic LSP Server (sxlsp)

**Priority:** CRITICAL - Release Blocker
**Status:** Exists but incomplete

**Current State:**
`tools/sxlsp.sx` exists but has stubbed/minimal functionality.

**Required LSP Features for 0.10.0:**

| Feature | LSP Method | Priority | Status |
|---------|------------|----------|--------|
| **Diagnostics** | `textDocument/publishDiagnostics` | CRITICAL | Partial |
| **Go to Definition** | `textDocument/definition` | CRITICAL | Stubbed |
| **Hover** | `textDocument/hover` | CRITICAL | Stubbed |
| **Document Symbols** | `textDocument/documentSymbol` | High | Stubbed |
| **Completion** | `textDocument/completion` | High | Stubbed |
| **Find References** | `textDocument/references` | High | Not Started |
| **Signature Help** | `textDocument/signatureHelp` | Medium | Not Started |

**Implementation Requirements:**
- Build AST from source file
- Track symbol definitions and references
- Provide type information on hover
- Report compiler errors as diagnostics

**Files to Modify:**
- `tools/sxlsp.sx` - Main LSP implementation
- New: `compiler/bootstrap/analyzer.sx` - Semantic analysis for LSP

---

### Tool B.2: VS Code Extension

**Priority:** CRITICAL - Release Blocker
**Status:** Exists but basic

**Current State:**
Basic syntax highlighting exists.

**Required Features for 0.10.0:**

| Feature | Status | Notes |
|---------|--------|-------|
| Syntax Highlighting | Done | TextMate grammar |
| LSP Client | Partial | Connects to sxlsp |
| Error Highlighting | Partial | Via LSP diagnostics |
| Go to Definition | Not Working | LSP stubbed |
| Hover Documentation | Not Working | LSP stubbed |
| Code Completion | Not Working | LSP stubbed |
| Snippets | Not Started | Common patterns |
| Debugger Config | Not Started | launch.json templates |

**Deliverables:**
- Update `vscode-simplex/` extension
- Publish to VS Code Marketplace
- Include debugging configuration templates

---

### Tool B.3: Syntax Highlighting (Tree-sitter)

**Priority:** High
**Status:** TextMate only

**Description:**
Create Tree-sitter grammar for better syntax highlighting and editor features.

**Benefits:**
- Faster parsing than TextMate
- Works in more editors (Neovim, Helix, Zed)
- Enables structural code navigation

**Deliverable:**
- `tree-sitter-simplex/` - Tree-sitter grammar package

---

## Section C: Error Messages (CRITICAL)

### Tool C.1: Source Location in Errors

**Priority:** CRITICAL - Release Blocker
**Status:** Partial

**Current State:**
```
Error: undefined variable 'foo'
```

**Required State:**
```
error[E0425]: cannot find value `foo` in this scope
  --> src/main.sx:15:12
   |
15 |     let x = foo + 1;
   |             ^^^ not found in this scope
```

**Requirements:**
- File path in every error
- Line and column numbers
- Source code snippet with caret pointing to error
- Error codes (E0001, E0002, etc.)

---

### Tool C.2: Suggested Fixes

**Priority:** High
**Status:** Not Implemented

**Description:**
Provide "Did you mean?" suggestions for common mistakes.

**Examples:**
```
error[E0425]: cannot find value `pirnt` in this scope
  --> src/main.sx:15:5
   |
15 |     pirnt("hello");
   |     ^^^^^ help: a function with a similar name exists: `print`
```

**Implementation:**
- Levenshtein distance for typo detection
- Scope analysis for import suggestions
- Type inference for method suggestions

---

### Tool C.3: Error Explanation Command

**Priority:** Medium
**Status:** Not Implemented

**Description:**
`simplex explain E0425` shows detailed explanation with examples.

**Deliverable:**
- `docs/errors/` - Error code documentation
- `sxc explain` subcommand

---

## Section D: Build & Package Management (HIGH)

### Tool D.1: Unified Build Command

**Priority:** High
**Status:** Exists (`sxc build`)

**Required Improvements:**
- Debug vs Release profiles (`sxc build --release`)
- Build caching
- Parallel compilation
- Better progress output

---

### Tool D.2: Package Registry

**Priority:** High
**Status:** Not Implemented

**Description:**
Central repository for publishing and downloading packages.

**Requirements:**
- Package publishing (`sxpm publish`)
- Package search (`sxpm search`)
- Version resolution
- Authentication for publishing

**Options:**
- Host own registry (registry.simplex-lang.org)
- Use GitHub Packages
- Use Cloudflare R2 for storage

---

### Tool D.3: Lock File

**Priority:** High
**Status:** Not Implemented

**Description:**
`simplex.lock` file for reproducible builds.

**Requirements:**
- Pin exact versions of all dependencies
- Include checksums for verification
- Auto-update on `sxpm update`

---

## Section E: Code Formatting (CRITICAL)

### Tool E.1: Code Formatter (sxfmt)

**Priority:** CRITICAL - Release Blocker
**Status:** Not Implemented

**Description:**
`sxfmt` or `simplex fmt` - Automatic code formatting like gofmt/rustfmt.

**Requirements:**
- Single canonical style (no configuration)
- Format on save in editors
- `sxfmt --check` for CI (exit 1 if unformatted)
- Handle all language constructs

**Implementation:**
- Parse source to AST
- Pretty-print AST with standard formatting rules
- Preserve comments

**Deliverable:**
- `tools/sxfmt.sx` - Formatter implementation
- `sxc fmt` subcommand

---

## Section F: Testing Tools (HIGH)

### Tool F.1: Test Runner Improvements

**Priority:** High
**Status:** Basic (`sxc test`)

**Required Improvements:**
- Test filtering by name (`sxc test --filter pattern`)
- Parallel test execution
- Better failure output with diffs
- Test timing statistics
- JUnit XML output for CI

---

### Tool F.2: Code Coverage

**Priority:** High
**Status:** Not Implemented

**Description:**
Measure which lines are executed during tests.

**Requirements:**
- `sxc test --coverage`
- HTML coverage report
- Coverage percentage in terminal
- Integration with codecov.io

**Implementation:**
- LLVM source-based coverage
- Or custom instrumentation

---

### Tool F.3: Benchmarking

**Priority:** Medium
**Status:** Not Implemented

**Description:**
Built-in benchmarking framework.

**Requirements:**
- `#[bench]` attribute for benchmark functions
- `sxc bench` command
- Statistical analysis (mean, stddev)
- Comparison between runs

---

## Section G: Documentation Tools (HIGH)

### Tool G.1: Documentation Generator (sxdoc)

**Priority:** High
**Status:** Exists, needs improvements

**Required Improvements:**
- Search functionality (fixed in 0.9.9)
- Doc tests (run code examples)
- Better cross-linking
- Markdown rendering improvements

---

### Tool G.2: Online Playground

**Priority:** High
**Status:** Not Implemented

**Description:**
Web-based code editor at play.simplex-lang.org

**Requirements:**
- Code editor with syntax highlighting
- Compile and run in browser (WASM) or server-side
- Share code via URL
- Example gallery

**Implementation Options:**
- WASM compilation of sxc
- Server-side compilation with sandboxing
- Use Monaco editor

---

## Section H: Linting (MEDIUM)

### Tool H.1: Linter (sxlint)

**Priority:** Medium
**Status:** Not Implemented

**Description:**
`sxlint` or `simplex lint` - Static analysis beyond compiler errors.

**Lint Categories:**
- Unused variables/imports
- Unreachable code
- Suspicious patterns
- Style issues
- Performance hints

**Deliverable:**
- `tools/sxlint.sx` - Linter implementation
- `sxc lint` subcommand

---

## Section I: Profiling (MEDIUM)

### Tool I.1: CPU Profiling

**Priority:** Medium
**Status:** Not Implemented

**Description:**
Integration with system profilers.

**Requirements:**
- Emit frame pointers for profiling (`-fno-omit-frame-pointer`)
- Support `perf` on Linux
- Support Instruments on macOS
- Output in pprof format

---

### Tool I.2: Memory Profiling

**Priority:** Medium
**Status:** Not Implemented

**Description:**
Track heap allocations.

**Requirements:**
- Allocation tracking hooks
- Report allocation hot spots
- Integration with Valgrind/heaptrack

---

# PART 3: PRIORITY MATRIX

## Release Blockers (Must Ship in 0.10.0)

| # | Item | Category | Status |
|---|------|----------|--------|
| 1 | Fix compiler segfaults | Bug | Not Started |
| 2 | Fix vec_set redefinition | Bug | Not Started |
| 3 | DWARF debug symbols (`-g` flag) | Debugging | Not Started |
| 4 | Stack traces with source locations | Debugging | Partial |
| 5 | Basic LSP (definition, hover, diagnostics) | IDE | Stubbed |
| 6 | VS Code extension (working) | IDE | Partial |
| 7 | Error messages with source snippets | Errors | Partial |
| 8 | Error codes (E0001, etc.) | Errors | Not Started |
| 9 | Code formatter (sxfmt) | Formatting | Not Started |

## High Priority (Should Ship)

| # | Item | Category | Status |
|---|------|----------|--------|
| 10 | Fix undefined 'Get' in actors | Bug | Not Started |
| 11 | Fix native library linking | Bug | Not Started |
| 12 | LSP completion | IDE | Not Started |
| 13 | LSP find references | IDE | Not Started |
| 14 | Suggested fixes in errors | Errors | Not Started |
| 15 | Package registry | Build | Not Started |
| 16 | Lock file | Build | Not Started |
| 17 | Code coverage | Testing | Not Started |
| 18 | Online playground | Docs | Not Started |

## Medium Priority (Nice to Have)

| # | Item | Category | Status |
|---|------|----------|--------|
| 19 | Fix async exit code 240 | Bug | Not Started |
| 20 | Tree-sitter grammar | IDE | Not Started |
| 21 | Linter (sxlint) | Quality | Not Started |
| 22 | Benchmarking | Testing | Not Started |
| 23 | CPU/Memory profiling | Performance | Not Started |
| 24 | Error explanation command | Errors | Not Started |

## Low Priority (Future Release)

| # | Item | Category | Status |
|---|------|----------|--------|
| 25 | Incremental compilation | Build | Not Started |
| 26 | JetBrains plugin | IDE | Not Started |
| 27 | Fuzzing support | Testing | Not Started |
| 28 | Interactive debugger | Debugging | Not Started |
| 29 | Jupyter kernel | Interactive | Not Started |

---

# PART 4: IMPLEMENTATION PLAN

## Phase 1: Fix Critical Bugs (Week 1-2)

1. Debug and fix compiler segfaults (Bug 2.1, 2.2)
2. Fix vec_set redefinition (Bug 1.1)
3. Fix undefined variable 'Get' (Bug 1.2)

## Phase 2: Core Debugging (Week 2-3)

1. Implement DWARF debug symbol generation
2. Add `-g` flag to sxc
3. Implement source-level stack traces
4. Write debugging documentation

## Phase 3: IDE Support (Week 3-4)

1. Implement LSP go-to-definition
2. Implement LSP hover
3. Improve LSP diagnostics
4. Update VS Code extension

## Phase 4: Error Messages (Week 4-5)

1. Add source snippets to all errors
2. Implement error codes
3. Add suggested fixes for common errors

## Phase 5: Code Formatter (Week 5-6)

1. Implement sxfmt
2. Add `sxc fmt` command
3. Integrate with VS Code

## Phase 6: Testing & Docs (Week 6-7)

1. Add code coverage
2. Improve test runner
3. Create online playground

## Phase 7: Polish & Release (Week 7-8)

1. Fix remaining high-priority bugs
2. Documentation review
3. Release 0.10.0

---

# PART 5: FILES TO CREATE/MODIFY

## New Files

| File | Description |
|------|-------------|
| `tools/sxfmt.sx` | Code formatter |
| `tools/sxlint.sx` | Linter |
| `compiler/bootstrap/debug.sx` | Debug info generation |
| `compiler/bootstrap/analyzer.sx` | Semantic analysis for LSP |
| `docs/debugging.md` | Debugging guide |
| `docs/errors/` | Error code documentation |
| `.vscode/launch.json` | Debug configuration template |

## Modified Files

| File | Changes |
|------|---------|
| `compiler/bootstrap/codegen.sx` | Fix bugs, add debug metadata |
| `compiler/bootstrap/parser.sx` | Fix segfaults |
| `tools/sxlsp.sx` | Implement LSP features |
| `runtime/standalone_runtime.c` | Source-level stack traces |
| `sxc` | Add `-g`, `fmt`, `lint` commands |
| `vscode-simplex/` | Update extension |

---

# PART 6: SUCCESS CRITERIA

## 0.10.0 is ready when:

### Bug Fixes
- [ ] All 27 stdlib tests pass
- [ ] All 9 edge-hive tests pass (currently 2/9)
- [ ] All 5 library tests pass (currently 0/5)
- [ ] No compiler segfaults
- [ ] No LLVM IR generation errors

### Developer Tools
- [ ] Can debug with LLDB/GDB (breakpoints, variable inspection)
- [ ] Stack traces show file:line on panic
- [ ] VS Code extension provides go-to-definition, hover, diagnostics
- [ ] `sxc fmt` formats code consistently
- [ ] Error messages show source snippets with line numbers

### Documentation
- [ ] Debugging guide written
- [ ] Error codes documented
- [ ] VS Code extension published

---

# PART 7: REFERENCES

## Inspiration
- [Rust Error Messages](https://blog.rust-lang.org/2016/08/10/Shape-of-errors-to-come.html)
- [Elm Compiler Errors for Humans](https://elm-lang.org/news/compiler-errors-for-humans)
- [Go Tools](https://pkg.go.dev/cmd)
- [Zig Tools](https://ziglang.org/learn/tools/)

## Technical References
- [DWARF 5 Specification](https://dwarfstd.org/)
- [LLVM Debug Info](https://llvm.org/docs/SourceLevelDebugging.html)
- [LSP Specification](https://microsoft.github.io/language-server-protocol/)
- [Tree-sitter](https://tree-sitter.github.io/tree-sitter/)

## Related Tasks
- TASK-017: Original bug audit
- TASK-018: Runtime void return bugs (fixed in 0.9.9)
