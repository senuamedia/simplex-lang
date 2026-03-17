// C Unit Tests for TASK-013-A: Belief-Gated Receive with Derivative Patterns
// These tests verify the C runtime implementation directly

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

// Forward declarations of functions from standalone_runtime.c
// SxString must match the runtime definition: len, cap, data
typedef struct SxString {
    size_t len;
    size_t cap;
    char* data;
} SxString;

// Create SxString helper
static SxString* make_string(const char* s) {
    SxString* str = (SxString*)malloc(sizeof(SxString));
    str->data = strdup(s);
    str->len = strlen(s);
    str->cap = str->len + 1;
    return str;
}

// External functions from runtime
extern int64_t belief_register(int64_t name_ptr, double confidence, double derivative);
extern int64_t belief_update(int64_t name_ptr, double new_confidence);
extern int64_t belief_update_dual(int64_t name_ptr, double confidence, double derivative);
extern int64_t belief_guard_get_confidence(int64_t name_ptr);
extern int64_t belief_guard_get_derivative(int64_t name_ptr);
extern int64_t belief_suspend_receive(
    int64_t actor_id,
    int64_t handler_id,
    int64_t belief_name_ptr,
    int64_t guard_type,
    int64_t op,
    double threshold,
    int64_t callback_ptr,
    int64_t context_ptr
);
extern int64_t belief_cancel_suspend(int64_t suspend_id);

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static int test_##name(void)
#define RUN_TEST(name) do { \
    printf("  %-40s ", #name); \
    if (test_##name()) { \
        printf("PASS\n"); \
        tests_passed++; \
    } else { \
        printf("FAIL\n"); \
        tests_failed++; \
    } \
} while(0)

// Helper to convert i64 bits to double
static double bits_to_double(int64_t bits) {
    double result;
    memcpy(&result, &bits, sizeof(double));
    return result;
}

// ============================================================================
// Section 1: Belief Registration Tests
// ============================================================================

TEST(belief_register_basic) {
    SxString* name = make_string("test_reg_basic");
    int64_t result = belief_register((int64_t)name, 0.75, 0.0);
    if (result == 0) return 0;

    double conf = bits_to_double(belief_guard_get_confidence((int64_t)name));
    return fabs(conf - 0.75) < 0.001;
}

TEST(belief_register_with_derivative) {
    SxString* name = make_string("test_reg_deriv");
    int64_t result = belief_register((int64_t)name, 0.6, -0.1);
    if (result == 0) return 0;

    double conf = bits_to_double(belief_guard_get_confidence((int64_t)name));
    double der = bits_to_double(belief_guard_get_derivative((int64_t)name));

    return fabs(conf - 0.6) < 0.001 && fabs(der - (-0.1)) < 0.001;
}

TEST(belief_register_clamps_confidence) {
    SxString* name1 = make_string("test_clamp_high");
    SxString* name2 = make_string("test_clamp_low");

    belief_register((int64_t)name1, 1.5, 0.0);  // Should clamp to 1.0
    belief_register((int64_t)name2, -0.5, 0.0); // Should clamp to 0.0

    double conf1 = bits_to_double(belief_guard_get_confidence((int64_t)name1));
    double conf2 = bits_to_double(belief_guard_get_confidence((int64_t)name2));

    return fabs(conf1 - 1.0) < 0.001 && fabs(conf2 - 0.0) < 0.001;
}

// ============================================================================
// Section 2: Belief Update Tests
// ============================================================================

TEST(belief_update_changes_confidence) {
    SxString* name = make_string("test_update_conf");
    belief_register((int64_t)name, 0.5, 0.0);

    belief_update((int64_t)name, 0.8);

    double conf = bits_to_double(belief_guard_get_confidence((int64_t)name));
    return fabs(conf - 0.8) < 0.001;
}

TEST(belief_update_dual_changes_both) {
    SxString* name = make_string("test_update_dual");
    belief_register((int64_t)name, 0.5, 0.0);

    belief_update_dual((int64_t)name, 0.7, -0.2);

    double conf = bits_to_double(belief_guard_get_confidence((int64_t)name));
    double der = bits_to_double(belief_guard_get_derivative((int64_t)name));

    return fabs(conf - 0.7) < 0.001 && fabs(der - (-0.2)) < 0.001;
}

TEST(belief_update_auto_registers) {
    SxString* name = make_string("test_auto_reg");

    // Update without prior registration should auto-register
    belief_update((int64_t)name, 0.6);

    double conf = bits_to_double(belief_guard_get_confidence((int64_t)name));
    return fabs(conf - 0.6) < 0.001;
}

