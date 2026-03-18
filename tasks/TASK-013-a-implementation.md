# TASK-013-A: Implementation of Belief-Gated Receive with Derivative Patterns

**Status**: ~95% Implemented (HiveBeliefManager integration remaining)
**Priority**: Critical (Foundational)
**Created**: 2026-01-15
**Updated**: 2026-03-16 (Phase 7 TASK-022 updates)

> **Audit Note (2026-03-16):**
> - Lexer tokens (At, KwConfidence, KwDerivative): COMPLETE — `compiler/bootstrap/lexer.sx:100-103`
> - AST tags: 5 of 6 (PAT_BELIEF_GUARD not found) — `lib/ast_defs.sx:84-89`
> - Parser (parse_belief_guard and related): COMPLETE — `compiler/bootstrap/parser.sx:3752-3938`
> - Belief.confidence as dual: COMPLETE — `simplex-learning/src/distributed/beliefs.sx:28`
> - Codegen: 5 of 5 types COMPLETE (AND/OR were already implemented with short-circuit) — `codegen.sx:5295-5556`
> - Codegen declarations: COMPLETE — belief_register/update/guard + suspend/wake functions — `codegen.sx:9149-9170`
> - WAKE mechanism: C runtime COMPLETE (`runtime/standalone_runtime.c:18201+`),
>   Simplex-level declarations wired in both stage0.py and codegen.sx
> - Tests: 3 files — `test_belief_runtime.sx`, `tests/ai/belief_guards/unit_belief_guard_syntax.sx`,
>   `tests/ai/belief_guards/unit_belief_derivative.sx`
**Target**: Complete implementation of formal model primitives
**Depends On**: TASK-013 (Formal Model)

---

## Overview

This task implements the belief-gated receive with derivative patterns required for the formal model in TASK-013. The implementation enables the core semantic primitive that cannot be encoded in traditional actor models.

**Target Syntax:**
```simplex
receive {
    Message(data) @ confidence(obstacle) < 0.5 => cautious_response(),
    Message(data) @ confidence(obstacle).derivative < -0.1 => emergency_brake(),
    Message(data) => normal_response(),
}
```

---

## Existing Infrastructure (Leverage, Don't Recreate)

### Already Implemented

| Component | File | Notes |
|-----------|------|-------|
| **Dual Numbers** | `simplex-std/src/dual.sx` | Complete autodiff with all operations |
| **Belief System** | `simplex-learning/src/distributed/beliefs.sx` | Conflict resolution, hive management |
| **Actor Parser** | `compiler/bootstrap/parser.sx:2750` | `parse_actor()` function |
| **Actor Keywords** | `compiler/bootstrap/lexer.sx:81-88` | `KwActor`, `KwReceive`, `KwSpecialist`, etc. |
| **AST Definitions** | `simplex-core/src/ast_defs.sx` | `TYPE_DUAL`, `TAG_ACTOR`, etc. |
| **Stage0 Bootstrap** | `compiler/bootstrap/stage0.py` | Python bootstrap compiler |

### Critical Change: Belief.confidence Must Be Dual

The existing `Belief` struct uses `f64` for confidence:
```simplex
// CURRENT (simplex-learning/src/distributed/beliefs.sx:17-18)
pub struct Belief {
    pub confidence: f64,  // ← Must change to dual
    ...
}
```

This must become:
```simplex
pub struct Belief {
    pub confidence: dual,  // ← Automatic derivative tracking
    ...
}
```

This single change enables:
- Derivatives propagate through all belief operations automatically
- `confidence(x).derivative` becomes a simple field access
- Chain rule applies when beliefs are combined

---

## Implementation Plan

### Phase 1: Lexer Extensions

#### File: `compiler/bootstrap/lexer.sx`

**Add to TokenKind enum (after line 94):**
```simplex
    // Belief guard tokens
    At,              // @
    KwConfidence,    // confidence
    KwDerivative,    // derivative (also accessible via .derivative)
```

