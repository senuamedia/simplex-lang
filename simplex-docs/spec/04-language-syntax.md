# Simplex Language Syntax

**Version 0.12.0**

Complete syntax reference for the Simplex programming language.

---

## Simplex Syntax Overview

Simplex is inspired by Rust syntax but has its own distinct conventions. Key syntax differences:

### Variable Bindings

```simplex
// Immutable by default (like Rust's `let`)
let x: i64 = 42
let name: String = "Alice"

// Mutable with `var` keyword (instead of `let mut`)
var counter: i64 = 0
```

**Implementation note:** All types currently compile to `i64` (integers, pointers, strings) or `f64` (floats) at the LLVM IR level. Type annotations are parsed and used for codegen decisions but not yet fully enforced by a type checker.

### Struct Definitions

```simplex
// Use `struct` keyword
struct Point {
    x: f64,
    y: f64
}

// Generic structs use angle brackets
struct Container<T> {
    value: T,
    count: i64
}
```

### Method Receivers

```simplex
// Methods use `self` as the receiver (like Rust)
impl Point {
    // Static method (no receiver)
    pub fn new(x: f64, y: f64) -> Point {
        Point { x: x, y: y }
    }

    // Instance method - bare `self` (implicit Self type)
    pub fn distance(self, other: Point) -> f64 {
        let dx = other.x - self.x
        let dy = other.y - self.y
        (dx * dx + dy * dy).sqrt()
    }

    // Borrowed self: &self, &mut self
    pub fn magnitude(&self) -> f64 {
        (self.x * self.x + self.y * self.y).sqrt()
    }

    // Explicit self type: self: Type
    pub fn into_tuple(self: Point) -> (f64, f64) {
        (self.x, self.y)
    }
}
```

### Module System

```simplex
// Import an entire module (makes its public functions available)
use mathlib;

// Import specific items from a module path
use simplex_std::collections::Vec;

// Import multiple items
use simplex_std::collections::{HashMap, HashSet};

// Glob import all public items
use simplex_std::prelude::*;
```

### Intrinsic Functions

```simplex
// Low-level intrinsics for memory and I/O operations
// Always return i64 for statement compatibility
fn intrinsic_alloc(size: i64) -> i64 { 0 }
fn intrinsic_read_byte(ptr: i64, offset: i64) -> i64 { 0 }
fn intrinsic_write_byte(ptr: i64, offset: i64, val: i64) -> i64 { 0 }

// Intrinsic calls must be assigned (not bare statements)
let _ = intrinsic_write_byte(ptr, 0, 42)
```

---

## Basic Types

### Primitives

```simplex
// Integers (i64 is the primary integer type in the current compiler)
let integer: i64 = 42

// Floating point
let float: f64 = 3.14

// Boolean
let boolean: bool = true
let negated: bool = false

// String
let text: String = "hello"
```

**Implementation note:** The current compiler uses `i64` for all integer types and `f64` for all floating-point types at the LLVM IR level. Additional numeric types (`u8`, `u64`, `i32`, `f32`) are recognized in the type system but compile to `i64` or `f64` respectively. The `Char` type and triple-quoted multiline strings (`"""..."""`) are planned but not yet implemented.

### Collections

```simplex
// Vec (ordered, growable) - built into the runtime
let list = vec_new()
vec_push(list, 42)

// Array literals
let items = [1, 2, 3]
```

**Implementation note:** The runtime provides `vec_new`, `vec_push`, `vec_get`, `vec_len`, `vec_set`, `vec_clear` as intrinsic functions. Higher-level collection types (`HashMap`, `HashSet`, `Map`, `Set`) are available through the standard library (`simplex-std::collections`). Map/Set literal syntax (`{"a": 1}`, `{1, 2, 3}`) and tuple types are planned but not yet implemented in the compiler.

### Optional and Result

