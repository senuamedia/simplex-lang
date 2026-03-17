#!/bin/bash
# Nexus Protocol Test Runner
#
# This script compiles and runs nexus tests by combining source files
# with test files, then compiling with stage0.py and the runtime.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIMPLEX_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
COMPILER="$SIMPLEX_ROOT/compiler/bootstrap/stage0.py"
RUNTIME="$SIMPLEX_ROOT/runtime/standalone_runtime.c"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Create temp directory
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

echo "=== Nexus Protocol Tests ==="
echo "Compiler: $COMPILER"
echo "Runtime: $RUNTIME"
echo ""

# Function to run a single test
run_test() {
    local test_name=$1
    local test_file="$SCRIPT_DIR/tests/${test_name}.sx"
    local combined_file="$TEMP_DIR/${test_name}_combined.sx"
    local ll_file="$TEMP_DIR/${test_name}.ll"
    local bin_file="$TEMP_DIR/${test_name}.bin"

    echo -n "Running $test_name... "

    # Check test file exists
    if [ ! -f "$test_file" ]; then
        echo -e "${YELLOW}SKIP${NC} (file not found)"
        return 0
    fi

    # Combine source files with test file
    # Order matters: types first, then bits, frame, sync, conn, control, lib, finally the test
    cat "$SCRIPT_DIR/src/types.sx" > "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/bits.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/frame.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/sync.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/conn.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/control.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/message.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/stf.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/vector_clock.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/crdt.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/federation.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/neural.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/coordinator.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/security.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/secure_frame.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/secure_conn.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/transport.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/tcp_transport.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/conn_pool.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/address.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/reconnect.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/session.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/multiplex.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/flow_control.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/request_response.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    # Phase 11: Dual Number Inference
    cat "$SCRIPT_DIR/src/dual.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/trajectory.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/prediction.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$SCRIPT_DIR/src/lib.sx" >> "$combined_file"
    echo "" >> "$combined_file"
    cat "$test_file" >> "$combined_file"

    # Compile with stage0.py
    # The compiler writes LLVM IR to a .ll file (replaces .sx with .ll)
    # and outputs debug info to stdout
    local combined_ll="${combined_file%.sx}.ll"
    if ! python3 "$COMPILER" "$combined_file" > "$TEMP_DIR/${test_name}_compile.log" 2>&1; then
        echo -e "${RED}FAIL${NC} (compilation error)"
        echo "Compilation output:"
        cat "$TEMP_DIR/${test_name}_compile.log"
        return 1
    fi

    # Check that the .ll file was generated
    if [ ! -f "$combined_ll" ]; then
        echo -e "${RED}FAIL${NC} (compiler error - no .ll file generated)"
        echo "Compiler output:"
        cat "$TEMP_DIR/${test_name}_compile.log"
        return 1
    fi

    # Move the generated .ll to our expected location
    mv "$combined_ll" "$ll_file"

    # Link with clang
    if ! clang -O2 "$ll_file" "$RUNTIME" -o "$bin_file" -lm -lssl -lcrypto -lsqlite3 2>"$TEMP_DIR/${test_name}_link.log"; then
        echo -e "${RED}FAIL${NC} (link error)"
        echo "Link output:"
        cat "$TEMP_DIR/${test_name}_link.log"
        return 1
    fi

    # Run the test
    if "$bin_file" 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        return 0
    else
        echo -e "${RED}FAIL${NC} (runtime error)"
        return 1
    fi
}

# Track results
passed=0
failed=0
skipped=0

# Run each test
for test_file in "$SCRIPT_DIR"/tests/test_*.sx; do
    if [ -f "$test_file" ]; then
        test_name=$(basename "$test_file" .sx)
        if run_test "$test_name"; then
            ((passed++))
        else
            ((failed++))
        fi
        echo ""
    fi
done

# Summary
echo "=== Summary ==="
echo -e "Passed: ${GREEN}$passed${NC}"
echo -e "Failed: ${RED}$failed${NC}"

if [ $failed -gt 0 ]; then
    exit 1
fi
