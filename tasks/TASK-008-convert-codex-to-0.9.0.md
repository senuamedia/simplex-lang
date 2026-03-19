# TASK-008: Convert Codex to Simplex 0.9.0

**Status**: Blocked — Codex is external project, not in this repository
**Priority**: High
**Created**: 2026-01-11
**Updated**: 2026-03-16 (audited against codebase)
**Target Version**: Codex 0.2.0 (using Simplex 0.9.0)

> **Audit Note (2026-03-16):** Codex lives at `/Users/rod/code/codex/` (external repo),
> not in the simplex repository. All 0.9.0+ infrastructure needed for migration exists
> in simplex (annealing, dual numbers, learnable schedules, inference pipeline), but
> the migration cannot proceed without access to the Codex source.
> Missing Codex-specific items: O(1) LRU cache, request deduplication, channel-based pool returns.
**Depends On**: Simplex 0.9.0 Release

## Overview

Convert the Codex backend application from its current state (using embedded/local simplex libraries) to use the canonical Simplex 0.9.0 libraries and take advantage of new features including self-learning annealing, the restructured test suite, and the new library architecture.

Codex is a Simplex-powered coding assistant backend for ciara.senuamedia.com, featuring:
- HTTP server with SSE streaming
- Cognitive hives for AI processing (coding, planning, documentation, personal)
- Native document storage with ACID transactions
- AWS S3/SES integration
- JWT authentication

---

## Current Codex Structure

### Source Files (`/Users/rod/code/codex/src/`)

| File | Purpose |
|------|---------|
| `main.sx` | Entry point, server initialization |
| `config.sx` | Configuration loading from environment |
| `state.sx` | Application state management |
| `middleware.sx` | HTTP middleware (logging, auth, compression) |
| `models.sx` | Data models (User, Project, Conversation, Message) |
| `lib.sx` | Library exports |

### API Modules (`src/api/`)

| Module | Endpoints |
|--------|-----------|
| `mod.sx` | Router builder |
| `health.sx` | `/health`, `/ready` |
| `auth.sx` | `/api/auth/register`, `/api/auth/login`, `/api/auth/session` |
| `projects.sx` | `/api/projects/*` |
| `conversations.sx` | `/api/conversations/*` |
| `messages.sx` | `/api/messages/*` (with SSE streaming) |
| `settings.sx` | `/api/settings/*` |
| `artifacts.sx` | `/api/artifacts/*` |

### Cognitive Hives (`src/hives/`)

| Hive | Purpose |
|------|---------|
| `orchestrator.sx` | Routes requests to appropriate hives |
| `coding.sx` | Code generation and analysis |
| `planning.sx` | Project planning and task breakdown |
| `documentation.sx` | Documentation generation |
| `personal.sx` | Personal assistant functionality |
| `auth.sx` | Authentication-related cognitive tasks |

### Inference (`src/inference/`)

| Module | Purpose |
|--------|---------|
| `mod.sx` | Inference coordination |
| `batcher.sx` | Request batching |

### Libraries (`/Users/rod/code/codex/lib/`)

| Library | Files | Purpose |
|---------|-------|---------|
| `simplex-http` | 10 files | HTTP server, client, routing, SSE, CORS |
| `simplex-storage` | 7 files | Document storage, KV store, WAL, transactions |
| `simplex-ses` | 5 files | AWS SES email integration |
| `simplex-s3` | 6 files | AWS S3 object storage |
| `simplex-inference` | 6 files | Inference pipeline, caching, batching, pooling |
| `simplex-training` | 14 files | Training schedules, meta-training, compression |

---

## Simplex 0.9.0 Changes to Leverage

### 1. New Library Architecture

Simplex 0.9.0 provides canonical libraries at `/Users/rod/code/simplex/`:

