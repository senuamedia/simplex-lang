# Simplex v0.16.0 — Enterprise Libraries & Hive Intelligence

**Release Date:** 2026-03-19

## Overview

v0.16.0 is a major ecosystem release delivering the libraries needed for production applications: data format parsing, unified database connectivity, real-time communication protocols, authentication, observability, and AI-native intelligence infrastructure. This release adds ~35,000 lines of pure Simplex code across 18 new modules.

---

## Data Format Libraries (TASK-023)

### CSV Parser/Serializer
RFC 4180 compliant CSV parsing with configurable delimiters, quoted field handling, and header detection.

```simplex
let table = csv_parse_default(string_from("name,age\nAlice,30\nBob,25"));
let name = csv_get_cell(table, 0, 0);  // "Alice"
```

### YAML 1.2 Parser
Full YAML Core Schema support including mappings, sequences, multi-line strings, flow style, and comments.

### XML Parser
Well-formed XML parsing with namespace awareness, entity references, CDATA sections, and XPath-lite queries.

## Unified Database Connectivity (TASK-024)

### simplex-db — One Package, All Databases

Unified CRUD interface with pluggable drivers:

```simplex
let config = db_config_new(DRIVER_POSTGRES());
db_config_set(config, string_from("host"), string_from("localhost"));
let conn = db_connect(config);
let result = db_query(conn, string_from("SELECT * FROM users"));
db_close(conn);
```

**SQL Drivers:** PostgreSQL (wire protocol v3), MySQL (client/server protocol), SQLite
**NoSQL Drivers:** Redis (RESP3)
**Planned:** MongoDB, DynamoDB

Features: connection pooling, transactions, prepared statements, key-value operations.

## Real-Time Communication (TASK-025)

### WebSocket (RFC 6455)
Client and server WebSocket with frame encoding/decoding, masking, fragmentation, and control frames.

### Protocol Buffers
Proto3 wire format encoding/decoding with varint, length-delimited, and fixed-width fields.

### NATS Messaging
Lightweight pub/sub messaging with request/reply, subject wildcards, and queue groups.

## Authentication & Security (TASK-026)

### JWT (JSON Web Tokens)
HS256 signing and verification with base64url encoding, HMAC-SHA256, standard claims, and expiration checking.

### OAuth 2.0
Authorization Code flow with PKCE, provider presets (GitHub, Google, Microsoft), token refresh.

### dotenv
`.env` file parser with quoted values, comments, variable expansion.

## Observability (TASK-028)

### Prometheus Metrics
Counter, gauge, histogram metric types with labels, registry, and text exposition format for `/metrics` endpoint.

### OpenTelemetry Tracing
W3C Trace Context propagation, span lifecycle, attributes/events, OTLP JSON export, auto-instrumentation helpers for actors/SLM/hives.

## Hive Intelligence (TASK-029)

### VectorDB
In-memory vector store with cosine similarity, HNSW index, brute-force search, metadata filtering, and disk persistence.

### RAG Pipeline
Document ingestion with chunking (fixed, sentence, paragraph), embedding, vector search, context building, and source attribution.

### Guardrails
Output validation with configurable rules: max/min length, must contain/not contain, PII detection, JSON validity, retry/fallback actions.

### Eval Suite
Model evaluation with exact match, Levenshtein distance, BLEU score, length ratio, and A/B comparison.

---

## New Packages

| Package | Description | Lines |
|---------|-------------|:-----:|
| `simplex-db` | Unified SQL/NoSQL database connectivity | 7,544 |
| `simplex-prometheus` | Prometheus metrics exposition | 1,792 |
| `simplex-opentelemetry` | Distributed tracing (OpenTelemetry) | 2,106 |
| `simplex-protobuf` | Protocol Buffers encoding/decoding | 1,465 |
| `simplex-nats` | NATS messaging protocol | 1,719 |
| `simplex-vectordb` | Vector similarity search | 1,976 |
| `simplex-rag` | Retrieval-Augmented Generation pipeline | 1,010 |
| `simplex-guardrails` | Output validation and safety | 989 |
| `simplex-eval` | Model evaluation framework | 855 |

## New Standard Library Modules

| Module | Description | Lines |
|--------|-------------|:-----:|
| `csv.sx` | CSV parser/serializer | 772 |
| `yaml.sx` | YAML 1.2 parser | 1,690 |
| `xml.sx` | XML parser | 1,402 |
| `jwt.sx` | JSON Web Tokens | 1,230 |
| `oauth.sx` | OAuth 2.0 flows | 1,083 |
| `dotenv.sx` | .env file parser | 591 |
| `websocket.sx` | WebSocket RFC 6455 | 1,660 |

## Test Suite

**215 tests across 21 categories — 100% pass rate**

19 new test files added for v0.16.0 covering all new libraries.

## Breaking Changes

- `simplex-postgres`, `simplex-mysql`, `simplex-redis`, `simplex-sql` replaced by unified `simplex-db`
- Wire protocol implementations preserved as drivers within `simplex-db/drivers/`

## Dependencies

No new external dependencies. All new code is pure Simplex.

## Contributors

- Rod Higgins
