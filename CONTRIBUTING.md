# Contributing to Simplex

Thank you for your interest in contributing to Simplex!

## Getting Started

1. Fork the repository
2. Clone your fork
3. Create a feature branch
4. Make your changes
5. Submit a pull request

## Development Setup

### Prerequisites

- LLVM/Clang toolchain
- A C compiler (gcc or clang)

### Building

```bash
# Build the runtime
cd runtime
clang -c -O2 standalone_runtime.c -o standalone_runtime.o

# The compiler is self-hosted - see docs for bootstrap process
```

## Project Structure

| Directory | Purpose |
|-----------|---------|
| `compiler/bootstrap/` | Self-hosted compiler — lexer, parser, codegen, error handling, main entry |
| `runtime/` | Standalone C runtime (`standalone_runtime.c`) |
| `lib/` | Shared libraries — platform, version, safety, strings, llm, ast_defs |
| `simplex-std/` | Standard library |
| `simplex-learning/` | ML/AI learning framework — dual numbers, tensors, beliefs, epistemics |
| `simplex-training/` | Training pipeline — neural gates, LoRA, schedules, data generators |
| `simplex-inference/` | High-performance SLM inference via native bindings |
| `simplex-nexus/` | Nexus protocol for hive communication |
| `simplex-edge-hive/` | Edge deployment framework |
| `simplex-quantum/` | Quantum computing framework |
| `tools/` | Developer tools — sxc, sxpm, sxdoc, sxlsp, sxfmt, sxlint, cursus |
| `tests/` | Test suite (unit, integration, language, toolchain) |
| `simplex-docs/` | Specification, tutorials, and API docs |

## Key Constraint

ALL code must be pure Simplex. No external tools, libraries, or dependencies. This is a core project principle — the language and its ecosystem are entirely self-contained.

## Code Style

- Run `sxfmt` to format your code before committing
- Use 4-space indentation
- Keep lines under 100 characters
- Follow existing naming conventions and patterns in nearby files
- Add comments for non-obvious code

## Running Tests

```bash
./tests/run_tests.sh
```

Tests are organized by category under `tests/`:

- `basics/` — closures, try operator, core language features
- `language/` — functions, modules, traits, types, generics
- `types/` — type system (generics, aliases, associated types)
- `actors/` — actor model
- `async/` — async/await
- `learning/` — ML framework (dual tensors, etc.)
- `integration/` — end-to-end programs
- `toolchain/` — compiler, sxpm, verification

Before submitting a PR:

1. Ensure all existing tests pass
2. Add tests for new features
3. Test on your local machine

## Pull Request Guidelines

- One feature/fix per PR
- Clear description of changes
- Reference any related issues
- Keep commits focused and atomic

## Reporting Issues

- Check existing issues first
- Include reproduction steps
- Provide system information
- Include error messages if applicable

## License

By contributing, you agree that your contributions will be licensed under the project's license.