| Library | Location | Notes |
|---------|----------|-------|
| `simplex-std` | `/simplex-std/` | Standard library |
| `simplex-http` | `/simplex-http/` | HTTP server/client |
| `simplex-json` | `/simplex-json/` | JSON parsing |
| `simplex-sql` | `/simplex-sql/` | SQL database |
| `simplex-toml` | `/simplex-toml/` | TOML parsing |
| `simplex-uuid` | `/simplex-uuid/` | UUID generation |
| `simplex-inference` | `/simplex-inference/` | Inference pipeline |
| `simplex-learning` | `/simplex-learning/` | Online learning |
| `simplex-training` | `/simplex-training/` or `/simplex-training/` | Training pipelines |

### 2. Self-Learning Annealing (v0.9.0)

The new `self_learn_anneal()` API can optimize:
- **Inference routing**: Learn optimal specialist selection
- **Batch sizing**: Learn optimal batch sizes for throughput/latency tradeoff
- **Cache eviction**: Learn optimal cache policies

```simplex
use simplex::optimize::anneal::{self_learn_anneal, AnnealConfig};

// In orchestrator: learn optimal routing
let routing_schedule = self_learn_anneal(
    |routing| evaluate_routing_quality(routing),
    initial_routing,
    |r| mutate_routing(r),
    AnnealConfig::default()
);
```

### 3. Dual Numbers for Gradient Tracking (v0.8.0)

Track inference quality gradients:

```simplex
let quality: dual = evaluate_response(response);
let sensitivity = quality.der;  // How sensitive is quality to input changes?
```

### 4. Test Suite Structure (v0.9.0)

New test organization with prefixes:
- `unit_` - Unit tests
- `spec_` - Specification tests
- `integ_` - Integration tests
- `e2e_` - End-to-end tests

---

## Migration Tasks

### Phase 1: Library Consolidation

**Goal**: Use canonical Simplex 0.9.0 libraries instead of embedded copies.

#### 1.1 Update Modulus.toml

```toml
[package]
name = "codex"
version = "0.2.0"
description = "Simplex-powered coding assistant backend"

[dependencies]
# Core from simplex 0.9.0
simplex-std = { path = "../simplex/simplex-std" }
simplex-http = { path = "../simplex/simplex-http" }
simplex-json = { path = "../simplex/simplex-json" }
simplex-sql = { path = "../simplex/simplex-sql" }
simplex-uuid = { path = "../simplex/simplex-uuid" }

# AI/Inference from simplex 0.9.0
simplex-inference = { path = "../simplex/simplex-inference" }
simplex-learning = { path = "../simplex/simplex-learning" }
simplex-training = { path = "../simplex/simplex-training" }

# Codex-specific (keep embedded)
simplex-storage = { path = "lib/simplex-storage" }
simplex-ses = { path = "lib/simplex-ses" }
simplex-s3 = { path = "lib/simplex-s3" }
```

#### 1.2 Reconcile Library Differences

| Codex Library | Simplex 0.9.0 Library | Action |
|---------------|----------------------|--------|
| `lib/simplex-http` | `simplex-http` | Replace with 0.9.0 version, migrate custom code |
| `lib/simplex-inference` | `simplex-inference` | Merge optimizations (O(1) cache, dedup) into 0.9.0 |
| `simplex-training` | `simplex-training`, `simplex-training` | Use 0.9.0 canonical version |
| `lib/simplex-storage` | N/A | Keep Codex-specific |
| `lib/simplex-ses` | N/A | Keep Codex-specific |
| `lib/simplex-s3` | N/A | Keep Codex-specific |

#### 1.3 Migrate Custom Optimizations

The Codex `simplex-inference` library has optimizations that should be contributed upstream:

1. **O(1) LRU Cache** (`cache.sx:LruList<K>`)
   - Doubly-linked list for O(1) access/eviction
   - Migrate to `simplex/simplex-inference/src/cache.sx`

2. **Request Deduplication** (`batcher.sx`)
   - `submit_with_key()` for coalescing identical requests
   - `requests_deduplicated` metric
   - Migrate to `simplex/simplex-inference/src/batcher.sx`

