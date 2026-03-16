# Simplex HTTP Server Specification

## Overview

The Simplex HTTP server (`simplex-http`) provides an actor-native HTTP server designed for building APIs that interact with cognitive hives. Unlike traditional thread-pool based servers, simplex-http treats the server, handlers, and middleware as actors communicating via messages.

## Design Principles

1. **Actor-Native**: Server, handlers, and middleware are actors
2. **Hive-Integrated**: First-class support for routing requests to specialists
3. **Async-First**: All I/O is non-blocking, using the Simplex async runtime
4. **Message-Based**: Request/response flow uses actor messages
5. **Zero Threads**: No exposed threading primitives; concurrency via actors

## Architecture

```
                                    ┌─────────────────────┐
                                    │   HttpServer Actor  │
                                    │  (accepts connections)│
                                    └──────────┬──────────┘
                                               │
                         ┌─────────────────────┼─────────────────────┐
                         │                     │                     │
                         ▼                     ▼                     ▼
                  ┌────────────┐        ┌────────────┐        ┌────────────┐
                  │ Connection │        │ Connection │        │ Connection │
                  │   Actor    │        │   Actor    │        │   Actor    │
                  └─────┬──────┘        └─────┬──────┘        └─────┬──────┘
                        │                     │                     │
                        ▼                     ▼                     ▼
                  ┌─────────────────────────────────────────────────────┐
                  │                     Router                          │
                  │   /api/query → QuerySpecialist                      │
                  │   /api/reason → ReasoningSpecialist                 │
                  │   /health → HealthHandler                           │
                  └───────────────────────┬─────────────────────────────┘
                                          │
                                          ▼
                  ┌─────────────────────────────────────────────────────┐
                  │                  Cognitive Hive                      │
                  │  ┌──────────────┐  ┌──────────────┐                 │
                  │  │ QuerySpec    │  │ ReasonSpec   │  ...            │
                  │  │  (Actor)     │  │  (Actor)     │                 │
                  │  └──────────────┘  └──────────────┘                 │
                  └─────────────────────────────────────────────────────┘
```

## Core Types

### HTTP Method

```simplex
enum Method {
    Get,
    Post,
    Put,
    Delete,
    Patch,
    Head,
    Options,
}
```

### Status Codes

```simplex
enum StatusCode {
    // 2xx Success
    Ok,                     // 200
    Created,                // 201
    Accepted,               // 202
    NoContent,              // 204

    // 3xx Redirection
    MovedPermanently,       // 301
    Found,                  // 302
    SeeOther,               // 303
    NotModified,            // 304
    TemporaryRedirect,      // 307
    PermanentRedirect,      // 308

    // 4xx Client Errors
    BadRequest,             // 400
    Unauthorized,           // 401
    Forbidden,              // 403
    NotFound,               // 404
    MethodNotAllowed,       // 405
    Conflict,               // 409
    Gone,                   // 410
    UnprocessableEntity,    // 422
    TooManyRequests,        // 429

    // 5xx Server Errors
    InternalServerError,    // 500
    NotImplemented,         // 501
    BadGateway,             // 502
    ServiceUnavailable,     // 503
    GatewayTimeout,         // 504
}

impl StatusCode {
    fn code(self) -> u16;
    fn is_success(self) -> bool;
    fn is_error(self) -> bool;
}
```

### Headers

```simplex
struct Headers {
    inner: Vec<(String, String)>,
}

impl Headers {
    fn new() -> Self;
    fn get(&self, name: &str) -> Option<&str>;
    fn set(&mut self, name: &str, value: &str);
    fn append(&mut self, name: &str, value: &str);
    fn remove(&mut self, name: &str);
    fn contains(&self, name: &str) -> bool;
    fn iter(&self) -> impl Iterator<Item = (&str, &str)>;

    // Common header accessors
    fn content_type(&self) -> Option<&str>;
    fn content_length(&self) -> Option<u64>;
    fn authorization(&self) -> Option<&str>;
}
```

### Body

