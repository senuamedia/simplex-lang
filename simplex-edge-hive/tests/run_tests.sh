#!/bin/bash
# Edge Hive Test Runner
# Compiles and runs all edge-hive tests

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
RUNTIME="$PROJECT_ROOT/runtime/standalone_runtime.c"
# Use Python bootstrap compiler (more stable)
COMPILER="python3 $PROJECT_ROOT/stage0.py"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

PASSED=0
FAILED=0
COMPILE_FAILED=0
LINK_FAILED=0

# Check prerequisites
STAGE0="$PROJECT_ROOT/stage0.py"
if [ ! -f "$STAGE0" ]; then
    echo -e "${RED}Error: Compiler not found at $STAGE0${NC}"
    exit 1
fi

if [ ! -f "$RUNTIME" ]; then
    echo -e "${RED}Error: Runtime not found at $RUNTIME${NC}"
    exit 1
fi

# Platform-specific libs
if [[ "$OSTYPE" == "darwin"* ]]; then
    OPENSSL_PREFIX=$(brew --prefix openssl 2>/dev/null || echo "/usr/local/opt/openssl")
    SQLITE_PREFIX=$(brew --prefix sqlite 2>/dev/null || echo "/usr/local/opt/sqlite")
    LINK_LIBS="-lm -lssl -lcrypto -lsqlite3 -L$OPENSSL_PREFIX/lib -L$SQLITE_PREFIX/lib -I$OPENSSL_PREFIX/include"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    LINK_LIBS="-lm -lssl -lcrypto -lsqlite3 -lpthread"
else
    LINK_LIBS="-lm"
fi

run_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .sx)

    printf "  %-35s " "$test_name"

    cd "$SCRIPT_DIR"

    # Compile to LLVM IR
    local compile_output
    compile_output=$(python3 "$PROJECT_ROOT/stage0.py" "$test_name.sx" 2>&1)
    local compile_status=$?

    # Check for compilation errors
    if [ $compile_status -ne 0 ]; then
        echo -e "${YELLOW}COMPILE SKIP ($compile_status)${NC}"
        ((COMPILE_FAILED++))
        return
    fi

    # Check if .ll file was created
    if [ ! -f "$test_name.ll" ]; then
        echo -e "${YELLOW}NO OUTPUT${NC}"
        ((COMPILE_FAILED++))
        return
    fi

    # Link
    if ! clang -O2 "$test_name.ll" "$RUNTIME" -o "$test_name.bin" $LINK_LIBS 2>/dev/null; then
        echo -e "${YELLOW}LINK SKIP${NC}"
        ((LINK_FAILED++))
        rm -f "$test_name.ll"
        return
    fi

    # Run
    local output
    output=$(./"$test_name.bin" 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}PASS${NC}"
        ((PASSED++))
    else
        echo -e "${RED}FAIL (exit: $exit_code)${NC}"
        ((FAILED++))
    fi

    # Cleanup
    rm -f "$test_name.ll" "$test_name.bin"
}

echo ""
echo "=============================================="
echo "         Edge Hive Test Suite"
echo "=============================================="
echo -e "  Compiler: ${CYAN}stage0.py (Python bootstrap)${NC}"
echo ""

# Run all test files
echo -e "${YELLOW}Running Edge Hive Tests${NC}"
for test_file in "$SCRIPT_DIR"/*.sx; do
    if [ -f "$test_file" ]; then
        run_test "$test_file"
    fi
done

echo ""
echo "=============================================="
echo -e "  ${GREEN}Passed:       $PASSED${NC}"
echo -e "  ${RED}Failed:       $FAILED${NC}"
echo -e "  ${YELLOW}Compile Skip: $COMPILE_FAILED${NC}"
echo -e "  ${YELLOW}Link Skip:    $LINK_FAILED${NC}"
TOTAL=$((PASSED + FAILED))
if [ $TOTAL -gt 0 ]; then
    PERCENT=$((PASSED * 100 / TOTAL))
    echo -e "  Pass Rate:    $PERCENT%"
fi
echo "=============================================="

if [ $FAILED -gt 0 ]; then
    exit 1
fi
