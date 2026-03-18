# TASK-024: Database & Cache Libraries

**Status**: Planning
**Priority**: High
**Created**: 2026-03-16
**Target Version**: 0.16.0
**Depends On**: TASK-022 Phase 8 (HTTP client, JSON parser), TASK-022 Phase 9 (async/await)

---

## Overview

Production database and cache connectivity. Simplex currently has `simplex-sql` (SQLite bindings). Real-world deployments need PostgreSQL, MySQL, and Redis. All implementations must be **pure Simplex** with minimal C runtime extensions for socket-level protocol handling where necessary.

These libraries speak native wire protocols — no C driver dependencies (libpq, libmysqlclient, etc.). The Simplex runtime already provides TCP sockets via `simplex-std/src/net.sx`.

---

## Library 1: simplex-postgres

**Location**: `simplex-postgres/src/mod.sx`
**Priority**: Critical — the production database for most serious workloads

### Core API
```simplex
struct PgConnection {
    host: String,
    port: i64,
    database: String,
    user: String,
    password: String,
    handle: i64        // Native TCP connection handle
}

fn pg_connect(config: PgConfig) -> Result<PgConnection, PgError>
fn pg_query(conn: PgConnection, sql: String) -> Result<PgResult, PgError>
fn pg_execute(conn: PgConnection, sql: String) -> Result<i64, PgError>
fn pg_prepare(conn: PgConnection, sql: String) -> Result<PgStatement, PgError>
fn pg_bind_execute(stmt: PgStatement, params: Vec<String>) -> Result<PgResult, PgError>
fn pg_close(conn: PgConnection)
```

### Wire Protocol
- Implement PostgreSQL wire protocol v3 (message-based, binary)
- Startup message, authentication (MD5, SCRAM-SHA-256)
- Simple query protocol (text results)
- Extended query protocol (prepared statements, binary parameters)
- Error and notice message handling

### Features
- Prepared statements with parameterized queries (SQL injection safe)
- Transaction support: `pg_begin()`, `pg_commit()`, `pg_rollback()`
- Connection pooling (pool of N connections, checkout/return)
- Result set iteration (row-by-row, not full materialization)
- Type mapping: PostgreSQL types -> Simplex types (text, int, float, bool, null, json, timestamp)
- TLS/SSL connection support

### Async Support
```simplex
async fn pg_query_async(conn: PgConnection, sql: String) -> Result<PgResult, PgError>
```

### Success Criteria
- Connect to PostgreSQL 14+ instance
- Execute DDL (CREATE TABLE), DML (INSERT, UPDATE, DELETE), queries (SELECT)
- Prepared statements prevent SQL injection
- Transactions commit and rollback correctly
- Connection pool handles concurrent access
- New tests: `tests/stdlib/spec_postgres.sx` (requires running PostgreSQL instance)

---

## Library 2: simplex-mysql

**Location**: `simplex-mysql/src/mod.sx`
**Priority**: Medium — second most common production DB

### Core API
```simplex
fn mysql_connect(config: MysqlConfig) -> Result<MysqlConnection, MysqlError>
fn mysql_query(conn: MysqlConnection, sql: String) -> Result<MysqlResult, MysqlError>
fn mysql_execute(conn: MysqlConnection, sql: String) -> Result<i64, MysqlError>
fn mysql_prepare(conn: MysqlConnection, sql: String) -> Result<MysqlStatement, MysqlError>
fn mysql_close(conn: MysqlConnection)
```

### Wire Protocol
- MySQL client/server protocol (COM_QUERY, COM_STMT_PREPARE, COM_STMT_EXECUTE)
- Authentication: `mysql_native_password`, `caching_sha2_password`
- Result set parsing (column definitions + row data)

### Features
- Prepared statements with parameter binding
- Transaction support
- Connection pooling
- Type mapping: MySQL types -> Simplex types
- TLS connection support

### Success Criteria
- Connect to MySQL 8.0+ instance
- CRUD operations with prepared statements
- Transactions work correctly
- New tests: `tests/stdlib/spec_mysql.sx`