```simplex
struct Body {
    data: Vec<u8>,
}

impl Body {
    fn empty() -> Self;
    fn from_bytes(bytes: Vec<u8>) -> Self;
    fn from_string(s: String) -> Self;

    async fn bytes(&self) -> Vec<u8>;
    async fn text(&self) -> Result<String, Utf8Error>;
    async fn json<T: Deserialize>(&self) -> Result<T, JsonError>;

    fn len(&self) -> usize;
    fn is_empty(&self) -> bool;
}
```

### Request

```simplex
struct Request {
    method: Method,
    path: String,
    headers: Headers,
    body: Body,
    params: HashMap<String, String>,   // Route parameters (e.g., /users/:id)
    query: HashMap<String, String>,    // Query string parameters
    extensions: Extensions,            // Type-safe request extensions
}

impl Request {
    // Accessors
    fn method(&self) -> Method;
    fn path(&self) -> &str;
    fn headers(&self) -> &Headers;
    fn header(&self, name: &str) -> Option<&str>;

    // Route parameters
    fn param(&self, name: &str) -> Option<&str>;
    fn param_as<T: FromStr>(&self, name: &str) -> Option<T>;

    // Query parameters
    fn query_param(&self, name: &str) -> Option<&str>;
    fn query_params(&self) -> &HashMap<String, String>;

    // Body parsing
    async fn body_bytes(&self) -> Vec<u8>;
    async fn body_text(&self) -> Result<String, HttpError>;
    async fn body_json<T: Deserialize>(&self) -> Result<T, HttpError>;

    // Extensions (for middleware to pass data)
    fn extension<T: 'static>(&self) -> Option<&T>;
    fn set_extension<T: 'static>(&mut self, value: T);
}
```

### Response

```simplex
struct Response {
    status: StatusCode,
    headers: Headers,
    body: Body,
}

impl Response {
    // Constructors
    fn new(status: StatusCode) -> Self;
    fn ok() -> Self;
    fn created() -> Self;
    fn no_content() -> Self;
    fn not_found() -> Self;
    fn bad_request(msg: &str) -> Self;
    fn unauthorized() -> Self;
    fn forbidden() -> Self;
    fn internal_error(msg: &str) -> Self;

    // Body builders
    fn text(body: &str) -> Self;
    fn html(body: &str) -> Self;
    fn json<T: Serialize>(data: &T) -> Result<Self, JsonError>;
    fn bytes(data: Vec<u8>) -> Self;

    // Redirect
    fn redirect(url: &str) -> Self;
    fn redirect_permanent(url: &str) -> Self;

    // Builder methods
    fn with_status(self, status: StatusCode) -> Self;
    fn with_header(self, name: &str, value: &str) -> Self;
    fn with_body(self, body: Body) -> Self;

    // Accessors
    fn status(&self) -> StatusCode;
    fn headers(&self) -> &Headers;
    fn body(&self) -> &Body;
}
```

## Handler Trait

Handlers process requests and return responses. Any actor can be a handler.

```simplex
trait Handler: Send + Sync {
    async fn handle(&self, req: Request) -> Response;
}

// Functions automatically implement Handler
impl<F, Fut> Handler for F
where
    F: Fn(Request) -> Fut + Send + Sync,
    Fut: Future<Output = Response> + Send,
{
    async fn handle(&self, req: Request) -> Response {
        (self)(req).await
    }
}
```

### Actor as Handler

```simplex
// Any actor can be a handler by implementing the Handler trait
actor ApiHandler {
    db: DatabaseRef,

    impl Handler {
        async fn handle(&self, req: Request) -> Response {
            match req.path() {
                "/users" => self.list_users(req).await,
                _ => Response::not_found(),
            }
        }
    }

    async fn list_users(&self, req: Request) -> Response {
        let users = self.db.query("SELECT * FROM users").await?;
        Response::json(&users).unwrap()
    }
}
```

## Router

The router maps paths and methods to handlers.

