# Simplex Tutorial

Welcome to the Simplex programming language tutorial. This series will take you from your first line of code to building distributed, AI-powered applications.

---

## Who Is This For?

This tutorial assumes you have some programming experience in any language. You don't need to know Rust, Erlang, or any specific language—we'll explain everything from the ground up.

If you've written code in Python, JavaScript, Java, Go, or similar languages, you'll feel right at home.

---

## What You'll Learn

By the end of this tutorial, you'll understand:

- How to write and run Simplex programs
- The type system and how it keeps your code safe
- Functions, closures, and control flow
- Actors: Simplex's core abstraction for building reliable systems
- Message passing and concurrent programming
- Fault tolerance through supervision
- AI integration as a first-class feature
- Building and deploying a complete application

---

## Tutorial Path

### Part 1: Foundations

| Chapter | Topic | What You'll Learn |
|---------|-------|-------------------|
| [01](01-variables-and-types.md) | Variables and Types | Declaring variables, primitive types, type inference |
| [02](02-functions.md) | Functions | Defining functions, parameters, return values, closures |
| [03](03-control-flow.md) | Control Flow | Conditionals, loops, pattern matching |
| [04](04-collections.md) | Collections | Lists, maps, sets, iteration |
| [05](05-custom-types.md) | Custom Types | Structs, enums, type aliases |
| [06](06-error-handling.md) | Error Handling | Option, Result, the ? operator |

### Part 2: Actors and Concurrency

| Chapter | Topic | What You'll Learn |
|---------|-------|-------------------|
| [07](07-actors.md) | Introduction to Actors | What actors are, spawning, state |
| [08](08-messages.md) | Message Passing | Sending messages, ask vs send, patterns |
| [09](09-supervision.md) | Supervision and Fault Tolerance | Supervisors, restart strategies, "let it crash" |

### Part 3: AI Integration

| Chapter | Topic | What You'll Learn |
|---------|-------|-------------------|
| [10](10-ai-basics.md) | AI Integration | Completions, embeddings, classification, extraction |
| [12](12-cognitive-hives.md) | Cognitive Hives | Building SLM swarms, specialists, routing |

### Part 4: Building Real Applications

| Chapter | Topic | What You'll Learn |
|---------|-------|-------------------|
| [11](11-capstone.md) | Capstone Project | Build a complete AI-powered document processor |

### Part 5: Advanced Topics (v0.11.0)

| Chapter | Topic | What You'll Learn |
|---------|-------|-------------------|
| [13](13-dual-numbers.md) | Dual Numbers | Automatic differentiation, exact derivatives, gradients |
| [14](14-self-learning-annealing.md) | Self-Learning Annealing | Meta-gradients, adaptive temperature schedules, neurosymbolic transition |

---

## How to Use This Tutorial

1. **Read in order**: Each chapter builds on previous ones
2. **Type the code**: Don't just read—type each example yourself
3. **Experiment**: Modify the examples and see what happens
4. **Check your understanding**: Try the exercises at the end of each chapter

---

## Setting Up

Before starting, install Simplex:

```bash
# macOS
brew install simplex

# Linux
curl -fsSL https://simplex-lang.org/install.sh | sh

# Verify installation
simplex --version
```

Create a directory for your tutorial work:

```bash
mkdir simplex-tutorial
cd simplex-tutorial
```

You're ready to begin. Let's start with [Chapter 1: Variables and Types](01-variables-and-types.md).

---

## Quick Reference

As you work through the tutorials, you might find these helpful:

- [Language Syntax Reference](../spec/04-language-syntax.md)
- [AI Integration Reference](../spec/07-ai-integration.md)
- [Cognitive Hive AI Specification](../spec/09-cognitive-hive.md)
- [Neural IR and Differentiable Execution](../spec/14-neural-ir.md)
- [Real-Time Learning](../spec/15-real-time-learning.md)
- [Complete Example](../examples/document-pipeline.md)

---

## Developer Tools (v0.11.0)

Simplex 0.11.0 includes a comprehensive developer toolchain:

| Tool | Command | Purpose |
|------|---------|---------|
| Formatter | `sxfmt` | Automatically format code to follow style guidelines |
| Linter | `sxlint` | Static analysis to catch bugs and code smells |
| Benchmarking | `sxc bench` | Measure and track performance |
| Coverage | `sxc test --coverage` | Understand test coverage |
| Error Help | `sxc explain` | Get detailed explanations for compiler errors |

Quick start:

```bash
sxfmt .                      # Format all files
sxlint .                     # Lint all files
sxc test --coverage          # Run tests with coverage
sxc explain E0423            # Explain an error code
```

For a complete guide, see [Chapter 11: Developer Tools](11-capstone.md#developer-tools-v0100).

---

## Getting Help

If you get stuck:

```bash
simplex help              # Built-in help
simplex help <command>    # Help for specific command
sxc explain <error-code>  # Explain compiler errors (v0.11.0)
```

---

*Let's begin: [Chapter 1: Variables and Types →](01-variables-and-types.md)*
