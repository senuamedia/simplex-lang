# Chapter 1: Variables and Types

In this chapter, you'll learn the basics of Simplex: how to declare variables, understand the type system, and work with primitive data types.

---

## Your First Program

Let's start with the simplest possible program. Create a file called `hello.sx`:

```simplex
fn main() {
    print("Hello, Simplex!")
}
```

Run it:

```bash
simplex run hello.sx
```

Output:
```
Hello, Simplex!
```

Every Simplex program needs a `main` function—that's where execution begins. The `print` function outputs text to the console.

> **Tip (v0.11.0)**: Use `sxfmt hello.sx` to automatically format your code, and `sxlint hello.sx` to check for potential issues. If you encounter a compiler error, run `sxc explain <error-code>` for a detailed explanation. See [Chapter 11](11-capstone.md#developer-tools-v0100) for the full developer tools guide.

---

## Declaring Variables

In Simplex, you declare variables with `let`:

```simplex
fn main() {
    let name = "Alice"
    let age = 30
    let height = 5.8

    print("Name: {name}")
    print("Age: {age}")
    print("Height: {height}")
}
```

Output:
```
Name: Alice
Age: 30
Height: 5.8
```

Notice how we didn't specify types? Simplex uses **type inference**—it figures out the types from the values you assign. But under the hood, every variable has a specific type.

---

## Immutability by Default

Variables in Simplex are **immutable by default**. Once set, they can't be changed:

```simplex
fn main() {
    let x = 10
    x = 20  // Error: cannot assign to immutable variable
}
```

This might seem restrictive, but it prevents a huge class of bugs. When you know a value won't change, your code is easier to reason about.

If you need a mutable variable, use `var`:

```simplex
fn main() {
    var counter = 0
    counter = counter + 1
    counter += 1  // Shorthand
    print("Counter: {counter}")  // Counter: 2
}
```

**Rule of thumb**: Use `let` unless you specifically need mutation. Most of the time, you don't.

---

## Primitive Types

Simplex has several built-in types:

### Integers

```simplex
let small: i32 = 42          // 32-bit signed integer
let normal: i64 = 42         // 64-bit signed integer (default)
let unsigned: u64 = 42       // 64-bit unsigned integer
let byte: u8 = 255           // 8-bit unsigned (0-255)
```

If you don't specify a type, integers default to `i64`:

```simplex
let x = 42  // Inferred as i64
```

### Floating Point

```simplex
let pi: f64 = 3.14159        // 64-bit float (default)
let approx: f32 = 3.14       // 32-bit float
```

Numbers with decimal points default to `f64`:

```simplex
let y = 3.14  // Inferred as f64
```

### Booleans

```simplex
let is_active = true
let is_complete = false
```

Booleans are type `Bool` and can only be `true` or `false`.

### Characters

```simplex
let letter: Char = 'A'
let special: Char = '*'
```

Characters are single Unicode code points, enclosed in single quotes.

### Strings

```simplex
let greeting = "Hello, world!"
let name = "Simplex"
```

Strings are sequences of characters, enclosed in double quotes.

---

## Type Annotations

While Simplex infers types, you can always be explicit:

```simplex
let x: i64 = 42
let name: String = "Alice"
let active: Bool = true
```

This is useful when:
- You want to be clear about your intentions
- The compiler can't infer the type
- You want a different type than the default (e.g., `i32` instead of `i64`)

---

## String Interpolation

Simplex makes it easy to embed values in strings using `{}`:

```simplex
fn main() {
    let name = "Alice"
    let age = 30

    // Simple interpolation
    print("Hello, {name}!")

    // Expressions work too
    print("{name} will be {age + 1} next year")

    // Multiple values
    print("{name} is {age} years old")
}
```

Output:
```
Hello, Alice!
Alice will be 31 next year
Alice is 30 years old
```

You can put any expression inside `{}`:

```simplex
print("2 + 2 = {2 + 2}")           // 2 + 2 = 4
print("Length: {name.len()}")       // Length: 5
```

---

## Multiline Strings

For longer text, use triple quotes:

```simplex
let poem = """
    Roses are red,
    Violets are blue,
    Simplex is simple,
    And powerful too.
"""

print(poem)
```

The indentation of the closing `"""` determines how much leading whitespace is stripped.

---

## Numeric Operations

Standard math operations work as expected:

```simplex
fn main() {
    let a = 10
    let b = 3

    print("Addition: {a + b}")       // 13
    print("Subtraction: {a - b}")    // 7
    print("Multiplication: {a * b}") // 30
    print("Division: {a / b}")       // 3 (integer division)
    print("Remainder: {a % b}")      // 1

    // Floating point division
    let x = 10.0
    let y = 3.0
    print("Float division: {x / y}") // 3.333...
}
```

**Note**: Division between integers gives an integer result. If you need decimal results, use floats.

---

## Comparison Operators

```simplex
fn main() {
    let a = 10
    let b = 20

    print("Equal: {a == b}")         // false
    print("Not equal: {a != b}")     // true
    print("Less than: {a < b}")      // true
    print("Greater than: {a > b}")   // false
    print("Less or equal: {a <= b}") // true
    print("Greater or equal: {a >= b}") // false
}
```

---

## Logical Operators

```simplex
fn main() {
    let sunny = true
    let warm = false

    print("AND: {sunny && warm}")    // false
    print("OR: {sunny || warm}")     // true
    print("NOT: {!sunny}")           // false
}
```

---

## Type Safety

Simplex is **statically typed**, meaning types are checked at compile time. This catches errors before your code runs:

```simplex
fn main() {
    let x = 42
    let y = "hello"

    // This won't compile:
    let z = x + y  // Error: cannot add i64 and String
}
```

You can't accidentally mix incompatible types. This prevents entire categories of bugs.

---

## Type Conversion

When you need to convert between types, be explicit:

```simplex
fn main() {
    let x: i64 = 42
    let y: f64 = x.to_f64()  // Convert to float

    let a: f64 = 3.7
    let b: i64 = a.to_i64()  // Truncates to 3

    let n: i64 = 42
    let s: String = n.to_string()  // "42"

    let text = "123"
    let num: i64 = text.parse_int().unwrap()  // Parse string to int
}
```

Explicit conversion makes it clear when you're changing types, preventing subtle bugs.

---

## Constants

For values that should never change and are known at compile time, use `const`:

```simplex
const MAX_USERS: u64 = 1000
const PI: f64 = 3.14159
const APP_NAME: String = "MyApp"

fn main() {
    print("Max users: {MAX_USERS}")
    print("Pi: {PI}")
}
```

Constants are:
- Always immutable
- Must have explicit types
- Evaluated at compile time
- Conventionally UPPER_SNAKE_CASE

---

## The Unit Type

Some expressions don't return a meaningful value. They return `()`, called the **unit type**:

```simplex
fn main() {
    let result = print("Hello")  // print returns ()
    // result is of type ()
}
```

You'll see this more when we discuss functions that don't return values.

---

## Summary

In this chapter, you learned:

| Concept | Syntax | Example |
|---------|--------|---------|
| Immutable variable | `let` | `let x = 42` |
| Mutable variable | `var` | `var x = 42` |
| Type annotation | `: Type` | `let x: i64 = 42` |
| Integers | `i64`, `i32`, `u64`, `u8` | `let n: i64 = 100` |
| Floats | `f64`, `f32` | `let pi = 3.14` |
| Booleans | `Bool` | `let active = true` |
| Strings | `String` | `let name = "Alice"` |
| Interpolation | `"{expr}"` | `"Hello, {name}"` |

---

## Exercises

1. **Temperature Converter**: Write a program that converts a temperature from Celsius to Fahrenheit. (Formula: F = C × 9/5 + 32)

2. **Greeting**: Create a program with variables for first name, last name, and age. Print a greeting that includes all three.

3. **Rectangle Area**: Declare variables for width and height, then calculate and print the area.

4. **Type Experiment**: Try adding a string to a number. What error do you get? How would you fix it?

---

## Answers

<details>
<summary>Click to reveal answers</summary>

**Exercise 1:**
```simplex
fn main() {
    let celsius = 25.0
    let fahrenheit = celsius * 9.0 / 5.0 + 32.0
    print("{celsius}°C = {fahrenheit}°F")
}
```

**Exercise 2:**
```simplex
fn main() {
    let first_name = "John"
    let last_name = "Doe"
    let age = 28
    print("Hello, {first_name} {last_name}! You are {age} years old.")
}
```

**Exercise 3:**
```simplex
fn main() {
    let width = 10
    let height = 5
    let area = width * height
    print("A rectangle {width}x{height} has area {area}")
}
```

**Exercise 4:**
```simplex
fn main() {
    let x = 42
    let y = "hello"
    // let z = x + y  // Error: cannot add i64 and String

    // Fix: convert the number to a string
    let z = x.to_string() + y
    print(z)  // "42hello"
}
```

</details>

---

*Next: [Chapter 2: Functions →](02-functions.md)*