**Add to check_keyword function (after line 302):**
```simplex
    // Belief guard keywords
    if string_eq(text, "confidence") { return TokenKind::KwConfidence; }
    if string_eq(text, "derivative") { return TokenKind::KwDerivative; }
```

**Add to lexer_next_token (single character tokens, around line 320):**
```simplex
    // @ symbol for belief guards (ASCII 64)
    if c == 64 { return token_new_loc(TokenKind::At, string_from("@"), start, start_line, start_col); }
```

#### File: `compiler/bootstrap/stage0.py`

**Add to TokenKind class (around line 115):**
```python
    # Belief guard tokens
    AT = 'AT'
    KW_CONFIDENCE = 'KW_CONFIDENCE'
    KW_DERIVATIVE = 'KW_DERIVATIVE'
```

**Add to KEYWORDS dict (around line 165):**
```python
    'confidence': TokenKind.KW_CONFIDENCE,
    'derivative': TokenKind.KW_DERIVATIVE,
```

**Add to single character handling in tokenize():**
```python
    elif char == '@':
        return Token(TokenKind.AT, '@', self.line, self.col)
```

---

### Phase 2: AST Node Definitions

#### File: `simplex-core/src/ast_defs.sx`

**Add new expression tags (after EXPR_INDEX on line 77):**
```simplex
pub fn EXPR_CONFIDENCE() -> i64 { 30 }    // confidence(belief_id)
pub fn EXPR_DERIVATIVE() -> i64 { 31 }    // expr.derivative
pub fn EXPR_BELIEF_GUARD() -> i64 { 32 }  // belief_expr cmp value
pub fn EXPR_BELIEF_AND() -> i64 { 33 }    // guard && guard
pub fn EXPR_BELIEF_OR() -> i64 { 34 }     // guard || guard
```

**Add receive guard pattern tag (after PAT_GUARD on line 158):**
```simplex
pub fn PAT_BELIEF_GUARD() -> i64 { 10 }   // pattern @ belief_guard
```

---

### Phase 3: Parser Extensions

#### File: `compiler/bootstrap/parser.sx`

**Modify parse_actor function (line 2797) to handle belief guards:**

Replace the receive handler parsing section:
```simplex
        // Check for receive handler
        if parser_check(parser, TokenKind::KwReceive) {
            parser_advance(parser);
            let msg_tok: i64 = parser_expect(parser, TokenKind::Ident);
            let msg_name: i64 = token_text(msg_tok);
            let params: i64 = vec_new();
            if parser_check(parser, TokenKind::LParen) {
                params = parse_params(parser);
            }
            let ret_type: i64 = string_from("void");
            if parser_check(parser, TokenKind::Arrow) {
                parser_advance(parser);
                ret_type = parse_type(parser);
            }

            // NEW: Parse optional belief guard
            let belief_guard: i64 = 0;
            if parser_check(parser, TokenKind::At) {
                parser_advance(parser);  // consume '@'
                belief_guard = parse_belief_guard(parser);
            }

            let body: i64 = parse_block(parser);
            let handler: i64 = malloc(40);  // Extended to hold guard
            store_ptr(handler, 0, msg_name);
            store_ptr(handler, 1, params);
            store_ptr(handler, 2, ret_type);
            store_ptr(handler, 3, body);
            store_ptr(handler, 4, belief_guard);  // NEW: belief guard (0 if none)
            vec_push(handlers, handler);
        }
```

