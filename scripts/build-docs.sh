#!/bin/bash
# build-docs.sh - Build Simplex API Documentation
#
# Copyright (c) 2025-2026 Rod Higgins
# Licensed under AGPL-3.0 - see LICENSE file
# https://github.com/senuamedia/simplex

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
API_DIR="$ROOT/simplex-docs/api"
SXC="$ROOT/sxc"

# Module definitions: category:source_dir
MODULES=(
    "std:simplex-std/src"
    "json:simplex-json/src"
    "sql:simplex-sql/src"
    "toml:simplex-toml/src"
    "uuid:simplex-uuid/src"
    "s3:simplex-s3/src"
    "ses:simplex-ses/src"
    "inference:simplex-inference/src"
    "learning:simplex-learning/src"
    "edge-hive:edge-hive/src"
    "nexus:nexus/src"
    "lib:lib"
)

echo "=== Building Simplex API Documentation ==="
echo "Root: $ROOT"
echo "Output: $API_DIR"
echo ""

# Check if sxc exists
if [[ ! -x "$SXC" ]]; then
    # Try platform-specific binaries
    if [[ -x "$ROOT/sxc-macos-arm64" ]]; then
        SXC="$ROOT/sxc-macos-arm64"
    elif [[ -x "$ROOT/sxc-macos-x86_64" ]]; then
        SXC="$ROOT/sxc-macos-x86_64"
    elif [[ -x "$ROOT/sxc-linux-x86_64" ]]; then
        SXC="$ROOT/sxc-linux-x86_64"
    else
        echo "Error: No sxc compiler found. Build the compiler first."
        exit 1
    fi
fi

echo "Using compiler: $SXC"

# Build sxdoc if not exists
SXDOC="$ROOT/sxdoc"
if [[ ! -x "$SXDOC" ]]; then
    echo "Building sxdoc..."
    "$SXC" "$ROOT/sxdoc.sx" -o "$ROOT/sxdoc.ll"
    clang -O2 "$ROOT/sxdoc.ll" "$ROOT/runtime/standalone_runtime.c" \
        -o "$SXDOC" -lm -lssl -lcrypto -lsqlite3 -lpthread 2>/dev/null || \
    clang -O2 "$ROOT/sxdoc.ll" "$ROOT/runtime/standalone_runtime.c" \
        -o "$SXDOC" -lm -lpthread 2>/dev/null || {
            echo "Warning: Could not build sxdoc binary"
        }
fi

# Build sxdoc-index if not exists
SXDOC_INDEX="$ROOT/sxdoc-index"
if [[ ! -x "$SXDOC_INDEX" ]]; then
    echo "Building sxdoc-index..."
    "$SXC" "$ROOT/sxdoc-index.sx" -o "$ROOT/sxdoc-index.ll"
    clang -O2 "$ROOT/sxdoc-index.ll" "$ROOT/runtime/standalone_runtime.c" \
        -o "$SXDOC_INDEX" -lm -lssl -lcrypto -lsqlite3 -lpthread 2>/dev/null || \
    clang -O2 "$ROOT/sxdoc-index.ll" "$ROOT/runtime/standalone_runtime.c" \
        -o "$SXDOC_INDEX" -lm -lpthread 2>/dev/null || {
            echo "Warning: Could not build sxdoc-index binary"
        }
fi

# Ensure output directories exist
mkdir -p "$API_DIR/assets"

# Generate documentation for each module
echo ""
echo "=== Generating Module Documentation ==="

for entry in "${MODULES[@]}"; do
    category="${entry%%:*}"
    srcdir="${entry#*:}"
    outdir="$API_DIR/$category"

    # Check if source directory exists
    if [[ ! -d "$ROOT/$srcdir" ]]; then
        echo "Skipping $category (source not found: $srcdir)"
        continue
    fi

    # Find source files
    files=$(find "$ROOT/$srcdir" -maxdepth 1 -name "*.sx" 2>/dev/null | head -50)
    if [[ -z "$files" ]]; then
        echo "Skipping $category (no .sx files)"
        continue
    fi

    mkdir -p "$outdir"
    echo "Generating $category..."

    if [[ -x "$SXDOC" ]]; then
        # Use compiled sxdoc
        "$SXDOC" --manifest --category "$category" -o "$outdir" $files 2>&1 || true
    else
        echo "  (sxdoc not available, skipping)"
    fi
done

# Generate index and API context
echo ""
echo "=== Generating Index ==="

if [[ -x "$SXDOC_INDEX" ]]; then
    "$SXDOC_INDEX" "$API_DIR" 2>&1 || true
else
    echo "sxdoc-index not available, skipping index generation"
fi

# Ensure assets are in place
echo ""
echo "=== Copying Assets ==="

if [[ -f "$ROOT/simplex-docs/api/assets/docs.css" ]]; then
    echo "Assets already in place"
else
    echo "Warning: assets not found at $ROOT/simplex-docs/api/assets/"
fi

# Summary
echo ""
echo "=== Documentation Build Complete ==="
echo "Output directory: $API_DIR"

# Count generated files
html_count=$(find "$API_DIR" -name "*.html" 2>/dev/null | wc -l | tr -d ' ')
json_count=$(find "$API_DIR" -name "*.json" 2>/dev/null | wc -l | tr -d ' ')
echo "Generated: $html_count HTML files, $json_count JSON files"

if [[ -f "$API_DIR/index.html" ]]; then
    echo ""
    echo "Open in browser: file://$API_DIR/index.html"
fi