3. **Channel-Based Pool Returns** (`pool.sx`)
   - Non-blocking instance returns via channels
   - Avoids task spawning in Drop
   - Migrate to `simplex/simplex-inference/src/pool.sx`

---

### Phase 2: API Compatibility

**Goal**: Ensure imports and APIs align with 0.9.0 conventions.

#### 2.1 Import Path Updates

```simplex
// Before (Codex embedded)
use simplex_http::server::HttpServer;
use simplex_http::cors::CorsMiddleware;

// After (Simplex 0.9.0)
use simplex_http::HttpServer;
use simplex_http::middleware::Cors;
```

#### 2.2 API Signature Updates

Review and update any API differences between embedded libraries and 0.9.0 versions.

| Area | Check |
|------|-------|
| HTTP Server | Request/Response types, routing API |
| JSON | Parse/serialize signatures |
| Inference | Pipeline configuration |
| Learning | Optimizer APIs |

---

### Phase 3: Leverage New Features

**Goal**: Use 0.9.0 features to improve Codex.

#### 3.1 Self-Learning Annealing for Orchestrator

```simplex
// src/hives/orchestrator.sx
use simplex::optimize::anneal::{self_learn_anneal, AnnealConfig, LearnableSchedule};

specialist Orchestrator {
    routing_schedule: LearnableSchedule,

    fn route_request(&self, request: InferRequest) -> HiveId {
        let temp = self.routing_schedule.temperature(
            dual::constant(self.total_requests as f64),
            dual::constant(self.recent_errors as f64)
        );

        if temp.val > 0.5 {
            // High temp: explore different hives
            self.exploratory_routing(request)
        } else {
            // Low temp: use learned optimal routing
            self.greedy_routing(request)
        }
    }

    fn update_routing(&mut self, feedback: RoutingFeedback) {
        // Meta-gradient update based on response quality
        let grad = self.routing_schedule.gradient();
        self.routing_schedule.update(grad, 0.001);
    }
}
```

#### 3.2 Dual Numbers for Quality Tracking

```simplex
// src/inference/quality.sx
use simplex::dual;

fn evaluate_response_quality(
    response: &Response,
    user_feedback: Option<Feedback>
) -> dual {
    let base_quality = dual::variable(response.confidence);

    let length_penalty = if response.tokens > 4000 {
        dual::constant(-0.1)
    } else {
        dual::constant(0.0)
    };

    let feedback_bonus = match user_feedback {
        Some(Feedback::Positive) => dual::constant(0.2),
        Some(Feedback::Negative) => dual::constant(-0.3),
        None => dual::constant(0.0),
    };

    base_quality + length_penalty + feedback_bonus
}
```

#### 3.3 Learnable Training Schedules

```simplex
// For fine-tuning specialists on user data
use simplex_training::{MetaTrainer, LearnableLRSchedule};

async fn fine_tune_specialist(
    specialist: &mut Specialist,
    user_data: Vec<Example>
) {
    let trainer = MetaTrainer::new()
        .with_learnable_lr()
        .with_learnable_distillation();

    let result = trainer.meta_train(specialist, user_data).await;
}
```

---

### Phase 4: Test Suite Migration

**Goal**: Create comprehensive tests following 0.9.0 conventions.

#### 4.1 Test Directory Structure

```
codex/tests/
├── unit/
│   ├── unit_config.sx          # Configuration parsing
│   ├── unit_models.sx          # Data model tests
│   ├── unit_middleware.sx      # Middleware tests
│   └── unit_routing.sx         # Router tests
├── spec/
│   ├── spec_auth.sx            # Auth specification compliance
│   ├── spec_api_contracts.sx   # API contract tests
│   └── spec_sse_streaming.sx   # SSE behavior tests
├── integ/
│   ├── integ_storage.sx        # Storage integration
│   ├── integ_hives.sx          # Hive coordination
│   └── integ_inference.sx      # Inference pipeline
├── e2e/
│   ├── e2e_user_journey.sx     # Full user workflows
│   ├── e2e_conversation.sx     # Conversation lifecycle
│   └── e2e_project_flow.sx     # Project management flow
└── run_tests.sh                # Test runner script
```