**Add new parsing functions for belief guards:**
```simplex
// Parse belief guard: confidence(id) < 0.5 && confidence(id).derivative < -0.1
fn parse_belief_guard(parser: i64) -> i64 {
    return parse_belief_or(parser);
}

fn parse_belief_or(parser: i64) -> i64 {
    let left: i64 = parse_belief_and(parser);

    while parser_check(parser, TokenKind::PipePipe) {
        parser_advance(parser);
        let right: i64 = parse_belief_and(parser);
        let node: i64 = malloc(24);
        store_i64(node, 0, EXPR_BELIEF_OR());
        store_ptr(node, 1, left);
        store_ptr(node, 2, right);
        left = node;
    }

    return left;
}

fn parse_belief_and(parser: i64) -> i64 {
    let left: i64 = parse_belief_comparison(parser);

    while parser_check(parser, TokenKind::AmpAmp) {
        parser_advance(parser);
        let right: i64 = parse_belief_comparison(parser);
        let node: i64 = malloc(24);
        store_i64(node, 0, EXPR_BELIEF_AND());
        store_ptr(node, 1, left);
        store_ptr(node, 2, right);
        left = node;
    }

    return left;
}

fn parse_belief_comparison(parser: i64) -> i64 {
    let left: i64 = parse_belief_primary(parser);

    // Check for comparison operator
    if parser_check(parser, TokenKind::Lt) ||
       parser_check(parser, TokenKind::Le) ||
       parser_check(parser, TokenKind::Gt) ||
       parser_check(parser, TokenKind::Ge) ||
       parser_check(parser, TokenKind::EqEq) ||
       parser_check(parser, TokenKind::Ne) {
        let op_tok: i64 = parser_advance(parser);
        let op: i64 = token_kind(op_tok);
        let right: i64 = parse_expr(parser);

        let node: i64 = malloc(32);
        store_i64(node, 0, EXPR_BELIEF_GUARD());
        store_ptr(node, 1, left);
        store_i64(node, 2, op);
        store_ptr(node, 3, right);
        return node;
    }

    return left;
}

fn parse_belief_primary(parser: i64) -> i64 {
    // confidence(belief_id)
    if parser_check(parser, TokenKind::KwConfidence) {
        parser_advance(parser);
        parser_expect(parser, TokenKind::LParen);
        let belief_id: i64 = parse_expr(parser);
        parser_expect(parser, TokenKind::RParen);

        let node: i64 = malloc(16);
        store_i64(node, 0, EXPR_CONFIDENCE());
        store_ptr(node, 1, belief_id);

        // Check for .derivative
        if parser_check(parser, TokenKind::Dot) {
            parser_advance(parser);
            if parser_check(parser, TokenKind::KwDerivative) {
                parser_advance(parser);
                let der_node: i64 = malloc(16);
                store_i64(der_node, 0, EXPR_DERIVATIVE());
                store_ptr(der_node, 1, node);
                return der_node;
            } else {
                parser_add_error(parser, string_from("Expected 'derivative' after '.'"));
                return node;
            }
        }

        return node;
    }

    // Parenthesized expression
    if parser_check(parser, TokenKind::LParen) {
        parser_advance(parser);
        let expr: i64 = parse_belief_guard(parser);
        parser_expect(parser, TokenKind::RParen);
        return expr;
    }

    parser_add_error(parser, string_from("Expected belief guard expression"));
    return 0;
}
```

#### File: `compiler/bootstrap/stage0.py`

**Add equivalent parsing in Python bootstrap (in Parser class, around line 1090):**
```python
    def parse_belief_guard(self):
        """Parse belief guard: confidence(id) < 0.5 && ..."""
        return self.parse_belief_or()

    def parse_belief_or(self):
        left = self.parse_belief_and()
        while self.check(TokenKind.PIPE_PIPE):
            self.advance()
            right = self.parse_belief_and()
            left = {'tag': 'BELIEF_OR', 'left': left, 'right': right}
        return left

    def parse_belief_and(self):
        left = self.parse_belief_comparison()
        while self.check(TokenKind.AMP_AMP):
            self.advance()
            right = self.parse_belief_comparison()
            left = {'tag': 'BELIEF_AND', 'left': left, 'right': right}
        return left

    def parse_belief_comparison(self):
        left = self.parse_belief_primary()
        if self.check_any([TokenKind.LT, TokenKind.LE, TokenKind.GT,
                          TokenKind.GE, TokenKind.EQ_EQ, TokenKind.NE]):
            op = self.advance()
            right = self.parse_expr()
            return {'tag': 'BELIEF_GUARD', 'left': left, 'op': op.kind, 'right': right}
        return left

    def parse_belief_primary(self):
        if self.check(TokenKind.KW_CONFIDENCE):
            self.advance()
            self.expect(TokenKind.LPAREN)
            belief_id = self.parse_expr()
            self.expect(TokenKind.RPAREN)

            node = {'tag': 'CONFIDENCE', 'belief_id': belief_id}

            if self.check(TokenKind.DOT):
                self.advance()
                if self.check(TokenKind.KW_DERIVATIVE):
                    self.advance()
                    return {'tag': 'DERIVATIVE', 'base': node}
                else:
                    self.error("Expected 'derivative' after '.'")

            return node

        if self.check(TokenKind.LPAREN):
            self.advance()
            expr = self.parse_belief_guard()
            self.expect(TokenKind.RPAREN)
            return expr

        self.error("Expected belief guard expression")
```

