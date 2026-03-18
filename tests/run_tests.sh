#!/bin/bash
# Simplex Test Runner
# Runs all tests in the test suite with support for filtering by category and type
#
# Usage: ./run_tests.sh [category] [type]
#
# Categories:
#   all, language, types, neural, stdlib, runtime, ai, integration,
#   toolchain, basics, async, actors, learning, quantum, observability,
#   training, contracts, math, fuzz, properties, safety, formal
#
# Types (based on naming convention):
#   all   - Run all test types
#   unit  - Run unit_*.sx tests (isolated function/module tests)
#   spec  - Run spec_*.sx tests (language specification tests)
#   integ - Run integ_*.sx tests (integration tests)
#   e2e   - Run e2e_*.sx tests (end-to-end workflow tests)
#
# Examples:
#   ./run_tests.sh                    # Run all tests
#   ./run_tests.sh neural             # Run only neural IR tests
#   ./run_tests.sh stdlib unit        # Run only stdlib unit tests
#   ./run_tests.sh all spec           # Run all spec tests across categories
#   ./run_tests.sh language           # Run all language tests

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
RUNTIME="$PROJECT_ROOT/runtime/standalone_runtime.c"
# Use compiled sxc instead of Python bootstrap
COMPILER="$PROJECT_ROOT/build/sxc"
# Fallback to stage0.py if sxc not built
if [ ! -x "$COMPILER" ]; then
    COMPILER="$PROJECT_ROOT/stage0.py"
    USE_PYTHON=1
fi

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color

PASSED=0
FAILED=0
SKIPPED=0
WARNINGS=0

# Test type filter (unit, spec, integ, e2e, or all)
TEST_TYPE="${2:-all}"

# Check if compiler exists
if [ ! -f "$COMPILER" ]; then
    echo -e "${RED}Error: Compiler not found at $COMPILER${NC}"
    exit 1
fi

if [ ! -f "$RUNTIME" ]; then
    echo -e "${RED}Error: Runtime not found at $RUNTIME${NC}"
    exit 1
fi

# Check if test matches the type filter
matches_type_filter() {
    local test_name="$1"

    if [ "$TEST_TYPE" = "all" ]; then
        return 0
    fi

    case "$TEST_TYPE" in
        unit)
            [[ "$test_name" == unit_* ]] && return 0
            ;;
        spec)
            [[ "$test_name" == spec_* ]] && return 0
            ;;
        integ)
            [[ "$test_name" == integ_* ]] && return 0
            ;;
        e2e)
            [[ "$test_name" == e2e_* ]] && return 0
            ;;
    esac

    return 1
}

# Get test type label with color
get_type_label() {
    local test_name="$1"

    if [[ "$test_name" == unit_* ]]; then
        echo -e "${BLUE}[unit]${NC}"
    elif [[ "$test_name" == spec_* ]]; then
        echo -e "${CYAN}[spec]${NC}"
    elif [[ "$test_name" == integ_* ]]; then
        echo -e "${MAGENTA}[integ]${NC}"
    elif [[ "$test_name" == e2e_* ]]; then
        echo -e "${YELLOW}[e2e]${NC}"
    else
        echo "[test]"
    fi
}

