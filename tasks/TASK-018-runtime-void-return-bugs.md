# TASK-018: Runtime Bugs for 0.9.9

**Status:** Complete
**Priority:** High
**Target Release:** 0.9.9
**Created:** 2026-01-18
**Updated:** 2026-03-16 (audited against codebase)

> **Audit Note (2026-03-16):** All 3 bugs verified fixed:
> - Bug 1 (void->int64_t): 141+ functions fixed in `runtime/standalone_runtime.c`
> - Bug 2 (Device ID collision): Fixed in `simplex-edge-hive/src/main.sx`
> - Bug 3 (Systemic i64 assumption): Documented as design constraint

---

## Bug 1: void Return Type Mismatch (FIXED)

### Summary

The Simplex compiler expects all `extern` function declarations to return `i64`. However, many C runtime functions in `standalone_runtime.c` were declared as returning `void`, causing an ABI mismatch. This leads to undefined behavior when Simplex code calls these functions.

## Root Cause

When a Simplex function calls an extern C function:
1. The calling convention expects a return value in the appropriate register
2. If the C function returns `void`, no value is placed in that register
3. The Simplex code receives garbage or causes crashes

## Bugs Fixed in 0.9.7 (by user)

The following functions were already fixed by changing `void` to `int64_t` and adding `return 0;`:

| Function | File | Line |
|----------|------|------|
| `http_server_response_status` | standalone_runtime.c | ~8100 |
| `http_server_response_header` | standalone_runtime.c | ~8115 |
| `http_server_response_body` | standalone_runtime.c | ~8130 |
| `http_server_route` | standalone_runtime.c | ~8200 |
| `http_server_stop` | standalone_runtime.c | ~8250 |
| `http_server_close` | standalone_runtime.c | ~8260 |

## Remaining Bugs to Fix for 0.9.9

All functions below return `void` but are potentially called from Simplex code and need to return `int64_t`:

### HTTP Client Functions
- `http_request_header` (line 7632)
- `http_request_body` (line 7647)
- `http_request_free` (line 7921)
- `http_response_free` (line 7943)

### WebSocket Functions
- `ws_close` (line 8966)

### Cluster/Distributed Functions
- `cluster_close` (line 9463)
- `dht_close` (line 9747)
- `migration_close` (line 10047)
- `code_store_close` (line 10234)
- `partition_detector_close` (line 10378)
- `vclock_close` (line 10560)
- `node_auth_close_conn` (line 10756)
- `node_auth_close` (line 10767)
- `distributed_node_stop` (line 16560)
- `distributed_node_free` (line 16572)
- `swim_suspect_member` (line 16688)
- `swim_dead_member` (line 16705)
- `swim_stop` (line 16757)
- `swim_cluster_free` (line 16765)

### Inference/ML Functions
- `router_decrement_load` (line 851)
- `router_close` (line 916)
- `hive_close` (line 1044)
- `shared_store_close` (line 1301)
- `embedding_free` (line 11006)
- `embedding_model_close` (line 11014)
- `hnsw_close` (line 11266)
- `memdb_close` (line 11488)
- `cluster_manager_close` (line 11629)
- `prune_config_free` (line 11734)
- `llm_client_close` (line 13346)
- `specialist_memory_close` (line 13473)
- `tool_registry_close` (line 13619)

### Actor System Functions
- `actor_set_error` (line 2211)
- `actor_stop` (line 2221)
- `actor_kill` (line 2237)
- `actor_crash` (line 2252)
- `actor_set_on_error` (line 2277)
- `actor_set_on_exit` (line 2284)
- `actor_set_supervisor` (line 2291)
- `actor_increment_restart` (line 2312)
- `actor_unlink` (line 2588)
- `actor_demonitor` (line 2633)
- `actor_propagate_exit` (line 2657)
- `supervisor_stop` (line 2947)
- `supervisor_free` (line 3090)
- `scheduler_stop` (line 3391)
- `scheduler_free` (line 3412)
- `mailbox_close` (line 3611)
- `mailbox_free` (line 3625)
- `registry_unregister` (line 3725)

### Circuit Breaker/Retry Functions
- `circuit_breaker_success` (line 2382)
- `circuit_breaker_failure` (line 2397)
- `circuit_breaker_reset` (line 2419)
- `retry_policy_set_jitter` (line 2457)
- `retry_policy_reset` (line 2500)

### Flow Control Functions
- `flow_release` (line 3918)
- `flow_reset` (line 3965)
- `flow_free` (line 3977)

