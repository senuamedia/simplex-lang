# TASK-028: Observability Libraries

**Status**: Complete
**Priority**: High
**Created**: 2026-03-16
**Target Version**: 0.16.0
**Depends On**: TASK-022 Phase 8 (HTTP client), TASK-022 Phase 9 (async/await)

---

## Overview

Observability is **critical** for Simplex's distributed actor/hive architecture. When you have dozens of actors, multiple hives, SLM inference, belief propagation, and specialist routing all running concurrently — you need to see what's happening. These libraries make the invisible visible.

All implementations must be **pure Simplex**.

---

## Library 1: simplex-prometheus

**Location**: `simplex-prometheus/src/mod.sx`
**Priority**: Critical — the standard for metrics in distributed systems

### Core API
```simplex
// Metric types
fn prom_counter(name: String, help: String) -> PromCounter
fn prom_gauge(name: String, help: String) -> PromGauge
fn prom_histogram(name: String, help: String, buckets: Vec<f64>) -> PromHistogram

// Counter operations
fn prom_counter_inc(counter: PromCounter)
fn prom_counter_add(counter: PromCounter, value: f64)

// Gauge operations
fn prom_gauge_set(gauge: PromGauge, value: f64)
fn prom_gauge_inc(gauge: PromGauge)
fn prom_gauge_dec(gauge: PromGauge)

// Histogram operations
fn prom_histogram_observe(histogram: PromHistogram, value: f64)

// Labels
fn prom_counter_with_labels(name: String, help: String, label_names: Vec<String>) -> PromCounterVec
fn prom_counter_vec_inc(counter: PromCounterVec, label_values: Vec<String>)

// Exposition
fn prom_serve(addr: String, port: i64) -> Result<PromServer, PromError>
fn prom_export() -> String  // Returns metrics in Prometheus text format
```

### Built-in Actor Metrics (Auto-Instrumented)
When enabled, Simplex automatically exposes:
```
# Actor metrics
simplex_actor_messages_received_total{actor="OrderProcessor"} 1523
simplex_actor_messages_processed_total{actor="OrderProcessor"} 1520
simplex_actor_message_processing_seconds{actor="OrderProcessor",quantile="0.99"} 0.045
simplex_actor_mailbox_depth{actor="OrderProcessor"} 3
simplex_actor_restarts_total{actor="OrderProcessor"} 2

# Hive metrics
simplex_hive_specialists_active{hive="main"} 5
simplex_hive_belief_updates_total{hive="main"} 892
simplex_hive_mnemonic_size_bytes{hive="main"} 45632

# SLM metrics
simplex_slm_inference_total{model="smollm-135m"} 340
simplex_slm_inference_seconds{model="smollm-135m",quantile="0.5"} 0.12
simplex_slm_context_usage_ratio{model="smollm-135m"} 0.67
simplex_slm_memory_bytes{model="smollm-135m"} 142606336
```

### Features
- Counter, Gauge, Histogram, Summary metric types
- Label support for dimensional metrics
- HTTP endpoint serving Prometheus text exposition format
- Auto-instrumentation hooks for actors, hives, SLM
- Metric registry (global, thread-safe)

### Success Criteria
- Prometheus scraper can pull metrics from `/metrics` endpoint
- Counter, gauge, histogram all produce correct exposition format
- Actor auto-instrumentation captures message throughput and latency
- SLM metrics track inference performance
- New test: `tests/observability/spec_prometheus.sx`

---

## Library 2: simplex-opentelemetry

**Location**: `simplex-opentelemetry/src/mod.sx`
**Priority**: High — distributed tracing across actors and hives

