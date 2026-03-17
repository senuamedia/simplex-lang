# Simplex v0.9.9 Release Notes

**Release Date:** 2026-01-18
**Codename:** Runtime Stability

---

## Overview

Simplex v0.9.9 is a **critical runtime bug fix release** addressing 175+ ABI-level function signature issues in the C runtime, along with device ID collision prevention and API documentation improvements.

---

## Critical Bug Fixes

### Bug 1: void Return Type ABI Mismatch (175 functions)

The Simplex compiler expects all `extern` function declarations to return `i64`. However, many C runtime functions were declared as returning `void`, causing an ABI mismatch that led to undefined behavior.

**Root Cause:**
- Calling convention expects return value in register
- void functions don't place values in return register
- Simplex code receives garbage or crashes

**Fix:** Changed 175 functions from `void` to `int64_t` with `return 0;`

| Category | Functions Fixed |
|----------|-----------------|
| HTTP Client | 4 (http_request_header, http_request_body, http_request_free, http_response_free) |
| WebSocket | 1 (ws_close) |
| Cluster/Distributed | 15 (cluster_close, dht_close, migration_close, swim_*, distributed_node_*) |
| Inference/ML | 14 (router_*, hive_close, embedding_*, llm_client_close, etc.) |
| Actor System | 19 (actor_*, supervisor_*, scheduler_*, mailbox_*, registry_*) |
| Circuit Breaker | 5 (circuit_breaker_*, retry_policy_*) |
| Flow Control | 3 (flow_release, flow_reset, flow_free) |
| Async/Executor | 6 (executor_run, scope_*, pin_*) |
| TLS | 4 (tls_context_*, tls_shutdown, tls_close) |
| BDI Agent | 6 (belief_store_close, goal_free, plan_free, etc.) |
| Evolution/Genetic | 6 (individual_free, population_close, evolution_gene_*) |
| Swarm/Consensus | 4 (consensus_close, pheromone_close, swarm_close, voting_close) |
| Testing/Debug | 5 (generator_close, test_runner_close, debugger_close, etc.) |
| Intrinsic Functions | 80+ (intrinsic_vec_*, intrinsic_println, intrinsic_actor_*, etc.) |

### Bug 2: Device ID Generation Collision Risk

`generate_device_id_simple()` used `time_now() % 1000000000` which created collision risk.

**Root Cause:**
- Only 10^9 unique IDs possible
- Devices initialized at same second got identical IDs
- No randomness incorporated

**Fix:** Combined timestamp with random component:
```simplex
fn generate_device_id_simple() -> i64 {
    let timestamp: i64 = time_now();
    let random_part: i64 = random_int(0, 999999);
    (timestamp % 1000000000) * 1000000 + random_part
}
```

Now provides ~10^15 unique IDs (vs old 10^9).

### Bug 3: Systemic extern i64 Assumption (Design Note)

**Documented Constraint:** All extern C functions must:
- Return `int64_t` (even if logically void)
- Have `return 0;` at function end
- Future runtime development must follow this pattern

---

## API Documentation Improvements

### Search Functionality Fixed

The documentation search tool wasn't working on nested pages due to incorrect path detection.

**Fix:** Updated `search.js` with proper `getBasePath()` function:
```javascript
function getBasePath() {
    const match = window.location.pathname.match(/^(.*\/api\/)/);
    return match ? match[1] : './';
}
```

### JSON-LD Schema Added

Module documentation now includes JSON-LD structured data for AI/LLM consumption and improved SEO.

---

## Test Results

### Passing Tests
| Test | Status |
|------|--------|
| test_nexus (12 tests) | PASS |
| test_types (5 tests) | PASS |
| test_buffer_final | PASS |
| unit_assert.sx | PASS |
| unit_string.sx | PASS |
| unit_vec.sx | PASS |
| unit_option.sx | PASS |
| unit_result.sx | PASS |
| integ_edge_cases.sx | PASS |

### Blocked Tests (Upstream Compiler Issues)
| Test | Issue |
|------|-------|
| spec_actor_basic.sx | Compiler: undefined variable 'Get' |
| spec_async_basic.sx | Runtime: exit code 240 |

---

## Tool Updates

All tools updated to version 0.9.9:

| Tool | Version |
|------|---------|
| **sxc** | 0.9.9 |
| **sxpm** | 0.9.9 |
| **cursus** | 0.9.9 |
| **sxdoc** | 0.9.9 |
| **sxlsp** | 0.9.9 |

---

## Files Modified

| File | Changes |
|------|---------|
| `runtime/standalone_runtime.c` | 175 void→int64_t fixes |
| `simplex-edge-hive/src/main.sx` | Device ID collision fix |
| `simplex-docs/api/assets/search.js` | Path detection fix |
| `simplex-core/src/version.sx` | Version 0.9.9 |
| `sxc` | Version 0.9.9 |
| `tasks/TASK-018-runtime-void-return-bugs.md` | Bug documentation |

---

## Upstream Compiler Blockers (Reference)

These compiler issues are documented for future releases:

| Bug ID | Description | Status |
|--------|-------------|--------|
| Bug 5 | Mnemonic block parsing | Blocked |
| Bug 7 | Hive constructor generation | Blocked |
| Bug 8 | Hive runtime implementation | Blocked |
| Bug 9-11 | JSON/SQL/TOML FFI bindings | Blocked |
| Bug 17 | SLM native bindings | Blocked |

---

## Compatibility

| Component | Minimum Version | Maximum Version |
|-----------|-----------------|-----------------|
| LLVM | 14.0.0 | - |
| Previous Simplex | 0.8.0 | 0.10.0 |

---

## What's Next (TASK-019)

### v0.10.0: Compiler Bug Fixes

The following compiler-level issues are documented in TASK-019 for the 0.10.0 release:

**Critical - Compiler Crashes:**
- Segfault on `test_beliefs.sx` and `test_epistemic.sx`
- Parser/codegen null pointer issues in belief/epistemic modules

**Critical - LLVM IR Generation:**
- `vec_set` function redefinition (5 edge-hive tests affected)
- Undefined variable 'Get' in actor message patterns
- Async state machine exit code 240

**High - Native Library Linking:**
- Missing `declare` statements for extern functions
- Affects: simplex-http, simplex-sql, simplex-json, simplex-toml, simplex-uuid

**Note:** These are bugs in the Simplex compiler source code (`compiler/bootstrap/*.sx`), NOT the C runtime. The 0.9.9 runtime fixes are complete and verified working.

### v0.11.0: Nexus Protocol & GPU
- Full Nexus Protocol implementation
- GPU acceleration backend

### v1.0.0: Production Release
- All compiler bugs resolved
- Full test suite passing
- Production-ready stability

---

## Credits

Bug fixes by Rod Higgins ([@senuamedia](https://github.com/senuamedia)).
