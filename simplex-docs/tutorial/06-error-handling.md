# Chapter 6: Error Handling

Every program encounters errors. Simplex handles them explicitly, making failure cases visible and impossible to ignore.

---

## The Problem with Null

Many languages use `null` to represent "no value." This leads to the infamous null pointer exception—one of the most common bugs in software.

```javascript
// JavaScript: This might crash
let user = getUser(id);
console.log(user.name);  // 💥 if user is null
```

Simplex doesn't have `null`. Instead, we explicitly model the possibility of absence.

---

## Option: Maybe There's a Value

`Option<T>` represents a value that might or might not exist:

```simplex
enum Option<T> {
    Some(T),  // There's a value
    None      // There's no value
}
```

### Returning Option

```simplex
fn find_user(id: String) -> Option<User> {
    // Search for user...
    if found {
        Some(user)
    } else {
        None
    }
}

fn main() {
    let result = find_user("alice123")

    // Must handle both cases
    match result {
        Some(user) => print("Found: {user.name}"),
        None => print("User not found")
    }
}
```

### Common Option Methods

```simplex
fn main() {
    let some_value: Option<i64> = Some(42)
    let no_value: Option<i64> = None

    // Check if value exists
    print(some_value.is_some())  // true
    print(some_value.is_none())  // false

    // Unwrap (panics if None - use carefully!)
    print(some_value.unwrap())   // 42

    // Unwrap with default
    print(no_value.unwrap_or(0))         // 0
    print(no_value.unwrap_or_else(|| compute_default()))

    // Map: transform the inner value
    let doubled = some_value.map(x => x * 2)  // Some(84)

    // And_then: chain operations that return Option
    let result = some_value
        .map(x => x * 2)           // Some(84)
        .and_then(x => if x > 50 { Some(x) } else { None })  // Some(84)
}
```

### The ? Operator for Option

The `?` operator unwraps `Some` or returns `None` early:

```simplex
fn get_username(user_id: String) -> Option<String> {
    let user = find_user(user_id)?  // Returns None if not found
    let profile = get_profile(user)?  // Returns None if no profile
    Some(profile.username)
}

// Equivalent to:
fn get_username_long(user_id: String) -> Option<String> {
    match find_user(user_id) {
        None => return None,
        Some(user) => {
            match get_profile(user) {
                None => return None,
                Some(profile) => Some(profile.username)
            }
        }
    }
}
```

---

## Result: Success or Failure

`Result<T, E>` represents an operation that might fail:

```simplex
enum Result<T, E> {
    Ok(T),   // Success with value
    Err(E)   // Failure with error
}
```

### Returning Result

```simplex
enum ParseError {
    InvalidFormat,
    NumberTooLarge,
    EmptyInput
}

fn parse_number(input: String) -> Result<i64, ParseError> {
    if input.is_empty() {
        return Err(ParseError::EmptyInput)
    }

    match input.parse_int() {
        Some(n) if n > 1000000 => Err(ParseError::NumberTooLarge),
        Some(n) => Ok(n),
        None => Err(ParseError::InvalidFormat)
    }
}

fn main() {
    match parse_number("42") {
        Ok(n) => print("Parsed: {n}"),
        Err(ParseError::InvalidFormat) => print("Invalid format"),
        Err(ParseError::NumberTooLarge) => print("Number too large"),
        Err(ParseError::EmptyInput) => print("Empty input")
    }
}
```

### Common Result Methods

```simplex
fn main() {
    let success: Result<i64, String> = Ok(42)
    let failure: Result<i64, String> = Err("something went wrong")

    // Check status
    print(success.is_ok())   // true
    print(success.is_err())  // false

    // Unwrap (panics on error - use carefully!)
    print(success.unwrap())  // 42

    // Unwrap with default
    print(failure.unwrap_or(0))  // 0

    // Map: transform success value
    let doubled = success.map(x => x * 2)  // Ok(84)

    // Map_err: transform error value
    let mapped_err = failure.map_err(e => "Error: {e}")
}
```

### The ? Operator for Result

The `?` operator is even more useful with Result:

```simplex
fn read_config() -> Result<Config, ConfigError> {
    let contents = read_file("config.toml")?  // Returns Err early if fails
    let parsed = parse_toml(contents)?         // Returns Err early if fails
    let config = validate_config(parsed)?      // Returns Err early if fails
    Ok(config)
}

// The ? operator:
// 1. If Ok(value), unwraps to value
// 2. If Err(e), returns Err(e) from the function
```

