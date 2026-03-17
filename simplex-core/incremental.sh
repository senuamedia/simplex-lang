#!/bin/bash
#
# Incremental Compilation Support for Simplex Compiler
# This file provides functions for caching and incremental builds
#

# Incremental compilation configuration
CACHE_DIR=".sx-cache"
CACHE_OBJECTS="$CACHE_DIR/objects"

# Global flags for incremental compilation
INCREMENTAL=${INCREMENTAL:-1}
VERBOSE=${VERBOSE:-0}
FORCE_REBUILD=${FORCE_REBUILD:-0}

# ========================================
# Utility Functions
# ========================================

# Calculate SHA256 hash of a file
file_hash() {
    local file="$1"
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$file" 2>/dev/null | cut -d' ' -f1
    elif command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$file" 2>/dev/null | cut -d' ' -f1
    else
        # Fallback: use md5 if SHA256 not available
        if command -v md5 >/dev/null 2>&1; then
            md5 -q "$file" 2>/dev/null
        elif command -v md5sum >/dev/null 2>&1; then
            md5sum "$file" 2>/dev/null | cut -d' ' -f1
        else
            # Last resort: use file modification time
            stat -f "%m" "$file" 2>/dev/null || stat -c "%Y" "$file" 2>/dev/null
        fi
    fi
}

# Initialize cache directory
init_cache() {
    mkdir -p "$CACHE_DIR"
    mkdir -p "$CACHE_OBJECTS"
}

# Clean cache directory
clean_cache() {
    if [ -d "$CACHE_DIR" ]; then
        rm -rf "$CACHE_DIR"
        echo "Cache cleared: $CACHE_DIR"
    else
        echo "No cache to clear"
    fi
}

# ========================================
# Dependency Tracking
# ========================================

