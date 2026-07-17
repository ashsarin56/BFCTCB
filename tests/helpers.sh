#!/usr/bin/env bash
# shared test helpers

set -euo pipefail

PASS_COUNT=0
FAIL_COUNT=0
GATEWAY_PID=""
BACKEND_PIDS=()
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GATEWAY_BIN="$PROJECT_ROOT/build/gateway"

start_backend() {
    local port=$1
    python3 "$PROJECT_ROOT/tests/echo_backend.py" "$port" &
    local pid=$!
    BACKEND_PIDS+=("$pid")
    sleep 0.3
    echo "  [backend] started on port $port (PID $pid)"
}

start_gateway() {
    "$GATEWAY_BIN" "$@" > /tmp/gateway_stdout.log 2>&1 &
    GATEWAY_PID=$!
    sleep 0.5
    echo "  [gateway] started (PID $GATEWAY_PID)"
}

send_and_receive() {
    local host=$1 port=$2 message=$3 timeout=${4:-2}
    echo "$message" | timeout "$timeout" nc -q 1 "$host" "$port" 2>/dev/null || true
}

send_data() {
    local host=$1 port=$2 message=$3 timeout=${4:-2}
    echo "$message" | timeout "$timeout" nc -q 0 "$host" "$port" 2>/dev/null || true
}

check_port_open() {
    local port=$1 timeout=${2:-2}
    timeout "$timeout" bash -c "echo '' | nc -q 0 localhost $port" 2>/dev/null
}

assert_contains() {
    local actual="$1" expected="$2" test_name="$3"
    if echo "$actual" | grep -qF "$expected"; then
        echo "  PASS: $test_name"
        ((PASS_COUNT++)) || true
    else
        echo "  FAIL: $test_name"
        echo "    expected to contain: '$expected'"
        echo "    actual: '$actual'"
        ((FAIL_COUNT++)) || true
    fi
}

assert_not_contains() {
    local actual="$1" unexpected="$2" test_name="$3"
    if echo "$actual" | grep -qF "$unexpected"; then
        echo "  FAIL: $test_name"
        echo "    should not contain: '$unexpected'"
        ((FAIL_COUNT++)) || true
    else
        echo "  PASS: $test_name"
        ((PASS_COUNT++)) || true
    fi
}

assert_exit_code() {
    local actual=$1 expected=$2 test_name="$3"
    if [ "$actual" -eq "$expected" ]; then
        echo "  PASS: $test_name"
        ((PASS_COUNT++)) || true
    else
        echo "  FAIL: $test_name (got $actual, expected $expected)"
        ((FAIL_COUNT++)) || true
    fi
}

cleanup() {
    if [ -n "$GATEWAY_PID" ] && kill -0 "$GATEWAY_PID" 2>/dev/null; then
        kill "$GATEWAY_PID" 2>/dev/null || true
        wait "$GATEWAY_PID" 2>/dev/null || true
    fi
    for pid in "${BACKEND_PIDS[@]}"; do
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
    done
    GATEWAY_PID=""
    BACKEND_PIDS=()
}

print_summary() {
    local phase_name=$1
    echo ""
    echo "$phase_name Summary"
    echo "  Passed: $PASS_COUNT"
    echo "  Failed: $FAIL_COUNT"
    if [ "$FAIL_COUNT" -gt 0 ]; then
        exit 1
    else
        echo "  All tests passed!"
    fi
}

trap cleanup EXIT