```simplex
struct Router {
    routes: Vec<Route>,
    middleware: Vec<Box<dyn Middleware>>,
    fallback: Option<Box<dyn Handler>>,
}

impl Router {
    fn new() -> Self;

    // Route registration
    fn route(self, method: Method, path: &str, handler: impl Handler) -> Self;
    fn get(self, path: &str, handler: impl Handler) -> Self;
    fn post(self, path: &str, handler: impl Handler) -> Self;
    fn put(self, path: &str, handler: impl Handler) -> Self;
    fn delete(self, path: &str, handler: impl Handler) -> Self;
    fn patch(self, path: &str, handler: impl Handler) -> Self;

    // Route groups
    fn nest(self, prefix: &str, router: Router) -> Self;
    fn merge(self, router: Router) -> Self;

    // Middleware
    fn with<M: Middleware>(self, middleware: M) -> Self;
    fn layer<L: Layer>(self, layer: L) -> Self;

    // Fallback handler
    fn fallback(self, handler: impl Handler) -> Self;
}
```

### Route Parameters

```simplex
let router = Router::new()
    .get("/users/:id", get_user)
    .get("/users/:user_id/posts/:post_id", get_user_post)
    .get("/files/*path", serve_file);  // Wildcard

async fn get_user(req: Request) -> Response {
    let id: u64 = req.param_as("id").unwrap();
    // ...
}
```

## Middleware

Middleware wraps handlers to add cross-cutting concerns.

```simplex
trait Middleware: Send + Sync {
    async fn handle(&self, req: Request, next: Next) -> Response;
}

struct Next {
    // Opaque type representing the next handler in the chain
}

impl Next {
    async fn run(self, req: Request) -> Response;
}
```

### Built-in Middleware

```simplex
// Logging middleware
struct Logger {
    level: LogLevel,
}

impl Middleware for Logger {
    async fn handle(&self, req: Request, next: Next) -> Response {
        let start = Instant::now();
        let method = req.method();
        let path = req.path().to_string();

        let response = next.run(req).await;

        log!(self.level, "{} {} -> {} ({:?})",
            method, path, response.status().code(), start.elapsed());

        response
    }
}

// CORS middleware
struct Cors {
    allowed_origins: Vec<String>,
    allowed_methods: Vec<Method>,
    allowed_headers: Vec<String>,
    max_age: Option<Duration>,
}

impl Cors {
    fn permissive() -> Self;  // Allow all
    fn new() -> CorsBuilder;
}

// Rate limiting middleware
actor RateLimiter {
    requests: HashMap<IpAddr, RingBuffer<Instant>>,
    max_requests: u32,
    window: Duration,

    impl Middleware {
        async fn handle(&self, req: Request, next: Next) -> Response {
            let ip = req.extension::<IpAddr>().unwrap();

            if self.is_rate_limited(ip) {
                return Response::new(StatusCode::TooManyRequests)
                    .with_header("Retry-After", "60");
            }

            self.record_request(ip);
            next.run(req).await
        }
    }
}

// Timeout middleware
struct Timeout {
    duration: Duration,
}

impl Middleware for Timeout {
    async fn handle(&self, req: Request, next: Next) -> Response {
        match timeout(self.duration, next.run(req)).await {
            Ok(response) => response,
            Err(_) => Response::new(StatusCode::GatewayTimeout),
        }
    }
}

// Compression middleware
struct Compression {
    algorithms: Vec<CompressionAlgorithm>,
}

enum CompressionAlgorithm {
    Gzip,
    Deflate,
    Brotli,
}
```

### Actor Middleware

```simplex
// Authentication as an actor
actor AuthMiddleware {
    secret_key: String,

    impl Middleware {
        async fn handle(&self, req: Request, next: Next) -> Response {
            let token = match req.header("Authorization") {
                Some(h) => h.strip_prefix("Bearer ").unwrap_or(""),
                None => return Response::unauthorized(),
            };

            match self.verify_token(token).await {
                Ok(user) => {
                    let mut req = req;
                    req.set_extension(user);
                    next.run(req).await
                }
                Err(_) => Response::unauthorized(),
            }
        }
    }

    async fn verify_token(&self, token: &str) -> Result<User, AuthError> {
        // JWT verification logic
    }
}
```