### Combining Results

```simplex
fn process_data() -> Result<Output, Error> {
    // Sequential: each step depends on previous
    let a = step_one()?
    let b = step_two(a)?
    let c = step_three(b)?
    Ok(c)
}

fn fetch_all() -> Result<List<Data>, Error> {
    // Collect results: fail if any fails
    let items = ["a", "b", "c"]
    let results: Result<List<Data>, Error> = items
        .map(id => fetch(id))
        .collect()

    results
}
```

---

## Creating Error Types

Define meaningful error types for your domain:

```simplex
// Simple enum error
enum FileError {
    NotFound,
    PermissionDenied,
    IoError(String)
}

// Structured error with details
type ValidationError {
    field: String,
    message: String
}

// Multiple error variants with data
enum AppError {
    Database(DatabaseError),
    Network(NetworkError),
    Validation(List<ValidationError>),
    Internal(String)
}

impl AppError {
    fn is_retryable(&self) -> Bool {
        match self {
            AppError::Network(_) => true,
            AppError::Database(db) => db.is_transient(),
            _ => false
        }
    }
}
```

### Converting Between Error Types

```simplex
fn load_user_data(id: String) -> Result<UserData, AppError> {
    // Convert FileError to AppError
    let file_contents = read_file(id)
        .map_err(e => AppError::Internal("File error: {e}"))?

    // Convert ParseError to AppError
    let data = parse_json(file_contents)
        .map_err(e => AppError::Validation([
            ValidationError { field: "file", message: e.to_string() }
        ]))?

    Ok(data)
}
```

---

## Handling Multiple Error Types

When calling functions with different error types:

```simplex
// Define a common error type
enum FetchError {
    Http(HttpError),
    Parse(ParseError),
    Timeout
}

fn fetch_and_parse(url: String) -> Result<Data, FetchError> {
    let response = http_get(url)
        .map_err(e => FetchError::Http(e))?

    let data = parse_json(response.body)
        .map_err(e => FetchError::Parse(e))?

    Ok(data)
}
```

---

## Panic: Unrecoverable Errors

For truly unexpected situations, use `panic`:

```simplex
fn main() {
    let index = -1

    if index < 0 {
        panic("Index cannot be negative: {index}")
    }
}
```

Panic should be used for:
- Programming errors (bugs)
- Situations that "should never happen"
- During development/prototyping

**Never** use panic for expected error conditions. Use `Result` instead.

### Unwrap and Expect

`unwrap()` panics on None/Err. Use `expect()` for better error messages:

```simplex
// Bad: unhelpful panic message
let value = some_option.unwrap()

// Better: explains what went wrong
let value = some_option.expect("Configuration file must exist")

// Best: handle the error properly
let value = match some_option {
    Some(v) => v,
    None => return Err(ConfigError::MissingFile)
}
```

---

## Practical Patterns

### Early Return with ?

```simplex
fn process_order(order_id: String) -> Result<Receipt, OrderError> {
    let order = find_order(order_id)?
    let user = find_user(order.user_id)?
    let payment = charge_card(user.card, order.total)?
    let shipment = schedule_shipment(order)?

    Ok(Receipt {
        order_id: order.id,
        payment_id: payment.id,
        tracking: shipment.tracking_number
    })
}
```

### Default Values

```simplex
fn get_setting(name: String) -> String {
    // Try to read from config, use default if not found
    read_config(name).unwrap_or_else(|| {
        match name {
            "timeout" => "30",
            "retries" => "3",
            _ => ""
        }
    })
}
```

### Logging Errors

```simplex
fn process_item(item: Item) -> Result<Output, ProcessError> {
    let result = do_processing(item);

    // Log errors but still return them
    if let Err(e) = &result {
        log::error("Failed to process item {item.id}: {e}")
    }

    result
}
```

### Retry Logic

```simplex
fn fetch_with_retry(url: String, max_retries: i64) -> Result<Response, FetchError> {
    var attempts = 0

    loop {
        attempts += 1

        match http_get(url) {
            Ok(response) => return Ok(response),
            Err(e) if attempts < max_retries && e.is_retryable() => {
                log::warn("Attempt {attempts} failed, retrying...")
                sleep(Duration::seconds(attempts))
                continue
            },
            Err(e) => return Err(e)
        }
    }
}
```

---

## Summary

