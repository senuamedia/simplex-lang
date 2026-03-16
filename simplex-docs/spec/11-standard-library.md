# Simplex Standard Library Reference

**Version 0.13.0**

The Simplex standard library provides core functionality for I/O, collections, networking, and more. All modules are written in pure Simplex.

---

## Design Philosophy

The Simplex standard library is organized around the **actor model**. Concurrency is achieved through actors and async/await, not OS threads.

**There is no `thread` module.** Raw OS threads are antithetical to the actor model. For concurrent work:
- Use `runtime::spawn` for async tasks
- Use actors for isolated concurrent units
- Use channels (`sync::mpsc`, `sync::oneshot`) for communication

### Module Categories

| Category | Modules | Description |
|----------|---------|-------------|
| **Actor-Native** | `runtime`, `sync`, `signal` | Core primitives for actor/async programming |
| **Hive Support** | `env`, `crypto`, `io`, `net`, `http` | Building blocks for hive API servers |

**Notably absent**: There are no `thread`, `fs`, or `process` modules. These are antithetical to the actor model:
- **Concurrency**: Use `runtime::spawn` and actors, not threads
- **Configuration**: Use `env` module, not config files
- **Logging**: Use actor-based logging, not file I/O
- **Lifecycle**: Actor system handles shutdown, not process control

---

## Module Overview

| Module | Description |
|--------|-------------|
| `std::core` | Core language primitives |
| `std::prelude` | Commonly used items, auto-imported |
| `std::io` | I/O traits (sync + async) and buffered streams |
| `std::collections` | Data structures (HashMap, HashSet, VecDeque) |
| `std::string` | String manipulation |
| `std::time` | Time and duration |
| `std::math` | Mathematical functions |
| `std::net` | TCP/UDP networking (sync + async) |
| `std::env` | Environment variables and directories |
| `std::signal` | OS signal handling |
| `std::runtime` | Async runtime primitives (spawn, block_on) |
| `std::fmt` | Formatting and display |
| `std::compress` | Gzip/deflate compression |
| `std::crypto` | Cryptographic functions (SHA256, bcrypt, tokens) |
| `std::sync` | Synchronization primitives (Arc, Mutex, RwLock) |
| `std::cli` | Command-line interface utilities |
| `std::regex` | Regular expression support |
| `std::log` | Logging framework |
| `std::assert` | Testing assertions |
| `std::task` | Async task management |
| `std::dual` | Dual numbers for automatic differentiation |
| `std::anneal` | Self-learning annealing optimization |
| `std::http` | Actor-based HTTP server with Hive integration |
| `simplex_learning` | Real-time learning library |
| `simplex_inference` | High-performance inference via llama.cpp |
| `simplex_training` | Self-optimizing training pipelines |

**Note:** `std::sync::mpsc` and `std::sync::oneshot` are submodules of `std::sync`. Benchmarking is provided via the `sxc bench` toolchain command rather than a library module.

---

## std::io

Input/output traits and buffered streams.

### Synchronous Traits

```simplex
trait Read {
    fn read(self, buf: &mut [u8]) -> Result<i64, IoError>
    fn read_exact(self, buf: &mut [u8]) -> Result<(), IoError>
    fn read_to_end(self, buf: &mut Vec<u8>) -> Result<i64, IoError>
    fn read_to_string(self, s: &mut String) -> Result<i64, IoError>
}

trait Write {
    fn write(self, buf: &[u8]) -> Result<i64, IoError>
    fn write_all(self, buf: &[u8]) -> Result<(), IoError>
    fn flush(self) -> Result<(), IoError>
}

trait BufRead: Read {
    fn fill_buf(self) -> Result<&[u8], IoError>
    fn consume(self, amt: i64)
    fn read_line(self, buf: &mut String) -> Result<i64, IoError>
    fn lines(self) -> Lines<Self>
}

trait Seek {
    fn seek(self, pos: SeekFrom) -> Result<i64, IoError>
    fn rewind(self) -> Result<(), IoError>
    fn stream_position(self) -> Result<i64, IoError>
}
```

### Asynchronous Traits

```simplex
/// Asynchronous read trait for non-blocking I/O
trait AsyncRead {
    async fn read(self, buf: &mut [u8]) -> Result<usize, IoError>
    async fn read_exact(self, buf: &mut [u8]) -> Result<(), IoError>
    async fn read_to_end(self) -> Result<Vec<u8>, IoError>
    async fn read_line(self) -> Result<String, IoError>
}

/// Asynchronous write trait for non-blocking I/O
trait AsyncWrite {
    async fn write(self, buf: &[u8]) -> Result<usize, IoError>
    async fn write_all(self, buf: &[u8]) -> Result<(), IoError>
    async fn flush(self) -> Result<(), IoError>
}
```

### Types

```simplex
type BufReader<R: Read>           // Buffered synchronous reader
type BufWriter<W: Write>          // Buffered synchronous writer
type BufReader<R: AsyncRead>      // Buffered async reader
type BufWriter<W: AsyncWrite>     // Buffered async writer
type Cursor<T>                    // In-memory buffer
type Stdin                        // Standard input
type Stdout                       // Standard output
type Stderr                       // Standard error
```

### IoError

```simplex
enum IoError {
    UnexpectedEof,        // End of file reached unexpectedly
    WriteZero,            // Write returned zero bytes
    InvalidData,          // Invalid data encoding
    ConnectionRefused,    // Connection refused
    ConnectionReset,      // Connection reset by peer
    ConnectionClosed,     // Connection closed
    Timeout,              // Operation timed out
    AddrInUse,            // Address already in use
    AddrNotAvailable,     // Address not available
    PermissionDenied,     // Permission denied
    NotFound,             // Resource not found
    Other(String),        // Other error with message
}
```

### Functions

```simplex
fn stdin() -> Stdin
fn stdout() -> Stdout
fn stderr() -> Stderr
fn copy<R: Read, W: Write>(reader: &mut R, writer: &mut W) -> Result<i64, IoError>
fn empty() -> Empty
fn repeat(byte: u8) -> Repeat
fn sink() -> Sink
```

---

## std::collections

Core data structures.

### Vec<T> - Dynamic Array

```simplex
type Vec<T> {
    fn new() -> Vec<T>
    fn with_capacity(capacity: i64) -> Vec<T>
    fn len(self) -> i64
    fn is_empty(self) -> bool
    fn push(self, value: T)
    fn pop(self) -> Option<T>
    fn insert(self, index: i64, value: T)
    fn remove(self, index: i64) -> T
    fn get(self, index: i64) -> Option<&T>
    fn first(self) -> Option<&T>
    fn last(self) -> Option<&T>
    fn contains(self, value: &T) -> bool
    fn sort(self)
    fn reverse(self)
    fn iter(self) -> VecIter<T>
}
```

### Map<K, V> - Hash Map

```simplex
type Map<K: Hash + Eq, V> {
    fn new() -> Map<K, V>
    fn insert(self, key: K, value: V) -> Option<V>
    fn get(self, key: &K) -> Option<&V>
    fn remove(self, key: &K) -> Option<V>
    fn contains_key(self, key: &K) -> bool
    fn len(self) -> i64
    fn is_empty(self) -> bool
    fn keys(self) -> MapKeys<K, V>
    fn values(self) -> MapValues<K, V>
    fn iter(self) -> MapIter<K, V>
    fn entry(self, key: K) -> Entry<K, V>
}
```

### Set<T> - Hash Set

```simplex
type Set<T: Hash + Eq> {
    fn new() -> Set<T>
    fn insert(self, value: T) -> bool
    fn remove(self, value: &T) -> bool
    fn contains(self, value: &T) -> bool
    fn union(self, other: &Set<T>) -> Set<T>
    fn intersection(self, other: &Set<T>) -> Set<T>
    fn difference(self, other: &Set<T>) -> Set<T>
    fn is_subset(self, other: &Set<T>) -> bool
}
```