#### 4.2 Migrate Existing Tests

Convert existing `api_tests.sx` to new structure:

| Current | New Location | Prefix |
|---------|--------------|--------|
| Auth tests | `tests/spec/spec_auth.sx` | `spec_` |
| Health tests | `tests/unit/unit_health.sx` | `unit_` |
| Message flow | `tests/e2e/e2e_conversation.sx` | `e2e_` |

---

### Phase 5: Build and Deploy

**Goal**: Compile and deploy Codex with sxc 0.9.0.

#### 5.1 Build Script Updates

```bash
#!/bin/bash
# codex/build.sh

# Use simplex 0.9.0 compiler
SXC="../simplex/sxc"

# Build Codex
$SXC build src/main.sx -o codex_server

# Run tests
./tests/run_tests.sh
```

#### 5.2 Deployment Configuration

```toml
# codex/deploy.toml
[deploy]
target = "linux-x86_64"
instance_type = "inf2.xlarge"  # Inferentia2 for inference
region = "ap-southeast-2"

[runtime]
model = "simplex-cognitive-7b"
pool_size = 4
batch_timeout_ms = 50
```

#### 5.3 EC2 Deployment

1. Build on Linux (inf2.xlarge instance)
2. Configure with environment variables
3. Run codex_server on port 8080
4. Verify at codex.senuamedia.com:8080

---

## Success Criteria

### Phase 1: Library Consolidation
- [ ] Modulus.toml updated with 0.9.0 dependencies
- [ ] Codex compiles with simplex 0.9.0 libraries
- [ ] O(1) cache optimization merged upstream
- [ ] Deduplication feature merged upstream
- [ ] Channel-based pool returns merged upstream

### Phase 2: API Compatibility
- [ ] All import paths updated
- [ ] No API compatibility errors
- [ ] All endpoints functional

### Phase 3: New Features
- [ ] Orchestrator uses self-learning annealing
- [ ] Quality tracking with dual numbers
- [ ] Learnable training schedules integrated

### Phase 4: Test Suite
- [ ] Tests organized with prefix convention
- [ ] All tests pass
- [ ] Coverage > 80%

### Phase 5: Deployment
- [ ] Compiles on Linux with sxc 0.9.0
- [ ] Runs on inf2.xlarge
- [ ] API tests pass against deployed instance
- [ ] Frontend at ciara.senuamedia.com works

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| API breaking changes | Medium | High | Careful testing, incremental migration |
| sxc compilation issues | High | High | Fix sxc issues first, report bugs |
| Performance regression | Low | Medium | Benchmark before/after |
| Missing dependencies | Medium | Medium | Document all dependencies |

---

## Timeline Estimate

| Phase | Scope |
|-------|-------|
| Phase 1 | Library consolidation, merge optimizations |
| Phase 2 | API compatibility updates |
| Phase 3 | New feature integration |
| Phase 4 | Test suite migration |
| Phase 5 | Build and deploy |

---

## Related Tasks

- **TASK-006**: Self-Learning Annealing (complete) - enables orchestrator improvements
- **TASK-005**: Dual Numbers (complete) - enables quality tracking
- **TASK-007**: Rebuild Training Pure Simplex - related training infrastructure

---

## Notes

Codex represents a real-world application of Simplex's AI-native capabilities. Converting it to 0.9.0 will:

1. **Validate 0.9.0**: Prove the release works for production applications
2. **Contribute optimizations**: The cache/batching improvements benefit all users
3. **Demonstrate features**: Show self-learning annealing in action
4. **Test toolchain**: Verify sxc compiles real applications on Linux

The goal is not just migration but improvement - Codex should be better after using 0.9.0 than before.