## HTTP Server Actor

```simplex
actor HttpServer {
    listener: TcpListener,
    router: Router,
    config: ServerConfig,
}

struct ServerConfig {
    max_connections: usize,
    read_timeout: Duration,
    write_timeout: Duration,
    keep_alive: Duration,
    max_request_size: usize,
}

impl Default for ServerConfig {
    fn default() -> Self {
        Self {
            max_connections: 10000,
            read_timeout: Duration::from_secs(30),
            write_timeout: Duration::from_secs(30),
            keep_alive: Duration::from_secs(60),
            max_request_size: 10 * 1024 * 1024,  // 10MB
        }
    }
}

impl HttpServer {
    fn bind(addr: &str) -> ServerBuilder;
}

struct ServerBuilder {
    addr: String,
    router: Option<Router>,
    config: ServerConfig,
    hive: Option<HiveRef>,
    graceful_shutdown: Option<Signal>,
}

impl ServerBuilder {
    fn router(self, router: Router) -> Self;
    fn config(self, config: ServerConfig) -> Self;
    fn with_hive(self, hive: HiveRef) -> Self;
    fn graceful_shutdown(self, signal: impl Future<Output = ()>) -> Self;

    async fn serve(self) -> Result<(), HttpError>;
}
```

## Hive Integration

First-class support for routing to cognitive hive specialists.

### HiveHandler

```simplex
struct HiveHandler<S: Specialist> {
    hive: HiveRef,
    _marker: PhantomData<S>,
}

impl<S: Specialist> Handler for HiveHandler<S> {
    async fn handle(&self, req: Request) -> Response {
        let input: S::Input = req.body_json().await?;

        match self.hive.ask::<S>(input).await {
            Ok(output) => Response::json(&output).unwrap(),
            Err(e) => Response::internal_error(&e.to_string()),
        }
    }
}

// Extension trait for HiveRef
impl HiveRef {
    fn handler<S: Specialist>(&self) -> HiveHandler<S>;
}
```

### Example: Full API Server

```simplex
use simplex_http::{HttpServer, Router, Request, Response, Method};
use simplex_hive::{Hive, Specialist, specialist};

// Define specialists
specialist QuerySpecialist {
    model: SLM,

    beliefs {
        domain_knowledge: Vec<String>,
        confidence_threshold: f64,
    }

    type Input = QueryRequest;
    type Output = QueryResponse;

    async fn process(&self, input: QueryRequest) -> QueryResponse {
        let context = self.beliefs.domain_knowledge.join("\n");
        let response = self.model.complete(&input.query, &context).await;

        QueryResponse {
            answer: response.text,
            confidence: response.confidence,
            sources: response.sources,
        }
    }
}

specialist ReasoningSpecialist {
    model: SLM,

    type Input = ReasoningRequest;
    type Output = ReasoningResponse;

    async fn process(&self, input: ReasoningRequest) -> ReasoningResponse {
        // Chain-of-thought reasoning
        let steps = self.model.reason(&input.problem).await;

        ReasoningResponse {
            conclusion: steps.last().unwrap().clone(),
            reasoning_chain: steps,
        }
    }
}

// Request/Response types
#[derive(Deserialize)]
struct QueryRequest {
    query: String,
    context: Option<String>,
}

#[derive(Serialize)]
struct QueryResponse {
    answer: String,
    confidence: f64,
    sources: Vec<String>,
}

#[derive(Deserialize)]
struct ReasoningRequest {
    problem: String,
    constraints: Vec<String>,
}

#[derive(Serialize)]
struct ReasoningResponse {
    conclusion: String,
    reasoning_chain: Vec<String>,
}

// Main server
async fn main() -> Result<(), HttpError> {
    // Build the cognitive hive
    let hive = Hive::builder()
        .add_specialist(QuerySpecialist::new(SLM::load("query-model")))
        .add_specialist(ReasoningSpecialist::new(SLM::load("reasoning-model")))
        .build()
        .await?;

    // Build the router
    let api = Router::new()
        .post("/query", hive.handler::<QuerySpecialist>())
        .post("/reason", hive.handler::<ReasoningSpecialist>())
        .get("/health", health_check)
        .get("/specialists", list_specialists);

    let router = Router::new()
        .nest("/api/v1", api)
        .with(Logger::new(LogLevel::Info))
        .with(Cors::permissive())
        .with(RateLimiter::new(100, Duration::from_secs(60)))
        .with(Timeout::new(Duration::from_secs(30)));

    // Start server with graceful shutdown
    HttpServer::bind("0.0.0.0:8080")
        .router(router)
        .with_hive(hive)
        .graceful_shutdown(signal::ctrl_c())
        .serve()
        .await
}

async fn health_check(_req: Request) -> Response {
    Response::json(&json!({
        "status": "healthy",
        "version": "0.10.0"
    })).unwrap()
}

async fn list_specialists(req: Request) -> Response {
    let hive = req.extension::<HiveRef>().unwrap();
    let specialists = hive.list_specialists().await;
    Response::json(&specialists).unwrap()
}
```