**Modify parse_actor_def to handle @ guard (around line 1090):**
```python
            elif self.check(TokenKind.KW_RECEIVE):
                self.advance()
                msg_name = self.expect(TokenKind.IDENT).text
                params = []
                if self.check(TokenKind.LPAREN):
                    params = self.parse_params()

                ret_type = 'void'
                if self.check(TokenKind.ARROW):
                    self.advance()
                    ret_type = self.parse_type()

                # NEW: Parse optional belief guard
                belief_guard = None
                if self.check(TokenKind.AT):
                    self.advance()
                    belief_guard = self.parse_belief_guard()

                body = self.parse_block()
                handlers.append({
                    'msg_name': msg_name,
                    'params': params,
                    'ret_type': ret_type,
                    'body': body,
                    'belief_guard': belief_guard  # NEW
                })
```

---

### Phase 4: Update Belief System to Use Dual Numbers

#### File: `simplex-learning/src/distributed/beliefs.sx`

**Change Belief struct to use dual numbers:**
```simplex
use simplex_std::dual::dual;

/// A belief held by a specialist - now with automatic derivative tracking
#[derive(Clone)]
pub struct Belief {
    /// Belief identifier (e.g., "obstacle_detected")
    pub id: String,

    /// Confidence as a dual number (value + derivative)
    /// The derivative tracks rate of change automatically
    pub confidence: dual,

    /// Evidence count supporting this belief
    pub evidence_count: u64,

    /// Last update timestamp (epoch ms)
    pub last_updated: u64,

    /// Source specialist
    pub source: String,

    /// Associated embedding (for semantic similarity)
    pub embedding: Option<Tensor>,
}

impl Belief {
    /// Create a new belief with initial confidence
    pub fn new(id: &str, confidence: f64, source: &str) -> Self {
        Belief {
            id: id.to_string(),
            confidence: dual::constant(confidence.max(0.0).min(1.0)),
            evidence_count: 1,
            last_updated: current_timestamp(),
            source: source.to_string(),
            embedding: None,
        }
    }

    /// Update confidence with new evidence - derivative computed automatically
    pub fn update(&mut self, new_evidence_confidence: f64, learning_rate: f64) {
        let old_val = self.confidence.val;
        let new_val = old_val * (1.0 - learning_rate) + new_evidence_confidence * learning_rate;
        let new_val_clamped = new_val.max(0.0).min(1.0);

        // Compute derivative as rate of change
        let dt = (current_timestamp() - self.last_updated) as f64 / 1000.0;  // seconds
        let derivative = if dt > 0.0 { (new_val_clamped - old_val) / dt } else { 0.0 };

        self.confidence = dual::new(new_val_clamped, derivative);
        self.evidence_count += 1;
        self.last_updated = current_timestamp();
    }

    /// Get confidence value
    pub fn value(&self) -> f64 {
        self.confidence.val
    }

    /// Get confidence derivative (rate of change)
    pub fn derivative(&self) -> f64 {
        self.confidence.der
    }

    /// Compute belief strength (confidence * evidence weight)
    pub fn strength(&self) -> dual {
        let evidence_weight = dual::constant(1.0 - 1.0 / (1.0 + self.evidence_count as f64));
        self.confidence * evidence_weight
    }
}
```

---

### Phase 5: Code Generation for Belief Guards

