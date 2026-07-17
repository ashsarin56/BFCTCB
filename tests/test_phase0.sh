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

# run and check output — gateway now blocks on accept(), use timeout to let it print then kill it
output=$(timeout 2 "$GATEWAY_BIN" 2>&1 || true)
assert_contains "$output" "listening" "Gateway prints startup message"

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
if [ -n "$YAML_CPP_LIB" ] && [ -f "$YAML_CPP_LIB" ]; then
    assert_exit_code 0 0 "yaml-cpp static library built at build/_deps/yaml-cpp-build"
else
    assert_exit_code 1 0 "yaml-cpp static library built at build/_deps/yaml-cpp-build"
fi

echo ""
echo "Commit 5.2: GatewayConfig types"

CONFIG_TYPES_TEST_SRC="/tmp/config_types_test.cpp"
CONFIG_TYPES_TEST_BIN="/tmp/config_types_test"

cat > "$CONFIG_TYPES_TEST_SRC" << 'EOF'
#include "config/config_types.h"
#include <cstdio>

int main() {
    BackendTarget bt{"127.0.0.1", 3001};
    if (bt.host == "127.0.0.1" && bt.port == 3001) {
        std::puts("BACKEND_TARGET:OK");
    } else {
        std::puts("BACKEND_TARGET:FAIL");
    }

    ServiceConfig sc{"web", 8080, bt};
    if (sc.name == "web" && sc.listen_port == 8080 && sc.backend.port == 3001) {
        std::puts("SERVICE_CONFIG:OK");
    } else {
        std::puts("SERVICE_CONFIG:FAIL");
    }

    GatewayConfig gc;
    gc.services.push_back(sc);
    if (gc.services.size() == 1 && gc.services[0].name == "web") {
        std::puts("GATEWAY_CONFIG:OK");
    } else {
        std::puts("GATEWAY_CONFIG:FAIL");
    }

    return 0;
}
EOF

g++ -std=c++17 -Wall -Wextra \
    -I"$PROJECT_ROOT/src" \
    "$CONFIG_TYPES_TEST_SRC" \
    -o "$CONFIG_TYPES_TEST_BIN" 2>/tmp/config_types_compile_errors.txt
assert_exit_code $? 0 "config_types.h compiles cleanly"

if [ -s /tmp/config_types_compile_errors.txt ]; then
    echo "  Compiler output:"
    cat /tmp/config_types_compile_errors.txt
fi

config_types_output=$("$CONFIG_TYPES_TEST_BIN" 2>&1) || true

assert_contains "$config_types_output" "BACKEND_TARGET:OK" "BackendTarget struct initialises correctly"
assert_contains "$config_types_output" "SERVICE_CONFIG:OK" "ServiceConfig struct initialises correctly"
assert_contains "$config_types_output" "GATEWAY_CONFIG:OK" "GatewayConfig vector stores ServiceConfig entries"

echo ""
echo "Commit 5.3: config parser — parse gateway.yaml"

CONFIG_PARSER_TEST_SRC="/tmp/config_parser_test.cpp"
CONFIG_PARSER_TEST_BIN="/tmp/config_parser_test"

YAML_CPP_INCLUDE="$PROJECT_ROOT/build/_deps/yaml-cpp-src/include"
YAML_CPP_LIB=$(ls "$PROJECT_ROOT/build/_deps/yaml-cpp-build/libyaml-cpp"*.a 2>/dev/null | head -1)

cat > "$CONFIG_PARSER_TEST_SRC" << 'EOF'
#include "config/config_parser.h"
#include "config/config_types.h"
#include <cstdio>
#include <stdexcept>
#include <fstream>

static void write_yaml(const char* path, const char* content) {
    std::ofstream f(path);
    f << content;
}

