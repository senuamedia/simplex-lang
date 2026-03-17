# Simplex Self-Hosted Compiler - Implementation Gaps

## Overview

Analysis of gaps between the Simplex language specification and the current self-hosted compiler implementation.

**Current Status:**
- Parsing: ~50% complete
- Code Generation: ~25% complete
- Runtime: ~10% complete

---

## Priority 1: Core Language Features (Must Have)

### 1.1 Tuple Types and Patterns
**Status: 0% implemented**

Missing:
- [ ] Tuple literal syntax `(a, b, c)`
- [ ] Tuple type annotation `(i64, String, bool)`
- [ ] Tuple destructuring in let: `let (x, y) = point`
- [ ] Tuple patterns in match
- [ ] Tuple field access `.0`, `.1`, etc.

Files to modify:
- `lexer.sx`: No changes needed (parentheses already tokenized)
- `parser.sx`: Add `parse_tuple()`, modify `parse_primary()`, `parse_pattern()`
- `codegen.sx`: Add `gen_tuple()`, tuple layout (sequential fields)

### 1.2 Struct Patterns in Match
**Status: 0% implemented**

Missing:
- [ ] Struct destructuring: `User { name, age }`
- [ ] Struct patterns with field aliases: `User { name: n, .. }`
- [ ] Rest pattern in structs: `User { name, .. }`

Files to modify:
- `parser.sx`: Extend `parse_pattern()` for struct syntax
- `codegen.sx`: Add struct pattern matching in `gen_match()`

### 1.3 For Loop Iterator Support
**Status: 20% (range-only)**

Missing:
- [ ] Iterator-based for: `for item in collection`
- [ ] Enumerate pattern: `for (i, item) in items.enumerate()`
- [ ] Range variants: `0..=10` (inclusive), `0..`, `..10`

Files to modify:
- `parser.sx`: Extend `parse_for()` for iterator patterns
- `codegen.sx`: Add iterator protocol codegen

### 1.4 While-Let and If-Let Guards
**Status: If-let 80%, While-let 0%**

Missing:
- [ ] `while let Some(x) = iter.next() { ... }`
- [ ] Pattern guards in if-let: `if let Some(x) = opt && x > 0`

Files to modify:
- `parser.sx`: Add `parse_while_let()`
- `codegen.sx`: Add while-let loop codegen

### 1.5 Loop Expression with Break Value
**Status: 0% implemented**

Missing:
- [ ] `loop { ... break value }` syntax
- [ ] Loop as expression returning value

Files to modify:
- `lexer.sx`: Add `KW_LOOP` keyword
- `parser.sx`: Add `parse_loop()`
- `codegen.sx`: Add loop codegen with break value handling

### 1.6 Try Operator (?)
**Status: 20% (parsed, not codegen)**

Missing:
- [ ] Early return on Err variant
- [ ] Proper Result/Option unwrapping
- [ ] Error propagation semantics

Files to modify:
- `codegen.sx`: Implement `gen_try()` with proper control flow

---

## Priority 2: Type System Features

### 2.1 Type Aliases
**Status: 30% (parsed, not expanded)**

Missing:
- [ ] Type alias expansion in type checking
- [ ] Recursive type alias resolution
- [ ] Generic type aliases: `type IntVec = Vec<i64>`

Files to modify:
- `codegen.sx`: Complete `cg_resolve_type_alias()`

### 2.2 Trait Bounds
**Status: 10%**

Missing:
- [ ] Multiple bounds: `T: Clone + Display`
- [ ] Where clauses: `where T: Clone`
- [ ] Associated type constraints: `Iterator<Item = i64>`

Files to modify:
- `parser.sx`: Extend `parse_type()` for bounds syntax
- `codegen.sx`: Add constraint validation

### 2.3 Associated Types
**Status: 30% (AST only)**

Missing:
- [ ] Associated type definitions in traits
- [ ] Associated type implementation in impl blocks
- [ ] Associated type resolution in generic code

Files to modify:
- `codegen.sx`: Complete associated type codegen

### 2.4 Generic Methods
**Status: 20%**

Missing:
- [ ] Methods with own type parameters: `fn foo<U>(&self, x: U)`
- [ ] Generic method instantiation

Files to modify:
- `codegen.sx`: Add method-level generics support

---

## Priority 3: Closure and Function Features

### 3.1 Closure Capture
**Status: 30% (parsing only)**

Missing:
- [ ] Capture analysis (identify captured variables)
- [ ] Closure struct generation (environment)
- [ ] Move vs borrow capture modes
- [ ] Closure trait implementation (Fn, FnMut, FnOnce)

Files to modify:
- `codegen.sx`: Add capture analysis, closure struct generation

### 3.2 Method References
**Status: 0%**

Missing:
- [ ] Function pointer from method: `Type::method`
- [ ] Closure from method reference

### 3.3 Anonymous Function Type Inference
**Status: 40%**

Missing:
- [ ] Infer parameter types from context
- [ ] Infer return type from body

---

## Priority 4: Control Flow Completions

### 4.1 Or-Patterns
**Status: 0%**

Missing:
- [ ] Pattern alternatives: `Pattern1 | Pattern2`
- [ ] In match arms and if-let