---

## Library 3: simplex-redis

**Location**: `simplex-redis/src/mod.sx`
**Priority**: Critical — caching, pub/sub, session storage, queues

### Core API
```simplex
fn redis_connect(host: String, port: i64) -> Result<RedisConnection, RedisError>
fn redis_auth(conn: RedisConnection, password: String) -> Result<bool, RedisError>
fn redis_command(conn: RedisConnection, args: Vec<String>) -> Result<RedisValue, RedisError>
fn redis_close(conn: RedisConnection)

enum RedisValue {
    SimpleString(String),
    Error(String),
    Integer(i64),
    BulkString(String),
    Array(Vec<RedisValue>),
    Null
}
```

### Wire Protocol
- RESP3 (Redis Serialization Protocol v3)
- Simple, well-documented text protocol — ideal for pure Simplex implementation

### Commands (Core Subset)
```simplex
// String operations
fn redis_get(conn: RedisConnection, key: String) -> Result<Option<String>, RedisError>
fn redis_set(conn: RedisConnection, key: String, value: String) -> Result<bool, RedisError>
fn redis_setex(conn: RedisConnection, key: String, seconds: i64, value: String) -> Result<bool, RedisError>
fn redis_del(conn: RedisConnection, key: String) -> Result<i64, RedisError>

// Hash operations
fn redis_hset(conn: RedisConnection, key: String, field: String, value: String) -> Result<i64, RedisError>
fn redis_hget(conn: RedisConnection, key: String, field: String) -> Result<Option<String>, RedisError>
fn redis_hgetall(conn: RedisConnection, key: String) -> Result<Vec<(String, String)>, RedisError>

// List operations
fn redis_lpush(conn: RedisConnection, key: String, value: String) -> Result<i64, RedisError>
fn redis_rpop(conn: RedisConnection, key: String) -> Result<Option<String>, RedisError>
fn redis_lrange(conn: RedisConnection, key: String, start: i64, stop: i64) -> Result<Vec<String>, RedisError>

// Pub/Sub
fn redis_subscribe(conn: RedisConnection, channel: String) -> Result<RedisSubscription, RedisError>
fn redis_publish(conn: RedisConnection, channel: String, message: String) -> Result<i64, RedisError>

// Key operations
fn redis_exists(conn: RedisConnection, key: String) -> Result<bool, RedisError>
fn redis_expire(conn: RedisConnection, key: String, seconds: i64) -> Result<bool, RedisError>
fn redis_ttl(conn: RedisConnection, key: String) -> Result<i64, RedisError>
```

### Features
- Connection pooling
- Pipeline support (batch multiple commands)
- Pub/Sub with message callback
- TTL-based expiry
- TLS connection support

### Cognitive Hive Integration
Redis is a natural fit for Simplex's distributed actor model:
- **Actor mailbox backing**: Persistent mailboxes via Redis lists
- **Hive shared state**: Redis hashes for mnemonic (shared memory)
- **Belief propagation**: Redis pub/sub for cross-hive belief updates
- **Session/checkpoint storage**: Actor checkpoint persistence

### Success Criteria
- Connect to Redis 7+ instance
- GET/SET/DEL with correct RESP3 encoding
- Pub/Sub message delivery
- Pipeline batching reduces round-trips
- Connection pool handles concurrent actor access
- New tests: `tests/stdlib/spec_redis.sx`

---

## Dependency Graph

```
TASK-022 Phase 8 (HTTP + JSON) + Phase 9 (Async)
    |
    +--> simplex-postgres (wire protocol over TCP)
    +--> simplex-mysql (wire protocol over TCP)
    +--> simplex-redis (RESP3 over TCP)
```

All three are independent and can be built in parallel. They all depend on `simplex-std/src/net.sx` for TCP socket support.

---

## Estimated Line Counts

| Library | Est. Lines |
|---------|-----------|
| simplex-postgres | ~2,000-2,800 |
| simplex-mysql | ~1,800-2,400 |
| simplex-redis | ~1,200-1,600 |
| **Total** | **~5,000-6,800** |