### Deque<T> - Double-Ended Queue

```simplex
type Deque<T> {
    fn new() -> Deque<T>
    fn push_front(self, value: T)
    fn push_back(self, value: T)
    fn pop_front(self) -> Option<T>
    fn pop_back(self) -> Option<T>
    fn front(self) -> Option<&T>
    fn back(self) -> Option<&T>
}
```

### PriorityQueue<T> - Binary Heap

```simplex
type PriorityQueue<T: Ord> {
    fn new() -> PriorityQueue<T>
    fn push(self, value: T)
    fn pop(self) -> Option<T>
    fn peek(self) -> Option<&T>
}
```

---

## std::string

String manipulation and UTF-8 handling.

### String Type

```simplex
type String {
    fn new() -> String
    fn from_str(s: &str) -> String
    fn from_utf8(bytes: Vec<u8>) -> Result<String, Utf8Error>
    fn len(self) -> i64
    fn is_empty(self) -> bool
    fn push(self, ch: char)
    fn push_str(self, s: &str)
    fn pop(self) -> Option<char>
    fn chars(self) -> Chars
    fn lines(self) -> Lines
    fn split(self, delimiter: &str) -> Split
    fn trim(self) -> String
    fn to_lowercase(self) -> String
    fn to_uppercase(self) -> String
    fn contains(self, pattern: &str) -> bool
    fn starts_with(self, prefix: &str) -> bool
    fn ends_with(self, suffix: &str) -> bool
    fn find(self, pattern: &str) -> Option<i64>
    fn replace(self, from: &str, to: &str) -> String
    fn repeat(self, n: i64) -> String
    fn parse<T: FromStr>(self) -> Result<T, T::Err>
}
```

### StringBuilder

```simplex
type StringBuilder {
    fn new() -> StringBuilder
    fn append(self, s: &str) -> &mut StringBuilder
    fn append_char(self, ch: char) -> &mut StringBuilder
    fn append_line(self, s: &str) -> &mut StringBuilder
    fn build(self) -> String
}
```

### Character Methods

```simplex
impl char {
    fn is_alphabetic(self) -> bool
    fn is_alphanumeric(self) -> bool
    fn is_numeric(self) -> bool
    fn is_whitespace(self) -> bool
    fn is_uppercase(self) -> bool
    fn is_lowercase(self) -> bool
    fn to_uppercase(self) -> char
    fn to_lowercase(self) -> char
    fn to_digit(self, radix: u32) -> Option<u32>
}
```

---

## std::time

Time and duration handling.

### Duration

```simplex
type Duration {
    fn zero() -> Duration
    fn from_secs(secs: i64) -> Duration
    fn from_millis(millis: i64) -> Duration
    fn from_micros(micros: i64) -> Duration
    fn from_nanos(nanos: i64) -> Duration
    fn as_secs(self) -> i64
    fn as_millis(self) -> i64
    fn as_secs_f64(self) -> f64
    fn is_zero(self) -> bool
}
```

### Instant (Monotonic Clock)

```simplex
type Instant {
    fn now() -> Instant
    fn elapsed(self) -> Duration
    fn duration_since(self, earlier: Instant) -> Duration
}
```

### SystemTime (Wall Clock)

```simplex
type SystemTime {
    fn now() -> SystemTime
    fn elapsed(self) -> Result<Duration, SystemTimeError>
    fn duration_since(self, earlier: SystemTime) -> Result<Duration, SystemTimeError>
}

const UNIX_EPOCH: SystemTime
```

### Functions

```simplex
fn sleep(dur: Duration)
fn sleep_ms(ms: i64)
fn timestamp() -> i64           // Unix timestamp in seconds
fn timestamp_millis() -> i64    // Unix timestamp in milliseconds
```

### Timer and Timeout

```simplex
type Timer {
    fn new(interval: Duration) -> Timer
    fn tick(self) -> bool
    fn wait(self)
    fn reset(self)
}

type Timeout {
    fn new(duration: Duration) -> Timeout
    fn is_expired(self) -> bool
    fn remaining(self) -> Duration
}
```

---

## std::math

Mathematical functions and constants.

### Constants

```simplex
const PI: f64 = 3.141592653589793
const E: f64 = 2.718281828459045
const TAU: f64 = 6.283185307179586
const SQRT_2: f64 = 1.4142135623730951
const LN_2: f64 = 0.6931471805599453
const LN_10: f64 = 2.302585092994046
```

### Basic Functions

```simplex
fn min<T: Ord>(a: T, b: T) -> T
fn max<T: Ord>(a: T, b: T) -> T
fn clamp<T: Ord>(value: T, min: T, max: T) -> T
fn abs(x: i64) -> i64
fn abs_f64(x: f64) -> f64
fn gcd(a: i64, b: i64) -> i64
fn lcm(a: i64, b: i64) -> i64
```

### Exponential and Logarithmic

```simplex
fn sqrt(x: f64) -> f64
fn cbrt(x: f64) -> f64
fn pow(base: f64, exp: f64) -> f64
fn exp(x: f64) -> f64
fn ln(x: f64) -> f64
fn log(x: f64, base: f64) -> f64
fn log2(x: f64) -> f64
fn log10(x: f64) -> f64
```

### Trigonometric

```simplex
fn sin(x: f64) -> f64
fn cos(x: f64) -> f64
fn tan(x: f64) -> f64
fn asin(x: f64) -> f64
fn acos(x: f64) -> f64
fn atan(x: f64) -> f64
fn atan2(y: f64, x: f64) -> f64
```

### Hyperbolic

```simplex
fn sinh(x: f64) -> f64
fn cosh(x: f64) -> f64
fn tanh(x: f64) -> f64
fn asinh(x: f64) -> f64
fn acosh(x: f64) -> f64
fn atanh(x: f64) -> f64
```

### Rounding

```simplex
fn floor(x: f64) -> f64
fn ceil(x: f64) -> f64
fn round(x: f64) -> f64
fn trunc(x: f64) -> f64
fn fract(x: f64) -> f64
```

### Special Functions

```simplex
fn gamma(x: f64) -> f64
fn lgamma(x: f64) -> f64
fn factorial(n: i64) -> i64
fn binomial(n: i64, k: i64) -> i64
fn erf(x: f64) -> f64
fn erfc(x: f64) -> f64
```

### Interpolation

```simplex
fn lerp(a: f64, b: f64, t: f64) -> f64
fn smoothstep(edge0: f64, edge1: f64, x: f64) -> f64
fn degrees(radians: f64) -> f64
fn radians(degrees: f64) -> f64
```

---

## std::net

Networking primitives.

### IP Addresses

```simplex
type Ipv4Addr {
    fn new(a: u8, b: u8, c: u8, d: u8) -> Ipv4Addr
    fn localhost() -> Ipv4Addr
    fn unspecified() -> Ipv4Addr
    fn is_loopback(self) -> bool
    fn is_private(self) -> bool
}

type Ipv6Addr {
    fn new(a: u16, ..., h: u16) -> Ipv6Addr
    fn localhost() -> Ipv6Addr
}

enum IpAddr { V4(Ipv4Addr), V6(Ipv6Addr) }
```

### Socket Addresses

```simplex
type SocketAddr {
    ip: String,              // IP address as string
    port: u16,               // Port number
}

impl SocketAddr {
    fn new(ip: &str, port: u16) -> SocketAddr
    fn parse(addr: &str) -> Result<SocketAddr, IoError>  // Parse "ip:port"
}
```

### Async TCP

