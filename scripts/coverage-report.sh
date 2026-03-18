#!/bin/bash
# coverage-report.sh - Generate code coverage reports for Simplex
#
# This script provides advanced coverage reporting capabilities using
# LLVM source-based coverage instrumentation.
#
# Usage:
#   ./coverage-report.sh [options] <test-dir>
#
# Options:
#   --html          Generate HTML coverage report
#   --json          Generate codecov.io compatible JSON
#   --lcov          Generate LCOV format report
#   --summary       Show coverage summary only
#   --clean         Clean previous coverage data
#   --help          Show this help message
#
# Examples:
#   ./coverage-report.sh tests/               # Run tests and show summary
#   ./coverage-report.sh --html tests/        # Generate HTML report
#   ./coverage-report.sh --json tests/        # Generate codecov.io JSON
#
# Copyright (c) 2025-2026 Rod Higgins
# Licensed under AGPL-3.0 - see LICENSE file
# https://github.com/senuamedia/simplex

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
COMPILER="$PROJECT_ROOT/sxc-compile"
RUNTIME="$PROJECT_ROOT/runtime/standalone_runtime.c"

# Coverage directories
COVERAGE_DIR="$PROJECT_ROOT/coverage"
PROFRAW_DIR="$COVERAGE_DIR/profraw"
HTML_DIR="$COVERAGE_DIR/html"
PROFDATA_FILE="$COVERAGE_DIR/coverage.profdata"

# Output formats
GENERATE_HTML=0
GENERATE_JSON=0
GENERATE_LCOV=0
SUMMARY_ONLY=0
CLEAN_ONLY=0

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# LLVM tools
LLVM_PROFDATA=""
LLVM_COV=""

show_help() {
    echo "Simplex Coverage Report Generator"
    echo ""
    echo "Usage: $0 [options] <test-dir>"
    echo ""
    echo "Options:"
    echo "  --html          Generate HTML coverage report"
    echo "  --json          Generate codecov.io compatible JSON"
    echo "  --lcov          Generate LCOV format report"
    echo "  --summary       Show coverage summary only"
    echo "  --clean         Clean previous coverage data"
    echo "  --help          Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 tests/                     Run tests and show summary"
    echo "  $0 --html tests/              Generate HTML report"
    echo "  $0 --json --html tests/       Generate both JSON and HTML"
    echo "  $0 --clean                    Clean coverage data"
    echo ""
    echo "Output:"
    echo "  coverage/html/index.html      HTML report"
    echo "  coverage/codecov.json         Codecov.io format"
    echo "  coverage/lcov.info            LCOV format"
    echo ""
}

find_llvm_tools() {
    # Try common LLVM installation paths
    local LLVM_PATHS=(
        "/opt/homebrew/opt/llvm/bin"
        "/usr/local/opt/llvm/bin"
        "/usr/lib/llvm-14/bin"
        "/usr/lib/llvm-15/bin"
        "/usr/lib/llvm-16/bin"
        "/usr/lib/llvm-17/bin"
        "/Library/Developer/CommandLineTools/usr/bin"
    )

    # Check PATH first
    if command -v llvm-profdata >/dev/null 2>&1; then
        LLVM_PROFDATA="llvm-profdata"
        LLVM_COV="llvm-cov"
        return 0
    fi

    # Search common paths
    for path in "${LLVM_PATHS[@]}"; do
        if [ -x "$path/llvm-profdata" ]; then
            LLVM_PROFDATA="$path/llvm-profdata"
            LLVM_COV="$path/llvm-cov"
            return 0
        fi
    done

    # Try xcrun on macOS
    if command -v xcrun >/dev/null 2>&1; then
        local xcrun_profdata=$(xcrun -find llvm-profdata 2>/dev/null)
        if [ -n "$xcrun_profdata" ] && [ -x "$xcrun_profdata" ]; then
            LLVM_PROFDATA="$xcrun_profdata"
            LLVM_COV=$(xcrun -find llvm-cov 2>/dev/null)
            return 0
        fi
    fi

    return 1
}

detect_platform() {
    case "$(uname -s)" in
        Darwin*)
            PLATFORM="macos"
            if command -v brew >/dev/null 2>&1; then
                OPENSSL_PREFIX=$(brew --prefix openssl@3 2>/dev/null || brew --prefix openssl 2>/dev/null || echo "")
                if [ -n "$OPENSSL_PREFIX" ]; then
                    LIBS="-lm -L$OPENSSL_PREFIX/lib -lssl -lcrypto -lsqlite3"
                    INCLUDES="-I$OPENSSL_PREFIX/include"
                else
                    LIBS="-lm -lssl -lcrypto -lsqlite3"
                    INCLUDES=""
                fi
            else
                LIBS="-lm -lssl -lcrypto -lsqlite3"
                INCLUDES=""
            fi
            ;;
        Linux*)
            PLATFORM="linux"
            LIBS="-lm -lssl -lcrypto -lsqlite3 -lpthread"
            INCLUDES=""
            ;;
        *)
            PLATFORM="unknown"
            LIBS="-lm"
            INCLUDES=""
            ;;
    esac
}

