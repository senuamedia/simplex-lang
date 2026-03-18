# Phase 3: Essential Libraries

**Priority**: HIGH - Enables real-world applications
**Estimated Effort**: 4-6 weeks
**Status**: Not Started
**Depends On**: Phase 1 (Core), Phase 2 (Package Ecosystem)

## Overview

Build the minimal set of libraries that enable developers to build real applications. Focus on quality over quantity - 10 excellent libraries beat 100 mediocre ones.

---

## 1. simplex-json (Complete JSON Library)

**Status**: Partially exists in Phase 1, package it properly
**Priority**: CRITICAL

### Subtasks

- [ ] 1.1 Create package structure
  ```
  simplex-json/
  ├── simplex.toml
  ├── src/
  │   ├── lib.sx
  │   ├── value.sx      # JsonValue type
  │   ├── parser.sx     # JSON parsing
  │   ├── serializer.sx # JSON serialization
  │   └── path.sx       # JSON path queries
  └── tests/
  ```

- [ ] 1.2 JSON Path queries
  - [ ] `json_path(value, "$.foo.bar[0]")` - JSONPath syntax
  - [ ] Wildcard support: `$.items[*].name`

- [ ] 1.3 Streaming parser (for large files)
  - [ ] Event-based parsing
  - [ ] Memory-efficient for large JSON

- [ ] 1.4 Serde-like traits (future)
  - [ ] `Serialize` trait
  - [ ] `Deserialize` trait
  - [ ] Derive macros

- [ ] 1.5 Tests and documentation
  - [ ] Comprehensive test suite
  - [ ] API documentation
  - [ ] Examples

---

## 2. simplex-http (High-Level HTTP)

**Status**: Low-level exists in runtime, need high-level API
**Priority**: CRITICAL

### Subtasks

- [ ] 2.1 HTTP Client
  ```simplex
  let client = HttpClient::new();

  // Simple GET
  let response = client.get("https://api.example.com/users")?;

  // With headers and JSON body
  let response = client
      .post("https://api.example.com/users")
      .header("Authorization", "Bearer token")
      .json(&user)
      .send()?;

  // Response handling
  let status = response.status();
  let body: User = response.json()?;
  ```

- [ ] 2.2 HTTP Server (high-level)
  ```simplex
  let server = HttpServer::new();

  server.get("/users", |req| {
      let users = get_all_users();
      Response::json(&users)
  });

  server.post("/users", |req| {
      let user: User = req.json()?;
      save_user(&user);
      Response::created()
  });

  server.listen("0.0.0.0:8080")?;
  ```

- [ ] 2.3 Request builder
  - [ ] Method chaining
  - [ ] Automatic content-type
  - [ ] Query parameter encoding
  - [ ] Form data support
  - [ ] Multipart uploads

- [ ] 2.4 Response handling
  - [ ] Status code helpers
  - [ ] Header access
  - [ ] Body parsing (text, json, bytes)
  - [ ] Streaming response body

- [ ] 2.5 Middleware support
  - [ ] Logging middleware
  - [ ] Authentication middleware
  - [ ] CORS middleware
  - [ ] Rate limiting

- [ ] 2.6 TLS configuration
  - [ ] Custom certificates
  - [ ] Client certificates
  - [ ] Insecure mode (for testing)

- [ ] 2.7 Tests and examples
  - [ ] Unit tests with mock server
  - [ ] Integration tests
  - [ ] Example: REST API client
  - [ ] Example: Web server

---

## 3. simplex-sql (SQLite Driver)

**Status**: Not implemented
**Priority**: HIGH

### Subtasks

- [ ] 3.1 SQLite C bindings
  - [ ] Link against libsqlite3
  - [ ] Or bundle SQLite amalgamation

- [ ] 3.2 Connection management
  ```simplex
  let db = Database::open("app.db")?;
  let db = Database::memory()?;  // In-memory
  ```