```simplex
// Optional - no null in Simplex
let maybe: Option<String> = Some("value")
let empty: Option<String> = None

// Unwrap with default
let value = maybe.unwrap_or("default")

// Result - explicit error handling
let result: Result<i64, Error> = Ok(42)
let failure: Result<i64, Error> = Err(Error("failed"))

// Propagate errors with ?
fn risky() -> Result<i64, Error> {
    let x = might_fail()?  // Returns early if Err
    Ok(x + 1)
}
```

### AI-Native Types -- Planned

The following AI-native types are planned but not yet fully implemented in the compiler:

```simplex
// Vector (for embeddings) -- Planned
let embedding: Vector<f64, 1536> = ai::embed("text")

// Tensor (for ML) -- Planned
let tensor: Tensor<f32, [3, 224, 224]> = load_image(path)
let batch: Tensor<f32, [32, 3, 224, 224]> = stack_images(images)

// Vector operations -- Planned
let similarity = dot(embedding1, embedding2)
let normalized = normalize(embedding)
```

Tensor operations are currently available through the `simplex-learning` library's tensor module rather than as built-in language types.

### Dual Numbers (v0.8.0)

![Dual Numbers](../diagrams/dual-numbers.svg)

Native forward-mode automatic differentiation using dual numbers:

```
a + bε   where ε² = 0
```

- `a` = the computed value
- `b` = the derivative (gradient)
- `ε` = infinitesimal (nilpotent element)

This representation computes both values and derivatives in a single forward pass.

```simplex
// First-order dual number
let x: dual = dual::variable(3.0)    // value=3, derivative seed=1
let c: dual = dual::constant(2.0)    // value=2, derivative=0

// Arithmetic propagates derivatives automatically
let y = x * x + c * x                // y.val = 15, y.der = 8 (= 2x + 2)

// All transcendental functions are differentiable
let z = x.sin() + x.exp()            // Chain rule applied automatically

// Access value and derivative
print(y.val)  // Function value
print(y.der)  // Derivative at x=3

// Multi-dimensional gradients
let dx = multidual::<3>::variable(x, 0);  // ∂/∂x
let dy = multidual::<3>::variable(y, 1);  // ∂/∂y
let result = dx * dx * dy;
let gradient = result.gradient();          // [2xy, x², 0]

// Higher-order derivatives
let d2 = dual2::variable(x);
let h = d2 * d2 * d2;                      // x³
// h.val = x³, h.d1 = 3x², h.d2 = 6x
```

#### Meta-Gradient Applications

Dual numbers power self-learning annealing in neural gates. The meta-gradient `∂Loss/∂τ` automatically controls temperature schedules:

```simplex
use simplex::optimize::anneal::{LearnableSchedule, MetaOptimizer};

// Temperature as dual number enables meta-gradient computation
let temp: dual = dual::variable(1.0);
let loss = train_with_temperature(model, data, temp);

// Meta-gradient tells us: heat up or cool down?
if loss.der > 0.0 {
    // Positive: re-heat to escape local minimum
} else {
    // Negative: continue cooling toward solution
}
```

For simple problems, temperature smoothly decreases. For complex patterns, the meta-gradient detects when the system is stuck in local minima and automatically triggers re-heating to escape.