int main() {
    write_yaml("/tmp/cp_test_good.yaml",
        "gateway:\n"
        "  services:\n"
        "    - name: \"svc-alpha\"\n"
        "      listen_port: 8080\n"
        "      backend:\n"
        "        host: \"127.0.0.1\"\n"
        "        port: 3001\n"
        "    - name: \"svc-beta\"\n"
        "      listen_port: 8443\n"
        "      backend:\n"
        "        host: \"10.0.0.1\"\n"
        "        port: 4001\n"
    );

    GatewayConfig cfg = ConfigParser::parse("/tmp/cp_test_good.yaml");

    if (cfg.services.size() == 2) {
        std::puts("PARSE_COUNT:OK");
    } else {
        std::puts("PARSE_COUNT:FAIL");
    }

    if (cfg.services[0].name == "svc-alpha" && cfg.services[0].listen_port == 8080) {
        std::puts("PARSE_SVC_ALPHA:OK");
    } else {
        std::puts("PARSE_SVC_ALPHA:FAIL");
    }

    if (cfg.services[0].backend.host == "127.0.0.1" && cfg.services[0].backend.port == 3001) {
        std::puts("PARSE_BACKEND_ALPHA:OK");
    } else {
        std::puts("PARSE_BACKEND_ALPHA:FAIL");
    }

    if (cfg.services[1].name == "svc-beta" && cfg.services[1].listen_port == 8443) {
        std::puts("PARSE_SVC_BETA:OK");
    } else {
        std::puts("PARSE_SVC_BETA:FAIL");
    }

    if (cfg.services[1].backend.host == "10.0.0.1" && cfg.services[1].backend.port == 4001) {
        std::puts("PARSE_BACKEND_BETA:OK");
    } else {
        std::puts("PARSE_BACKEND_BETA:FAIL");
    }

    try {
        ConfigParser::parse("/tmp/nonexistent_gateway_xyz.yaml");
        std::puts("FILE_NOT_FOUND:FAIL");
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        if (msg.find("not found") != std::string::npos) {
            std::puts("FILE_NOT_FOUND:OK");
        } else {
            std::puts("FILE_NOT_FOUND:FAIL");
        }
    }

    write_yaml("/tmp/cp_test_bad.yaml", "gateway: [[[invalid\n");
    try {
        ConfigParser::parse("/tmp/cp_test_bad.yaml");
        std::puts("MALFORMED_YAML:FAIL");
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        if (msg.find("malformed") != std::string::npos) {
            std::puts("MALFORMED_YAML:OK");
        } else {
            std::puts("MALFORMED_YAML:FAIL");
        }
    }

    write_yaml("/tmp/cp_test_empty.yaml", "gateway:\n  services:\n");
    GatewayConfig empty_cfg = ConfigParser::parse("/tmp/cp_test_empty.yaml");
    if (empty_cfg.services.empty()) {
        std::puts("EMPTY_SERVICES:OK");
    } else {
        std::puts("EMPTY_SERVICES:FAIL");
    }

    return 0;
}
EOF

if [ -d "$YAML_CPP_INCLUDE" ] && [ -n "$YAML_CPP_LIB" ] && [ -f "$YAML_CPP_LIB" ]; then
    g++ -std=c++17 -Wall -Wextra \
        -I"$PROJECT_ROOT/src" \
        -I"$YAML_CPP_INCLUDE" \
        "$PROJECT_ROOT/src/config/config_parser.cpp" \
        "$CONFIG_PARSER_TEST_SRC" \
        "$YAML_CPP_LIB" \
        -o "$CONFIG_PARSER_TEST_BIN" 2>/tmp/config_parser_compile_errors.txt
    assert_exit_code $? 0 "config_parser.cpp compiles cleanly"

    if [ -s /tmp/config_parser_compile_errors.txt ]; then
        echo "  Compiler output:"
        cat /tmp/config_parser_compile_errors.txt
    fi

    parser_output=$("$CONFIG_PARSER_TEST_BIN" 2>&1) || true

    assert_contains "$parser_output" "PARSE_COUNT:OK"          "parse() loads the correct number of services"
    assert_contains "$parser_output" "PARSE_SVC_ALPHA:OK"      "parse() reads service name and listen_port for svc-alpha"
    assert_contains "$parser_output" "PARSE_BACKEND_ALPHA:OK"  "parse() reads backend host and port for svc-alpha"
    assert_contains "$parser_output" "PARSE_SVC_BETA:OK"       "parse() reads service name and listen_port for svc-beta"
    assert_contains "$parser_output" "PARSE_BACKEND_BETA:OK"   "parse() reads backend host and port for svc-beta"
    assert_contains "$parser_output" "FILE_NOT_FOUND:OK"       "parse() throws with 'not found' for missing file"
    assert_contains "$parser_output" "MALFORMED_YAML:OK"       "parse() throws with 'malformed' for invalid YAML"
    assert_contains "$parser_output" "EMPTY_SERVICES:OK"       "parse() returns empty GatewayConfig when services list is empty"
else
    assert_exit_code 1 0 "yaml-cpp headers and static library found (required for config_parser tests)"
    echo "    Note: run cmake first so FetchContent downloads yaml-cpp"
fi

print_summary "Phase 0"