run_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .sx)
    local test_dir=$(dirname "$test_file")
    local display_name="${test_dir#$SCRIPT_DIR/}/$test_name"

    # Skip library/helper files
    if [[ "$test_name" == "spec_mathlib" ]] || [[ "$test_name" == "mathlib" ]] || [[ "$test_name" == "helpers" ]]; then
        return
    fi

    # Check type filter
    if ! matches_type_filter "$test_name"; then
        return
    fi

    local type_label=$(get_type_label "$test_name")
    printf "    %-45s %s " "$display_name" "$type_label"

    local orig_dir=$(pwd)
    cd "$test_dir"

    # Pre-compile module dependencies BEFORE main test (needed for declare extraction)
    local LL_FILES=""
    for module in $(grep -E "^use [a-z_]+;" "$test_name.sx" 2>/dev/null | sed 's/use \([a-z_]*\);/\1/' || true); do
        if [ -f "${module}.sx" ]; then
            "$COMPILER" "${module}.sx" >/dev/null 2>&1
            if [ -f "${module}.ll" ]; then
                LL_FILES="$LL_FILES ${module}.ll"
            fi
        fi
    done

    # Compile main test file (after modules so it can read their .ll for declarations)
    rm -f "$test_name.ll"
    local compile_output
    if [ -n "$USE_PYTHON" ]; then
        compile_output=$(python3 "$COMPILER" "$test_name.sx" 2>&1)
    else
        compile_output=$("$COMPILER" "$test_name.sx" 2>&1)
    fi
    local compile_status=$?

    # Check for static analysis warnings
    if echo "$compile_output" | grep -q "Warning:"; then
        ((WARNINGS++))
    fi

    if [ $compile_status -ne 0 ] || [ ! -f "$test_name.ll" ]; then
        echo -e "${RED}COMPILE FAIL${NC}"
        ((FAILED++))
        cd "$orig_dir"
        return
    fi

    # Link with platform-specific libs
    local LINK_LIBS="-lm"
    if [[ "$OSTYPE" == "darwin"* ]]; then
        OPENSSL_PREFIX=$(brew --prefix openssl 2>/dev/null || echo "/usr/local/opt/openssl")
        SQLITE_PREFIX=$(brew --prefix sqlite 2>/dev/null || echo "/usr/local/opt/sqlite")
        LINK_LIBS="-lm -lssl -lcrypto -L$OPENSSL_PREFIX/lib -L$SQLITE_PREFIX/lib"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        LINK_LIBS="-lm -lssl -lcrypto -lpthread"
    fi

    # Collect all .ll files to link
    LL_FILES="$test_name.ll $LL_FILES"
    if ! clang -O2 $LL_FILES "$RUNTIME" -o "$test_name.bin" $LINK_LIBS 2>/dev/null; then
        echo -e "${RED}LINK FAIL${NC}"
        ((FAILED++))
        rm -f "$test_name.ll"
        cd "$orig_dir"
        return
    fi

    # Run
    if ./"$test_name.bin" >/dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        ((PASSED++))
    else
        echo -e "${RED}FAIL${NC}"
        ((FAILED++))
    fi

    # Cleanup
    rm -f "$test_name.ll" "$test_name.bin"
    for module in $(grep -E "^use [a-z_]+;" "$test_name.sx" 2>/dev/null | sed 's/use \([a-z_]*\);/\1/' || true); do
        rm -f "${module}.ll"
    done
    cd "$orig_dir"
}

run_category() {
    local category_path="$1"
    local category_name="$2"
    local indent="${3:-  }"

    if [ -d "$category_path" ]; then
        local has_tests=false

        # Check for .sx files matching filter (excluding helpers)
        shopt -s nullglob
        for test in "$category_path"/*.sx; do
            if [ -f "$test" ]; then
                local name=$(basename "$test" .sx)
                if [[ "$name" != "spec_mathlib" ]] && [[ "$name" != "mathlib" ]] && [[ "$name" != "helpers" ]]; then
                    if matches_type_filter "$name"; then
                        has_tests=true
                        break
                    fi
                fi
            fi
        done

        if $has_tests; then
            echo -e "${indent}${CYAN}$category_name${NC}"
            for test in "$category_path"/*.sx; do
                [ -f "$test" ] && run_test "$test"
            done
        fi

        # Recurse into subdirectories
        for subdir in "$category_path"/*/; do
            if [ -d "$subdir" ]; then
                local subname=$(basename "$subdir")
                run_category "$subdir" "$subname" "${indent}  "
            fi
        done
    fi
}

print_header() {
    echo ""
    echo "=============================================="
    echo "         Simplex Language Test Suite"
    echo "=============================================="
    if [ -n "$USE_PYTHON" ]; then
        echo -e "  Compiler: ${YELLOW}stage0.py (Python bootstrap)${NC}"
    else
        echo -e "  Compiler: ${GREEN}sxc v0.14.0 (self-hosted)${NC}"
    fi
    if [ "$TEST_TYPE" != "all" ]; then
        echo -e "  Filter: ${CYAN}$TEST_TYPE${NC} tests only"
    fi
    echo ""
}

print_summary() {
    echo ""
    echo "=============================================="
    echo -e "  ${GREEN}Passed:   $PASSED${NC}"
    echo -e "  ${RED}Failed:   $FAILED${NC}"
    if [ $WARNINGS -gt 0 ]; then
        echo -e "  ${YELLOW}Warnings: $WARNINGS${NC}"
    fi
    TOTAL=$((PASSED + FAILED))
    if [ $TOTAL -gt 0 ]; then
        PERCENT=$((PASSED * 100 / TOTAL))
        echo -e "  Total:    $TOTAL ($PERCENT% pass rate)"
    fi
    echo ""
    echo "  Test Types:"
    echo -e "    ${BLUE}unit${NC}  - Isolated function/module tests"
    echo -e "    ${CYAN}spec${NC}  - Language specification tests"
    echo -e "    ${MAGENTA}integ${NC} - Integration tests"
    echo -e "    ${YELLOW}e2e${NC}   - End-to-end workflow tests"
    echo "=============================================="
}