## WebSocket Support

For real-time communication with the hive.

```simplex
trait WebSocketHandler: Send + Sync {
    async fn on_connect(&mut self, ws: &WebSocket);
    async fn on_message(&mut self, ws: &WebSocket, msg: Message);
    async fn on_close(&mut self, ws: &WebSocket, reason: CloseReason);
}

struct WebSocket {
    // Opaque handle
}

impl WebSocket {
    async fn send(&self, msg: Message) -> Result<(), WsError>;
    async fn close(&self, reason: CloseReason) -> Result<(), WsError>;
}

enum Message {
    Text(String),
    Binary(Vec<u8>),
    Ping(Vec<u8>),
    Pong(Vec<u8>),
}

// Actor as WebSocket handler
actor HiveStreamHandler {
    hive: HiveRef,

    impl WebSocketHandler {
        async fn on_connect(&mut self, ws: &WebSocket) {
            ws.send(Message::Text("Connected to Hive".into())).await;
        }

        async fn on_message(&mut self, ws: &WebSocket, msg: Message) {
            if let Message::Text(query) = msg {
                // Stream responses from hive
                let stream = self.hive.stream::<QuerySpecialist>(query).await;

                while let Some(chunk) = stream.next().await {
                    ws.send(Message::Text(chunk)).await;
                }
            }
        }

        async fn on_close(&mut self, _ws: &WebSocket, _reason: CloseReason) {
            // Cleanup
        }
    }
}

// Router integration
let router = Router::new()
    .get("/ws/hive", WebSocket::upgrade(HiveStreamHandler::new(hive)));
```

## Server-Sent Events (SSE)

For streaming responses from specialists.

```simplex
struct SseStream {
    // Opaque handle
}

impl SseStream {
    async fn send(&self, event: SseEvent) -> Result<(), SseError>;
    async fn close(&self);
}

struct SseEvent {
    id: Option<String>,
    event: Option<String>,
    data: String,
    retry: Option<Duration>,
}

impl SseEvent {
    fn new(data: impl Into<String>) -> Self;
    fn with_id(self, id: impl Into<String>) -> Self;
    fn with_event(self, event: impl Into<String>) -> Self;
}

// Usage
async fn stream_reasoning(req: Request) -> Response {
    let hive = req.extension::<HiveRef>().unwrap();
    let input: ReasoningRequest = req.body_json().await?;

    Response::sse(|stream| async move {
        let reasoning = hive.stream::<ReasoningSpecialist>(input).await;

        while let Some(step) = reasoning.next().await {
            stream.send(SseEvent::new(step).with_event("reasoning_step")).await?;
        }

        stream.send(SseEvent::new("done").with_event("complete")).await?;
        Ok(())
    })
}
```

## Error Handling