- [ ] 3.3 Query execution
  ```simplex
  // Execute with parameters
  db.execute("INSERT INTO users (name, email) VALUES (?, ?)",
             &["Alice", "alice@example.com"])?;

  // Query rows
  let users = db.query("SELECT * FROM users WHERE active = ?", &[true])?;
  for row in users {
      let name: String = row.get("name")?;
      let email: String = row.get("email")?;
  }
  ```

- [ ] 3.4 Prepared statements
  - [ ] Statement preparation and caching
  - [ ] Parameter binding
  - [ ] Reuse for performance

- [ ] 3.5 Transactions
  ```simplex
  let tx = db.transaction()?;
  tx.execute("INSERT INTO ...")?;
  tx.execute("UPDATE ...")?;
  tx.commit()?;
  // Or tx.rollback()?
  ```

- [ ] 3.6 Type mapping
  - [ ] INTEGER → i64
  - [ ] REAL → f64
  - [ ] TEXT → String
  - [ ] BLOB → Vec<u8>
  - [ ] NULL → Option<T>

- [ ] 3.7 Migrations (optional)
  - [ ] Migration file format
  - [ ] Up/down migrations
  - [ ] Version tracking

- [ ] 3.8 Tests
  - [ ] CRUD operations
  - [ ] Transactions
  - [ ] Error handling
  - [ ] Concurrent access

---

## 4. simplex-regex (Pattern Matching)

**Status**: Not implemented
**Priority**: HIGH

### Subtasks

- [ ] 4.1 Strategy decision
  - [ ] Option A: Bind to PCRE2 (full-featured)
  - [ ] Option B: Bind to RE2 (safe, linear time)
  - [ ] Option C: Simple regex engine in Simplex (limited)

- [ ] 4.2 Core API
  ```simplex
  let re = Regex::new(r"\d{3}-\d{4}")?;

  // Matching
  let is_match = re.is_match("123-4567");

  // Find
  if let Some(m) = re.find("call 123-4567 now") {
      println(m.as_str());  // "123-4567"
      println(m.start());   // 5
      println(m.end());     // 13
  }

  // Find all
  for m in re.find_all(text) {
      println(m.as_str());
  }

  // Capture groups
  let re = Regex::new(r"(\d{3})-(\d{4})")?;
  if let Some(caps) = re.captures("123-4567") {
      println(caps.get(1));  // "123"
      println(caps.get(2));  // "4567"
  }

  // Replace
  let result = re.replace_all(text, "XXX-XXXX");
  ```

- [ ] 4.3 Regex features to support
  - [ ] Character classes: `[a-z]`, `\d`, `\w`, `\s`
  - [ ] Quantifiers: `*`, `+`, `?`, `{n,m}`
  - [ ] Anchors: `^`, `$`, `\b`
  - [ ] Groups: `()`, `(?:)` non-capturing
  - [ ] Alternation: `|`
  - [ ] Escaping: `\.`, `\\`

- [ ] 4.4 Compilation and caching
  - [ ] Compile once, use many times
  - [ ] Error messages for invalid patterns

- [ ] 4.5 Tests
  - [ ] Pattern syntax coverage
  - [ ] Edge cases
  - [ ] Performance benchmarks

---

## 5. simplex-crypto (Cryptography)

**Status**: TLS exists, need standalone crypto
**Priority**: HIGH

### Subtasks

- [ ] 5.1 Strategy decision
  - [ ] Bind to OpenSSL/LibreSSL
  - [ ] Or: ring library (Rust, may need different approach)
  - [ ] Or: libsodium (modern, simpler API)

- [ ] 5.2 Hashing
  ```simplex
  let hash = sha256("hello world");
  let hash = sha512(data);
  let hash = blake2b(data);
  ```

- [ ] 5.3 HMAC
  ```simplex
  let mac = hmac_sha256(key, message);
  let valid = hmac_verify(key, message, mac);
  ```

- [ ] 5.4 Random bytes
  ```simplex
  let bytes = random_bytes(32);  // Cryptographically secure
  let uuid = uuid_v4();
  ```

