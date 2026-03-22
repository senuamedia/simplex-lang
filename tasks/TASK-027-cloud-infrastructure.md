# TASK-027: Cloud Infrastructure Libraries

**Status**: Complete
**Priority**: Medium
**Created**: 2026-03-16
**Target Version**: 0.17.0
**Depends On**: TASK-022 Phase 8 (HTTP client, JSON parser), TASK-026 (auth/JWT for signing), TASK-023 (XML for some AWS APIs)

---

## Overview

Cloud service connectivity for production deployments. Simplex already has `simplex-s3` and `simplex-ses` (AWS). This task extends cloud support to cover the most critical services for distributed actor/hive workloads.

All implementations must be **pure Simplex**. AWS services use HTTP REST APIs with Signature V4 auth — all implementable over `simplex-std/src/http_client.sx`.

---

## Library 1: simplex-sqs

**Location**: `simplex-sqs/src/mod.sx`
**Priority**: Critical — message queues are the backbone of distributed actor systems

### Core API
```simplex
fn sqs_send(queue_url: String, body: String, config: AwsConfig) -> Result<SqsSendResult, SqsError>
fn sqs_receive(queue_url: String, max_messages: i64, wait_seconds: i64, config: AwsConfig) -> Result<Vec<SqsMessage>, SqsError>
fn sqs_delete(queue_url: String, receipt_handle: String, config: AwsConfig) -> Result<bool, SqsError>
fn sqs_create_queue(name: String, config: AwsConfig) -> Result<String, SqsError>
fn sqs_delete_queue(queue_url: String, config: AwsConfig) -> Result<bool, SqsError>

struct SqsMessage {
    message_id: String,
    receipt_handle: String,
    body: String,
    attributes: Vec<SqsAttribute>
}
```

### Features
- Standard and FIFO queues
- Long polling (wait_seconds parameter)
- Message attributes (metadata)
- Batch send/receive/delete
- Dead letter queue configuration
- Message visibility timeout management

### Cognitive Hive Integration
- **Actor mailbox persistence**: SQS as durable mailbox for actors that survive crashes
- **Inter-hive messaging**: SQS as reliable transport between hives in different regions
- **Work distribution**: SQS queues feeding specialist actors in a hive

### Success Criteria
- Send and receive messages from SQS queue
- Long polling returns messages efficiently
- FIFO ordering maintained
- Batch operations work correctly
- New test: `tests/cloud/spec_sqs.sx`

---

## Library 2: simplex-sns

**Location**: `simplex-sns/src/mod.sx`
**Priority**: Medium — pub/sub at scale, fan-out to multiple subscribers

### Core API
```simplex
fn sns_publish(topic_arn: String, message: String, config: AwsConfig) -> Result<String, SnsError>
fn sns_create_topic(name: String, config: AwsConfig) -> Result<String, SnsError>
fn sns_subscribe(topic_arn: String, protocol: String, endpoint: String, config: AwsConfig) -> Result<String, SnsError>
fn sns_unsubscribe(subscription_arn: String, config: AwsConfig) -> Result<bool, SnsError>
```

### Features
- Topic creation and management
- Publish to topic (fan-out to all subscribers)
- Subscribe: SQS, HTTP/HTTPS, email, Lambda
- Message filtering policies
- Message attributes

### Success Criteria
- Create topic and publish message
- SQS subscription receives published messages
- Message attributes preserved through delivery
- New test: `tests/cloud/spec_sns.sx`

---

## Library 3: simplex-dynamodb

**Location**: `simplex-dynamodb/src/mod.sx`
**Priority**: Medium — serverless NoSQL, natural fit for actor state persistence

### Core API
```simplex
fn dynamo_put(table: String, item: Vec<DynamoAttr>, config: AwsConfig) -> Result<bool, DynamoError>
fn dynamo_get(table: String, key: Vec<DynamoAttr>, config: AwsConfig) -> Result<Option<Vec<DynamoAttr>>, DynamoError>
fn dynamo_query(table: String, key_condition: String, values: Vec<DynamoAttr>, config: AwsConfig) -> Result<Vec<Vec<DynamoAttr>>, DynamoError>
fn dynamo_delete(table: String, key: Vec<DynamoAttr>, config: AwsConfig) -> Result<bool, DynamoError>
fn dynamo_update(table: String, key: Vec<DynamoAttr>, update_expr: String, values: Vec<DynamoAttr>, config: AwsConfig) -> Result<bool, DynamoError>

struct DynamoAttr {
    name: String,
    value: DynamoValue
}

enum DynamoValue {
    S(String),       // String
    N(String),       // Number (stored as string)
    B(Vec<i64>),     // Binary
    BOOL(bool),
    NULL,
    L(Vec<DynamoValue>),              // List
    M(Vec<DynamoAttr>)                // Map
}
```