#### File: `compiler/bootstrap/codegen.sx`

**Add codegen cases for belief guard expressions:**
```simplex
fn codegen_expr(node: i64, ctx: i64) -> i64 {
    let tag: i64 = load_i64(node, 0);

    match tag {
        // ... existing cases ...

        EXPR_CONFIDENCE() => {
            return codegen_confidence_access(node, ctx);
        }

        EXPR_DERIVATIVE() => {
            return codegen_derivative_access(node, ctx);
        }

        EXPR_BELIEF_GUARD() => {
            return codegen_belief_comparison(node, ctx);
        }

        EXPR_BELIEF_AND() => {
            return codegen_belief_and(node, ctx);
        }

        EXPR_BELIEF_OR() => {
            return codegen_belief_or(node, ctx);
        }

        // ... existing cases ...
    }
}

// Generate code for confidence(belief_id)
// Returns: dual value from belief store
fn codegen_confidence_access(node: i64, ctx: i64) -> i64 {
    let belief_id: i64 = load_ptr(node, 1);
    let belief_id_code: i64 = codegen_expr(belief_id, ctx);

    // Generate call to runtime: belief_get_confidence(belief_id) -> dual
    return emit_call(ctx, "belief_get_confidence", vec![belief_id_code]);
}

// Generate code for expr.derivative
// Returns: f64 (the derivative field of the dual)
fn codegen_derivative_access(node: i64, ctx: i64) -> i64 {
    let base: i64 = load_ptr(node, 1);
    let base_code: i64 = codegen_expr(base, ctx);

    // Access .der field of dual number
    // In LLVM IR: extractvalue %dual %base, 1
    return emit_extract_field(ctx, base_code, 1);  // der is field 1
}

// Generate code for belief_expr cmp value
fn codegen_belief_comparison(node: i64, ctx: i64) -> i64 {
    let left: i64 = load_ptr(node, 1);
    let op: i64 = load_i64(node, 2);
    let right: i64 = load_ptr(node, 3);

    let left_code: i64 = codegen_expr(left, ctx);
    let right_code: i64 = codegen_expr(right, ctx);

    // If left is a dual, extract .val for comparison
    // Compare: left.val <op> right
    let left_val: i64 = emit_extract_field(ctx, left_code, 0);  // val is field 0

    return emit_comparison(ctx, left_val, op, right_code);
}

// Generate code for guard && guard
fn codegen_belief_and(node: i64, ctx: i64) -> i64 {
    let left: i64 = load_ptr(node, 1);
    let right: i64 = load_ptr(node, 2);

    let left_code: i64 = codegen_expr(left, ctx);
    let right_code: i64 = codegen_expr(right, ctx);

    return emit_and(ctx, left_code, right_code);
}

// Generate code for guard || guard
fn codegen_belief_or(node: i64, ctx: i64) -> i64 {
    let left: i64 = load_ptr(node, 1);
    let right: i64 = load_ptr(node, 2);

    let left_code: i64 = codegen_expr(left, ctx);
    let right_code: i64 = codegen_expr(right, ctx);

    return emit_or(ctx, left_code, right_code);
}
```