- [ ] 5.5 Password hashing
  ```simplex
  let hash = password_hash("secret");
  let valid = password_verify("secret", hash);
  // Uses bcrypt or argon2
  ```

- [ ] 5.6 Symmetric encryption
  ```simplex
  let key = generate_key();
  let (ciphertext, nonce) = encrypt_aes_gcm(key, plaintext);
  let plaintext = decrypt_aes_gcm(key, nonce, ciphertext)?;
  ```

- [ ] 5.7 Asymmetric encryption (optional)
  - [ ] Key pair generation
  - [ ] RSA encrypt/decrypt
  - [ ] ECDSA sign/verify

- [ ] 5.8 Base64/Hex encoding
  ```simplex
  let encoded = base64_encode(bytes);
  let decoded = base64_decode(encoded)?;
  let hex = hex_encode(bytes);
  ```

- [ ] 5.9 Tests
  - [ ] Known answer tests (KAT)
  - [ ] Round-trip tests
  - [ ] Error handling

---

## 6. simplex-cli (Command Line Tools)

**Status**: Not implemented
**Priority**: MEDIUM

### Subtasks

- [ ] 6.1 Argument parsing
  ```simplex
  let cli = Cli::new("myapp")
      .version("1.0.0")
      .about("My application")
      .arg(Arg::new("verbose")
          .short('v')
          .long("verbose")
          .help("Enable verbose output"))
      .arg(Arg::new("config")
          .short('c')
          .long("config")
          .takes_value(true)
          .help("Config file path"))
      .subcommand(Command::new("run")
          .about("Run the application"));

  let matches = cli.parse()?;

  if matches.is_present("verbose") {
      // ...
  }
  let config = matches.value_of("config").unwrap_or("config.toml");
  ```

- [ ] 6.2 Terminal colors
  ```simplex
  println(red("Error: ") + "Something went wrong");
  println(green("Success!"));
  println(bold(blue("Info: ")) + "Processing...");
  ```

- [ ] 6.3 Progress bars
  ```simplex
  let pb = ProgressBar::new(100);
  for i in 0..100 {
      do_work();
      pb.inc(1);
  }
  pb.finish_with_message("Done!");
  ```

- [ ] 6.4 Spinners
  ```simplex
  let spinner = Spinner::new("Loading...");
  do_async_work();
  spinner.finish();
  ```

- [ ] 6.5 Tables
  ```simplex
  let table = Table::new()
      .header(&["Name", "Age", "City"])
      .row(&["Alice", "30", "NYC"])
      .row(&["Bob", "25", "LA"]);
  println(table.to_string());
  ```

- [ ] 6.6 Prompts
  ```simplex
  let name = prompt("Enter your name: ")?;
  let password = prompt_password("Password: ")?;
  let confirm = confirm("Continue?")?;
  let choice = select("Choose:", &["Option A", "Option B"])?;
  ```

- [ ] 6.7 Tests and examples

---

## 7. simplex-log (Structured Logging)

**Status**: println exists, need proper logging
**Priority**: MEDIUM

### Subtasks

- [ ] 7.1 Log levels
  ```simplex
  log::trace("Detailed trace info");
  log::debug("Debug information");
  log::info("General info");
  log::warn("Warning message");
  log::error("Error occurred");
  ```

- [ ] 7.2 Structured logging
  ```simplex
  log::info("User logged in", &[
      ("user_id", user.id),
      ("ip", request.ip),
  ]);
  ```

- [ ] 7.3 Output formats
  - [ ] Human-readable (default)
  - [ ] JSON (for log aggregators)
  - [ ] Compact (minimal)

- [ ] 7.4 Output destinations
  - [ ] stdout/stderr
  - [ ] File
  - [ ] Rolling files
  - [ ] Custom handlers

- [ ] 7.5 Filtering
  - [ ] By level
  - [ ] By module
  - [ ] Environment variable config (SIMPLEX_LOG)