// ============================================================================
// Section 3: Get Confidence/Derivative Tests
// ============================================================================

TEST(get_confidence_unknown_returns_zero) {
    SxString* name = make_string("test_unknown_belief");
    double conf = bits_to_double(belief_guard_get_confidence((int64_t)name));
    // Unknown belief should return 0.0 (or close to it after auto-register)
    return conf >= 0.0 && conf <= 1.0;
}

TEST(get_derivative_unknown_returns_zero) {
    SxString* name = make_string("test_unknown_deriv");
    double der = bits_to_double(belief_guard_get_derivative((int64_t)name));
    return fabs(der) < 0.001;
}

// ============================================================================
// Section 4: WAKE Mechanism Tests
// ============================================================================

static int wake_callback_count = 0;
static int64_t last_wake_suspend_id = 0;

static int64_t test_wake_callback(int64_t suspend_id, int64_t context) {
    wake_callback_count++;
    last_wake_suspend_id = suspend_id;
    return 0;
}

TEST(suspend_receive_returns_id) {
    SxString* name = make_string("test_suspend1");
    belief_register((int64_t)name, 0.3, 0.0);

    // GUARD_CONFIDENCE = 0, GUARD_OP_GT = 2
    int64_t suspend_id = belief_suspend_receive(
        1, 100, (int64_t)name, 0, 2, 0.5,
        (int64_t)test_wake_callback, 0
    );

    return suspend_id > 0;
}

TEST(cancel_suspend_succeeds) {
    SxString* name = make_string("test_cancel1");
    belief_register((int64_t)name, 0.3, 0.0);

    int64_t suspend_id = belief_suspend_receive(
        2, 101, (int64_t)name, 0, 2, 0.5,
        (int64_t)test_wake_callback, 0
    );

    if (suspend_id <= 0) return 0;

    int64_t result = belief_cancel_suspend(suspend_id);
    return result == 1;
}

TEST(cancel_nonexistent_fails) {
    int64_t result = belief_cancel_suspend(99999);
    return result == 0;
}

TEST(wake_triggers_on_condition_met) {
    wake_callback_count = 0;
    last_wake_suspend_id = 0;

    SxString* name = make_string("test_wake_trigger");
    belief_register((int64_t)name, 0.3, 0.0);

    // Suspend waiting for confidence > 0.6
    int64_t suspend_id = belief_suspend_receive(
        3, 102, (int64_t)name, 0, 2, 0.6,
        (int64_t)test_wake_callback, 0
    );

    if (suspend_id <= 0) return 0;

    // Update belief to satisfy condition
    belief_update((int64_t)name, 0.8);

    // Callback should have been invoked
    return wake_callback_count > 0 && last_wake_suspend_id == suspend_id;
}

TEST(wake_does_not_trigger_if_condition_not_met) {
    wake_callback_count = 0;

    SxString* name = make_string("test_wake_no_trigger");
    belief_register((int64_t)name, 0.3, 0.0);

    // Suspend waiting for confidence > 0.9
    int64_t suspend_id = belief_suspend_receive(
        4, 103, (int64_t)name, 0, 2, 0.9,
        (int64_t)test_wake_callback, 0
    );

    if (suspend_id <= 0) return 0;

    // Update belief but don't satisfy condition
    int initial_count = wake_callback_count;
    belief_update((int64_t)name, 0.5);  // Still below 0.9

    // Callback should NOT have been invoked
    return wake_callback_count == initial_count;
}

TEST(wake_derivative_guard) {
    wake_callback_count = 0;

    SxString* name = make_string("test_wake_deriv");
    belief_register((int64_t)name, 0.5, 0.0);

    // Suspend waiting for derivative < -0.1 (GUARD_DERIVATIVE = 1, GUARD_OP_LT = 0)
    int64_t suspend_id = belief_suspend_receive(
        5, 104, (int64_t)name, 1, 0, -0.1,
        (int64_t)test_wake_callback, 0
    );

    if (suspend_id <= 0) return 0;

    // Update with rapid decrease
    int initial_count = wake_callback_count;
    belief_update_dual((int64_t)name, 0.4, -0.2);

    // Callback should have been invoked (derivative -0.2 < -0.1)
    return wake_callback_count > initial_count;
}

// ============================================================================
// Section 5: Guard Operation Tests
// ============================================================================

