# Simplex v0.11.0 Release Notes

**Release Date:** 2026-01-19
**Codename:** Module System

---

## Overview

Simplex v0.11.0 is a **language foundation release** delivering a complete cross-module import system. This release enables multi-file Simplex projects with proper function declarations across module boundaries.

---

## New Features

### Cross-Module Function Imports

The `use` statement now properly imports functions from other compiled modules.

```simplex
// mathlib.sx - a utility module
fn add(a: i64, b: i64) -> i64 {
    a + b
}

fn multiply(a: i64, b: i64) -> i64 {
    a * b
}
```

```simplex
// main.sx - using the module
use mathlib;

fn main() -> i64 {
    let result = add(10, multiply(3, 4));  // Uses imported functions
    result  // Returns 22
}
```

**How It Works:**
1. Compile modules first: `sxc mathlib.sx -o mathlib.ll`
2. The compiler parses `mathlib.ll` to extract function signatures
3. LLVM `declare` statements are auto-generated for imported functions
4. Link all `.ll` files together: `llc + clang mathlib.ll main.ll ...`

### Automatic Declaration Generation

When the compiler encounters `use module;`, it:

1. Looks for `module.ll` in the same directory as the source file
2. Parses `define` statements to extract function signatures
3. Generates corresponding `declare` statements in the output

```llvm
; Imported from mathlib
declare i64 @add(i64, i64)
declare i64 @multiply(i64, i64)
```

### Multi-File Project Support

Building multi-file projects is now straightforward:

```bash
# Compile each module
sxc lib/utils.sx -o build/utils.ll
sxc lib/math.sx -o build/math.ll
sxc src/main.sx -o build/main.ll

# Link together
llc -filetype=obj build/*.ll
clang build/*.o runtime/standalone_runtime.c -o myapp -lm
```

---

## Compiler Improvements

### Source Directory Awareness

The compiler now tracks the source file's directory to resolve module paths correctly.

```simplex
// In /project/src/main.sx
use utils;  // Looks for /project/src/utils.ll
```

### LL File Parsing

New internal function `parse_ll_file_for_declarations()`:
- Reads compiled LLVM IR files
- Extracts function definitions (`define` statements)
- Converts to external declarations (`declare` statements)

---

## Tool Updates

All tools updated to version 0.11.0:

| Tool | Version | Changes |
|------|---------|---------|
| **sxc** | 0.11.0 | Module import system |
| **sxpm** | 0.11.0 | - |
| **cursus** | 0.11.0 | - |
| **sxdoc** | 0.11.0 | - |
| **sxlsp** | 0.11.0 | - |
| **sxfmt** | 0.11.0 | - |
| **sxlint** | 0.11.0 | - |

---

## Edge Hive & Nexus Updates

Both the Edge Hive framework and Nexus Protocol have been updated to 0.11.0:

- **edge-hive**: Local AI inference, federation, security modules
- **nexus**: High-frequency hive communication with bit-packed delta streams

---

## Test Results

### Passing Tests

| Test Category | Tests | Status |
|---------------|-------|--------|
| Modules | spec_import, spec_mathlib | PASS |
| Traits | spec_impl_trait, spec_trait_ref, spec_trait_self_ref | PASS |
| Basics | enums, for loops, match, try operator | PASS |
| Types | All type tests | PASS |
| Functions | All function tests | PASS |
| Control | All control flow tests | PASS |
| Closures | All closure tests | PASS |
| Actors | All actor tests | PASS |
| Async | All async tests | PASS |

### Known Issues

| Test | Issue | Status |
|------|-------|--------|
| spec_assoc_types.sx | `&mut self` syntax not supported | Planned for 0.12.0 |

---

## Breaking Changes

None. This release is fully backwards compatible with 0.10.x.

---

## Upgrade Guide

1. **Update version imports** (if using centralized version):
   ```simplex
   use simplex_core::version;
   // Now returns "0.11.0"
   ```

2. **Use the new module system**:
   ```simplex
   // Before: manual extern declarations
   extern fn add(a: i64, b: i64) -> i64;

   // After: automatic via use statement
   use mathlib;  // Imports add, multiply, etc.
   ```

3. **Build multi-file projects**:
   ```bash
   # Compile modules in dependency order
   sxc base.sx -o base.ll
   sxc utils.sx -o utils.ll    # Can use base
   sxc main.sx -o main.ll      # Can use base, utils

   # Link all together
   ```

---

## Compatibility

| Component | Minimum Version | Maximum Version |
|-----------|-----------------|-----------------|
| LLVM | 14.0.0 | - |
| Previous Simplex | 0.8.0 | 0.11.0 |

---

## What's Next (v0.12.0)

### v0.12.0: Reference Types
- `&mut self` syntax for mutable references in methods
- `&mut T` parameter types
- Improved error recovery in parser

### v1.0.0: Production Release
- All compiler features complete
- Full test suite passing
- Production-ready stability

---

## Files Changed

| File/Directory | Change |
|----------------|--------|
| `compiler/bootstrap/codegen.sx` | Module import parsing and declaration generation |
| `compiler/bootstrap/main.sx` | Source directory tracking |
| `runtime/standalone_runtime.c` | Added `print_i64` function |
| `tests/run_tests.sh` | Multi-module test linking |
| `simplex-core/src/version.sx` | Version 0.11.0 |
| All tools | Version bump to 0.11.0 |

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
# sxc 0.11.0
```
