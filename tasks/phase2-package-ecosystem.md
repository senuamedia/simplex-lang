# Phase 2: Package Ecosystem

**Priority**: HIGH - Enables community contributions
**Status**: IN PROGRESS
**Depends On**: Phase 1 (JSON for manifests)
**Last Updated**: 2026-01-07

## Progress Summary

### Completed:
- [x] Package manifest format (simplex.json) - `bootstrap_mini/package.sx`
- [x] Semver parsing and comparison
- [x] Version range support (^, ~, >=, *)
- [x] Manifest validation
- [x] Manifest serialization to JSON
- [x] sxpm new command - creates package structure
- [x] sxpm init command - initializes existing directory
- [x] sxpm build command - builds packages
- [x] sxpm run command - build and run
- [x] sxpm test command - runs tests
- [x] sxpm clean command - removes target/
- [x] sxpm add command - adds dependencies
- [x] sxpm remove command - removes dependencies
- [x] sxpm search command - searches registry (stub)
- [x] sxpm info command - shows package info (stub)
- [x] sxpm install command - installs packages
- [x] Basic dependency resolution (path, git, registry stubs)
- [x] LLVM IR merge tool (merge.sx)
- [x] Runtime exit() function added

### Files Created:
- `bootstrap_mini/package.sx` - Package manifest module
- `bootstrap_mini/sxpm.sx` - Package manager CLI
- `bootstrap_mini/registry.sx` - Registry client module

### Critical Remaining (Must Complete):
- [ ] **File I/O Operations** - Complete I/O primitives (Section 9)
- [ ] **Dependency Graph** - DAG construction, cycle detection, topological sort (Section 3.1)
- [ ] **Lock File** - simplex.lock for reproducible builds (Section 3.3)
- [ ] **Module System** - `use`, `mod`, `pub` keywords (Section 5)
- [ ] Build and test sxpm binary
- [ ] Integration tests for package workflow

## Overview

A language without a package ecosystem cannot grow. This phase establishes the infrastructure for sharing and reusing Simplex code.

---

## 1. Package Manifest Format

**File**: New `simplex.toml` specification
**Status**: Not defined

### Subtasks

- [ ] 1.1 Define manifest schema
  ```toml
  [package]
  name = "my-package"
  version = "0.1.0"
  authors = ["Name <email>"]
  description = "Short description"
  license = "MIT"
  repository = "https://github.com/user/repo"
  keywords = ["keyword1", "keyword2"]

  [dependencies]
  simplex-json = "0.1.0"
  simplex-http = { version = "0.2.0", features = ["tls"] }
  local-lib = { path = "../local-lib" }
  git-lib = { git = "https://github.com/user/lib" }

  [dev-dependencies]
  simplex-test = "0.1.0"

  [build]
  entry = "src/lib.sx"  # or "src/main.sx" for binaries

  [features]
  default = ["std"]
  std = []
  async = ["simplex-async"]
  ```

- [ ] 1.2 Implement TOML parser (or use JSON initially)
  - [ ] Basic TOML parsing
  - [ ] Or: Use `simplex.json` as simpler alternative initially

- [ ] 1.3 Manifest validation
  - [ ] Required fields check
  - [ ] Version format validation (semver)
  - [ ] Dependency format validation

---

## 2. Cursus Integration

**File**: `cursus.sx` / `cursus` binary
**Status**: Binary exists but disconnected from compiler

### Subtasks

- [ ] 2.1 Cursus command structure
  ```bash
  cursus new <name>           # Create new package
  cursus init                 # Initialize in existing directory
  cursus build                # Build current package
  cursus run                  # Build and run
  cursus test                 # Run tests
  cursus check                # Type check without building
  cursus clean                # Remove build artifacts
  cursus doc                  # Generate documentation
  cursus add <package>        # Add dependency
  cursus remove <package>     # Remove dependency
  cursus update               # Update dependencies
  cursus publish              # Publish to registry
  cursus search <query>       # Search registry
  cursus install <package>    # Install binary package
  ```

