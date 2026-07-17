#!/usr/bin/env bash
# test_phase1.sh - phase 1: gateway accepts a single client connection

source "$(dirname "$0")/helpers.sh"

echo "Phase 1: Client Connection"
cd "$PROJECT_ROOT/build" && make -j"$(nproc)" > /dev/null 2>&1

# test1
start_gateway
sleep 0.5
gateway_log=$(cat /tmp/gateway_stdout.log)
assert_contains "$gateway_log" "listening" "gateway prints listening message"

# test2
check_port_open 8080 2
assert_exit_code $? 0 "port 8080 is open and accepting connections"

# test3
sleep 0.3
gateway_log=$(cat /tmp/gateway_stdout.log)
assert_contains "$gateway_log" "127.0.0.1" "gateway logs client IP address"

cleanup

bash "$(dirname "$0")/test_phase0.sh"

print_summary "Phase 1"