### Features
- Single-item operations (Get, Put, Delete, Update)
- Query with key conditions and filter expressions
- Scan with filter (for small tables)
- Batch operations (BatchGetItem, BatchWriteItem)
- Conditional writes (optimistic locking)
- Expression attribute names and values

### Cognitive Hive Integration
- **Actor checkpoint storage**: DynamoDB as checkpoint backend for actor state
- **Belief persistence**: Store belief stores with confidence scores
- **Hive mnemonic storage**: Shared memory backed by DynamoDB

### Success Criteria
- CRUD operations on DynamoDB table
- Query with key conditions returns correct results
- Conditional writes fail on condition mismatch
- Batch operations handle 25-item limit correctly
- New test: `tests/cloud/spec_dynamodb.sx`

---

## Library 4: simplex-kafka

**Location**: `simplex-kafka/src/mod.sx`
**Priority**: Medium — event streaming for data pipelines

### Core API
```simplex
fn kafka_producer(config: KafkaConfig) -> Result<KafkaProducer, KafkaError>
fn kafka_produce(producer: KafkaProducer, topic: String, key: String, value: String) -> Result<KafkaOffset, KafkaError>

fn kafka_consumer(config: KafkaConfig, group_id: String) -> Result<KafkaConsumer, KafkaError>
fn kafka_subscribe(consumer: KafkaConsumer, topics: Vec<String>) -> Result<bool, KafkaError>
fn kafka_poll(consumer: KafkaConsumer, timeout_ms: i64) -> Result<Vec<KafkaRecord>, KafkaError>
fn kafka_commit(consumer: KafkaConsumer) -> Result<bool, KafkaError>

struct KafkaRecord {
    topic: String,
    partition: i64,
    offset: i64,
    key: String,
    value: String,
    timestamp: i64
}
```

### Wire Protocol
- Kafka binary protocol over TCP
- Producer: Produce request/response
- Consumer: Fetch, OffsetFetch, OffsetCommit
- Group coordinator: JoinGroup, SyncGroup, Heartbeat
- Metadata discovery

### Features
- Producer with batching and compression
- Consumer with group coordination
- Offset management (auto-commit and manual)
- Partition assignment strategies
- SASL/PLAIN authentication
- TLS transport

### Cognitive Hive Integration
- **Event sourcing**: Kafka topics as event log for hive decisions
- **Data pipeline input**: Kafka consumer as data source feeding specialist actors
- **Audit trail**: All belief updates and SLM inferences logged to Kafka

### Success Criteria
- Produce messages to Kafka topic
- Consumer group reads messages with offset tracking
- Multiple consumers in same group share partitions
- Manual commit prevents message reprocessing
- New test: `tests/cloud/spec_kafka.sx`

---

## Shared: simplex-aws-auth

**Location**: `simplex-aws/src/auth.sx`
**Priority**: Critical (blocker for all AWS libraries)

### AWS Signature V4
```simplex
fn aws_sign_request(
    method: String,
    url: String,
    headers: Vec<String>,
    body: String,
    config: AwsConfig
) -> Vec<String>  // Returns signed headers

struct AwsConfig {
    access_key_id: String,
    secret_access_key: String,
    region: String,
    service: String
}
```

This is shared by simplex-s3, simplex-ses, simplex-sqs, simplex-sns, simplex-dynamodb. Extract from existing `simplex-s3` implementation if it already has signing.

---

## Dependency Graph

```
TASK-022 Phase 8 (HTTP + JSON)
    |
    v
simplex-aws-auth (Signature V4) -- shared by all AWS libs
    |
    +--> simplex-sqs
    +--> simplex-sns
    +--> simplex-dynamodb

TASK-022 Phase 8 + Phase 9 (Async)
    |
    +--> simplex-kafka (binary protocol, independent of AWS)
```

---

## Estimated Line Counts

| Library | Est. Lines |
|---------|-----------|
| simplex-aws-auth | ~400-600 |
| simplex-sqs | ~800-1,000 |
| simplex-sns | ~600-800 |
| simplex-dynamodb | ~1,200-1,600 |
| simplex-kafka | ~2,000-2,800 |
| **Total** | **~5,000-6,800** |