clean_coverage() {
    echo "Cleaning coverage data..."
    rm -rf "$COVERAGE_DIR"
    echo "Done."
}

run_tests_with_coverage() {
    local TEST_DIR="$1"
    local PASSED=0
    local FAILED=0
    local BINARIES=()

    # Create directories
    mkdir -p "$COVERAGE_DIR" "$PROFRAW_DIR" "$HTML_DIR"

    # Clean old data
    rm -f "$PROFRAW_DIR"/*.profraw
    rm -f "$PROFDATA_FILE"

    echo -e "${CYAN}Running tests with coverage instrumentation...${NC}"
    echo ""

    # Find test files
    local test_files=$(find "$TEST_DIR" -name "*.sx" -type f | sort)

    for test_file in $test_files; do
        local test_name=$(basename "$test_file" .sx)
        local test_dir=$(dirname "$test_file")

        # Skip non-test files
        if [[ "$test_name" == "helpers" ]] || [[ "$test_name" == "spec_mathlib" ]]; then
            continue
        fi

        # Only run test files
        if [[ ! "$test_name" =~ ^(test_|unit_|spec_|integ_|e2e_) ]]; then
            continue
        fi

        local display_name="${test_file#$TEST_DIR/}"
        printf "  %-50s " "$display_name"

        # Compile to LLVM IR
        local ll_file="${test_file%.sx}.ll"
        if ! "$COMPILER" "$test_file" >/dev/null 2>&1; then
            echo -e "${RED}COMPILE FAIL${NC}"
            ((FAILED++))
            continue
        fi

        if [ ! -f "$ll_file" ]; then
            echo -e "${RED}COMPILE FAIL${NC}"
            ((FAILED++))
            continue
        fi

        # Build with coverage instrumentation
        local bin_file="${test_file%.sx}.bin"
        local clang_flags="-fprofile-instr-generate -fcoverage-mapping -O0"
        export LLVM_PROFILE_FILE="$PROFRAW_DIR/${test_name}.profraw"

        if ! clang $clang_flags $INCLUDES "$ll_file" "$RUNTIME" -o "$bin_file" $LIBS 2>/dev/null; then
            echo -e "${RED}LINK FAIL${NC}"
            ((FAILED++))
            rm -f "$ll_file"
            continue
        fi

        BINARIES+=("$bin_file")

        # Run the test
        if "$bin_file" >/dev/null 2>&1; then
            echo -e "${GREEN}PASS${NC}"
            ((PASSED++))
        else
            echo -e "${RED}FAIL${NC}"
            ((FAILED++))
        fi

        # Clean up .ll file
        rm -f "$ll_file"
    done

    echo ""
    echo "=============================================="
    echo -e "  ${GREEN}Passed:${NC}   $PASSED"
    echo -e "  ${RED}Failed:${NC}   $FAILED"
    local total=$((PASSED + FAILED))
    if [ $total -gt 0 ]; then
        local percent=$((PASSED * 100 / total))
        echo "  Total:    $total ($percent% pass rate)"
    fi
    echo "=============================================="
    echo ""

    # Store binaries for coverage report
    echo "${BINARIES[@]}" > "$COVERAGE_DIR/.binaries"
}

merge_coverage_data() {
    echo -e "${CYAN}Merging coverage data...${NC}"

    local profraw_files=$(find "$PROFRAW_DIR" -name "*.profraw" 2>/dev/null)
    if [ -z "$profraw_files" ]; then
        echo -e "${YELLOW}Warning: No coverage data found${NC}"
        return 1
    fi

    "$LLVM_PROFDATA" merge -sparse $profraw_files -o "$PROFDATA_FILE"
    echo "Merged coverage data: $PROFDATA_FILE"
}

generate_summary() {
    echo ""
    echo -e "${CYAN}Coverage Summary:${NC}"
    echo ""

    # Get first binary for report
    local binaries=$(cat "$COVERAGE_DIR/.binaries" 2>/dev/null | tr ' ' '\n' | head -1)
    if [ -z "$binaries" ] || [ ! -f "$binaries" ]; then
        echo -e "${YELLOW}Warning: No test binaries found for coverage report${NC}"
        return 1
    fi

    "$LLVM_COV" report "$binaries" -instr-profile="$PROFDATA_FILE" 2>/dev/null || {
        echo -e "${YELLOW}Warning: Could not generate coverage report${NC}"
        return 1
    }
}

generate_html_report() {
    echo ""
    echo -e "${CYAN}Generating HTML coverage report...${NC}"

    local binaries=$(cat "$COVERAGE_DIR/.binaries" 2>/dev/null | tr ' ' '\n' | head -1)
    if [ -z "$binaries" ] || [ ! -f "$binaries" ]; then
        echo -e "${YELLOW}Warning: No test binaries found${NC}"
        return 1
    fi

    "$LLVM_COV" show "$binaries" \
        -instr-profile="$PROFDATA_FILE" \
        -format=html \
        -output-dir="$HTML_DIR" 2>/dev/null || {
        echo -e "${YELLOW}Warning: Could not generate HTML report${NC}"
        return 1
    }

    echo "HTML report: $HTML_DIR/index.html"
}

generate_json_report() {
    echo ""
    echo -e "${CYAN}Generating codecov.io JSON report...${NC}"

    local binaries=$(cat "$COVERAGE_DIR/.binaries" 2>/dev/null | tr ' ' '\n' | head -1)
    if [ -z "$binaries" ] || [ ! -f "$binaries" ]; then
        echo -e "${YELLOW}Warning: No test binaries found${NC}"
        return 1
    fi

    "$LLVM_COV" export "$binaries" \
        -instr-profile="$PROFDATA_FILE" \
        -format=text > "$COVERAGE_DIR/codecov.json" 2>/dev/null || {
        echo -e "${YELLOW}Warning: Could not generate JSON report${NC}"
        return 1
    }

    echo "Codecov JSON: $COVERAGE_DIR/codecov.json"
}

generate_lcov_report() {
    echo ""
    echo -e "${CYAN}Generating LCOV report...${NC}"

    local binaries=$(cat "$COVERAGE_DIR/.binaries" 2>/dev/null | tr ' ' '\n' | head -1)
    if [ -z "$binaries" ] || [ ! -f "$binaries" ]; then
        echo -e "${YELLOW}Warning: No test binaries found${NC}"
        return 1
    fi

    "$LLVM_COV" export "$binaries" \
        -instr-profile="$PROFDATA_FILE" \
        -format=lcov > "$COVERAGE_DIR/lcov.info" 2>/dev/null || {
        echo -e "${YELLOW}Warning: Could not generate LCOV report${NC}"
        return 1
    }

    echo "LCOV report: $COVERAGE_DIR/lcov.info"
}

cleanup_binaries() {
    local binaries=$(cat "$COVERAGE_DIR/.binaries" 2>/dev/null)
    for bin in $binaries; do
        rm -f "$bin"
    done
    rm -f "$COVERAGE_DIR/.binaries"
}

# Parse arguments
TEST_DIR=""
while [ $# -gt 0 ]; do
    case "$1" in
        --html)
            GENERATE_HTML=1
            ;;
        --json)
            GENERATE_JSON=1
            ;;
        --lcov)
            GENERATE_LCOV=1
            ;;
        --summary)
            SUMMARY_ONLY=1
            ;;
        --clean)
            CLEAN_ONLY=1
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        -*)
            echo "Error: Unknown option $1"
            show_help
            exit 1
            ;;
        *)
            TEST_DIR="$1"
            ;;
    esac
    shift
done

# Handle clean only
if [ $CLEAN_ONLY -eq 1 ]; then
    clean_coverage
    exit 0
fi

# Check for test directory
if [ -z "$TEST_DIR" ]; then
    TEST_DIR="$PROJECT_ROOT/tests"
fi

if [ ! -d "$TEST_DIR" ]; then
    echo "Error: Test directory not found: $TEST_DIR"
    exit 1
fi

# Check for LLVM tools
if ! find_llvm_tools; then
    echo -e "${RED}Error: LLVM coverage tools not found${NC}"
    echo ""
    echo "Install LLVM to enable coverage:"
    echo "  macOS:   brew install llvm"
    echo "  Ubuntu:  sudo apt install llvm"
    echo ""
    echo "Or add LLVM to your PATH:"
    echo "  export PATH=\"/opt/homebrew/opt/llvm/bin:\$PATH\""
    exit 1
fi

echo "Using LLVM tools:"
echo "  llvm-profdata: $LLVM_PROFDATA"
echo "  llvm-cov:      $LLVM_COV"
echo ""

# Check for compiler and runtime
if [ ! -f "$COMPILER" ]; then
    # Try alternate location
    COMPILER="$PROJECT_ROOT/build/sxc"
    if [ ! -f "$COMPILER" ]; then
        echo "Error: Compiler not found"
        exit 1
    fi
fi

if [ ! -f "$RUNTIME" ]; then
    RUNTIME="$PROJECT_ROOT/standalone_runtime.c"
    if [ ! -f "$RUNTIME" ]; then
        echo "Error: Runtime not found"
        exit 1
    fi
fi

# Detect platform
detect_platform

# Run tests with coverage
run_tests_with_coverage "$TEST_DIR"

# Merge coverage data
if ! merge_coverage_data; then
    cleanup_binaries
    exit 1
fi

# Generate reports
if [ $SUMMARY_ONLY -eq 0 ] || [ $GENERATE_HTML -eq 0 ] && [ $GENERATE_JSON -eq 0 ] && [ $GENERATE_LCOV -eq 0 ]; then
    generate_summary
fi

if [ $GENERATE_HTML -eq 1 ]; then
    generate_html_report
fi

if [ $GENERATE_JSON -eq 1 ]; then
    generate_json_report
fi

if [ $GENERATE_LCOV -eq 1 ]; then
    generate_lcov_report
fi

# Cleanup
cleanup_binaries

echo ""
echo -e "${GREEN}Coverage report complete!${NC}"