See [Meta-Gradient Temperature Control](09-cognitive-hive.md#meta-gradient-temperature-control) for the full explanation with diagrams.

---

## Type Definitions

### Struct Types

```simplex
// Basic struct
struct User {
    id: UserId,
    name: String,
    email: String,
    created_at: Timestamp
}

// Generic struct
struct Pair<A, B> {
    first: A,
    second: B
}

// Struct with optional fields
struct Config {
    host: String,
    port: u16,
    timeout: Option<Duration>
}
```

### Enum Types

```simplex
// Simple enum
enum Status {
    Pending,
    Active,
    Completed,
    Failed
}

// Enum with data
enum Message {
    Text(String),
    Image(Bytes, String),  // data, mime_type
    Location(f64, f64)     // lat, lng
}

// Generic enum
enum Result<T, E> {
    Ok(T),
    Err(E)
}
```

### Type Aliases

```simplex
type UserId = String
type Embedding = Vector<f64, 1536>
type UserMap = Map<UserId, User>
```

---

## Functions

### Basic Functions

```simplex
// Explicit return type
fn add(a: i64, b: i64) -> i64 {
    return a + b
}

// Implicit return (last expression)
fn multiply(a: i64, b: i64) -> i64 {
    a * b
}

// No return value
fn log_message(msg: String) {
    print("[LOG] {msg}")
}
```

### Generic Functions

```simplex
// Single type parameter
fn first<T>(list: List<T>) -> Option<T> {
    match list {
        [] => None,
        [head, ..] => Some(head)
    }
}

// Multiple type parameters
fn zip<A, B>(a: List<A>, b: List<B>) -> List<(A, B)> {
    a.iter().zip(b.iter()).collect()
}

// Constrained generics
fn sum<T: Numeric>(list: List<T>) -> T {
    list.fold(T::zero(), (acc, x) => acc + x)
}
```

### Async Functions

```simplex
// Async function returns Future
async fn fetch(url: String) -> Result<Response, HttpError> {
    let response = await http::get(url)
    response
}

// Await multiple futures
async fn fetch_all(urls: List<String>) -> List<Response> {
    let futures = urls.map(url => fetch(url))
    await parallel(futures)
}

// Sequential await
async fn process_pipeline(input: String) -> Output {
    let step1 = await transform(input)
    let step2 = await enrich(step1)
    let step3 = await finalize(step2)
    step3
}
```

### Closures

```simplex
// Inline closure
let doubled = numbers.map(x => x * 2)

// Multi-line closure
let processed = items.map(item => {
    let transformed = transform(item)
    let validated = validate(transformed)
    validated
})

// Closure with type annotation
let parser: fn(String) -> i64 = s => s.parse_int()

// Capturing closure
let multiplier = 10
let scaled = numbers.map(x => x * multiplier)
```

---

## Actors

### Actor Definition

```simplex
actor Counter {
    // State (mutable within actor)
    var count: i64 = 0
    var history: List<i64> = []

    // Constructor
    init(initial: i64) {
        count = initial
    }

    // Message handlers
    receive Increment {
        count += 1
        history.push(count)
    }

    receive Add(n: i64) {
        count += n
        history.push(count)
    }

    receive GetCount -> i64 {
        count
    }

    receive GetHistory -> List<i64> {
        history.clone()
    }

    // Lifecycle hooks (Planned - set via runtime API currently)
    // on_start(), on_checkpoint(), on_resume(), on_stop()
    // Use intrinsic_actor_set_on_start() / intrinsic_actor_set_on_stop() at runtime
}
```

### Spawning and Messaging

```simplex
// Spawn an actor
let counter = spawn Counter(initial: 0)

// Send message (fire and forget)
send(counter, Increment)
send(counter, Add(10))

// Ask (request-response)
let value = ask(counter, GetCount)  // Blocks for response

// Ask with timeout
let value = ask(counter, GetCount, timeout: Duration::seconds(5))

// Send to self
receive ProcessItem(item: Item) {
    // ... process ...
    send(self, ProcessItem(next_item))  // Continue processing
}
```

### Actor References

```simplex
// Typed actor reference
let counter: ActorRef<Counter> = spawn Counter(initial: 0)

// Store references
actor Coordinator {
    var workers: List<ActorRef<Worker>> = []

    receive RegisterWorker(worker: ActorRef<Worker>) {
        workers.push(worker)
    }

    receive Broadcast(msg: WorkerMessage) {
        for worker in workers {
            send(worker, msg)
        }
    }
}
```

---

## Supervision -- Planned Syntax

Supervision is currently available through runtime API functions (`supervisor_new`, `supervisor_add_child`, `supervisor_start`, `supervisor_handle_exit`). The following declarative syntax is planned but not yet implemented as a language construct:

### Supervisor Definition

```simplex
supervisor OrderSystem {
    // Supervision strategy
    strategy: OneForOne,      // Only restart failed child
    max_restarts: 3,          // Max 3 restarts
    within: Duration::seconds(60),  // Within 60 seconds

    // Child specifications
    children: [
        child(OrderProcessor, restart: Always),
        child(PaymentHandler, restart: Always),
        child(NotificationService, restart: Transient),
    ]
}
```

### Supervision Strategies

```simplex
// OneForOne - only restart the failed child
supervisor Pool {
    strategy: OneForOne,
    children: [
        child(Worker),
        child(Worker),
        child(Worker)
    ]
}

// OneForAll - restart all children if one fails
supervisor Pipeline {
    strategy: OneForAll,
    children: [
        child(Ingester),
        child(Processor),
        child(Writer)
    ]
}

// RestForOne - restart failed child and all after it
supervisor Chain {
    strategy: RestForOne,
    children: [
        child(Fetcher),    // If this fails, restart all
        child(Parser),     // If this fails, restart Parser + Writer
        child(Writer)      // If this fails, only restart Writer
    ]
}
```

### Restart Policies

```simplex
// Always restart
child(CriticalService, restart: Always)

// Never restart
child(OneTimeTask, restart: Never)

// Restart only on abnormal exit
child(Worker, restart: Transient)

// Custom restart logic
child(Service, restart: Custom(should_restart))

fn should_restart(exit_reason: ExitReason) -> Bool {
    match exit_reason {
        ExitReason::Normal => false,
        ExitReason::Error(e) if e.is_retryable() => true,
        _ => false
    }
}
```

---

## Pattern Matching

### Basic Matching

```simplex
fn describe(value: Value) -> String {
    match value {
        Value::Number(n) => "number: {n}",
        Value::Text(s) => "text: {s}",
        Value::List(items) => "list with {items.len()} items",
        _ => "unknown"
    }
}
```

### Guards

```simplex
fn classify(n: i64) -> String {
    match n {
        x if x < 0 => "negative",
        0 => "zero",
        x if x < 10 => "small",
        x if x < 100 => "medium",
        _ => "large"
    }
}
```

### Destructuring

```simplex
// Tuple destructuring
let (x, y) = get_point()

// Struct destructuring
let User { name, email, .. } = get_user()

// List destructuring
match items {
    [] => "empty",
    [only] => "single: {only}",
    [first, second] => "pair: {first}, {second}",
    [head, ..tail] => "head: {head}, rest: {tail.len()}"
}

// Nested destructuring
match response {
    Response { status: 200, body: Body::Json(data) } => process(data),
    Response { status: 404, .. } => not_found(),
    Response { status, .. } if status >= 500 => server_error(status),
    _ => unknown_response()
}
```

---

## Error Handling

### Result-Based Errors

```simplex
// Return Result
fn divide(a: i64, b: i64) -> Result<i64, MathError> {
    if b == 0 {
        return Err(MathError::DivisionByZero)
    }
    Ok(a / b)
}

// Propagation with ?
fn calculate(x: i64, y: i64, z: i64) -> Result<i64, MathError> {
    let first = divide(x, y)?   // Returns early on error
    let second = divide(first, z)?
    Ok(second)
}

// Explicit handling
match divide(10, 0) {
    Ok(result) => print("Result: {result}"),
    Err(MathError::DivisionByZero) => print("Cannot divide by zero"),
    Err(e) => print("Error: {e}")
}
```

### Custom Error Types

```simplex
enum AppError {
    NotFound(String),
    Unauthorized,
    ValidationFailed(List<String>),
    Internal(String)
}

impl AppError {
    fn is_retryable(self) -> Bool {
        match self {
            AppError::Internal(_) => true,
            _ => false
        }
    }
}
```

### Error Conversion

```simplex
// Convert between error types
fn process() -> Result<Output, AppError> {
    let data = fetch_data()
        .map_err(e => AppError::Internal(e.message()))?

    let parsed = parse(data)
        .map_err(e => AppError::ValidationFailed(e.errors()))?

    Ok(parsed)
}
```

---

## Modules

### Module Definition

Modules in Simplex are defined by file. Each `.sx` file is a module. Public functions are exported with `pub`:

```simplex
// file: math/vectors.sx

// Public items - accessible when imported
pub type Vector = List<f64>;

pub fn dot(a: Vector, b: Vector) -> f64 {
    a.iter().zip(b.iter()).map((x, y) => x * y).sum()
}

pub fn magnitude(v: Vector) -> f64 {
    dot(v, v).sqrt()
}

// Private items (default) - only accessible within this file
fn validate(v: Vector) -> Bool {
    v.len() > 0
}
```

**Note:** The `mod` keyword is recognized by the lexer for submodule declarations (e.g., `mod submodule;`), but the primary module system uses file-based modules imported via `use`.

### Imports

```simplex
// Import a module (makes its public functions available)
use mathlib;

// Import specific items
use math::vectors::{dot, Vector};

// Import all public items
use math::vectors::*;

// Nested imports
use std::{
    collections::{Map, Set},
    time::{Duration, Instant}
};
```

**Note:** Import aliasing (`use X as Y`) is planned but not yet implemented. Semicolons are required after `use` statements.

### Visibility

```simplex
// Public - accessible from anywhere
pub fn public_function() { }
pub struct PublicType { }

// Private (default) - only within module
fn private_function() { }
struct PrivateType { }
```

---

## Control Flow

### Conditionals

```simplex
// If expression
let status = if count > 0 { "active" } else { "empty" }

// If-else chain
let grade = if score >= 90 {
    "A"
} else if score >= 80 {
    "B"
} else if score >= 70 {
    "C"
} else {
    "F"
}
```

### Loops

```simplex
// For loop
for item in items {
    process(item)
}

// For with index
for (i, item) in items.enumerate() {
    print("{i}: {item}")
}

// Range
for i in 0..10 {
    print(i)
}

// While loop
while condition {
    do_something()
}

// Loop with break
loop {
    let result = try_operation()
    if result.is_ok() {
        break result.unwrap()
    }
}
```

### Iteration

```simplex
// Map
let doubled = numbers.map(x => x * 2)

// Filter
let evens = numbers.filter(x => x % 2 == 0)

// Fold/Reduce
let sum = numbers.fold(0, (acc, x) => acc + x)

// Chaining
let result = items
    .filter(item => item.active)
    .map(item => item.value)
    .filter(v => v > 0)
    .sum()

// Collect into different types
let list: List<i64> = iter.collect()
let set: Set<i64> = iter.collect()
let map: Map<String, i64> = pairs.collect()
```

---

## String Formatting

Simplex uses f-strings (prefixed with `f`) for string interpolation:

```simplex
// F-string interpolation
let name = "Alice"
let greeting = f"Hello, {name}!"

// Expressions in interpolation
let message = f"Count: {items.len()}, Sum: {items.sum()}"

// Escape braces with backslash
let json = f"value: \{{name}\}"

// Regular strings have no interpolation
let plain = "Hello, {this is literal text}"
```

**Note:** Format specifiers (e.g., `{value:.2}`, `{id:05}`) and multiline triple-quoted strings (`"""..."""`) are planned but not yet implemented in the compiler.

---

## Next Steps

- [Examples](../examples/document-pipeline.md): See syntax in a complete program
- [AI Integration](07-ai-integration.md): AI-specific syntax
- [Virtual Machine](05-virtual-machine.md): How code executes