```simplex
enum HttpError {
    // Request errors
    BadRequest { message: String },
    Unauthorized { message: String },
    Forbidden { message: String },
    NotFound { path: String },
    MethodNotAllowed { method: Method, path: String },
    PayloadTooLarge { size: usize, max: usize },

    // Server errors
    Internal { message: String },
    Timeout { duration: Duration },

    // Connection errors
    ConnectionClosed,
    ConnectionReset,

    // Parse errors
    InvalidHeader { name: String },
    InvalidBody { message: String },
    JsonError { message: String },
}

impl From<HttpError> for Response {
    fn from(error: HttpError) -> Response {
        match error {
            HttpError::BadRequest { message } =>
                Response::bad_request(&message),
            HttpError::NotFound { path } =>
                Response::not_found(),
            HttpError::Internal { message } =>
                Response::internal_error(&message),
            // ... etc
        }
    }
}

// Result type for handlers
type HandlerResult = Result<Response, HttpError>;

// Handlers can return Result
async fn create_user(req: Request) -> HandlerResult {
    let user: CreateUser = req.body_json().await?;  // ? converts JsonError to HttpError

    if user.email.is_empty() {
        return Err(HttpError::BadRequest {
            message: "Email required".into()
        });
    }

    Ok(Response::created())
}
```

## Testing Support

```simplex
// Test client
struct TestClient {
    router: Router,
}

impl TestClient {
    fn new(router: Router) -> Self;

    async fn get(&self, path: &str) -> Response;
    async fn post(&self, path: &str, body: impl Into<Body>) -> Response;
    async fn request(&self, req: Request) -> Response;
}

// Example test
#[test]
async fn test_query_endpoint() {
    let hive = test_hive().await;
    let router = create_router(hive);
    let client = TestClient::new(router);

    let response = client.post("/api/v1/query", json!({
        "query": "What is Simplex?"
    })).await;

    assert_eq!(response.status(), StatusCode::Ok);

    let body: QueryResponse = response.json().await.unwrap();
    assert!(body.confidence > 0.5);
}
```

## Performance Considerations

1. **Connection Pooling**: The server maintains a pool of connection actors
2. **Zero-Copy**: Request/response bodies use zero-copy where possible
3. **Backpressure**: Built-in backpressure via actor mailbox limits
4. **Keep-Alive**: HTTP/1.1 keep-alive is enabled by default
5. **Pipelining**: HTTP pipelining supported via actor message ordering

## Configuration

```simplex
// Server configuration via environment or file
let config = ServerConfig::from_env()
    .or_else(|| ServerConfig::from_file("server.toml"))
    .unwrap_or_default();

// Or programmatically
let config = ServerConfig {
    max_connections: 10000,
    read_timeout: Duration::from_secs(30),
    write_timeout: Duration::from_secs(30),
    keep_alive: Duration::from_secs(60),
    max_request_size: 10 * 1024 * 1024,
};
```

## Module Structure

```
simplex-http/
├── src/
│   ├── lib.sx           # Public API exports
│   ├── server.sx        # HttpServer actor
│   ├── router.sx        # Router and routing
│   ├── request.sx       # Request type
│   ├── response.sx      # Response type
│   ├── handler.sx       # Handler trait
│   ├── middleware.sx    # Middleware trait and built-ins
│   ├── headers.sx       # Headers type
│   ├── body.sx          # Body type
│   ├── status.sx        # StatusCode enum
│   ├── method.sx        # Method enum
│   ├── error.sx         # HttpError enum
│   ├── websocket.sx     # WebSocket support
│   ├── sse.sx           # Server-Sent Events
│   ├── hive.sx          # Hive integration
│   └── test.sx          # Testing utilities
└── Modulus.toml
```

## Summary

The simplex-http server provides:

- **Actor-native design**: No threads, all concurrency via actors
- **Seamless hive integration**: Route directly to specialists
- **Streaming support**: WebSocket and SSE for real-time communication
- **Middleware as actors**: Authentication, rate limiting, etc.
- **Type-safe**: Strong typing throughout
- **Async-first**: Built on the Simplex async runtime
- **Production-ready**: Timeouts, backpressure, graceful shutdown