```simplex
/// TCP listener for accepting incoming connections
type TcpListener {
    handle: i64,
    local_addr: SocketAddr,
}

impl TcpListener {
    /// Bind to address and start listening
    async fn bind(addr: &str) -> Result<TcpListener, IoError>

    /// Accept incoming connection, returns stream and remote address
    async fn accept(self) -> Result<(TcpStream, SocketAddr), IoError>

    /// Get local address
    fn local_addr(self) -> &SocketAddr
}

/// Bidirectional TCP connection implementing AsyncRead + AsyncWrite
type TcpStream {
    handle: i64,
    peer_addr: SocketAddr,
}

impl TcpStream {
    /// Connect to remote address
    async fn connect(addr: &str) -> Result<TcpStream, IoError>

    /// Connect with timeout
    async fn connect_timeout(addr: &str, timeout_ms: u64) -> Result<TcpStream, IoError>

    /// Get peer address
    fn peer_addr(self) -> &SocketAddr

    /// Graceful shutdown
    async fn shutdown(self) -> Result<(), IoError>

    /// Set TCP nodelay (disable Nagle's algorithm)
    fn set_nodelay(self, nodelay: bool)

    /// Set read/write timeouts
    fn set_read_timeout(self, timeout_ms: Option<u64>)
    fn set_write_timeout(self, timeout_ms: Option<u64>)
}

// TcpStream implements AsyncRead and AsyncWrite
impl AsyncRead for TcpStream { ... }
impl AsyncWrite for TcpStream { ... }
impl Clone for TcpStream { ... }  // Clone creates new handle to same connection
```

### Synchronous TCP

```simplex
type TcpListener {
    fn bind(addr: &str) -> Result<TcpListener, IoError>
    fn accept(self) -> Result<(TcpStream, SocketAddr), IoError>
    fn incoming(self) -> Incoming
    fn set_nonblocking(self, nonblocking: bool) -> Result<(), IoError>
}

type TcpStream {
    fn connect(addr: &str) -> Result<TcpStream, IoError>
    fn peer_addr(self) -> Result<SocketAddr, IoError>
    fn shutdown(self, how: Shutdown) -> Result<(), IoError>
    fn set_nodelay(self, nodelay: bool) -> Result<(), IoError>
}

// TcpStream implements Read and Write
```

### UDP

```simplex
type UdpSocket {
    fn bind(addr: &str) -> Result<UdpSocket, IoError>
    fn recv_from(self, buf: &mut [u8]) -> Result<(i64, SocketAddr), IoError>
    fn send_to(self, buf: &[u8], addr: &str) -> Result<i64, IoError>
    fn connect(self, addr: &str) -> Result<(), IoError>
    fn recv(self, buf: &mut [u8]) -> Result<i64, IoError>
    fn send(self, buf: &[u8]) -> Result<i64, IoError>
}
```

### DNS

```simplex
fn lookup_host(host: &str) -> Result<Vec<IpAddr>, IoError>
fn resolve(host: &str, port: u16) -> Result<Vec<SocketAddr>, IoError>
```

---

## std::env

Environment variables and directory operations.

### Environment Variables

```simplex
/// Get environment variable by key
fn var(key: &str) -> Result<String, VarError>

/// Set environment variable
fn set_var(key: &str, value: &str)

/// Remove environment variable
fn remove_var(key: &str)

/// Check if environment variable is set
fn var_is_set(key: &str) -> bool
```

### Directories

```simplex
/// Get current working directory
fn current_dir() -> Result<String, VarError>

/// Set current working directory
fn set_current_dir(path: &str) -> Result<(), VarError>
```

### VarError

```simplex
/// Error when accessing environment variables
enum VarError {
    /// Variable not found
    NotPresent,
    /// Variable contains invalid UTF-8
    NotUnicode(String),
}

impl Display for VarError { ... }
impl std::error::Error for VarError { ... }
```

### Example

```simplex
use simplex_std::env::{var, set_var, current_dir, VarError}

// Get PATH
match var("PATH") {
    Ok(path) => print("PATH = {path}"),
    Err(VarError::NotPresent) => print("PATH not set"),
    Err(VarError::NotUnicode(s)) => print("Invalid UTF-8: {s}"),
}

// Set custom variable
set_var("MY_VAR", "hello")

// Get current directory
let cwd = current_dir()?
```

---

## std::signal

OS signal handling with async support.

### Signal Types

```simplex
/// OS signal types
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Signal {
    Interrupt,   // SIGINT (2) - Ctrl+C
    Terminate,   // SIGTERM (15) - termination request
    Hangup,      // SIGHUP (1) - terminal disconnect
    User1,       // SIGUSR1 (10) - user-defined
    User2,       // SIGUSR2 (12) - user-defined
}

impl Signal {
    /// Get platform-specific signal number
    fn as_i32(self) -> i32
}
```

### Signal Futures

```simplex
/// Wait for Ctrl+C (SIGINT)
async fn ctrl_c() -> ()

/// Wait for SIGTERM
async fn terminate() -> ()

/// Wait for SIGHUP
async fn hangup() -> ()

/// Wait for SIGUSR1
async fn user1() -> ()

/// Wait for SIGUSR2
async fn user2() -> ()
```

### SignalFuture

```simplex
/// Future that completes when signal is received
type SignalFuture {
    signal: Signal,
    handle: i64,
    registered: bool,
}

impl Future for SignalFuture {
    type Output = ();
    fn poll(self: Pin<&mut Self>, cx: &mut Context) -> Poll<()>
}

impl Drop for SignalFuture {
    fn drop(&mut self)  // Unregisters signal handler
}
```

### Example

```simplex
use simplex_std::signal::ctrl_c

// Wait for Ctrl+C with graceful shutdown
print("Press Ctrl+C to exit...")
ctrl_c().await
print("Shutting down gracefully...")
```

---

## std::runtime

Async runtime primitives for spawning and managing tasks.

### Block On

```simplex
/// Run a future to completion on the current thread
/// Main entry point for async programs
fn block_on<F, T>(future: F) -> T
where
    F: Future<Output = T>
```

### Spawn

```simplex
/// Spawn a future as a background task
fn spawn<F>(future: F) -> JoinHandle<F::Output>
where
    F: Future + Send + 'static,
    F::Output: Send + 'static
```

### JoinHandle

```simplex
/// Handle to a spawned task, can be awaited
type JoinHandle<T> {
    handle: i64,
    _marker: PhantomData<T>,
}

impl<T> Future for JoinHandle<T> {
    type Output = Result<T, JoinError>;
    fn poll(self: Pin<&mut Self>, cx: &mut Context) -> Poll<Self::Output>
}

impl<T> Drop for JoinHandle<T> {
    fn drop(&mut self)  // Cleans up task handle
}
```

### JoinError

```simplex
/// Error when joining a task
enum JoinError {
    /// Task was cancelled
    Cancelled,
    /// Task panicked
    Panic,
}

impl Display for JoinError { ... }
impl std::error::Error for JoinError { ... }
```

### Example

```simplex
use simplex_std::runtime::{block_on, spawn}

fn main() {
    block_on(async {
        let handle = spawn(async {
            // Background computation
            compute_value()
        })

        // Do other work...

        // Wait for result
        let result = handle.await?
    })
}
```

---

## std::http

Actor-based HTTP server with native Hive integration. No threads - concurrency through actors.

### Core Types

```simplex
/// HTTP method
enum Method {
    Get, Post, Put, Delete, Patch, Head, Options, Connect, Trace
}

/// HTTP status code
enum StatusCode {
    Ok,                     // 200
    Created,                // 201
    NoContent,              // 204
    BadRequest,             // 400
    Unauthorized,           // 401
    Forbidden,              // 403
    NotFound,               // 404
    InternalServerError,    // 500
    // ... and more
}

impl StatusCode {
    fn code(&self) -> u16
    fn reason(&self) -> &str
    fn is_success(&self) -> bool
    fn is_error(&self) -> bool
}
```

