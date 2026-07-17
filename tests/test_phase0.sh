#!/usr/bin/env bash
source "$(dirname "$0")/helpers.sh"
echo "Phase 0: Build Verification"

# build the project
echo "  Building project..."
cd "$PROJECT_ROOT"
if mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug > build.log 2>&1 && make -j"$(nproc)" >> build.log 2>&1; then
    assert_exit_code 0 0 "Project builds successfully"
else
    assert_exit_code 1 0 "Project builds successfully"
    echo "    Build logs:"
    cat build.log
fi

# check binary exists
if [ -f "$GATEWAY_BIN" ]; then
    assert_exit_code 0 0 "Gateway binary exists"
else
    assert_exit_code 1 0 "Gateway binary exists"
fi

# run and check output
output=$("$GATEWAY_BIN" 2>&1 || true)
assert_contains "$output" "Gateway" "Gateway prints startup message"

print_summary "Phase 0"
