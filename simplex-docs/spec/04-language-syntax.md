# Simplex Language Syntax

**Version 0.11.0**

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
var buffer: List<u8> = []
```

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
// Methods use `this: TypeName` (not `self`)
impl Point {
    // Static method (no receiver)
    pub fn new(x: f64, y: f64) -> Point {
        Point { x: x, y: y }
    }

    // Instance method - explicit `this` receiver
    pub fn distance(this: Point, other: Point) -> f64 {
        let dx = other.x - this.x
        let dy = other.y - this.y
        (dx * dx + dy * dy).sqrt()
    }

    // Method invocation can use Type::method(instance) style
    // Point::distance(p1, p2)
    // Or method call syntax: p1.distance(p2)
}
```

### Module System

```simplex
// Use `use modulus::` for external package imports
use modulus::simplex_std::collections::Vec
use modulus::simplex_std::io

// Use `use` for same-package imports
use mymodule::helper_function
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
// Integers
let integer: i64 = 42
let unsigned: u64 = 42
let small: i32 = 42
let byte: u8 = 255

// Floating point
let float: f64 = 3.14
let float32: f32 = 3.14

// Boolean
let boolean: Bool = true
let negated: Bool = false

// Character and String
let character: Char = 'a'
let text: String = "hello"
let multiline: String = """
    This is a
    multiline string
"""
```

### Collections

```simplex
// List (ordered, growable)
let list: List<i64> = [1, 2, 3]
let empty_list: List<String> = []

// Map (key-value)
let map: Map<String, i64> = {"a": 1, "b": 2}
let empty_map: Map<String, User> = {}

// Set (unique values)
let set: Set<i64> = {1, 2, 3}

// Tuple (fixed size, mixed types)
let tuple: (i64, String) = (42, "answer")
let triple: (i64, String, Bool) = (1, "yes", true)
```

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

### AI-Native Types

```simplex
// Vector (for embeddings)
let embedding: Vector<f64, 1536> = ai::embed("text")

// Tensor (for ML)
let tensor: Tensor<f32, [3, 224, 224]> = load_image(path)
let batch: Tensor<f32, [32, 3, 224, 224]> = stack_images(images)

// Vector operations
let similarity = dot(embedding1, embedding2)
let normalized = normalize(embedding)
```

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

    // Lifecycle hooks
    on_start() {
        log::info("Counter started with count: {count}")
    }

    on_checkpoint() {
        log::debug("Checkpointing at count: {count}")
    }

    on_resume() {
        log::info("Resumed with count: {count}")
    }

    on_stop() {
        log::info("Counter stopping")
    }
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

## Supervision

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
    fn is_retryable(this: AppError) -> Bool {
        match this {
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

```simplex
// file: math/vectors.sx
module math::vectors

// Public items
pub type Vector = List<f64>

pub fn dot(a: Vector, b: Vector) -> f64 {
    a.iter().zip(b.iter()).map((x, y) => x * y).sum()
}

pub fn magnitude(v: Vector) -> f64 {
    dot(v, v).sqrt()
}

// Private items (default)
fn validate(v: Vector) -> Bool {
    v.len() > 0
}
```

### Imports

```simplex
// Import specific items
use math::vectors::{dot, Vector}

// Import all public items
use math::vectors::*

// Import with alias
use math::vectors::dot as dot_product

// Nested imports
use std::{
    collections::{Map, Set},
    time::{Duration, Instant}
}
```

### Visibility

```simplex
module mylib

// Public - accessible from anywhere
pub fn public_function() { }
pub type PublicType { }

// Private (default) - only within module
fn private_function() { }
type PrivateType { }

// Pub(modulus) - accessible within modulus only
pub(modulus) fn internal_function() { }
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

```simplex
// Interpolation
let name = "Alice"
let greeting = "Hello, {name}!"

// Expressions in interpolation
let message = "Count: {items.len()}, Sum: {items.sum()}"

// Formatting specifiers
let formatted = "Value: {value:.2}"      // 2 decimal places
let padded = "ID: {id:05}"               // Zero-padded to 5 digits
let hex = "Address: {addr:x}"            // Hexadecimal

// Multiline with interpolation
let report = """
    Report for {user.name}
    ======================
    Items processed: {stats.processed}
    Errors: {stats.errors}
    Duration: {stats.duration}
"""
```

---

## Next Steps

- [Examples](../examples/document-pipeline.md): See syntax in a complete program
- [AI Integration](07-ai-integration.md): AI-specific syntax
- [Virtual Machine](05-virtual-machine.md): How code executes