### Request and Response

```simplex
/// HTTP request
struct Request {
    method: Method,
    path: String,
    headers: Headers,
    body: Body,
    params: HashMap<String, String>,   // Route parameters
    query: HashMap<String, String>,    // Query string
}

impl Request {
    fn method(&self) -> &Method
    fn path(&self) -> &str
    fn header(&self, name: &str) -> Option<&str>
    fn param(&self, name: &str) -> Option<&str>
    fn param_as<T: FromStr>(&self, name: &str) -> Option<T>
    fn query_param(&self, name: &str) -> Option<&str>
    fn body_text(&self) -> Result<String, HttpError>
    fn body_json<T: Deserialize>(&self) -> Result<T, HttpError>
    fn extension<T: 'static>(&self) -> Option<&T>
    fn set_extension<T: 'static>(&mut self, value: T)
}

/// HTTP response
struct Response {
    status: StatusCode,
    headers: Headers,
    body: Body,
}

impl Response {
    // Constructors
    fn ok() -> Response
    fn created() -> Response
    fn not_found() -> Response
    fn bad_request(msg: &str) -> Response
    fn unauthorized() -> Response
    fn internal_error(msg: &str) -> Response

    // Body builders
    fn text(body: &str) -> Response
    fn html(body: &str) -> Response
    fn json<T: Serialize>(data: &T) -> Result<Response, HttpError>
    fn redirect(url: &str) -> Response

    // Builder pattern
    fn with_status(self, status: StatusCode) -> Response
    fn with_header(self, name: &str, value: &str) -> Response
    fn with_body(self, body: Body) -> Response
}
```

### Handler and Middleware

```simplex
/// Handler trait for processing requests
trait Handler: Send + Sync {
    async fn handle(&self, req: Request) -> Response
}

/// Middleware trait for cross-cutting concerns
trait Middleware: Send + Sync {
    async fn handle(&self, req: Request, next: Next) -> Response
}

/// Next handler in middleware chain
struct Next { ... }
impl Next {
    async fn run(self, req: Request) -> Response
}
```

### Router

```simplex
/// HTTP router
struct Router { ... }

impl Router {
    fn new() -> Router
    fn get(self, path: &str, handler: impl Handler) -> Router
    fn post(self, path: &str, handler: impl Handler) -> Router
    fn put(self, path: &str, handler: impl Handler) -> Router
    fn delete(self, path: &str, handler: impl Handler) -> Router
    fn nest(self, prefix: &str, router: Router) -> Router
    fn with<M: Middleware>(self, middleware: M) -> Router
    fn fallback(self, handler: impl Handler) -> Router
}
```

### Built-in Middleware

```simplex
/// Logging middleware
struct Logger { ... }
impl Logger {
    fn new() -> Logger
    fn with_level(level: LogLevel) -> Logger
}

/// CORS middleware
struct Cors { ... }
impl Cors {
    fn permissive() -> Cors
    fn builder() -> CorsBuilder
}

/// Rate limiter (actor-based)
actor RateLimiter {
    fn new(max_requests: u32, window: Duration) -> RateLimiter
}

/// Timeout middleware
struct Timeout { ... }
impl Timeout {
    fn new(duration: Duration) -> Timeout
}

/// Compression middleware
struct Compression { ... }
impl Compression {
    fn new() -> Compression
}
```

### HTTP Server Actor

```simplex
/// HTTP server actor
actor HttpServer { ... }

impl HttpServer {
    fn bind(addr: &str) -> ServerBuilder
}

struct ServerBuilder { ... }
impl ServerBuilder {
    fn router(self, router: Router) -> ServerBuilder
    fn config(self, config: ServerConfig) -> ServerBuilder
    fn with_hive(self, hive: HiveRef) -> ServerBuilder
    fn graceful_shutdown<F: Future>(self, signal: F) -> ServerBuilder
    async fn serve(self) -> Result<(), HttpError>
}

struct ServerConfig {
    max_connections: usize,    // Default: 10000
    read_timeout: Duration,    // Default: 30s
    write_timeout: Duration,   // Default: 30s
    max_request_size: usize,   // Default: 10MB
}
```

### Hive Integration

```simplex
/// Handler that routes to a specialist
struct HiveHandler<S: Specialist> { ... }

impl HiveRef {
    /// Create handler for specialist type
    fn handler<S: Specialist>(&self) -> HiveHandler<S>
}
```

### WebSocket Support

```simplex
enum WsMessage {
    Text(String),
    Binary(Vec<u8>),
    Ping(Vec<u8>),
    Pong(Vec<u8>),
    Close(Option<CloseReason>),
}

trait WebSocketHandler: Send + Sync {
    async fn on_connect(&mut self, ws: &WebSocket)
    async fn on_message(&mut self, ws: &WebSocket, msg: WsMessage)
    async fn on_close(&mut self, ws: &WebSocket, reason: Option<CloseReason>)
}

struct WebSocket { ... }
impl WebSocket {
    async fn send(&self, msg: WsMessage) -> Result<(), HttpError>
    async fn close(&self, reason: Option<CloseReason>) -> Result<(), HttpError>
}
```

### Server-Sent Events

```simplex
struct SseEvent {
    id: Option<String>,
    event: Option<String>,
    data: String,
}

impl SseEvent {
    fn new(data: &str) -> SseEvent
    fn with_id(self, id: &str) -> SseEvent
    fn with_event(self, event: &str) -> SseEvent
}

struct SseStream { ... }
impl SseStream {
    async fn send(&self, event: SseEvent) -> Result<(), HttpError>
}
```

### HttpError

```simplex
enum HttpError {
    BadRequest { message: String },
    Unauthorized { message: String },
    Forbidden { message: String },
    NotFound { path: String },
    Internal { message: String },
    JsonError { message: String },
    // ... more variants
}

impl From<HttpError> for Response { ... }
```

### Example

```simplex
use simplex_std::http::{HttpServer, Router, Request, Response}
use simplex_std::signal::ctrl_c

// Define handlers
async fn health_check(_req: Request) -> Response {
    Response::json(&json!({ "status": "healthy" })).unwrap()
}

async fn get_user(req: Request) -> Response {
    let id: u64 = req.param_as("id").unwrap_or(0)
    Response::json(&json!({ "id": id })).unwrap()
}

// Build router
let router = Router::new()
    .get("/health", health_check)
    .get("/users/:id", get_user)
    .with(Logger::new())
    .with(Cors::permissive())

// Start server
HttpServer::bind("0.0.0.0:8080")
    .router(router)
    .graceful_shutdown(ctrl_c())
    .serve()
    .await?
```

### Hive API Example

```simplex
use simplex_std::http::{HttpServer, Router}
use simplex_hive::{Hive, specialist}

specialist QuerySpecialist {
    model: SLM,
    type Input = QueryRequest
    type Output = QueryResponse

    async fn process(&self, input: QueryRequest) -> QueryResponse {
        let response = self.model.complete(&input.query).await
        QueryResponse { answer: response.text }
    }
}

let hive = Hive::builder()
    .add_specialist(QuerySpecialist::new(SLM::load("model")))
    .build().await?

let router = Router::new()
    .post("/api/query", hive.handler::<QuerySpecialist>())
    .with(RateLimiter::new(100, Duration::from_secs(60)))

HttpServer::bind("0.0.0.0:8080")
    .router(router)
    .with_hive(hive)
    .serve().await?
```

---

## std::fmt

Formatting and display.

### Traits

```simplex
trait Display {
    fn fmt(self, f: &mut Formatter) -> Result<(), FormatError>
}

trait Debug {
    fn fmt(self, f: &mut Formatter) -> Result<(), FormatError>
}

trait Binary { ... }
trait Octal { ... }
trait LowerHex { ... }
trait UpperHex { ... }
```

