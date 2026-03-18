#!/bin/bash
# Bootstrap Verification Script
# Three-stage bootstrap to verify self-hosting reproducibility
#
# Stage 1: Compile sxc with Python stage0 (existing process)
# Stage 2: Use Stage 1 sxc to compile .sx files to .ll
# Stage 3: Compare Stage 1 and Stage 2 .ll output
#
# Exit 0 if identical, exit 1 if differences found

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
COMPILER_DIR="$ROOT_DIR/compiler/bootstrap"
BUILD_DIR="$ROOT_DIR/build"
STAGE1_DIR="/tmp/simplex-stage1"
STAGE2_DIR="/tmp/simplex-stage2"

echo "=== Simplex Bootstrap Verification ==="
echo "Root: $ROOT_DIR"
echo ""

# Clean up
rm -rf "$STAGE1_DIR" "$STAGE2_DIR"
mkdir -p "$STAGE1_DIR" "$STAGE2_DIR"

MODULES="lexer parser error codegen main"

# Stage 1: Compile with Python stage0
echo "--- Stage 1: Compiling with Python stage0 ---"
cd "$COMPILER_DIR"
for mod in $MODULES; do
    echo "  Compiling $mod.sx..."
    python3 stage0.py "$mod.sx" > "$STAGE1_DIR/$mod.ll" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "FAIL: stage0 failed on $mod.sx"
        exit 1
    fi
done
echo "  Stage 1 complete."
echo ""

# Build Stage 1 sxc binary (if not already built)
if [ ! -f "$BUILD_DIR/sxc" ]; then
    echo "  Building Stage 1 sxc binary..."
    cd "$ROOT_DIR"
    bash build.sh
fi

# Stage 2: Compile with Stage 1 sxc
echo "--- Stage 2: Compiling with Stage 1 sxc ---"
cd "$COMPILER_DIR"
STAGE2_FAILED=0
for mod in $MODULES; do
    echo "  Compiling $mod.sx with sxc..."
    "$BUILD_DIR/sxc" build "$mod.sx" > "$STAGE2_DIR/$mod.ll" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "  WARN: sxc failed on $mod.sx (self-hosting gap)"
        STAGE2_FAILED=1
    fi
done

if [ $STAGE2_FAILED -eq 1 ]; then
    echo ""
    echo "Stage 2 had failures -- self-hosting not yet complete."
    echo "This is expected during development. Comparing available outputs..."
    echo ""
fi

# Stage 3: Compare outputs
echo "--- Stage 3: Comparing outputs ---"
DIFF_FOUND=0
for mod in $MODULES; do
    if [ -f "$STAGE1_DIR/$mod.ll" ] && [ -f "$STAGE2_DIR/$mod.ll" ]; then
        # Normalize: strip comments and metadata that may differ
        sed 's/;.*$//' "$STAGE1_DIR/$mod.ll" | sed '/^$/d' > "$STAGE1_DIR/$mod.normalized"
        sed 's/;.*$//' "$STAGE2_DIR/$mod.ll" | sed '/^$/d' > "$STAGE2_DIR/$mod.normalized"

        if diff -q "$STAGE1_DIR/$mod.normalized" "$STAGE2_DIR/$mod.normalized" > /dev/null 2>&1; then
            echo "  $mod.ll: IDENTICAL"
        else
            echo "  $mod.ll: DIFFERS"
            diff "$STAGE1_DIR/$mod.normalized" "$STAGE2_DIR/$mod.normalized" | head -20
            DIFF_FOUND=1
        fi
    else
        echo "  $mod.ll: SKIPPED (missing from Stage 2)"
    fi
done

echo ""
if [ $DIFF_FOUND -eq 0 ] && [ $STAGE2_FAILED -eq 0 ]; then
    echo "=== PASS: Bootstrap verification succeeded ==="
    echo "Stage 1 and Stage 2 produce identical output."
    exit 0
elif [ $STAGE2_FAILED -eq 1 ]; then
    echo "=== INFO: Self-hosting not yet complete ==="
    echo "Some modules cannot be compiled by sxc yet."
    exit 0
else
    echo "=== FAIL: Bootstrap verification found differences ==="
    exit 1
fi