**Add codegen for receive handlers with belief guards:**
```simplex
// Generate actor receive handler with optional belief guard
fn codegen_receive_handler(handler: i64, actor_ctx: i64) -> i64 {
    let msg_name: i64 = load_ptr(handler, 0);
    let params: i64 = load_ptr(handler, 1);
    let ret_type: i64 = load_ptr(handler, 2);
    let body: i64 = load_ptr(handler, 3);
    let belief_guard: i64 = load_ptr(handler, 4);

    // Generate message pattern matching
    let pattern_match: i64 = codegen_message_pattern(msg_name, params, actor_ctx);

    if belief_guard != 0 {
        // Generate belief guard evaluation
        let guard_code: i64 = codegen_expr(belief_guard, actor_ctx);

        // Generate conditional: if pattern_matches && guard_satisfied
        let condition: i64 = emit_and(actor_ctx, pattern_match, guard_code);

        // Generate body
        let body_code: i64 = codegen_block(body, actor_ctx);

        // Generate: if (condition) { body } else { suspend_receive(guard) }
        let suspend_code: i64 = codegen_suspend_receive(belief_guard, actor_ctx);

        return emit_if_else(actor_ctx, condition, body_code, suspend_code);
    } else {
        // No guard - simple pattern match
        let body_code: i64 = codegen_block(body, actor_ctx);
        return emit_if(actor_ctx, pattern_match, body_code);
    }
}

// Generate code to suspend receive and register for WAKE
fn codegen_suspend_receive(belief_guard: i64, ctx: i64) -> i64 {
    // Store current actor, message, and guard for later WAKE evaluation
    let actor_id: i64 = emit_get_current_actor(ctx);
    let msg_id: i64 = emit_get_current_message(ctx);

    // Register suspended receive with belief system
    return emit_call(ctx, "suspend_receive_on_belief", vec![actor_id, msg_id, belief_guard]);
}
```

---

### Phase 6: Runtime Support for WAKE Transitions

#### File: `simplex-learning/src/distributed/beliefs.sx`

**Add suspended receive tracking to HiveBeliefManager:**
```simplex
/// Suspended receive waiting for belief change
pub struct SuspendedReceive {
    /// Actor waiting
    pub actor_id: u64,

    /// Message ID in mailbox
    pub message_id: u64,

    /// Belief IDs this guard depends on
    pub watched_beliefs: Vec<String>,

    /// Guard evaluation function (compiled)
    pub guard_fn: fn(&HiveBeliefManager) -> bool,
}

impl HiveBeliefManager {
    // ... existing code ...

    /// Suspended receives waiting for WAKE
    suspended_receives: Vec<SuspendedReceive>,

    /// Register a suspended receive
    pub fn suspend_receive(&mut self, recv: SuspendedReceive) {
        self.suspended_receives.push(recv);
    }

    /// Check and wake suspended receives after belief update
    fn check_wake_transitions(&mut self) {
        let mut woken: Vec<usize> = Vec::new();

        for (i, recv) in self.suspended_receives.iter().enumerate() {
            // Re-evaluate guard with current beliefs
            if (recv.guard_fn)(self) {
                // WAKE: Resume actor with message
                wake_actor(recv.actor_id, recv.message_id);
                woken.push(i);
            }
        }

        // Remove woken receives (in reverse order to preserve indices)
        for i in woken.into_iter().rev() {
            self.suspended_receives.remove(i);
        }
    }

    /// Submit a belief - now triggers WAKE check
    pub fn submit_belief(&mut self, belief: Belief) {
        // ... existing belief submission code ...

        // After updating consensus, check for WAKE transitions
        self.check_wake_transitions();
    }
}

// External function to wake an actor
extern fn wake_actor(actor_id: u64, message_id: u64);
```

---

### Phase 7: Tests

#### File: `tests/ai/belief_guards/unit_belief_guard_syntax.sx`

```simplex
// Test: Basic belief-gated receive parsing and execution
// Verifies the @ syntax parses correctly and guards evaluate properly

use simplex_std::dual::dual;

actor TestActor {
    var result: i64 = 0;

    receive Process(data: i64) @ confidence("sensor") < 0.5 => {
        // Low confidence path
        result = 1;
    }

    receive Process(data: i64) @ confidence("sensor").derivative < -0.1 => {
        // Rapid decline path
        result = 2;
    }

    receive Process(data: i64) => {
        // Default path
        result = 3;
    }

    receive GetResult() -> i64 => {
        return result;
    }
}

fn main() -> i64 {
    // Initialize belief system
    let beliefs = HiveBeliefManager::new(ConflictResolution::HighestConfidence);

    // Test 1: High confidence, stable - should trigger default
    beliefs.submit_belief(Belief::new("sensor", 0.8, "test"));

    let actor = spawn TestActor;
    send(actor, Process(42));
    let r1 = ask(actor, GetResult());

    if r1 != 3 {
        println("FAIL: Expected 3 (default), got {r1}");
        return 1;
    }

    // Test 2: Low confidence - should trigger low confidence path
    beliefs.submit_belief(Belief::new("sensor", 0.3, "test"));

    let actor2 = spawn TestActor;
    send(actor2, Process(42));
    let r2 = ask(actor2, GetResult());

    if r2 != 1 {
        println("FAIL: Expected 1 (low confidence), got {r2}");
        return 1;
    }

    println("PASS: Belief guard syntax tests");
    return 0;
}
```

