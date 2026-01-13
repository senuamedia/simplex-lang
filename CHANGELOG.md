# Changelog

All notable changes to the Simplex language and toolchain will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.9.2] - 2026-01-14

### Bug Fixes

- **Cross-Platform Compilation**: Fixed GitHub issue #56 - LLVM target triple was hardcoded to `x86_64-apple-macosx14.0.0`, causing compilation failures on Linux systems. The compiler now dynamically detects the host platform and generates the appropriate target triple:
  - macOS: `x86_64-apple-macosx14.0.0`
  - Linux: `x86_64-unknown-linux-gnu`
  - Windows: `x86_64-pc-windows-msvc`

### Internal

- Added `get_os_name()` and `get_target_triple()` platform detection functions to codegen
- Added `intrinsic_getenv` declaration to Python bootstrap for platform detection
- Updated all toolchain binaries to version 0.9.2

## [0.9.1] - 2026-01-12

### Bug Fixes

- **Parser**: Fixed optional semicolons for `let`, `return`, `var`, `break`, and `continue` statements. Statements no longer require trailing semicolons in block contexts, enabling more concise single-line syntax.

- **Codegen**: Fixed boolean NOT operator to use `xor i64` instead of `xor i1`, resolving type mismatches in boolean expressions.

### Enhancements

- **JSON Support**: Added missing JSON function handlers in the codegen:
  - `json_string_sx` - Create JSON string from Simplex string
  - `json_array_len` - Get length of JSON array
  - `json_object_len` - Get number of keys in JSON object
  - `json_as_i64` - Extract integer from JSON value
  - `json_get_index` - Get array element by index
  - `json_is_null` - Check if JSON value is null
  - `json_object_key_at` - Get object key by index
  - `json_object_value_at` - Get object value by index
  - `json_object_set_sx` - Set object key with Simplex string

- **Runtime**: Added `print_i64()` function for direct integer printing without string conversion.

### Build System

- **Merge Tool**: Added `merge_ll.py` utility for merging LLVM IR files with proper string constant handling and deduplication.

### Internal

- Improved LLVM IR generation for external function declarations
- Better handling of string constant label renaming during IR merge

## [0.9.0] - 2026-01-07

### Initial Release

- Self-hosted Simplex compiler (`sxc`)
- Package manager (`sxpm`)
- Documentation generator (`sxdoc`)
- Language Server Protocol support (`sxlsp`)
- Build system (`cursus`)
- Comprehensive runtime with:
  - String handling and manipulation
  - Vector/array operations
  - HashMap and HashSet collections
  - JSON parsing and serialization
  - File I/O operations
  - HTTP client capabilities
  - SQLite database support
  - Cryptographic functions (SHA-256, HMAC)
  - Arena allocator for performance
  - StringBuilder for efficient string building

---

[0.9.2]: https://github.com/senuamedia/simplex-lang/compare/v0.9.1...v0.9.2
[0.9.1]: https://github.com/senuamedia/simplex-lang/compare/v0.9.0...v0.9.1
[0.9.0]: https://github.com/senuamedia/simplex-lang/releases/tag/v0.9.0