Files to modify:
- `parser.sx`: Add or-pattern parsing
- `codegen.sx`: Generate alternative checks

### 4.2 Pattern Guards
**Status: 60% (parsed, partial codegen)**

Missing:
- [ ] Guard evaluation in match
- [ ] Guard with pattern bindings

### 4.3 Range Patterns
**Status: 0%**

Missing:
- [ ] Numeric range patterns: `0..=9 => "digit"`
- [ ] Character range patterns

---

## Priority 5: Module System

### 5.1 Module Resolution
**Status: 15%**

Missing:
- [ ] Module file discovery
- [ ] Namespace isolation
- [ ] Module-qualified names

### 5.2 Use Statements
**Status: 15%**

Missing:
- [ ] Import resolution
- [ ] Glob imports: `use module::*`
- [ ] Nested imports: `use mod::{A, B, C}`
- [ ] Re-exports: `pub use`

### 5.3 Visibility Enforcement
**Status: 10%**

Missing:
- [ ] Private by default
- [ ] `pub` accessibility check
- [ ] `pub(crate)` / `pub(super)` scoping

---

## Priority 6: Async/Await Runtime

### 6.1 Async Function Transformation
**Status: 20%**

Missing:
- [ ] State machine generation
- [ ] Suspension point handling
- [ ] Future struct creation

### 6.2 Await Expression
**Status: 20%**

Missing:
- [ ] Poll loop generation
- [ ] Waker integration
- [ ] Resume point handling

### 6.3 Async Runtime
**Status: 5%**

Missing:
- [ ] Executor implementation
- [ ] Task scheduling
- [ ] Event loop integration

---

## Priority 7: Actor System

### 7.1 Actor Codegen
**Status: 5%**

Missing:
- [ ] Actor struct with mailbox
- [ ] Message handler dispatch
- [ ] State management

### 7.2 Message Passing
**Status: 0%**

Missing:
- [ ] `send()` function
- [ ] `ask()` function (request-response)
- [ ] Message queue implementation

### 7.3 Actor Lifecycle
**Status: 0%**

Missing:
- [ ] `spawn()` implementation
- [ ] Actor supervision
- [ ] Shutdown handling

---

## Priority 8: Specialist/Hive System

### 8.1 Specialist Codegen
**Status: 5%**

Missing:
- [ ] Model binding
- [ ] Inference dispatch
- [ ] Response handling

### 8.2 Hive Coordination
**Status: 0%**

Missing:
- [ ] Specialist registry
- [ ] Router implementation
- [ ] Strategy execution

---

## Priority 9: AI-Native Types

### 9.1 Vector Type
**Status: 0%**

Missing:
- [ ] `Vector<T, N>` type syntax
- [ ] Vector operations (dot, normalize, etc.)
- [ ] SIMD optimization

### 9.2 Tensor Type
**Status: 0%**

Missing:
- [ ] `Tensor<T, [D1, D2, ...]>` syntax
- [ ] Tensor operations
- [ ] Shape inference

### 9.3 Dual Numbers
**Status: 0%**

Missing:
- [ ] `dual` type
- [ ] Dual arithmetic
- [ ] Automatic differentiation

---

## Priority 10: Memory Safety (Long Term)

### 10.1 References
**Status: 0%**

Missing:
- [ ] Reference syntax `&T`, `&mut T`
- [ ] Reference semantics
- [ ] Dereference operator `*`

### 10.2 Borrow Checking
**Status: 0%**

Missing:
- [ ] Borrow tracking
- [ ] Lifetime inference
- [ ] Ownership enforcement

---

## Implementation Order Recommendation

### Phase A: Language Completeness (Weeks 1-2)
1. Tuple types and patterns
2. For loop iterators
3. While-let
4. Loop expression
5. Try operator

### Phase B: Type System (Weeks 3-4)
1. Type alias expansion
2. Trait bounds
3. Associated types
4. Generic methods

### Phase C: Closures & Functions (Week 5)
1. Closure capture analysis
2. Closure struct generation
3. Method references

### Phase D: Pattern Matching (Week 6)
1. Struct patterns
2. Or-patterns
3. Pattern guards completion

### Phase E: Module System (Week 7)
1. Module resolution
2. Import handling
3. Visibility enforcement

### Phase F: Async/Actor Runtime (Weeks 8-10)
1. Async state machine
2. Actor mailbox
3. Message passing

### Phase G: AI Features (Weeks 11-12)
1. Vector type
2. Tensor type
3. Specialist runtime

---

## Files Summary

| File | Primary Changes Needed |
|------|------------------------|
| `lexer.sx` | Add `loop` keyword, range operators |
| `parser.sx` | Tuple parsing, pattern extensions, while-let, loop |
| `codegen.sx` | Tuple codegen, try operator, closure capture, pattern matching extensions |
| `stdlib.sx` | Iterator trait, Option/Result methods |

---

## Test Coverage Needed

Each feature needs:
1. Parser test (syntax accepted)
2. Codegen test (LLVM IR generated)
3. Runtime test (correct execution)
4. Error test (proper error messages)

Current test pass rate: 45%
Target after Priority 1: 70%
Target after all priorities: 95%