#### File: `tests/ai/belief_guards/unit_belief_derivative.sx`

```simplex
// Test: Derivative pattern in belief guards
// Verifies that .derivative accesses the dual number derivative field

use simplex_std::dual::dual;

fn main() -> i64 {
    // Create belief with known derivative
    var belief = Belief::new("obstacle", 0.7, "sensor");

    // Update rapidly to create derivative
    // Simulating: was 0.7, now 0.3, over 1 second = -0.4/s derivative
    belief.confidence = dual::new(0.3, -0.4);

    // Check derivative access
    let der = belief.derivative();

    if der > -0.39 || der < -0.41 {
        println("FAIL: Expected derivative ~-0.4, got {der}");
        return 1;
    }

    // Check that confidence.val is correct
    let val = belief.value();
    if val < 0.29 || val > 0.31 {
        println("FAIL: Expected value ~0.3, got {val}");
        return 1;
    }

    println("PASS: Derivative access tests");
    return 0;
}
```

#### File: `tests/ai/belief_guards/unit_dual_propagation.sx`

```simplex
// Test: Dual number propagation through belief operations
// Verifies that derivatives flow correctly through computations

use simplex_std::dual::dual;

fn main() -> i64 {
    // Create two beliefs as dual numbers
    let a = dual::new(0.6, 0.1);   // confidence 0.6, rising at 0.1/s
    let b = dual::new(0.4, -0.2);  // confidence 0.4, falling at 0.2/s

    // Test multiplication: (a * b)' = a' * b + a * b'
    let product = a * b;
    // Expected: val = 0.6 * 0.4 = 0.24
    // Expected: der = 0.1 * 0.4 + 0.6 * (-0.2) = 0.04 - 0.12 = -0.08

    if product.val < 0.239 || product.val > 0.241 {
        println("FAIL: Product value expected ~0.24, got {}", product.val);
        return 1;
    }

    if product.der < -0.081 || product.der > -0.079 {
        println("FAIL: Product derivative expected ~-0.08, got {}", product.der);
        return 1;
    }

    // Test addition: (a + b)' = a' + b'
    let sum = a + b;
    // Expected: val = 1.0, der = 0.1 + (-0.2) = -0.1

    if sum.der < -0.101 || sum.der > -0.099 {
        println("FAIL: Sum derivative expected ~-0.1, got {}", sum.der);
        return 1;
    }

    println("PASS: Dual propagation tests");
    return 0;
}
```

#### File: `tests/ai/belief_guards/integ_wake_transition.sx`

```simplex
// Integration Test: WAKE transition
// Tests that a suspended receive wakes when belief changes satisfy guard

use simplex_std::dual::dual;

actor WakeTestActor {
    var woken: bool = false;

    receive Trigger(data: i64) @ confidence("gate") > 0.5 => {
        woken = true;
    }

    receive CheckWoken() -> bool => {
        return woken;
    }
}

fn main() -> i64 {
    let beliefs = HiveBeliefManager::new(ConflictResolution::HighestConfidence);

    // Start with low confidence - receive should suspend
    beliefs.submit_belief(Belief::new("gate", 0.3, "test"));

    let actor = spawn WakeTestActor;

    // Send message - should suspend (guard not satisfied)
    send(actor, Trigger(1));

    // Check not yet woken
    let before = ask(actor, CheckWoken());
    if before {
        println("FAIL: Actor woke before belief change");
        return 1;
    }

    // Update belief to satisfy guard - should trigger WAKE
    beliefs.submit_belief(Belief::new("gate", 0.7, "test"));

    // Allow WAKE to process
    yield();

    // Check now woken
    let after = ask(actor, CheckWoken());
    if !after {
        println("FAIL: Actor did not wake after belief change");
        return 1;
    }

    println("PASS: WAKE transition test");
    return 0;
}
```