### Formatter

```simplex
type Formatter {
    fn write_str(self, s: &str) -> Result<(), FormatError>
    fn write_char(self, c: char) -> Result<(), FormatError>
    fn width(self) -> Option<i64>
    fn precision(self) -> Option<i64>
    fn fill(self) -> char
    fn align(self) -> Alignment
    fn alternate(self) -> bool
    fn pad(self, s: &str) -> Result<(), FormatError>
}
```

### Functions

```simplex
fn format(fmt: &str, values: Vec<FormatValue>) -> String
fn write<W: Write>(writer: &mut W, fmt: &str, values: Vec<FormatValue>) -> Result<(), FormatError>
```

---

## std::compress

Compression and decompression utilities using zlib.

### One-Shot Functions

```simplex
/// Compress data using gzip
fn gzip(data: &[u8]) -> Result<Vec<u8>, CompressError>

/// Decompress gzip data
fn gunzip(data: &[u8]) -> Result<Vec<u8>, CompressError>

/// Compress data using gzip with specified compression level (0-9)
fn gzip_level(data: &[u8], level: u32) -> Result<Vec<u8>, CompressError>

/// Compress data using raw DEFLATE (no header/trailer)
fn deflate(data: &[u8]) -> Result<Vec<u8>, CompressError>

/// Decompress raw DEFLATE data
fn inflate(data: &[u8]) -> Result<Vec<u8>, CompressError>
```

### Streaming Compression

```simplex
/// Gzip compressor for streaming data
type GzipEncoder {
    handle: i64,
}

impl GzipEncoder {
    /// Create new encoder with default compression level (6)
    fn new() -> GzipEncoder

    /// Create encoder with custom compression level (0-9)
    fn with_level(level: u32) -> GzipEncoder

    /// Compress a chunk of data
    fn compress(self, data: &[u8]) -> Result<Vec<u8>, CompressError>

    /// Finish compression and return final bytes
    fn finish(self) -> Result<Vec<u8>, CompressError>
}

/// Gzip decompressor for streaming data
type GzipDecoder {
    handle: i64,
}

impl GzipDecoder {
    /// Create new decoder
    fn new() -> GzipDecoder

    /// Decompress a chunk of data
    fn decompress(self, data: &[u8]) -> Result<Vec<u8>, CompressError>

    /// Finish decompression
    fn finish(self) -> Result<Vec<u8>, CompressError>
}
```

### Error Types

```simplex
enum CompressError {
    InvalidData,                    // Corrupt compressed data
    BufferTooSmall,                 // Output buffer insufficient
    CompressionFailed(String),      // Compression error with details
    DecompressionFailed(String),    // Decompression error with details
}
```

### Example

```simplex
use simplex_std::compress::{gzip, gunzip}

let data = b"Hello, World!"
let compressed = gzip(data)?
let decompressed = gunzip(&compressed)?
assert_eq(data, &decompressed[..])
```

---

## std::sync::mpsc

Multi-producer, single-consumer channel for async message passing.

### Channel Creation

```simplex
/// Create bounded channel with capacity
fn channel<T>(capacity: usize) -> (Sender<T>, Receiver<T>)

/// Create unbounded channel (no capacity limit)
fn unbounded<T>() -> (Sender<T>, Receiver<T>)
```

### Sender

```simplex
type Sender<T> {
    handle: i64,
}

impl<T> Sender<T> {
    /// Send a value, waiting if channel is full (async)
    async fn send(self, value: T) -> Result<(), SendError<T>>

    /// Try to send without blocking
    fn try_send(self, value: T) -> Result<(), TrySendError<T>>

    /// Check if receiver has been dropped
    fn is_closed(self) -> bool
}

impl<T> Clone for Sender<T>  // Multi-producer support
```

### Receiver

```simplex
type Receiver<T> {
    handle: i64,
}

impl<T> Receiver<T> {
    /// Receive a value, waiting if channel is empty (async)
    async fn recv(self) -> Option<T>

    /// Try to receive without blocking
    fn try_recv(self) -> Result<T, TryRecvError>

    /// Close the receiver (no more receives)
    fn close(self)
}
```

### Error Types

```simplex
/// Error when send fails because receiver was dropped
struct SendError<T> {
    value: T,  // The value that couldn't be sent
}

/// Error when try_send fails
enum TrySendError<T> {
    Full(T),         // Channel at capacity
    Disconnected(T), // Receiver dropped
}

/// Error when try_recv fails
enum TryRecvError {
    Empty,        // No messages available
    Disconnected, // All senders dropped
}
```

### Example

```simplex
use simplex_std::sync::mpsc

let (tx, rx) = mpsc::channel::<i64>(10)

// Producer
spawn(async {
    for i in 0..5 {
        tx.send(i).await.unwrap()
    }
})

// Consumer
while let Some(value) = rx.recv().await {
    print("Received: {value}")
}
```

---

## std::sync::oneshot

Single-value, single-use channels for one-time communication.

### Channel Creation

```simplex
/// Create a oneshot channel
/// Returns (Sender, Receiver) pair
fn channel<T>() -> (Sender<T>, Receiver<T>)
```

### Sender

```simplex
/// Sending half of a oneshot channel
type Sender<T> {
    inner: Arc<OneshotInner<T>>,
}

impl<T> Sender<T> {
    /// Send a value, consuming the sender
    /// Returns Err(value) if receiver was dropped
    fn send(self, value: T) -> Result<(), T>

    /// Check if receiver is still connected
    fn is_closed(&self) -> bool
}

// Note: Sender is NOT Clone - can only send once
```

### Receiver

```simplex
/// Receiving half of a oneshot channel
type Receiver<T> {
    inner: Arc<OneshotInner<T>>,
}

impl<T> Receiver<T> {
    /// Wait for the value (async, consumes receiver)
    async fn recv(self) -> Result<T, RecvError>

    /// Try to receive without blocking
    fn try_recv(&self) -> Result<T, TryRecvError>

    /// Close the receiving end
    fn close(&self)
}
```

### Error Types

```simplex
/// Error when receiving from closed channel
#[derive(Debug, Clone)]
enum RecvError {
    /// Channel was closed before a value was sent
    Closed,
}

/// Error when try_recv fails
#[derive(Debug, Clone)]
enum TryRecvError {
    /// No value available yet
    Empty,
    /// Channel was closed
    Closed,
}
```

### Example

```simplex
use simplex_std::sync::oneshot

// Create oneshot channel
let (tx, rx) = oneshot::channel::<String>()

// Sender sends exactly once
spawn(async {
    tx.send("Hello!".to_string()).unwrap()
})

// Receiver gets the value
let message = rx.recv().await?
print("Received: {message}")
```

---

## std::crypto

Cryptographic functions including hashing and password verification.

### Password Hashing (bcrypt)

```simplex
/// Hash a password using bcrypt
/// cost: work factor (4-31, recommended: 12)
fn bcrypt_hash(password: &str, cost: u32) -> Result<String, CryptoError>

/// Verify password against bcrypt hash
fn bcrypt_verify(password: &str, hash: &str) -> Result<bool, CryptoError>
```

### Token Generation

```simplex
/// Generate cryptographically secure random token
/// byte_len: number of random bytes (output is hex, 2x length)
fn generate_token(byte_len: usize) -> String
```

### CryptoError

```simplex
/// Error type for cryptographic operations
enum CryptoError {
    /// Hashing operation failed
    HashError(String),
    /// Verification operation failed
    VerifyError(String),
}

impl Display for CryptoError { ... }
impl std::error::Error for CryptoError { ... }
```

### Example