print_usage() {
    echo "Usage: ./run_tests.sh [category] [type]"
    echo ""
    echo "Categories:"
    echo "  all          Run all categories"
    echo "  language     Core language feature tests"
    echo "  types        Type system tests"
    echo "  basics       Basic language construct tests"
    echo "  async        Async/await tests"
    echo "  actors       Actor model tests"
    echo "  neural       Neural IR and gates tests"
    echo "  stdlib       Standard library tests"
    echo "  runtime      Runtime system tests"
    echo "  ai           AI/cognitive framework tests"
    echo "  learning     Automatic differentiation tests"
    echo "  quantum      Quantum computing tests"
    echo "  toolchain    Compiler toolchain tests"
    echo "  integration  End-to-end integration tests"
    echo "  observability Metrics and tracing tests"
    echo ""
    echo "Types (filter by naming convention):"
    echo "  all   - Run all test types (default)"
    echo "  unit  - Run unit_*.sx tests only"
    echo "  spec  - Run spec_*.sx tests only"
    echo "  integ - Run integ_*.sx tests only"
    echo "  e2e   - Run e2e_*.sx tests only"
    echo ""
    echo "Examples:"
    echo "  ./run_tests.sh                    # All tests"
    echo "  ./run_tests.sh stdlib             # All stdlib tests"
    echo "  ./run_tests.sh stdlib unit        # Only stdlib unit tests"
    echo "  ./run_tests.sh all spec           # All spec tests"
    echo "  ./run_tests.sh neural spec        # Neural spec tests"
}

run_all_tests() {
    # Language Tests
    echo -e "${YELLOW}Language${NC}"
    run_category "$SCRIPT_DIR/language" "" "  "
    echo ""

    # Type System Tests
    echo -e "${YELLOW}Types${NC}"
    run_category "$SCRIPT_DIR/types" "" "  "
    echo ""

    # Basics Tests
    echo -e "${YELLOW}Basics${NC}"
    run_category "$SCRIPT_DIR/basics" "" "  "
    echo ""

    # Async Tests
    echo -e "${YELLOW}Async${NC}"
    run_category "$SCRIPT_DIR/async" "" "  "
    echo ""

    # Actor Tests
    echo -e "${YELLOW}Actors${NC}"
    run_category "$SCRIPT_DIR/actors" "" "  "
    echo ""

    # Neural IR Tests
    echo -e "${YELLOW}Neural IR${NC}"
    run_category "$SCRIPT_DIR/neural" "" "  "
    echo ""

    # Standard Library Tests
    echo -e "${YELLOW}Standard Library${NC}"
    run_category "$SCRIPT_DIR/stdlib" "" "  "
    echo ""

    # Runtime Tests
    echo -e "${YELLOW}Runtime${NC}"
    run_category "$SCRIPT_DIR/runtime" "" "  "
    echo ""

    # AI/Cognitive Tests
    echo -e "${YELLOW}AI / Cognitive${NC}"
    run_category "$SCRIPT_DIR/ai" "" "  "
    echo ""

    # Learning Tests
    echo -e "${YELLOW}Learning / AD${NC}"
    run_category "$SCRIPT_DIR/learning" "" "  "
    echo ""

    # Toolchain Tests
    echo -e "${YELLOW}Toolchain${NC}"
    run_category "$SCRIPT_DIR/toolchain" "" "  "
    echo ""

    # Observability Tests
    echo -e "${YELLOW}Observability${NC}"
    run_category "$SCRIPT_DIR/observability" "" "  "
    echo ""

    # Quantum Tests
    echo -e "${YELLOW}Quantum${NC}"
    run_category "$SCRIPT_DIR/quantum" "" "  "
    echo ""

    # Integration Tests
    echo -e "${YELLOW}Integration${NC}"
    run_category "$SCRIPT_DIR/integration" "" "  "
    echo ""

    # Contracts Tests
    echo -e "${YELLOW}Contracts${NC}"
    run_category "$SCRIPT_DIR/contracts" "" "  "
    echo ""

    # Math Tests
    echo -e "${YELLOW}Math${NC}"
    run_category "$SCRIPT_DIR/math" "" "  "
    echo ""

    # Fuzz Tests
    echo -e "${YELLOW}Fuzz Testing${NC}"
    run_category "$SCRIPT_DIR/fuzz" "" "  "
    echo ""

    # Property-Based Tests
    echo -e "${YELLOW}Property-Based Tests${NC}"
    run_category "$SCRIPT_DIR/properties" "" "  "
    echo ""

    # Safety Tests
    echo -e "${YELLOW}Safety / Sanitizer Tests${NC}"
    run_category "$SCRIPT_DIR/safety" "" "  "
    echo ""

    # Formal Invariant Tests
    echo -e "${YELLOW}Formal Invariants${NC}"
    run_category "$SCRIPT_DIR/formal" "" "  "
    echo ""
}