---

## Integration Checklist

### Phase 1: Lexer/Parser
- [ ] Add `At`, `KwConfidence`, `KwDerivative` tokens to lexer.sx
- [ ] Add same tokens to stage0.py
- [ ] Add `@` single character token handling
- [ ] Add belief guard keywords to check_keyword

### Phase 2: AST
- [ ] Add EXPR_CONFIDENCE, EXPR_DERIVATIVE, EXPR_BELIEF_GUARD tags
- [ ] Add EXPR_BELIEF_AND, EXPR_BELIEF_OR tags
- [ ] Add PAT_BELIEF_GUARD tag

### Phase 3: Parser
- [ ] Implement parse_belief_guard() and related functions in parser.sx
- [ ] Implement same in stage0.py
- [ ] Modify parse_actor to handle `@ guard` after receive pattern

### Phase 4: Belief System
- [ ] Change Belief.confidence from f64 to dual
- [ ] Update Belief::new() and Belief::update() for dual
- [ ] Add derivative() accessor

### Phase 5: Codegen
- [ ] Add codegen for EXPR_CONFIDENCE, EXPR_DERIVATIVE
- [ ] Add codegen for EXPR_BELIEF_GUARD, EXPR_BELIEF_AND, EXPR_BELIEF_OR
- [ ] Add codegen for receive handlers with belief guards
- [ ] Add codegen for suspend_receive

### Phase 6: WAKE Mechanism
- [ ] Add SuspendedReceive struct
- [ ] Add suspend tracking to HiveBeliefManager
- [ ] Add check_wake_transitions() called after belief updates
- [ ] Implement wake_actor() in runtime

### Phase 7: Tests
- [ ] unit_belief_guard_syntax.sx passes
- [ ] unit_belief_derivative.sx passes
- [ ] unit_dual_propagation.sx passes
- [ ] integ_wake_transition.sx passes

---

## Success Criteria

### Minimum Success
- [ ] Parser accepts `receive Msg(params) @ confidence(id) < 0.5 => { }` syntax
- [ ] Belief.confidence is a dual number
- [ ] `confidence(id).derivative` compiles and returns f64
- [ ] Unit tests pass

### Full Success
- [ ] WAKE transitions work (suspended receives wake on belief changes)
- [ ] Derivative patterns work correctly via dual number propagation
- [ ] Logical operators (`&&`, `||`) in belief guards work
- [ ] Integration tests pass
- [ ] Performance: belief evaluation < 1ms

### Stretch Success
- [ ] Complex autonomous scenarios work (multiple guards, nested logic)
- [ ] Belief system scales to 1000+ concurrent beliefs
- [ ] Formal model properties verified through testing

---

## Key Differences from Original TASK-013-A

| Original | Corrected |
|----------|-----------|
| C runtime with manual derivative tracking | Use existing `dual` type from simplex-std |
| `Belief.confidence: f64` | `Belief.confidence: dual` |
| Numerical differentiation `(new-old)/dt` | Automatic differentiation via dual numbers |
| File paths like `compiler/bootstrap/ast.sx` | Actual path: `simplex-core/src/ast_defs.sx` |
| Custom belief storage | Use existing `HiveBeliefManager` |
| Line numbers "around line 22215" | Actual: `parser.sx:2750` for parse_actor |

---

## Dependencies

**Required (all already exist):**
- `simplex-std/src/dual.sx` - Dual number implementation
- `simplex-learning/src/distributed/beliefs.sx` - Belief system
- `compiler/bootstrap/parser.sx` - Actor parsing
- `compiler/bootstrap/lexer.sx` - Token definitions

**Related Tasks:**
- TASK-013: Formal Model (this implements the core primitive)
- TASK-012: Nexus Protocol (will use beliefs for hive sync)
- TASK-005: Dual Numbers (already implemented)