```simplex
use simplex_std::crypto::{bcrypt_hash, bcrypt_verify, generate_token, CryptoError}

// Hash a password (cost 12 is recommended)
let hash = bcrypt_hash("my_password", 12)?

// Verify password
match bcrypt_verify("my_password", &hash) {
    Ok(true) => print("Password matches!"),
    Ok(false) => print("Password incorrect"),
    Err(CryptoError::VerifyError(msg)) => print("Error: {msg}"),
    _ => {}
}

// Generate secure token (32 bytes = 64 hex chars)
let token = generate_token(32)
print("Token: {token}")
```

---

## Message Passing (std::runtime::channel)

Channel-based communication between actors (runtime channels).

### MPSC Channels (Runtime)

```simplex
fn mpsc_channel<T>(capacity: i64) -> (Sender<T>, Receiver<T>)
fn mpsc_unbounded<T>() -> (Sender<T>, Receiver<T>)

impl Sender<T> {
    fn send(self, value: T) -> Result<(), ChannelError>
    fn try_send(self, value: T) -> Result<(), ChannelError>
    fn is_closed(self) -> bool
}

impl Receiver<T> {
    fn receive(self) -> Result<T, ChannelError>
    fn try_receive(self) -> Option<T>
}
```

### Oneshot Channels

```simplex
fn oneshot<T>() -> (OneshotSender<T>, OneshotReceiver<T>)
```

### Broadcast Channels

```simplex
fn broadcast<T>(capacity: i64) -> Broadcaster<T>

impl Broadcaster<T> {
    fn send(self, value: T) -> Result<(), ChannelError>
    fn subscribe(self, capacity: i64) -> BroadcastReceiver<T>
}
```

### Watch Channels

```simplex
fn watch<T>(initial: T) -> (WatchSender<T>, WatchReceiver<T>)

impl WatchReceiver<T> {
    fn borrow(self) -> &T
    fn has_changed(self) -> bool
}
```

### Select

```simplex
fn select<T>(receivers: Vec<&Receiver<T>>) -> SelectResult<T>
fn select_timeout<T>(receivers: Vec<&Receiver<T>>, timeout_ms: i64) -> SelectResult<T>
```

---

## simplex_learning (v0.7.0)

Real-time continuous learning library for AI specialists.

### Tensor Operations

```simplex
use simplex_learning::tensor::{Tensor, ops};

type Tensor {
    fn zeros(shape: &[i64]) -> Tensor
    fn ones(shape: &[i64]) -> Tensor
    fn randn(shape: &[i64]) -> Tensor
    fn from_slice(data: &[f64], shape: &[i64]) -> Tensor
    fn requires_grad_(self) -> Tensor
    fn grad(self) -> Option<Tensor>
    fn backward(self)
    fn shape(self) -> &[i64]
    fn reshape(self, shape: &[i64]) -> Tensor
}

// Operations
fn ops::matmul(a: &Tensor, b: &Tensor) -> Tensor
fn ops::batch_matmul(a: &Tensor, b: &Tensor) -> Tensor
fn ops::add(a: &Tensor, b: &Tensor) -> Tensor
fn ops::mul(a: &Tensor, b: &Tensor) -> Tensor
fn ops::relu(x: &Tensor) -> Tensor
fn ops::sigmoid(x: &Tensor) -> Tensor
fn ops::softmax(x: &Tensor, dim: i64) -> Tensor
fn ops::mse_loss(pred: &Tensor, target: &Tensor) -> Tensor
fn ops::cross_entropy(logits: &Tensor, labels: &Tensor) -> Tensor
```

### Optimizers

```simplex
use simplex_learning::optim::{StreamingSGD, StreamingAdam, AdamW};

type StreamingSGD {
    fn new(lr: f64) -> StreamingSGD
    fn momentum(self, m: f64) -> StreamingSGD
    fn weight_decay(self, wd: f64) -> StreamingSGD
    fn max_grad_norm(self, max: f64) -> StreamingSGD
    fn step(self, params: &mut [Tensor])
}

type StreamingAdam {
    fn new(lr: f64) -> StreamingAdam
    fn betas(self, b1: f64, b2: f64) -> StreamingAdam
    fn eps(self, e: f64) -> StreamingAdam
    fn accumulation_steps(self, steps: i64) -> StreamingAdam
    fn step(self, params: &mut [Tensor])
}

type AdamW {
    fn new(lr: f64) -> AdamW
    fn weight_decay(self, wd: f64) -> AdamW
    fn step(self, params: &mut [Tensor])
}

// Gradient clipping
fn clip_grad_norm(params: &mut [Tensor], max_norm: f64) -> f64
fn clip_grad_value(params: &mut [Tensor], max_value: f64)
```

### Safety

```simplex
use simplex_learning::safety::{SafeLearner, SafeFallback, ConstraintManager};

enum SafeFallback<T> {
    fn with_default(value: T) -> SafeFallback<T>
    fn last_good() -> SafeFallback<T>
    fn with_function(f: fn(&Input) -> T) -> SafeFallback<T>
    fn checkpoint(path: &str) -> SafeFallback<T>
    fn skip_update() -> SafeFallback<T>
}

type SafeLearner<T> {
    fn new(learner: OnlineLearner, fallback: SafeFallback<T>) -> SafeLearner<T>
    fn with_validator(self, f: fn(&T) -> bool) -> SafeLearner<T>
    fn max_failures(self, n: i64) -> SafeLearner<T>
    fn try_process(self, input: &Input, f: fn(&Input) -> T) -> Result<T, SafetyError>
}

type ConstraintManager {
    fn new() -> ConstraintManager
    fn add_soft(self, constraint: Constraint) -> ConstraintManager
    fn add_hard(self, constraint: Constraint) -> ConstraintManager
    fn check(self, metrics: &Metrics) -> ConstraintResult
}
```

### Distributed Learning

```simplex
use simplex_learning::distributed::{
    FederatedLearner, KnowledgeDistiller, HiveBeliefManager, HiveLearningCoordinator
};

// Federated Learning
type FederatedLearner {
    fn new(config: FederatedConfig, params: Vec<Tensor>) -> FederatedLearner
    fn submit_update(self, update: NodeUpdate)
    fn global_params(self) -> Vec<Tensor>
    fn round(self) -> i64
}

enum AggregationStrategy {
    FedAvg,
    WeightedAvg,
    PerformanceWeighted,
    Median,
    TrimmedMean,
    AttentionWeighted,
}

// Knowledge Distillation
type KnowledgeDistiller {
    fn new(config: DistillationConfig) -> KnowledgeDistiller
    fn distillation_loss(self, student: &Tensor, teacher: &Tensor, labels: &Tensor) -> Tensor
}

// Belief Resolution
type HiveBeliefManager {
    fn new(strategy: ConflictResolution) -> HiveBeliefManager
    fn submit_belief(self, belief: Belief)
    fn consensus(self, key: &str) -> f64
    fn all_beliefs(self) -> Vec<Belief>
}

enum ConflictResolution {
    HighestConfidence,
    MostRecent,
    MostEvidence,
    EvidenceWeighted,
    BayesianCombination,
    SemanticWeighted,
    MajorityVote,
}

// Hive Coordinator
type HiveLearningCoordinator {
    fn new(config: HiveLearningConfig, params: Vec<Tensor>) -> HiveLearningCoordinator
    fn register_specialist(self, name: &str, params: Vec<Tensor>)
    fn submit_gradients(self, name: &str, grads: &[Tensor])
    fn submit_belief(self, name: &str, belief: Belief)
    fn step(self)
    fn get_specialist_params(self, name: &str) -> Vec<Tensor>
}
```

### Runtime

