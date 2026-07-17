#!/usr/bin/env bash
# runs all phase tests in order
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
echo "Network Gateway - Full Test Suite"
echo ""

for test_script in "$SCRIPT_DIR"/test_phase*.sh; do
    [ -f "$test_script" ] || continue
    echo "Running $(basename "$test_script")..."
    bash "$test_script"
    echo ""
done

echo "ALL PHASES PASSED"
