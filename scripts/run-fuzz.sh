#!/bin/bash
# Fuzz Runner Script
# Compiles and runs fuzz targets for the Simplex compiler.
# Each target runs for a configurable duration (default: 60 seconds).

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
FUZZ_DIR="$ROOT_DIR/tests/fuzz"
RUNTIME="$ROOT_DIR/runtime/standalone_runtime.c"

DURATION=${1:-60}  # seconds per target (default 60)
SXC="$BUILD_DIR/sxc"
CRASHES=0
TARGETS_RUN=0

echo "=== Simplex Fuzz Runner ==="
echo "Duration per target: ${DURATION}s"
echo ""

# Platform-specific flags
LDFLAGS="-lm"
if [ "$(uname)" = "Darwin" ]; then
    BREW_PREFIX=$(brew --prefix 2>/dev/null || echo "/opt/homebrew")
    LDFLAGS="$LDFLAGS -L$BREW_PREFIX/opt/openssl@3/lib -lssl -lcrypto"
else
    LDFLAGS="$LDFLAGS -lssl -lcrypto -lpthread"
fi

for target in "$FUZZ_DIR"/fuzz_*.sx; do
    if [ ! -f "$target" ]; then
        continue
    fi

    name=$(basename "$target" .sx)
    echo "--- Running $name ---"

    # Compile fuzz target
    if [ -f "$SXC" ]; then
        "$SXC" build "$target" > "/tmp/$name.ll" 2>/dev/null
    else
        python3 "$ROOT_DIR/compiler/bootstrap/stage0.py" "$target" > "/tmp/$name.ll" 2>/dev/null
    fi

    if [ $? -ne 0 ]; then
        echo "  WARN: Failed to compile $name, skipping"
        continue
    fi

    # Link
    clang -O1 "/tmp/$name.ll" "$RUNTIME" -o "/tmp/$name.bin" $LDFLAGS 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "  WARN: Failed to link $name, skipping"
        continue
    fi

    # Run with timeout
    timeout "$DURATION" "/tmp/$name.bin" 2>/dev/null
    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 0 ]; then
        echo "  PASS: $name completed successfully"
    elif [ $EXIT_CODE -eq 124 ]; then
        echo "  PASS: $name ran for ${DURATION}s without crash"
    else
        echo "  FAIL: $name crashed with exit code $EXIT_CODE"
        CRASHES=$((CRASHES + 1))
    fi

    TARGETS_RUN=$((TARGETS_RUN + 1))

    # Cleanup
    rm -f "/tmp/$name.ll" "/tmp/$name.bin"
done

echo ""
echo "=== Results ==="
echo "Targets run: $TARGETS_RUN"
echo "Crashes: $CRASHES"

if [ $CRASHES -gt 0 ]; then
    echo "FAIL: $CRASHES fuzz targets crashed"
    exit 1
fi

echo "PASS: All fuzz targets clean"
exit 0