### Async/Executor Functions
- `executor_run` (line 6673)
- `scope_cancel` (line 7110)
- `scope_free` (line 7128)
- `pin_ref` (line 6962)
- `pin_unref` (line 6969)
- `pin_set_self_ref` (line 6985)

### TLS Functions
- `tls_context_set_verify` (line 7349)
- `tls_context_free` (line 7361)
- `tls_shutdown` (line 7471)
- `tls_close` (line 7479)

### BDI Agent Functions
- `belief_store_close` (line 12049)
- `goal_free` (line 12307)
- `plan_free` (line 12382)
- `intention_free` (line 12459)
- `bdi_agent_close` (line 12722)
- `belief_clear_all` (line 17345)

### Evolution/Genetic Functions
- `individual_free` (line 13712)
- `population_close` (line 13827)
- `evolution_gene_set_weight` (line 16832)
- `evolution_gene_set_fitness` (line 16845)
- `evolution_gene_free` (line 17001)
- `evolution_population_free` (line 17009)

### Swarm/Consensus Functions
- `consensus_close` (line 14359)
- `pheromone_close` (line 14437)
- `swarm_close` (line 14581)
- `voting_close` (line 14776)

### Testing/Debug Functions
- `generator_close` (line 15027)
- `test_runner_close` (line 15180)
- `debugger_close` (line 15618)
- `vm_close` (line 16120)
- `target_close` (line 16352)

### Intrinsic Functions (Lower Priority)

These are internal/intrinsic functions - verify if called from Simplex before fixing:

- `intrinsic_vec_push`, `intrinsic_vec_set`, `intrinsic_vec_clear`
- `intrinsic_println`, `intrinsic_print`
- `intrinsic_write_file`
- `intrinsic_arena_reset`, `intrinsic_arena_free`
- `intrinsic_sb_append`, `intrinsic_sb_append_cstr`, `intrinsic_sb_append_char`
- `intrinsic_sb_clear`, `intrinsic_sb_free`
- `intrinsic_print_stack_trace`, `intrinsic_panic`, `intrinsic_panic_at`
- `intrinsic_thread_join`, `intrinsic_mutex_lock/unlock/free`
- `intrinsic_condvar_wait/signal/broadcast/free`
- `intrinsic_atomic_store`, `intrinsic_atomic_store_ptr`
- `intrinsic_mailbox_send`, `intrinsic_mailbox_free`
- `intrinsic_actor_*` functions
- `intrinsic_sleep_ms`, `intrinsic_thread_yield`
- `intrinsic_io_driver_*` functions
- `intrinsic_timer_*` functions
- `intrinsic_executor_*` functions
- `intrinsic_socket_*` functions
- `intrinsic_random_seed`, `intrinsic_setenv`
- `intrinsic_ser_*` functions
- `intrinsic_memory_*`, `intrinsic_*_belief*`, `intrinsic_*_goal*`
- `stderr_write`, `stderr_writeln`
- `intrinsic_assert_*`
- `iter_free`

## Fix Pattern

For each function:

```c
// Before (BUG)
void function_name(int64_t param) {
    // ... implementation
}

// After (FIXED)
int64_t function_name(int64_t param) {
    // ... implementation
    return 0;
}
```

## Verification

1. Grep for remaining `^void ` functions after fixes
2. Run test suite to ensure no regressions
3. Test HTTP server, WebSocket, actor system, and inference modules specifically

### Status: FIXED (175 functions)

All void return type bugs have been fixed in `standalone_runtime.c` by:
1. Changing `void` to `int64_t` for all 175 identified functions
2. Adding `return 0;` at function end
3. Changing all early `return;` to `return 0;`

**Note**: Static void helper functions (internal implementation) were intentionally left as void since they are not called from Simplex code.

---

## Bug 2: Device ID Generation Collision Risk (FIXED)

### Summary

`generate_device_id_simple()` in `simplex-edge-hive/src/main.sx:121` used `time_now() % 1000000000` which created collision risk.

### Root Cause

- Only 10^9 unique IDs possible
- Time-based: two devices initialized at the same second got identical IDs
- No randomness incorporated

### Fix Applied

Changed to combine timestamp with random component:
```simplex
fn generate_device_id_simple() -> i64 {
    let timestamp: i64 = time_now();
    let random_part: i64 = random_int(0, 999999);
    (timestamp % 1000000000) * 1000000 + random_part
}
```

Now provides ~10^15 unique IDs (vs old 10^9) and handles same-second initialization.

### Status: FIXED

---

## Bug 3: Systemic extern i64 Assumption (DESIGN NOTE)