```simplex
use simplex_learning::runtime::{OnlineLearner, Checkpoint, Metrics};

type OnlineLearner {
    fn new(params: Vec<Tensor>) -> OnlineLearner
    fn optimizer<O: Optimizer>(self, opt: O) -> OnlineLearner
    fn fallback(self, fb: SafeFallback) -> OnlineLearner
    fn constraint(self, c: Constraint) -> OnlineLearner
    fn forward(self, input: &Tensor) -> Tensor
    fn learn(self, feedback: &FeedbackSignal)
    fn metrics(self) -> Metrics
}

type Checkpoint {
    fn save(path: &str, learner: &OnlineLearner) -> Result<(), IoError>
    fn load(path: &str) -> Result<OnlineLearner, IoError>
}

type Metrics {
    loss: f64,
    lr: f64,
    grad_norm: f64,
    step_count: i64,
    samples_per_sec: f64,
}
```

---

## std::dual (v0.8.0)

Native dual numbers for forward-mode automatic differentiation.

### Core Types

```simplex
type dual {
    val: f64,  // Function value
    der: f64,  // Derivative value
}

// Constructors
fn dual::new(val: f64, der: f64) -> dual
fn dual::constant(val: f64) -> dual        // der = 0
fn dual::variable(val: f64) -> dual        // der = 1
fn dual::zero() -> dual                    // val = 0, der = 0

// Multi-dimensional for gradients
type multidual<const N: usize> {
    val: f64,
    der: [f64; N],
}

fn multidual<N>::variable(val: f64, dim: usize) -> multidual<N>
fn multidual<N>::gradient(self) -> [f64; N]

// Second-order for Hessians
type dual2 {
    val: f64,
    d1: f64,   // First derivative
    d2: f64,   // Second derivative
}
```

### Arithmetic Operations

All arithmetic propagates derivatives via chain rule:

```simplex
impl Add for dual { ... }  // d/dx (a + b) = da + db
impl Sub for dual { ... }  // d/dx (a - b) = da - db
impl Mul for dual { ... }  // d/dx (a * b) = a*db + da*b
impl Div for dual { ... }  // d/dx (a / b) = (da*b - a*db) / b²
impl Neg for dual { ... }  // d/dx (-a) = -da
```

### Transcendental Functions

```simplex
impl dual {
    fn sin(self) -> dual     // d/dx sin(x) = cos(x)
    fn cos(self) -> dual     // d/dx cos(x) = -sin(x)
    fn tan(self) -> dual     // d/dx tan(x) = sec²(x)
    fn exp(self) -> dual     // d/dx e^x = e^x
    fn ln(self) -> dual      // d/dx ln(x) = 1/x
    fn sqrt(self) -> dual    // d/dx √x = 1/(2√x)
    fn pow(self, n: f64) -> dual  // d/dx x^n = n*x^(n-1)
    fn tanh(self) -> dual    // d/dx tanh(x) = 1 - tanh²(x)
    fn sigmoid(self) -> dual // d/dx σ(x) = σ(x)(1-σ(x))
    fn relu(self) -> dual    // d/dx relu(x) = 1 if x > 0 else 0
    fn max(self, other: dual) -> dual
    fn min(self, other: dual) -> dual
    fn abs(self) -> dual
}
```

### Differentiation Utilities

```simplex
use simplex::diff;

// Single-variable derivative
fn diff::derivative<F>(f: F, x: f64) -> f64
where F: Fn(dual) -> dual;

// Multi-variable gradient
fn diff::gradient<F, const N: usize>(f: F, x: [f64; N]) -> [f64; N]
where F: Fn([multidual<N>; N]) -> multidual<N>;

// Jacobian matrix
fn diff::jacobian<F, const N: usize, const M: usize>(f: F, x: [f64; N]) -> [[f64; N]; M];

// Hessian matrix
fn diff::hessian<F, const N: usize>(f: F, x: [f64; N]) -> [[f64; N]; N];
```

---

## std::anneal

Self-learning annealing where optimization schedules are learned through meta-gradients.

### Core Types

```simplex
use simplex_std::anneal::{LearnableSchedule, MetaOptimizer, AnnealConfig};

/// Learnable schedule with dual number parameters
type LearnableSchedule {
    initial_temp: dual,       // T₀: starting temperature
    cool_rate: dual,          // α: exponential decay rate
    min_temp: dual,           // T_min: temperature floor
    reheat_threshold: dual,   // ρ: stagnation steps before reheat
    reheat_intensity: dual,   // γ: reheat magnitude
    oscillation_amp: dual,    // β: oscillation amplitude
    oscillation_freq: dual,   // ω: oscillation frequency
    accept_threshold: dual,   // τ: soft acceptance threshold
    accept_sharpness: dual,   // Acceptance boundary sharpness
}

impl LearnableSchedule {
    fn new() -> LearnableSchedule
    fn from_params(initial_temp: f64, cool_rate: f64, ...) -> LearnableSchedule
    fn temperature(self, step: dual, stagnation: dual) -> dual
    fn accept_probability(self, delta_e: dual, temp: dual) -> dual
    fn gradient(self) -> ScheduleGradient
    fn update(self, grad: &ScheduleGradient, lr: f64)
}
```

### Meta-Optimizer

```simplex
type MetaOptimizer {
    schedule: LearnableSchedule,
    meta_learning_rate: f64,
    regularization: f64,
    history: MetaHistory,
}

impl MetaOptimizer {
    fn new() -> MetaOptimizer
    fn with_schedule(schedule: LearnableSchedule) -> MetaOptimizer
    fn learning_rate(self, lr: f64) -> MetaOptimizer
    fn anneal_episode<S, F, N>(self, initial: S, objective: F, neighbor: N, steps: i64) -> (S, dual)
    fn optimize<S, F, N>(self, initial: S, objective: F, neighbor: N, config: &AnnealConfig) -> S
}
```

### Configuration

```simplex
type AnnealConfig {
    meta_epochs: i64,           // Number of meta-training epochs
    steps_per_epoch: i64,       // Annealing steps per epoch
    meta_learning_rate: f64,    // Learning rate for schedule params
    regularization: f64,        // L2 regularization strength
    convergence_threshold: i64, // Stagnation threshold
}

impl AnnealConfig {
    fn default() -> AnnealConfig    // Balanced for most problems
    fn quick() -> AnnealConfig      // Fast exploration
    fn thorough() -> AnnealConfig   // Difficult problems
}
```

### Convenience API

```simplex
/// Main entry point for self-learning annealing
fn self_learn_anneal<S, F, N>(
    objective: F,
    initial: S,
    neighbor: N,
    config: AnnealConfig
) -> S
where
    F: Fn(&S) -> dual,
    N: Fn(&S) -> S;

/// Simple (non-learning) simulated annealing for comparison
fn simple_anneal<S, F, N>(
    objective: F,
    initial: S,
    neighbor: N,
    initial_temp: f64,
    cool_rate: f64,
    max_steps: i64
) -> S;
```

### Fixed Schedules (for comparison)

```simplex
use simplex_std::anneal::fixed;

fn fixed::exponential(t0: f64, alpha: f64, step: i64) -> f64
fn fixed::linear(t0: f64, t_final: f64, step: i64, total_steps: i64) -> f64
fn fixed::logarithmic(c: f64, step: i64) -> f64
fn fixed::cosine(t0: f64, t_min: f64, step: i64, total_steps: i64) -> f64
```

---

## Benchmarking API (v0.10.0) -- Planned

Benchmarking framework for performance testing, used by the `sxc bench` command. This API is planned as part of the toolchain integration; it does not currently exist as a standalone `std::bench` library module.

### Core Types

