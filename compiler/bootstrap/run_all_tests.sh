#!/bin/bash

# Simplex Bootstrap Compiler Test Suite
# Runs all feature tests and reports combined results

# Get script directory for relative paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

RUNTIME="../../standalone_runtime.c"
COMPILER="python3 stage0.py"
LINK_FLAGS="-lm -lpthread -lsqlite3 -lssl -lcrypto"

# Test files in order
TESTS=(
    "test_basic.sx:Basic Language Features"
    "test_enum_match.sx:Enums and Match"
    "test_closures_loops.sx:Closures and Loops"
    "test_vec_string.sx:Vectors and Strings"
    "test_filesystem.sx:File System"
    "test_bitwise_tuples.sx:Bitwise and Tuples"
    "test_time_threading.sx:Time and Threading"
    "test_net_crypto.sx:Networking and Crypto"
    "test_hashmap.sx:HashMap"
    "test_sb_arena.sx:StringBuilder and Arena"
    "test_string_sysinfo.sx:String Ops and System Info"
    "test_thread_mailbox.sx:Threading and Mailbox"
    "test_json.sx:JSON Operations"
    "test_regex.sx:Regex Operations"
    "test_traits.sx:Trait System"
    "test_async.sx:Async/Await"
    "test_actors.sx:Actor Model"
    "test_cognitive.sx:Cognitive Hive"
    "test_infer.sx:Infer Expression"
    "test_memory.sx:Memory Intrinsics"
    "test_bdi.sx:BDI Goals/Intentions"
    "test_evolution.sx:Evolution Engine"
    "test_swarm.sx:Swarm Communication"
    "test_ai_native.sx:AI Native Intrinsics"
    "test_llm.sx:LLM Integration"
    "test_neural.sx:Neural Network Intrinsics"
    "test_result_option.sx:Result and Option Types"
    "test_hashset.sx:HashSet Operations"
    "test_http.sx:HTTP Client/Server"
    "test_websocket.sx:WebSocket Protocol"
    "test_tls.sx:TLS/SSL Security"
    "test_consensus.sx:Consensus Protocols"
    "test_belief_ext.sx:Belief System Extended"
    "test_bdi_ext.sx:BDI Agent Extended"
    "test_anima.sx:Anima Memory"
    "test_cognitive_ext.sx:Cognitive Actors"
)

echo "========================================"
echo "  Simplex Bootstrap Compiler Test Suite"
echo "========================================"
echo ""

total_passed=0
total_tests=0
failed_suites=""

for entry in "${TESTS[@]}"; do
    file="${entry%%:*}"
    name="${entry##*:}"

    if [ ! -f "$file" ]; then
        echo "SKIP: $name ($file not found)"
        continue
    fi

    echo "----------------------------------------"
    echo "Running: $name"
    echo "----------------------------------------"

    # Compile
    ll_file="${file%.sx}.ll"
    exe_file="${file%.sx}"

    compile_output=$($COMPILER "$file" 2>&1)
    if [ $? -ne 0 ]; then
        echo "COMPILE ERROR: $file"
        echo "$compile_output"
        failed_suites="$failed_suites $name(compile)"
        continue
    fi

    # Link
    link_output=$(clang -O0 "$ll_file" "$RUNTIME" -o "$exe_file" $LINK_FLAGS 2>&1)
    if [ $? -ne 0 ]; then
        echo "LINK ERROR: $file"
        echo "$link_output"
        failed_suites="$failed_suites $name(link)"
        continue
    fi

    # Run and capture output
    output=$(./"$exe_file" 2>&1)
    passed=$?

    echo "$output"
    echo ""

    # Extract passed count from output (assumes "Passed: X / Y" format)
    if [[ "$output" =~ Passed:\ ([0-9]+)\ /\ ([0-9]+) ]]; then
        suite_passed="${BASH_REMATCH[1]}"
        suite_total="${BASH_REMATCH[2]}"
        total_passed=$((total_passed + suite_passed))
        total_tests=$((total_tests + suite_total))

        if [ "$suite_passed" -ne "$suite_total" ]; then
            failed_suites="$failed_suites $name($suite_passed/$suite_total)"
        fi
    elif [[ "$output" =~ "passed" ]] || [[ "$output" =~ "PASS" ]]; then
        # Simple test that just prints success - count as 1 test
        total_passed=$((total_passed + 1))
        total_tests=$((total_tests + 1))
    fi

    # Cleanup
    rm -f "$exe_file"
done

echo ""
echo "========================================"
echo "           FINAL SUMMARY"
echo "========================================"
echo ""
echo "Total Passed: $total_passed / $total_tests"
echo ""

if [ -z "$failed_suites" ]; then
    echo "Status: ALL TESTS PASSED"
    exit 0
else
    echo "Failed suites:$failed_suites"
    exit 1
fi
