#!/bin/bash
# Sanitizer Sweep Script
# Compiles and runs tests with AddressSanitizer, UBSan, and ThreadSanitizer.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
RUNTIME="$ROOT_DIR/runtime/standalone_runtime.c"
TEST_DIR="$ROOT_DIR/tests"

SANITIZERS=${1:-"address undefined"}
SXC="$BUILD_DIR/sxc"
FAILURES=0
TESTS_RUN=0

echo "=== Sanitizer Sweep ==="
echo "Sanitizers: $SANITIZERS"
echo ""

# Platform-specific flags
LDFLAGS="-lm"
if [ "$(uname)" = "Darwin" ]; then
    BREW_PREFIX=$(brew --prefix 2>/dev/null || echo "/opt/homebrew")
    LDFLAGS="$LDFLAGS -L$BREW_PREFIX/opt/openssl@3/lib -lssl -lcrypto"
else
    LDFLAGS="$LDFLAGS -lssl -lcrypto -lpthread"
fi

# Build sanitizer flags
SAN_FLAGS=""
for san in $SANITIZERS; do
    SAN_FLAGS="$SAN_FLAGS -fsanitize=$san"
done
SAN_FLAGS="$SAN_FLAGS -fno-omit-frame-pointer -O1 -g"

# Set sanitizer environment options
export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:print_stats=0"
export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
export TSAN_OPTIONS="halt_on_error=1"

# Test files to run under sanitizers (core tests)
CORE_TESTS=(
    "tests/safety/integ_asan.sx"
    "tests/safety/integ_ubsan.sx"
    "tests/basics/spec_closure.sx"
    "tests/types/spec_generics.sx"
)

for test_file in "${CORE_TESTS[@]}"; do
    full_path="$ROOT_DIR/$test_file"
    if [ ! -f "$full_path" ]; then
        echo "  SKIP: $test_file (not found)"
        continue
    fi

    name=$(basename "$test_file" .sx)
    echo "--- Testing $name with sanitizers ---"

    # Compile
    if [ -f "$SXC" ]; then
        "$SXC" build "$full_path" > "/tmp/san_$name.ll" 2>/dev/null
    else
        python3 "$ROOT_DIR/compiler/bootstrap/stage0.py" "$full_path" > "/tmp/san_$name.ll" 2>/dev/null
    fi

    if [ $? -ne 0 ]; then
        echo "  WARN: Failed to compile $name"
        continue
    fi

    # Link with sanitizer flags
    clang $SAN_FLAGS "/tmp/san_$name.ll" "$RUNTIME" -o "/tmp/san_$name.bin" $LDFLAGS 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "  WARN: Failed to link $name with sanitizers"
        continue
    fi

    # Run
    "/tmp/san_$name.bin" 2>/tmp/san_$name.log
    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 0 ]; then
        echo "  PASS: $name"
    else
        echo "  FAIL: $name (exit $EXIT_CODE)"
        if [ -f "/tmp/san_$name.log" ]; then
            head -20 "/tmp/san_$name.log"
        fi
        FAILURES=$((FAILURES + 1))
    fi

    TESTS_RUN=$((TESTS_RUN + 1))

    # Cleanup
    rm -f "/tmp/san_$name.ll" "/tmp/san_$name.bin" "/tmp/san_$name.log"
done

echo ""
echo "=== Sanitizer Results ==="
echo "Tests run: $TESTS_RUN"
echo "Failures: $FAILURES"

if [ $FAILURES -gt 0 ]; then
    echo "FAIL: $FAILURES sanitizer issues found"
    exit 1
fi

echo "PASS: All sanitizer checks clean"
exit 0
