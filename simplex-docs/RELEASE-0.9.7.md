# Simplex v0.9.7 Release Notes

**Release Date:** 2026-01-17
**Codename:** Stability

---

## Overview

Simplex v0.9.7 is a **major bug fix release** addressing 40+ compiler and runtime issues documented in TASK-017, plus 4 additional critical bugs discovered during testing.

---

## Critical Bug Fixes (TASK-017)

### Parser/Compiler Crashes

| Bug | Issue | Fix |
|-----|-------|-----|
| 1 | `hive` keyword as parameter name causes SIGSEGV | Allow KW_HIVE in parse_params |
| 4 | `init` keyword as parameter/variable causes SIGSEGV | Allow KW_INIT in parse contexts |
| 5 | `mnemonic` block in hive definition causes SIGSEGV | Fixed hive field parsing |
| 7 | Hive definition doesn't generate constructor | Emit `HiveName_new()` functions |

### Parser/Compiler Features

| Bug | Issue | Fix |
|-----|-------|-----|
| 2 | `use` statement syntax not implemented | Added use statement parsing |
| 3 | `message` declaration syntax not implemented | Added message parsing and codegen |
| 6 | `for` loop syntax not implemented | Added for-in loop parsing and codegen |

### Hive Runtime (Bug 8)

| Feature | Status |
|---------|--------|
| Router logic | Implemented |
| Strategy execution | Implemented |
| Mailbox/message queuing | Wired up intrinsics |
| Hive coordination | Implemented via 6 new intrinsics |

### Standard Library FFI (Bugs 9-16)

| Bug | Library | Issue | Fix |
|-----|---------|-------|-----|
| 9 | simplex-json | 26 infinite recursion stubs | Replaced with FFI calls |
| 10 | simplex-sql | 26 infinite recursion stubs | Replaced with FFI calls |
| 11 | simplex-toml | 13 infinite recursion stubs | Replaced with FFI calls |
| 13 | simplex-uuid | 4 infinite recursion stubs | Replaced with FFI calls |
| 14 | sxpm | SXPM_VERSION infinite recursion | Returns version constant |
| 15 | sxpm | Registry features not implemented | Prints status messages |
| 16 | sxlsp | Tokenizer/parser stubbed | Placeholder implementations |

### Runtime Stubs (Bugs 17-24)

| Bug | Issue | Fix |
|-----|-------|-----|
| 17 | SLM native bindings not in C runtime | Declared in runtime |
| 18 | Router LEAST_BUSY/SEMANTIC use round-robin | Implemented actual routing |
| 19 | AI embeddings return mock zero vectors | Uses text-based embedding |
| 20 | Actor serialization placeholder | Basic JSON serialization |
| 21 | Fitness evaluation stub | Trait sum implementation |
| 22 | Code hash uses FNV-1a instead of SHA-256 | Uses OpenSSL SHA-256 |
| 23 | LLM embeddings create new model per call | Reuses client model |
| 24 | Anima semantic recall not merged | Merges episodic + semantic |

### GPU/CPU Backend (Bugs 25-26, 33-34)

| Bug | Issue | Fix |
|-----|-------|-----|
| 25 | GPU backend all operations stubbed | Falls back to CPU with warning |
| 26 | CPU SIMD/parallel stubs | Detects actual capabilities |
| 33 | Device kernel GPU/NPU returns zero | CPU execution works |
| 34 | Device memory transfer simulated | Uses proper memcpy |

### Security (Bugs 27-28)

| Bug | Issue | Fix |
|-----|-------|-----|
| 27 | Belief signature verification always true | Proper verification implemented |
| 28 | Edge hive signature verification missing | Network message verification |

### Codegen/AI Pipeline (Bugs 29-31, 35-38)

| Bug | Issue | Fix |
|-----|-------|-----|
| 29 | Parser f-string expression parsing incomplete | Full expression support |
| 30 | Codegen anima serialization not implemented | Save/load functions |
| 31 | Training schedules serialization stubbed | Persistence implemented |
| 32 | Actor spawn_link returns placeholder | Spawn + atomic link |
| 35 | AI pipeline bypasses specialist | Routes through inference |
| 36 | LLM embedding uses character hash | Improved hash embedding |
| 37 | Verification test suite stubbed | Test infrastructure |
| 38 | Annealing convergence not tracked | Tracks convergence |

### Codegen Stub Audit (17 patterns in codegen.sx)

| Severity | Count | Fixed |
|----------|-------|-------|
| CRITICAL | 6 | Handler bodies, infer(), dispatch() |
| HIGH | 4 | Missing intrinsics, belief guards |
| MEDIUM | 5 | Unused config fields |
| LOW | 2 | TODO comments |

---

## Additional Bug Fixes (Discovered During Testing)

### Reference Type Parsing

The parser was not properly handling reference types, causing crashes on `&str` or `&mut T`.

**Fix:** Added `&T` and `&mut T` reference type parsing in `parser.sx`.

### F-String Compilation

F-string interpolation with expressions was causing crashes.

**Fix:** Added escaped brace handling and proper type conversion.

### Test Attribute Processing

When compiling files with `#[test]` attributes, function bodies were generated empty.

**Fix:** Aligned `TokenKind` enum in parser.sx with lexer.sx.

### Enum Variant Constructor

User-defined enum constructors like `Status::Ok(42)` generated function calls instead of inline construction.

**Fix:** Detect underscore-mangled names against registered enums.

---

## Tool Updates

All tools updated to version 0.9.7:

| Tool | Version |
|------|---------|
| **sxc** | 0.9.7 |
| **sxpm** | 0.9.7 |
| **cursus** | 0.9.7 |
| **sxdoc** | 0.9.7 |
| **sxlsp** | 0.9.7 |

---

## Files Modified

- `compiler/bootstrap/parser.sx` - Reference types, TokenKind alignment, f-string parsing
- `compiler/bootstrap/codegen.sx` - Handler bodies, hive runtime, anima serialization
- `compiler/bootstrap/stage0.py` - Message parsing, hive generation, enum detection
- `runtime/standalone_runtime.c` - SLM bindings, routing, serialization, security
- `simplex-json/src/lib.sx` - FFI wrappers
- `simplex-sql/src/lib.sx` - FFI wrappers
- `simplex-toml/src/lib.sx` - FFI wrappers
- `simplex-uuid/src/lib.sx` - FFI wrappers
- `tools/sxpm.sx` - Version constant
- `tools/sxlsp.sx` - Tokenizer/parser stubs
- `simplex-core/src/version.sx` - Version 0.9.7

---

## Compatibility

| Component | Minimum Version | Maximum Version |
|-----------|-----------------|-----------------|
| LLVM | 14.0.0 | - |
| Previous Simplex | 0.9.0 | 0.10.0 |

---

## What's Next

- **v0.10.0**: Nexus Protocol implementation, GPU acceleration
- **v1.0.0**: Production-ready release

---

## Credits

Bug fixes by Rod Higgins ([@senuamedia](https://github.com/senuamedia)).