| Type | Use When | Example |
|------|----------|---------|
| `Option<T>` | Value might not exist | `find_user(id) -> Option<User>` |
| `Result<T, E>` | Operation might fail | `parse(input) -> Result<Data, Error>` |
| `panic!` | Unrecoverable bug | `panic("invariant violated")` |

| Method | Purpose | Example |
|--------|---------|---------|
| `unwrap()` | Get value or panic | `opt.unwrap()` |
| `unwrap_or(default)` | Get value or default | `opt.unwrap_or(0)` |
| `?` | Early return on None/Err | `let x = fallible()?` |
| `map(f)` | Transform inner value | `opt.map(x => x * 2)` |
| `and_then(f)` | Chain fallible operations | `opt.and_then(x => another(x))` |

---

## Exercises

1. **Safe Division**: Write `safe_divide(a: f64, b: f64) -> Option<f64>` that returns `None` when dividing by zero.

2. **Parse Config**: Write a function that parses a config string like `"port=8080"` into a key-value pair, returning appropriate errors for invalid formats.

3. **Validate User**: Create a `validate_user` function that checks name (non-empty), email (contains @), and age (positive). Return all validation errors, not just the first.

4. **Chain Operations**: Write a function that reads a file, parses JSON, and extracts a specific field, using the `?` operator.

---

## Answers

<details>
<summary>Click to reveal answers</summary>

**Exercise 1:**
```simplex
fn safe_divide(a: f64, b: f64) -> Option<f64> {
    if b == 0.0 {
        None
    } else {
        Some(a / b)
    }
}

fn main() {
    print(safe_divide(10.0, 2.0))  // Some(5.0)
    print(safe_divide(10.0, 0.0))  // None
}
```

**Exercise 2:**
```simplex
enum ConfigError {
    MissingEquals,
    EmptyKey,
    EmptyValue
}

fn parse_config_line(line: String) -> Result<(String, String), ConfigError> {
    let parts: List<String> = line.split("=").collect()

    if parts.len() != 2 {
        return Err(ConfigError::MissingEquals)
    }

    let key = parts[0].trim()
    let value = parts[1].trim()

    if key.is_empty() {
        return Err(ConfigError::EmptyKey)
    }

    if value.is_empty() {
        return Err(ConfigError::EmptyValue)
    }

    Ok((key, value))
}

fn main() {
    print(parse_config_line("port=8080"))    // Ok(("port", "8080"))
    print(parse_config_line("invalid"))      // Err(MissingEquals)
    print(parse_config_line("=8080"))        // Err(EmptyKey)
}
```

**Exercise 3:**
```simplex
type ValidationError {
    field: String,
    message: String
}

type UserInput {
    name: String,
    email: String,
    age: i64
}

fn validate_user(input: UserInput) -> Result<UserInput, List<ValidationError>> {
    var errors: List<ValidationError> = []

    if input.name.is_empty() {
        errors.push_mut(ValidationError {
            field: "name",
            message: "Name cannot be empty"
        })
    }

    if !input.email.contains("@") {
        errors.push_mut(ValidationError {
            field: "email",
            message: "Email must contain @"
        })
    }

    if input.age <= 0 {
        errors.push_mut(ValidationError {
            field: "age",
            message: "Age must be positive"
        })
    }

    if errors.is_empty() {
        Ok(input)
    } else {
        Err(errors)
    }
}

fn main() {
    let input = UserInput {
        name: "",
        email: "invalid",
        age: -5
    }

    match validate_user(input) {
        Ok(_) => print("Valid!"),
        Err(errors) => {
            for e in errors {
                print("{e.field}: {e.message}")
            }
        }
    }
}
```

**Exercise 4:**
```simplex
enum DataError {
    FileNotFound,
    ParseError(String),
    FieldMissing(String)
}

fn load_field(path: String, field: String) -> Result<String, DataError> {
    let contents = read_file(path)
        .map_err(|_| DataError::FileNotFound)?

    let json = parse_json(contents)
        .map_err(|e| DataError::ParseError(e.to_string()))?

    let value = json.get(field)
        .ok_or(DataError::FieldMissing(field))?

    Ok(value.to_string())
}

fn main() {
    match load_field("config.json", "database_url") {
        Ok(url) => print("Database URL: {url}"),
        Err(DataError::FileNotFound) => print("Config file not found"),
        Err(DataError::ParseError(e)) => print("Invalid JSON: {e}"),
        Err(DataError::FieldMissing(f)) => print("Missing field: {f}")
    }
}
```

</details>

---

*Next: [Chapter 7: Introduction to Actors →](07-actors.md)*
