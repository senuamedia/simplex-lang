#!/bin/bash
# Incremental Build Script
# Only recompiles .sx files whose SHA256 hash has changed.
# Tracks hashes in .build-cache/ directory.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CACHE_DIR="$ROOT_DIR/.build-cache"
COMPILER_DIR="$ROOT_DIR/compiler/bootstrap"
BUILD_DIR="$ROOT_DIR/build"

mkdir -p "$CACHE_DIR"

# Determine hash command
if command -v sha256sum > /dev/null 2>&1; then
    HASH_CMD="sha256sum"
elif command -v shasum > /dev/null 2>&1; then
    HASH_CMD="shasum -a 256"
else
    echo "ERROR: No SHA256 tool found"
    exit 1
fi

# Hash a file and return just the hash
file_hash() {
    $HASH_CMD "$1" 2>/dev/null | cut -d' ' -f1
}

# Check if file needs recompilation
needs_recompile() {
    local src="$1"
    local cache_file="$CACHE_DIR/$(basename "$src").hash"

    if [ ! -f "$cache_file" ]; then
        return 0  # No cache → needs compile
    fi

    local current_hash=$(file_hash "$src")
    local cached_hash=$(cat "$cache_file" 2>/dev/null)

    if [ "$current_hash" != "$cached_hash" ]; then
        return 0  # Hash changed → needs compile
    fi

    # Check dependencies (use declarations)
    local deps=$(grep -E "^use " "$src" 2>/dev/null | sed 's/use //;s/;//' || true)
    for dep in $deps; do
        local dep_file="$COMPILER_DIR/$dep.sx"
        if [ -f "$dep_file" ]; then
            local dep_cache="$CACHE_DIR/$dep.sx.hash"
            if [ ! -f "$dep_cache" ]; then
                return 0  # Dependency not cached → recompile
            fi
            local dep_hash=$(file_hash "$dep_file")
            local dep_cached=$(cat "$dep_cache" 2>/dev/null)
            if [ "$dep_hash" != "$dep_cached" ]; then
                return 0  # Dependency changed → recompile
            fi
        fi
    done

    return 1  # No changes → skip
}

# Update hash cache
update_cache() {
    local src="$1"
    local cache_file="$CACHE_DIR/$(basename "$src").hash"
    file_hash "$src" > "$cache_file"
}

echo "=== Incremental Build ==="
echo "Cache: $CACHE_DIR"

MODULES="lexer parser error codegen main"
RECOMPILED=0
SKIPPED=0

cd "$COMPILER_DIR"

for mod in $MODULES; do
    local_src="$mod.sx"
    if [ ! -f "$local_src" ]; then
        echo "  SKIP: $local_src not found"
        continue
    fi

    if needs_recompile "$local_src"; then
        echo "  COMPILE: $local_src (changed)"
        python3 stage0.py "$local_src" > "$mod.ll" 2>/dev/null
        if [ $? -eq 0 ]; then
            update_cache "$local_src"
            RECOMPILED=$((RECOMPILED + 1))
        else
            echo "  ERROR: Failed to compile $local_src"
        fi
    else
        echo "  SKIP: $local_src (unchanged)"
        SKIPPED=$((SKIPPED + 1))
    fi
done

echo ""
echo "Compiled: $RECOMPILED, Skipped: $SKIPPED"

# If anything was recompiled, re-merge and re-link
if [ $RECOMPILED -gt 0 ]; then
    echo "Re-linking sxc..."
    cd "$ROOT_DIR"
    bash build.sh
    echo "Build complete."
else
    echo "Nothing changed, build up to date."
fi