- [ ] 2.2 `cursus new` implementation
  - [ ] Create directory structure:
    ```
    my-package/
    ├── simplex.toml
    ├── src/
    │   └── main.sx (or lib.sx)
    ├── tests/
    │   └── test_main.sx
    └── .gitignore
    ```
  - [ ] Template for library vs binary

- [ ] 2.3 `cursus build` implementation
  - [ ] Read simplex.toml
  - [ ] Resolve dependencies
  - [ ] Compile in dependency order
  - [ ] Link final binary
  - [ ] Cache compiled artifacts

- [ ] 2.4 `cursus run` implementation
  - [ ] Build + execute
  - [ ] Pass arguments to program

- [ ] 2.5 `cursus test` implementation
  - [ ] Find test files (tests/*.sx or #[test] functions)
  - [ ] Compile and run tests
  - [ ] Report results

---

## 3. Dependency Resolution

**File**: `sxpm.sx` resolver module
**Status**: Partial - Basic linear resolution exists, needs DAG

### Subtasks

- [ ] 3.1 Dependency Graph Construction (CRITICAL)
  - [x] Parse all simplex.json files
  - [ ] Build directed acyclic graph (DAG) data structure
    - Node: package name + version
    - Edge: dependency relationship
  - [ ] Topological sort for correct build order
  - [ ] Cycle detection algorithm (DFS-based)
    - Detect direct cycles (A -> B -> A)
    - Detect indirect cycles (A -> B -> C -> A)
    - Report cycle path in error message
  - [ ] Handle diamond dependencies (A -> B, A -> C, B -> D, C -> D)

- [ ] 3.2 Version resolution
  - [x] Semver parsing and comparison
  - [ ] Version range support: `^1.0`, `~1.0`, `>=1.0,<2.0`
  - [ ] Conflict detection (same package, incompatible versions)
  - [ ] Version unification (prefer highest compatible)

- [ ] 3.3 Lock file (CRITICAL)
  - [ ] `simplex.lock` JSON format:
    ```json
    {
      "version": 1,
      "packages": {
        "package-name": {
          "version": "1.2.3",
          "source": "registry|git|path",
          "checksum": "sha256:...",
          "dependencies": ["dep1", "dep2"]
        }
      }
    }
    ```
  - [ ] Generate lock file on first build
  - [ ] Use lock file for reproducible builds
  - [ ] Update lock file with `sxpm update`

- [ ] 3.4 Dependency sources
  - [x] Local paths (`{ path = "../lib" }`)
  - [x] Git repositories (`{ git = "url" }`)
  - [ ] Registry packages (version strings)
  - [ ] Source download and caching in `.simplex/deps/`

---

## 4. Package Registry

**File**: New registry service or static hosting
**Status**: Not implemented

### Subtasks

- [ ] 4.1 Registry API design
  ```
  GET  /api/v1/packages                    # List packages
  GET  /api/v1/packages/<name>             # Get package info
  GET  /api/v1/packages/<name>/<version>   # Get specific version
  GET  /api/v1/packages/<name>/download    # Download tarball
  POST /api/v1/packages                    # Publish (authenticated)
  GET  /api/v1/search?q=<query>            # Search
  ```

- [ ] 4.2 Option A: Static file registry (simpler)
  - [ ] GitHub-based: packages stored as releases
  - [ ] Index file listing all packages
  - [ ] No server needed initially

- [ ] 4.3 Option B: Full registry server
  - [ ] Written in Simplex (dogfooding)
  - [ ] SQLite backend
  - [ ] API key authentication for publishing
  - [ ] Package validation on upload

- [ ] 4.4 Package format
  - [ ] Tarball structure
  - [ ] Checksum verification
  - [ ] Signature verification (optional)

- [ ] 4.5 `cursus publish` implementation
  - [ ] Package tarball creation
  - [ ] Upload to registry
  - [ ] Version validation

- [ ] 4.6 `cursus add` implementation
  - [ ] Search registry
  - [ ] Download and extract
  - [ ] Update simplex.toml
  - [ ] Update lock file

---

## 5. Module System Enhancement (CRITICAL)

**File**: Parser (`parser.sx`), codegen (`stage0.py`), module resolver
**Status**: Partial - `use` keyword parsed but not resolved

### Subtasks

- [ ] 5.1 Module Path Resolution (CRITICAL)
  - [ ] Parse `use package::module::item` syntax
  - [ ] Resolve module paths to actual files:
    - `use foo::bar` → look for `foo/bar.sx` or `foo/bar/mod.sx`
    - `use foo::bar::Baz` → import `Baz` from `foo/bar.sx`
  - [ ] Relative paths: `use super::sibling`
  - [ ] Self reference: `use self::submodule`
  - [ ] Track imported symbols in scope

- [ ] 5.2 Module Declarations
  - [ ] `mod foo;` - load external file (`foo.sx` or `foo/mod.sx`)
  - [ ] `mod foo { ... }` - inline module definition
  - [ ] Nested modules: `mod outer { mod inner { ... } }`

- [ ] 5.3 Visibility modifiers
  - [ ] `pub fn` - public function
  - [ ] `pub struct` - public struct
  - [ ] `pub enum` - public enum
  - [ ] Default: private to current module
  - [ ] `pub(modulus)` - visible within modulus only (optional)

- [ ] 5.4 Re-exports
  - [ ] `pub use other::Thing` - re-export single item
  - [ ] `pub use other::*` - re-export all public items
  - [ ] `pub use other::Thing as Alias` - re-export with rename

- [ ] 5.5 Import Resolution in Codegen
  - [ ] Build module dependency graph
  - [ ] Compile modules in dependency order
  - [ ] Merge LLVM IR from multiple modules
  - [ ] Handle cross-module function calls
  - [ ] Handle cross-module struct/enum usage

- [ ] 5.6 Prelude (auto-imported)
  - [ ] Vec, String (already available)
  - [ ] Option, Result (once implemented)
  - [ ] HashMap, HashSet
  - [ ] print, println, exit

---

## 6. Build Infrastructure

**File**: Cursus build system + merge.sx
**Status**: Merge tool exists but needs optimization

### Subtasks

- [ ] 6.0 Optimize LLVM IR merge tool (merge.sx)
  - [ ] Profile and identify performance bottlenecks
  - [ ] Use streaming I/O instead of loading entire files
  - [ ] Optimize string constant deduplication algorithm
  - [ ] Consider hash-based duplicate detection
  - [ ] Target: <5 seconds for merging 4 compiler modules (~50k lines total)

- [ ] 6.1 Artifact caching
  - [ ] Cache directory structure
  - [ ] Hash-based invalidation
  - [ ] Dependency tracking

- [ ] 6.2 Incremental compilation
  - [ ] Track file modification times
  - [ ] Recompile only changed modules
  - [ ] Link incrementally if possible

- [ ] 6.3 Parallel builds
  - [ ] Build independent modules in parallel
  - [ ] Respect dependency order

---

## 7. Documentation Integration

**File**: sxdoc integration with cursus
**Status**: sxdoc exists separately

### Subtasks

- [ ] 7.1 `cursus doc` command
  - [ ] Invoke sxdoc on package
  - [ ] Output to `target/doc/`
  - [ ] Include dependencies optionally

- [ ] 7.2 README integration
  - [ ] Include README.md in package docs
  - [ ] Example extraction from doc comments

- [ ] 7.3 docs.sx hosting (future)
  - [ ] Auto-publish docs for registry packages
  - [ ] Versioned documentation

---

## 8. Standard Package Structure

**Specification**
**Status**: Not defined

### Subtasks

- [ ] 8.1 Define standard layout
  ```
  my-package/
  ├── simplex.toml          # Package manifest
  ├── simplex.lock          # Lock file (generated)
  ├── README.md             # Package documentation
  ├── LICENSE               # License file
  ├── src/
  │   ├── lib.sx            # Library entry (if library)
  │   ├── main.sx           # Binary entry (if binary)
  │   └── *.sx              # Other modules
  ├── tests/
  │   └── *.sx              # Integration tests
  ├── examples/
  │   └── *.sx              # Example programs
  ├── benches/
  │   └── *.sx              # Benchmarks
  └── target/               # Build output (gitignored)
      ├── debug/
      └── release/
  ```

- [ ] 8.2 Conventions
  - [ ] `lib.sx` vs `main.sx` detection
  - [ ] Test file naming: `test_*.sx` or `*_test.sx`
  - [ ] Example naming conventions

---

## 9. File I/O Operations (NEW)

**File**: `standalone_runtime.c` + Simplex intrinsics
**Status**: Partial - Basic read/write exists, needs enhancement

### Subtasks

- [ ] 9.1 File Handle Operations
  - [ ] `file_open(path, mode)` - Open file with mode ("r", "w", "a", "rb", "wb")
  - [ ] `file_close(handle)` - Close file handle
  - [ ] `file_read_bytes(handle, n)` - Read n bytes
  - [ ] `file_write_bytes(handle, bytes)` - Write bytes
  - [ ] `file_read_line(handle)` - Read single line
  - [ ] `file_read_all(handle)` - Read entire file
  - [ ] `file_seek(handle, pos)` - Seek to position
  - [ ] `file_tell(handle)` - Get current position
  - [ ] `file_flush(handle)` - Flush buffers

- [ ] 9.2 Path Operations
  - [ ] `path_exists(path)` - Check if path exists
  - [ ] `path_is_file(path)` - Check if regular file
  - [ ] `path_is_dir(path)` - Check if directory
  - [ ] `path_join(a, b)` - Join path components
  - [ ] `path_parent(path)` - Get parent directory
  - [ ] `path_filename(path)` - Get filename component
  - [ ] `path_extension(path)` - Get file extension
  - [ ] `path_absolute(path)` - Get absolute path

- [ ] 9.3 Directory Operations
  - [ ] `dir_list(path)` - List directory contents (Vec<String>)
  - [ ] `dir_create(path)` - Create directory
  - [ ] `dir_create_all(path)` - Create directory recursively (mkdir -p)
  - [ ] `dir_remove(path)` - Remove empty directory
  - [ ] `dir_remove_all(path)` - Remove directory recursively

- [ ] 9.4 File Metadata
  - [ ] `file_size(path)` - Get file size in bytes
  - [ ] `file_modified(path)` - Get modification timestamp
  - [ ] `file_copy(src, dst)` - Copy file
  - [ ] `file_rename(src, dst)` - Rename/move file
  - [ ] `file_remove(path)` - Delete file

- [ ] 9.5 Standard I/O
  - [ ] `stdin_read_line()` - Read line from stdin
  - [ ] `stdin_read_all()` - Read all from stdin
  - [ ] `stdout_write(s)` - Write to stdout (already have print)
  - [ ] `stderr_write(s)` - Write to stderr

---

## Completion Criteria

Phase 2 is complete when:
- [ ] File I/O operations work reliably
- [ ] Dependency graph properly detects cycles
- [ ] Lock file ensures reproducible builds
- [ ] Module system (`use`, `mod`, `pub`) works
- [ ] `sxpm new` creates a valid package
- [ ] `sxpm build` compiles packages with dependencies
- [ ] `sxpm add <package>` fetches and installs dependencies
- [ ] Integration tests pass

---

## Dependencies

- Phase 1: JSON parsing for manifests (COMPLETE)
- Phase 1: HashMap for dependency graph (COMPLETE)
- Phase 1: String operations (COMPLETE)

## Dependents

- Phase 3: Libraries will be published as packages
- Phase 4: AI/Actor libraries use `use` imports