# Validate test type
validate_type() {
    case "$TEST_TYPE" in
        all|unit|spec|integ|e2e)
            return 0
            ;;
        *)
            echo -e "${RED}Unknown test type: $TEST_TYPE${NC}"
            echo "Available types: all, unit, spec, integ, e2e"
            exit 1
            ;;
    esac
}

# Main execution
CATEGORY="${1:-all}"

# Handle help
if [ "$CATEGORY" = "-h" ] || [ "$CATEGORY" = "--help" ] || [ "$CATEGORY" = "help" ]; then
    print_usage
    exit 0
fi

# Single file mode
if [ "$CATEGORY" = "file" ] || [ "$CATEGORY" = "f" ]; then
    TEST_TYPE="all"
    shift
    print_header
    for target in "$@"; do
        if [[ "$target" == *.sx ]]; then test_file="$target"; else test_file="${target}.sx"; fi
        if [ -f "$SCRIPT_DIR/$test_file" ]; then run_test "$SCRIPT_DIR/$test_file"
        elif [ -f "$test_file" ]; then run_test "$test_file"
        else echo -e "${RED}Not found: $test_file${NC}"; ((FAILED++)); fi
    done
    print_summary
    if [ $FAILED -gt 0 ]; then exit 1; fi
    exit 0
fi

validate_type
print_header

case "$CATEGORY" in
    all)
        run_all_tests
        ;;
    language)
        echo -e "${YELLOW}Language${NC}"
        run_category "$SCRIPT_DIR/language" "" "  "
        ;;
    types)
        echo -e "${YELLOW}Types${NC}"
        run_category "$SCRIPT_DIR/types" "" "  "
        ;;
    basics)
        echo -e "${YELLOW}Basics${NC}"
        run_category "$SCRIPT_DIR/basics" "" "  "
        ;;
    async)
        echo -e "${YELLOW}Async${NC}"
        run_category "$SCRIPT_DIR/async" "" "  "
        ;;
    actors)
        echo -e "${YELLOW}Actors${NC}"
        run_category "$SCRIPT_DIR/actors" "" "  "
        ;;
    neural)
        echo -e "${YELLOW}Neural IR${NC}"
        run_category "$SCRIPT_DIR/neural" "" "  "
        ;;
    stdlib)
        echo -e "${YELLOW}Standard Library${NC}"
        run_category "$SCRIPT_DIR/stdlib" "" "  "
        ;;
    runtime)
        echo -e "${YELLOW}Runtime${NC}"
        run_category "$SCRIPT_DIR/runtime" "" "  "
        ;;
    ai)
        echo -e "${YELLOW}AI / Cognitive${NC}"
        run_category "$SCRIPT_DIR/ai" "" "  "
        ;;
    learning)
        echo -e "${YELLOW}Learning / AD${NC}"
        run_category "$SCRIPT_DIR/learning" "" "  "
        ;;
    toolchain)
        echo -e "${YELLOW}Toolchain${NC}"
        run_category "$SCRIPT_DIR/toolchain" "" "  "
        ;;
    integration)
        echo -e "${YELLOW}Integration${NC}"
        run_category "$SCRIPT_DIR/integration" "" "  "
        ;;
    quantum)
        echo -e "${YELLOW}Quantum${NC}"
        run_category "$SCRIPT_DIR/quantum" "" "  "
        ;;
    observability)
        echo -e "${YELLOW}Observability${NC}"
        run_category "$SCRIPT_DIR/observability" "" "  "
        ;;
    training)
        echo -e "${YELLOW}Training${NC}"
        run_category "$SCRIPT_DIR/training" "" "  "
        ;;
    contracts)
        echo -e "${YELLOW}Contracts${NC}"
        run_category "$SCRIPT_DIR/contracts" "" "  "
        ;;
    math)
        echo -e "${YELLOW}Math${NC}"
        run_category "$SCRIPT_DIR/math" "" "  "
        ;;
    fuzz)
        echo -e "${YELLOW}Fuzz Testing${NC}"
        run_category "$SCRIPT_DIR/fuzz" "" "  "
        ;;
    properties)
        echo -e "${YELLOW}Property-Based Tests${NC}"
        run_category "$SCRIPT_DIR/properties" "" "  "
        ;;
    safety)
        echo -e "${YELLOW}Safety / Sanitizer Tests${NC}"
        run_category "$SCRIPT_DIR/safety" "" "  "
        ;;
    formal)
        echo -e "${YELLOW}Formal Invariants${NC}"
        run_category "$SCRIPT_DIR/formal" "" "  "
        ;;
    *)
        echo -e "${RED}Unknown category: $CATEGORY${NC}"
        echo ""
        print_usage
        exit 1
        ;;
esac

print_summary

if [ $FAILED -gt 0 ]; then
    exit 1
fi