```simplex
use std::bench::{Bencher, BenchResult, black_box}

/// Benchmark runner that measures function execution time
type Bencher {
    iterations: u64,
    elapsed: Duration,
}

impl Bencher {
    /// Run the benchmarked function repeatedly
    fn iter<T, F: Fn() -> T>(&mut self, f: F)

    /// Run with explicit iteration count
    fn iter_custom<F: Fn(u64)>(&mut self, f: F)

    /// Get total elapsed time
    fn elapsed(&self) -> Duration

    /// Get iterations per second
    fn ops_per_sec(&self) -> f64
}

/// Benchmark result
type BenchResult {
    name: String,
    ns_per_iter: u64,
    variance: u64,
    throughput: Option<Throughput>,
}

/// Throughput measurement
enum Throughput {
    Bytes(u64),      // Bytes per iteration
    Elements(u64),   // Elements per iteration
}
```

### Preventing Optimization

```simplex
/// Prevent compiler from optimizing away benchmark code
fn black_box<T>(x: T) -> T

/// Use black_box to ensure computation isn't elided
#[bench]
fn bench_computation(b: &Bencher) {
    b.iter(|| {
        let result = expensive_computation()
        black_box(result)  // Prevent optimization
    })
}
```

### Benchmark Attributes

```simplex
/// Standard benchmark
#[bench]
fn bench_basic(b: &Bencher) { ... }

/// Benchmark with throughput measurement
#[bench]
fn bench_with_throughput(b: &Bencher) {
    let data = vec![0u8; 1024]
    b.throughput(Throughput::Bytes(1024))
    b.iter(|| {
        process(&data)
    })
}

/// Ignored benchmark
#[bench]
#[ignore]
fn bench_slow(b: &Bencher) { ... }
```

### Example: Complete Benchmark File

```simplex
// benchmarks/collections.sx
use std::bench::{Bencher, black_box, Throughput}
use std::collections::{Vec, Map, Set}

#[bench]
fn bench_vec_push_1000(b: &Bencher) {
    b.iter(|| {
        let mut v: Vec<i64> = Vec::with_capacity(1000)
        for i in 0..1000 {
            v.push(black_box(i))
        }
        v
    })
}

#[bench]
fn bench_map_lookup(b: &Bencher) {
    // Setup: create map once
    let mut m: Map<String, i64> = Map::new()
    for i in 0..10000 {
        m.insert(format("key_{i}"), i)
    }

    b.iter(|| {
        // Benchmark: lookup operations
        for i in 0..1000 {
            black_box(m.get(&format("key_{i}")))
        }
    })
}

#[bench]
fn bench_set_intersection(b: &Bencher) {
    let s1: Set<i64> = (0..1000).collect()
    let s2: Set<i64> = (500..1500).collect()

    b.throughput(Throughput::Elements(500))  // Expected intersection size
    b.iter(|| {
        black_box(s1.intersection(&s2))
    })
}
```

### Running Benchmarks

```bash
# Run all benchmarks in file
sxc bench benchmarks/collections.sx

# Filter by name
sxc bench benchmarks/ --filter vec

# Save results
sxc bench benchmarks/ --output results.json

# Compare against baseline
sxc bench benchmarks/ --compare baseline.json
```

---

## simplex_inference

High-performance inference library with native llama.cpp bindings.

### Pipeline Builder

```simplex
use simplex_inference::{InferencePipeline, PipelineBuilder, BatchConfig, CacheConfig};

let pipeline = InferencePipeline::builder()
    .with_batching(BatchConfig { max_size: 8, timeout_ms: 50 })
    .with_prompt_cache(1000)
    .with_response_cache(CacheConfig { capacity: 10000, ttl_ms: 3600000 })
    .with_routing(RouterConfig::default())
    .build();

let result = pipeline.infer(request).await?;
```

### Model Configuration

```simplex
type ModelLoadConfig {
    gpu_layers: i32,        // Number of GPU layers (0 = CPU only)
    context_size: u32,      // Context size in tokens
    threads: u32,           // CPU threads
    use_mmap: bool,         // Memory mapping for weights
    use_mlock: bool,        // Memory locking for weights
    flash_attention: bool,  // Flash attention (requires compatible GPU)
}

type InferConfig {
    max_tokens: u32,        // Maximum tokens to generate
    temperature: f32,       // Sampling temperature
    top_p: f32,             // Nucleus sampling
    top_k: i32,             // Top-k sampling
    repeat_penalty: f32,    // Repetition penalty
    stop_sequences: Vec<String>,
}
```

### Batch Inference

```simplex
trait BatchInference {
    type Request: Batchable;
    type Response;
    type Error;

    fn infer_batch(self, requests: Vec<Self::Request>) -> Result<Vec<Self::Response>, Self::Error>;
    async fn infer_batch_async(self, requests: Vec<Self::Request>) -> Result<Vec<Self::Response>, Self::Error>;
    fn max_batch_size(self) -> usize;
    fn supports_streaming(self) -> bool;
}
```

### Native FFI (llama.cpp)

```simplex
// Model management
fn slm_model_load(path: &str, config: &ModelLoadConfig) -> Result<ModelHandle, SlmError>;
fn slm_model_unload(handle: ModelHandle);
fn slm_model_info(handle: ModelHandle) -> ModelInfo;

// Inference
fn slm_batch_infer(handle: ModelHandle, prompts: &[String], config: &InferConfig) -> Result<Vec<String>, SlmError>;
fn slm_stream_infer(handle: ModelHandle, prompt: &str, config: &InferConfig, callback: impl FnMut(&str) -> bool) -> Result<(), SlmError>;

// Tokenization
fn slm_tokenize(handle: ModelHandle, text: &str) -> Result<Vec<i32>, SlmError>;
fn slm_detokenize(handle: ModelHandle, tokens: &[i32]) -> Result<String, SlmError>;
```

### Smart Routing

```simplex
trait ComplexityAnalyzer {
    type Query;
    fn analyze(self, query: &Self::Query) -> f32;  // 0.0 - 1.0
    fn select_tier(self, complexity: f32) -> ModelTier;
}

enum ModelTier {
    Tiny,   // < 0.2 complexity: 1B model
    Light,  // < 0.5 complexity: 7B model
    Full,   // >= 0.5 complexity: 13B+ model
}
```

---

## simplex_training

Self-optimizing training pipelines with learnable schedules.

### Learnable Schedules

```simplex
use simplex_training::schedules::{LearnableLRSchedule, LearnableDistillation, LearnablePruning};

/// Learning rate schedule with meta-gradient support
type LearnableLRSchedule {
    initial_lr: dual,
    decay_rate: dual,
    warmup_steps: dual,
    plateau_threshold: dual,
    plateau_boost: dual,
}

/// Knowledge distillation schedule
type LearnableDistillation {
    initial_temp: dual,
    temp_decay: dual,
    alpha_start: dual,
    alpha_decay: dual,
}

/// Pruning schedule
type LearnablePruning {
    initial_sparsity: dual,
    final_sparsity: dual,
    pruning_rate: dual,
    layer_sensitivity: Vec<dual>,
}

/// Quantization schedule
type LearnableQuantization {
    initial_bits: dual,
    final_bits: dual,
    quant_rate: dual,
    layer_precision: Vec<dual>,
}
```

### Meta-Trainer

```simplex
use simplex_training::{MetaTrainer, SpecialistConfig, TrainResult};

let trainer = MetaTrainer::new()
    .with_learnable_lr()
    .with_learnable_distillation()
    .with_learnable_pruning()
    .with_learnable_quantization();

let result = trainer.meta_train(&specialists, &teacher).await;
```

### Compression Pipeline

```simplex
use simplex_training::{CompressionPipeline, CompressedModel};

let pipeline = CompressionPipeline::for_seed_models();

for specialist in specialists {
    let compressed = pipeline.compress(&specialist).await;
    compressed.export_gguf(&path).await?;
}
```

---

*All standard library modules are written in pure Simplex with no external dependencies (except simplex_inference which links to llama.cpp).*