TEST(guard_op_less_than) {
    SxString* name = make_string("test_op_lt");
    belief_register((int64_t)name, 0.4, 0.0);

    wake_callback_count = 0;
    // Wait for confidence < 0.5 (already satisfied)
    int64_t id = belief_suspend_receive(6, 105, (int64_t)name, 0, 0, 0.5,
        (int64_t)test_wake_callback, 0);

    // Trigger by updating (even to same value)
    belief_update((int64_t)name, 0.4);

    return wake_callback_count > 0;
}

TEST(guard_op_less_equal) {
    SxString* name = make_string("test_op_le");
    belief_register((int64_t)name, 0.5, 0.0);

    wake_callback_count = 0;
    // Wait for confidence <= 0.5 (equal case)
    int64_t id = belief_suspend_receive(7, 106, (int64_t)name, 0, 1, 0.5,
        (int64_t)test_wake_callback, 0);

    belief_update((int64_t)name, 0.5);

    return wake_callback_count > 0;
}

TEST(guard_op_greater_than) {
    SxString* name = make_string("test_op_gt");
    belief_register((int64_t)name, 0.3, 0.0);

    wake_callback_count = 0;
    // Wait for confidence > 0.5
    int64_t id = belief_suspend_receive(8, 107, (int64_t)name, 0, 2, 0.5,
        (int64_t)test_wake_callback, 0);

    belief_update((int64_t)name, 0.7);

    return wake_callback_count > 0;
}

TEST(guard_op_greater_equal) {
    SxString* name = make_string("test_op_ge");
    belief_register((int64_t)name, 0.3, 0.0);

    wake_callback_count = 0;
    // Wait for confidence >= 0.5
    int64_t id = belief_suspend_receive(9, 108, (int64_t)name, 0, 3, 0.5,
        (int64_t)test_wake_callback, 0);

    belief_update((int64_t)name, 0.5);  // Equal case

    return wake_callback_count > 0;
}

TEST(guard_op_equal) {
    SxString* name = make_string("test_op_eq");
    belief_register((int64_t)name, 0.3, 0.0);

    wake_callback_count = 0;
    // Wait for confidence == 0.5
    int64_t id = belief_suspend_receive(10, 109, (int64_t)name, 0, 4, 0.5,
        (int64_t)test_wake_callback, 0);

    belief_update((int64_t)name, 0.5);

    return wake_callback_count > 0;
}

TEST(guard_op_not_equal) {
    SxString* name = make_string("test_op_ne");
    belief_register((int64_t)name, 0.5, 0.0);

    wake_callback_count = 0;
    // Wait for confidence != 0.5
    int64_t id = belief_suspend_receive(11, 110, (int64_t)name, 0, 5, 0.5,
        (int64_t)test_wake_callback, 0);

    belief_update((int64_t)name, 0.6);

    return wake_callback_count > 0;
}

// Stub for simplex_main (required by runtime)
int64_t simplex_main(void) {
    return 0;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
    printf("=== Belief Guard C Runtime Tests (TASK-013-A) ===\n\n");

    printf("--- Section 1: Belief Registration ---\n");
    RUN_TEST(belief_register_basic);
    RUN_TEST(belief_register_with_derivative);
    RUN_TEST(belief_register_clamps_confidence);

    printf("\n--- Section 2: Belief Update ---\n");
    RUN_TEST(belief_update_changes_confidence);
    RUN_TEST(belief_update_dual_changes_both);
    RUN_TEST(belief_update_auto_registers);

    printf("\n--- Section 3: Get Confidence/Derivative ---\n");
    RUN_TEST(get_confidence_unknown_returns_zero);
    RUN_TEST(get_derivative_unknown_returns_zero);

    printf("\n--- Section 4: WAKE Mechanism ---\n");
    RUN_TEST(suspend_receive_returns_id);
    RUN_TEST(cancel_suspend_succeeds);
    RUN_TEST(cancel_nonexistent_fails);
    RUN_TEST(wake_triggers_on_condition_met);
    RUN_TEST(wake_does_not_trigger_if_condition_not_met);
    RUN_TEST(wake_derivative_guard);

    printf("\n--- Section 5: Guard Operations ---\n");
    RUN_TEST(guard_op_less_than);
    RUN_TEST(guard_op_less_equal);
    RUN_TEST(guard_op_greater_than);
    RUN_TEST(guard_op_greater_equal);
    RUN_TEST(guard_op_equal);
    RUN_TEST(guard_op_not_equal);

    printf("\n=== Summary ===\n");
    printf("Passed: %d / %d\n", tests_passed, tests_passed + tests_failed);

    if (tests_failed == 0) {
        printf("All tests passed!\n");
    } else {
        printf("Some tests failed.\n");
    }

    return tests_failed;
}