# Extract imports/uses from a source file
# Returns a list of module paths that this file depends on
extract_dependencies() {
    local file="$1"
    local deps=""

    # Look for: mod <name>; and use <path>;
    while IFS= read -r line; do
        # Remove comments
        line="${line%%//*}"

        # Match: mod name;
        if [[ "$line" =~ ^[[:space:]]*mod[[:space:]]+([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*\; ]]; then
            local mod_name="${BASH_REMATCH[1]}"
            local dir=$(dirname "$file")
            if [ -f "$dir/$mod_name.sx" ]; then
                deps="$deps $dir/$mod_name.sx"
            elif [ -f "$dir/$mod_name/mod.sx" ]; then
                deps="$deps $dir/$mod_name/mod.sx"
            fi
        fi

        # Match: use path::to::module;
        if [[ "$line" =~ ^[[:space:]]*use[[:space:]]+([a-zA-Z_][a-zA-Z0-9_:]*)[[:space:]]*\; ]]; then
            local use_path="${BASH_REMATCH[1]}"
            local file_path=$(echo "$use_path" | sed 's/::/\//g')
            local dir=$(dirname "$file")
            if [ -f "$dir/$file_path.sx" ]; then
                deps="$deps $dir/$file_path.sx"
            fi
        fi

    done < "$file"

    echo "$deps"
}

# Get object file path for a source file
get_object_path() {
    local file="$1"
    local abs_file
    if [[ "$file" = /* ]]; then
        abs_file="$file"
    else
        abs_file="$(cd "$(dirname "$file")" && pwd)/$(basename "$file")"
    fi
    echo "$CACHE_OBJECTS/$(echo "$abs_file" | sed 's/[\/]/_/g').o"
}

# Get hash file path for a source file
get_hash_path() {
    local file="$1"
    local abs_file
    if [[ "$file" = /* ]]; then
        abs_file="$file"
    else
        abs_file="$(cd "$(dirname "$file")" && pwd)/$(basename "$file")"
    fi
    echo "$CACHE_OBJECTS/$(echo "$abs_file" | sed 's/[\/]/_/g').hash"
}

# Check if a file needs recompilation
needs_rebuild() {
    local file="$1"
    local depth="${2:-0}"  # Track recursion depth to prevent infinite loops

    # Prevent infinite recursion
    if [ "$depth" -gt 20 ]; then
        return 0
    fi

    if [ $FORCE_REBUILD -eq 1 ]; then
        return 0  # true - needs rebuild
    fi

    if [ $INCREMENTAL -eq 0 ]; then
        return 0  # true - not using incremental
    fi

    local current_hash=$(file_hash "$file")
    local hash_file=$(get_hash_path "$file")
    local object_file=$(get_object_path "$file")

    # Check if hash file exists
    if [ ! -f "$hash_file" ]; then
        return 0  # true - not in cache
    fi

    # Check if cached hash matches
    local cached_hash=$(cat "$hash_file" 2>/dev/null)
    if [ "$current_hash" != "$cached_hash" ]; then
        return 0  # true - hash changed
    fi

    # Check if object file exists
    if [ ! -f "$object_file" ]; then
        return 0  # true - no cached object
    fi

    # Check if any dependency changed
    local deps=$(extract_dependencies "$file")
    for dep in $deps; do
        if [ -f "$dep" ]; then
            if needs_rebuild "$dep" $((depth + 1)); then
                return 0  # true - dependency changed
            fi
        fi
    done

    return 1  # false - no rebuild needed
}

# ========================================
# Source File Discovery
# ========================================

# Find all source files in a project
# Starts from entry file and follows mod declarations
discover_sources() {
    local entry="$1"
    local sources=""
    local to_process="$entry"
    local processed=""

    while [ -n "$to_process" ]; do
        # Get first file to process
        local current=$(echo "$to_process" | awk '{print $1}')
        to_process=$(echo "$to_process" | awk '{$1=""; print $0}' | sed 's/^ *//')

        # Skip if empty
        if [ -z "$current" ]; then
            continue
        fi

        # Skip if already processed
        if echo " $processed " | grep -q " $current "; then
            continue
        fi

        if [ ! -f "$current" ]; then
            continue
        fi

        processed="$processed $current"
        sources="$sources $current"

        # Extract dependencies and add to process list
        local deps=$(extract_dependencies "$current")
        for dep in $deps; do
            if [ -f "$dep" ]; then
                if ! echo " $processed " | grep -q " $dep "; then
                    to_process="$to_process $dep"
                fi
            fi
        done
    done

    echo "$sources"
}

# ========================================
# Incremental Build
# ========================================

# Compile a single file to object file, using cache if possible
# Returns: 0 = compiled, 1 = error, 2 = cached
compile_file_cached() {
    local file="$1"
    local compiler="$2"
    local includes="$3"
    local object_file=$(get_object_path "$file")
    local hash_file=$(get_hash_path "$file")

    if needs_rebuild "$file"; then
        if [ $VERBOSE -eq 1 ]; then
            echo "  Compiling: $file"
        fi

        # Compile to LLVM IR
        local ll_file="${file%.sx}.ll"
        if ! "$compiler" "$file" 2>&1; then
            return 1
        fi

        if [ ! -f "$ll_file" ]; then
            echo "Error: Compilation failed for $file"
            return 1
        fi

        # Compile LLVM IR to object file
        if ! clang -c -O2 $includes "$ll_file" -o "$object_file" 2>&1; then
            echo "Error: Failed to compile $file to object"
            rm -f "$ll_file"
            return 1
        fi

        # Clean up .ll file
        rm -f "$ll_file"

        # Update hash cache
        file_hash "$file" > "$hash_file"

        return 0  # File was recompiled
    else
        if [ $VERBOSE -eq 1 ]; then
            echo "  Cached: $file"
        fi
        return 2  # File was cached
    fi
}

# Build project with incremental compilation
# Arguments: entry_file output_file compiler runtime includes libs
build_incremental() {
    local entry="$1"
    local output="$2"
    local compiler="$3"
    local runtime="$4"
    local includes="$5"
    local libs="$6"

    local start_time=$(date +%s)

    # Initialize cache
    init_cache

    # Discover all source files
    echo "Checking source files..."
    local sources=$(discover_sources "$entry")
    local total_files=$(echo "$sources" | wc -w | tr -d ' ')

    if [ "$total_files" -eq 0 ]; then
        echo "Error: No source files found"
        return 1
    fi

    if [ $VERBOSE -eq 1 ]; then
        echo "  Found $total_files source file(s)"
    fi

    # Determine which files need recompilation
    local to_compile=""
    local cached_count=0
    local compile_count=0

    for file in $sources; do
        if needs_rebuild "$file"; then
            to_compile="$to_compile $file"
            compile_count=$((compile_count + 1))
            echo "  $(basename "$file") (changed)"
        else
            cached_count=$((cached_count + 1))
            echo "  $(basename "$file") (unchanged, cached)"
        fi
    done

    # Compile files that need it
    local objects=""
    local recompiled=0

    if [ $compile_count -gt 0 ]; then
        echo "Compiling $compile_count file(s)..."
    fi

    for file in $sources; do
        local object_file=$(get_object_path "$file")

        compile_file_cached "$file" "$compiler" "$includes"
        local result=$?

        if [ $result -eq 0 ]; then
            recompiled=$((recompiled + 1))
            echo "  [OK] $(basename "$file")"
        elif [ $result -eq 2 ]; then
            # Cached, don't report
            :
        else
            echo "  [FAIL] $(basename "$file")"
            return 1
        fi

        objects="$objects $object_file"
    done

    if [ $compile_count -eq 0 ]; then
        echo "All files up to date, checking link..."
    fi

    # Link all objects together
    echo "Linking..."

    # Compile runtime if needed
    local runtime_obj="$CACHE_OBJECTS/runtime.o"
    local runtime_hash_file="$CACHE_OBJECTS/runtime.hash"
    local runtime_hash=$(file_hash "$runtime")

    if [ ! -f "$runtime_obj" ] || [ ! -f "$runtime_hash_file" ] || [ "$(cat "$runtime_hash_file" 2>/dev/null)" != "$runtime_hash" ]; then
        if [ $VERBOSE -eq 1 ]; then
            echo "  Compiling runtime..."
        fi
        clang -c -O2 $includes "$runtime" -o "$runtime_obj" 2>&1
        echo "$runtime_hash" > "$runtime_hash_file"
    fi

    # Link everything
    if ! clang -O2 $objects "$runtime_obj" -o "$output" $libs 2>&1; then
        echo "Error: Linking failed"
        return 1
    fi

    local end_time=$(date +%s)
    local elapsed=$((end_time - start_time))

    echo "Build complete (${elapsed}s, $recompiled/$total_files files recompiled)"
    echo "Built: $output"

    return 0
}