### Core API
```simplex
// Tracing
fn otel_start_span(name: String) -> OtelSpan
fn otel_start_child_span(parent: OtelSpan, name: String) -> OtelSpan
fn otel_end_span(span: OtelSpan)
fn otel_set_attribute(span: OtelSpan, key: String, value: String)
fn otel_set_status(span: OtelSpan, status: OtelStatus)
fn otel_add_event(span: OtelSpan, name: String, attributes: Vec<OtelAttribute>)

// Context propagation
fn otel_inject(span: OtelSpan) -> Vec<String>   // Returns trace context headers
fn otel_extract(headers: Vec<String>) -> OtelContext  // Extracts trace context from headers

// Exporter
fn otel_export_otlp(endpoint: String, spans: Vec<OtelSpan>) -> Result<bool, OtelError>
fn otel_export_stdout(spans: Vec<OtelSpan>)

struct OtelSpan {
    trace_id: String,
    span_id: String,
    parent_span_id: Option<String>,
    name: String,
    start_time: i64,
    end_time: i64,
    attributes: Vec<OtelAttribute>,
    events: Vec<OtelEvent>,
    status: OtelStatus
}
```

### Built-in Actor Tracing (Auto-Instrumented)
```
Trace: OrderProcessing (trace_id: abc123)
├── Span: actor.receive OrderProcessor::Process  [12ms]
│   ├── Span: actor.send PaymentHandler::Charge  [3ms]
│   │   ├── Span: slm.infer fraud_check          [45ms]
│   │   │   └── Event: belief_update confidence=0.92
│   │   └── Span: pg.query charge_card            [8ms]
│   ├── Span: actor.send ShippingActor::Ship      [2ms]
│   └── Span: hive.specialist_route               [1ms]
│       └── Event: specialist_selected=NLP
```

### Features
- W3C Trace Context propagation (traceparent/tracestate headers)
- OTLP exporter (HTTP/JSON to Jaeger, Tempo, Datadog, etc.)
- Auto-instrumentation for:
  - Actor `send`/`receive`/`ask` — each message creates a span
  - SLM `infer` — inference calls create spans with model/latency attributes
  - Hive specialist routing — routing decisions create spans
  - Belief updates — recorded as span events
  - HTTP client calls — outbound HTTP creates child spans
  - Database queries — query spans with sanitized SQL
- Sampling (head-based, tail-based, always-on)
- Batch span processor (buffer and flush periodically)

### Cognitive Hive Integration
This is where OpenTelemetry becomes uniquely valuable for Simplex:
- **Trace belief propagation** across hives — see how a belief update in one hive affects decisions in another
- **Trace SLM reasoning chains** — follow the memory-augmented context through inference
- **Trace specialist routing** — understand why the queen routed to a specific specialist
- **Cross-hive traces** — when Nexus Protocol connects hives, trace context flows with messages

### Success Criteria
- Spans export to OTLP endpoint (Jaeger or stdout)
- Actor message sends automatically create linked spans
- SLM inference spans capture model, token count, latency
- Trace context propagates through actor.send -> actor.receive
- Cross-hive traces link correctly via Nexus Protocol
- New test: `tests/observability/spec_opentelemetry.sx`

---

## Library 3: simplex-log (Enhancement)

**Location**: `simplex-std/src/log.sx` (exists — enhance)
**Priority**: Medium — structured logging to complement metrics and traces

### Enhancements
- **Structured logging**: JSON output format for log aggregators
- **Log levels**: TRACE, DEBUG, INFO, WARN, ERROR with filtering
- **Trace correlation**: Attach `trace_id` and `span_id` to log entries
- **Actor context**: Automatically include actor name and hive in log entries
- **Output targets**: stdout, stderr, file, custom sink

### Success Criteria
- Structured JSON log output
- Log entries include trace_id when tracing is active
- Actor context is auto-attached
- Log level filtering works at runtime

---

## Dependency Graph

```
TASK-022 Phase 8 (HTTP client)
    |
    +--> simplex-prometheus (HTTP endpoint for /metrics)
    |
    +--> simplex-opentelemetry (HTTP POST to OTLP endpoint)
    |
    +--> simplex-log enhancement (independent, file I/O)
```

All three are independent. Prometheus and OpenTelemetry can be built in parallel.

---

## Estimated Line Counts

| Library | Est. Lines |
|---------|-----------|
| simplex-prometheus | ~1,200-1,600 |
| simplex-opentelemetry | ~1,800-2,400 |
| simplex-log enhancement | ~400-600 |
| **Total** | **~3,400-4,600** |
