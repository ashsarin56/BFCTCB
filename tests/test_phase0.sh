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

echo ""
echo "Commit 4.1: Router class unit tests"

ROUTER_TEST_BIN="/tmp/router_test"

echo "  Compiling router class..."
g++ -std=c++17 -Wall -Wextra \
    -I"$PROJECT_ROOT/src" \
    "$PROJECT_ROOT/src/routing/router.cpp" \
    "$PROJECT_ROOT/tests/router_test_main.cpp" \
    -o "$ROUTER_TEST_BIN" 2>/tmp/router_compile_errors.txt
assert_exit_code $? 0 "router.cpp compiles cleanly"

if [ -s /tmp/router_compile_errors.txt ]; then
    echo "  Compiler output:"
    cat /tmp/router_compile_errors.txt
fi

router_output=$("$ROUTER_TEST_BIN" 2>&1) || true

assert_contains "$router_output" "RESOLVE_KNOWN:OK"   "resolve() finds a registered port"
assert_contains "$router_output" "RESOLVE_HOST:OK"    "resolve() returns the correct backend host"
assert_contains "$router_output" "RESOLVE_NAME:OK"    "resolve() returns the correct service name"
assert_contains "$router_output" "RESOLVE_UNKNOWN:OK" "resolve() returns nullptr for an unregistered port"
assert_contains "$router_output" "ROUTE_COUNT:OK"     "get_routes() returns the correct number of entries"
assert_contains "$router_output" "OVERWRITE:OK"       "add_route() overwrites an existing entry for the same port"

echo ""
echo "Commit 5.1: yaml-cpp dependency"

YAML_TEST_SRC="/tmp/yaml_link_test.cpp"
YAML_TEST_BIN="/tmp/yaml_link_test"

cat > "$YAML_TEST_SRC" << 'EOF'
// Minimal smoke test: verify yaml-cpp headers are reachable and the library links.
#include <yaml-cpp/yaml.h>
#include <cstdio>

int main() {
    YAML::Node node = YAML::Load("key: value");
    if (node["key"].as<std::string>() == "value") {
        std::puts("YAML_LINK:OK");
        return 0;
    }
    std::puts("YAML_LINK:FAIL");
    return 1;
}
EOF

YAML_CPP_INCLUDE="$PROJECT_ROOT/build/_deps/yaml-cpp-src/include"
# The library name gets a 'd' suffix in Debug builds (libyaml-cppd.a).
YAML_CPP_LIB=$(ls "$PROJECT_ROOT/build/_deps/yaml-cpp-build/libyaml-cpp"*.a 2>/dev/null | head -1)

if [ -d "$YAML_CPP_INCLUDE" ] && [ -n "$YAML_CPP_LIB" ] && [ -f "$YAML_CPP_LIB" ]; then
    g++ -std=c++17 \
        -I"$YAML_CPP_INCLUDE" \
        "$YAML_TEST_SRC" \
        "$YAML_CPP_LIB" \
        -o "$YAML_TEST_BIN" 2>/tmp/yaml_compile_errors.txt
    assert_exit_code $? 0 "yaml-cpp smoke test compiles"

    if [ -s /tmp/yaml_compile_errors.txt ]; then
        echo "  Compiler output:"
        cat /tmp/yaml_compile_errors.txt
    fi

    yaml_output=$("$YAML_TEST_BIN" 2>&1) || true
    assert_contains "$yaml_output" "YAML_LINK:OK" "yaml-cpp parses a simple YAML node correctly"
else
    assert_exit_code 1 0 "yaml-cpp headers and static library found in build/_deps"
    echo "    Note: run cmake first so FetchContent downloads yaml-cpp"
fi

# Verify yaml-cpp static library exists in the build tree.
# At Commit 5.1 main.cpp does not include yaml-cpp, so no yaml symbols appear in the
# gateway binary — checking the archive directly is the correct proof of linkage.
if [ -n "$YAML_CPP_LIB" ] && [ -f "$YAML_CPP_LIB" ]; then
    assert_exit_code 0 0 "yaml-cpp static library built at build/_deps/yaml-cpp-build"
else
    assert_exit_code 1 0 "yaml-cpp static library built at build/_deps/yaml-cpp-build"
fi

print_summary "Phase 0"