- [ ] 7.6 Context/spans (for tracing)
  ```simplex
  let span = log::span("handle_request");
  // ... work ...
  span.end();
  ```

---

## 8. simplex-test (Testing Framework)

**Status**: Basic assert exists, need framework
**Priority**: MEDIUM

### Subtasks

- [ ] 8.1 Test discovery
  ```simplex
  #[test]
  fn test_addition() {
      assert_eq(2 + 2, 4);
  }

  #[test]
  #[should_panic]
  fn test_panic() {
      panic("expected");
  }

  #[test]
  #[ignore]
  fn test_slow() {
      // ...
  }
  ```

- [ ] 8.2 Assertions
  ```simplex
  assert(condition);
  assert_eq(left, right);
  assert_ne(left, right);
  assert_lt(left, right);
  assert_gt(left, right);
  assert_contains(haystack, needle);
  assert_matches(value, pattern);
  ```

- [ ] 8.3 Test fixtures
  ```simplex
  #[before_each]
  fn setup() -> TestContext {
      // ...
  }

  #[after_each]
  fn teardown(ctx: TestContext) {
      // ...
  }
  ```

- [ ] 8.4 Test runner
  - [ ] Parallel execution
  - [ ] Filtering by name
  - [ ] Verbose output
  - [ ] JUnit XML output

- [ ] 8.5 Mocking (basic)
  - [ ] Function mocking
  - [ ] Trait mocking

- [ ] 8.6 Property-based testing (optional)
  ```simplex
  #[property]
  fn prop_reverse_reverse(xs: Vec<i64>) {
      assert_eq(xs.reverse().reverse(), xs);
  }
  ```

---

## 9. simplex-toml (Config Files)

**Status**: Not implemented (needed for Phase 2)
**Priority**: MEDIUM

### Subtasks

- [ ] 9.1 TOML parsing
  ```simplex
  let config = toml::parse(content)?;
  let name = config.get_string("package.name")?;
  let version = config.get_string("package.version")?;
  ```

- [ ] 9.2 TOML serialization
  ```simplex
  let mut config = TomlTable::new();
  config.set("name", "my-package");
  config.set("version", "1.0.0");
  let content = toml::stringify(&config);
  ```

- [ ] 9.3 Type-safe parsing
  ```simplex
  #[derive(TomlDeserialize)]
  struct Config {
      name: String,
      version: String,
      debug: bool,
  }

  let config: Config = toml::parse_as(content)?;
  ```

- [ ] 9.4 TOML spec compliance
  - [ ] Basic values
  - [ ] Arrays
  - [ ] Tables
  - [ ] Inline tables
  - [ ] Array of tables
  - [ ] Multiline strings
  - [ ] Date/time

---

## 10. simplex-uuid (UUID Generation)

**Status**: Not implemented
**Priority**: LOW

### Subtasks

- [ ] 10.1 UUID v4 (random)
  ```simplex
  let id = Uuid::v4();
  println(id.to_string());  // "550e8400-e29b-41d4-a716-446655440000"
  ```

- [ ] 10.2 UUID v7 (time-ordered)
  ```simplex
  let id = Uuid::v7();
  ```

- [ ] 10.3 Parsing
  ```simplex
  let id = Uuid::parse("550e8400-e29b-41d4-a716-446655440000")?;
  ```

- [ ] 10.4 Comparison and hashing
  - [ ] Implement Eq, Hash
  - [ ] Use in HashMaps

---

## Completion Criteria

Phase 3 is complete when:
- [ ] All 10 libraries are published to registry
- [ ] Each library has comprehensive tests
- [ ] Each library has documentation
- [ ] At least 2 example applications using the libraries
- [ ] Libraries are used to build Phase 4 (AI/Actor)

---

## Dependencies

- Phase 1: Core features (HashMap, iterators, JSON)
- Phase 2: Package ecosystem for publishing

## Dependents

- Phase 4: AI/Actor libraries will use these
- Community: Can start building real applications