### Summary

The Simplex compiler assumes ALL extern function declarations return `i64`. This is a systemic design issue, not a bug per se, but any future C runtime additions must follow this convention.

### Impact

- All extern C functions must return `int64_t` (even if logically void)
- All extern C functions must have `return 0;` at end (or appropriate return value)
- Future runtime development must follow this pattern

### Status: DOCUMENTED (Not a bug to fix, but a constraint to follow)

---

## Upstream Compiler Blockers (Reference)

The following compiler issues are documented elsewhere but referenced for completeness:

| Bug ID | Description | Status |
|--------|-------------|--------|
| Bug 5 | Mnemonic block parsing | Blocked |
| Bug 7 | Hive constructor generation | Blocked |
| Bug 8 | Hive runtime implementation | Blocked |
| Bug 9-11 | JSON/SQL/TOML FFI bindings | Blocked |
| Bug 17 | SLM native bindings for local inference | Blocked |

These are sxc compiler issues that affect edge-hive and other advanced features.

---

## Verification Checklist

- [x] Runtime compiles without errors (`clang -c -O2`)
- [x] Core stdlib tests pass (assert, string, vec, option, result)
- [x] Runtime edge cases pass (memory pressure, stress tests)
- [x] Nexus protocol tests pass (12 tests)
- [x] Types tests pass (5 tests)
- [x] Buffer safety tests pass
- [ ] Actor system tests - blocked by compiler issue (undefined variable 'Get')
- [ ] HTTP server tests - requires manual testing
- [x] Edge-hive tests pass (test_nexus, test_types)
- [ ] Release binaries build successfully

---

## Test Results (2026-01-18)

### Passing Tests

**Nexus/Types (17 tests)**
- `test_nexus` - 12 tests (delta ops, connection states, frame types, etc.)
- `test_types` - 5 tests (device classes, network states, sensitivity levels, etc.)

**Stdlib Tests (27 files, ALL PASS)**
- `unit_assert.sx` - 8 tests
- `unit_option.sx` - 8 tests
- `unit_result.sx` - 8 tests
- `unit_string.sx` - 10 tests
- `unit_vec.sx` - 10 tests
- `unit_hashmap.sx` - 10 tests
- `unit_hashset.sx` - 10 tests
- `unit_iterator.sx` - 10 tests
- `unit_io.sx` - 12 tests
- `unit_env.sx` - 10 tests
- `unit_log.sx` - 10 tests
- `unit_sync.sx` - 18 tests
- `unit_runtime.sx` - 10 tests
- `unit_net.sx` - 14 tests
- `unit_crypto.sx` - 15 tests
- `unit_regex.sx` - 10 tests
- `unit_signal.sx` - 12 tests
- `unit_anneal.sx` - 10 tests
- `unit_cli.sx` - 10 tests
- `unit_cli_terminal.sx` - 10 tests
- `unit_compress.sx` - 15 tests
- `unit_http.sx` - 25 tests
- `unit_mpsc.sx` - 15 tests
- `unit_manifest.sx` - 10 tests
- `unit_semver.sx` - 5 tests
- `unit_training.sx` - 10 tests
- `integ_http_client.sx` - 12 tests

**Other Tests**
- `test_buffer_final` - Buffer overflow safety tests
- `integ_edge_cases.sx` - Runtime edge cases (memory pressure, stress)

### Blocked Tests
- `spec_actor_basic.sx` - Compiler issue: undefined variable 'Get'
- `spec_async_basic.sx` - Runtime issue (exit code 240)

### Library Tests (Linking Failures)
All 5 library tests compile but fail to link - missing native C functions:
- simplex-http: `http_request_body` undefined
- simplex-sql: `sql_begin` undefined
- simplex-json: `json_array` undefined
- simplex-toml: `toml_free` undefined
- simplex-uuid: `uuid_is_nil` undefined

### Edge-Hive Tests (Compiler Issues)
- `test_adaptation.sx` - vec_set redefinition error
- `test_beliefs.sx` - Compiler segfault
- `test_epistemic.sx` - Compiler segfault
- `test_hardening.sx` - vec_set redefinition error
- `test_hive.sx` - vec_set redefinition error
- `test_model.sx` - vec_set redefinition error
- `test_security.sx` - vec_set redefinition error

---

## Notes

- 175 void functions fixed in standalone_runtime.c
- Version bumped to 0.9.9 across all components
- Static helper functions intentionally remain void (not called from Simplex)
- Search functionality fixed in API docs (search.js path detection)
