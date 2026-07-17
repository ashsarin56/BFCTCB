// router_test_main.cpp - standalone unit tests for the Router class.
// Each test prints TOKEN:OK or TOKEN:FAIL so test_phase0.sh can assert on them.
#include <cstdio>
#include "routing/router.h"

int main() {
    Router r;
    r.add_route(8080, {"web-service", "192.168.1.10", 3001});
    r.add_route(8443, {"api-service", "10.0.0.5",    4001});

    // RESOLVE_KNOWN: resolve() finds a registered port
    const ServiceTarget* t = r.resolve(8080);
    if (t != nullptr) {
        std::puts("RESOLVE_KNOWN:OK");
    } else {
        std::puts("RESOLVE_KNOWN:FAIL");
    }

    // RESOLVE_HOST: resolve() returns the correct backend host
    if (t != nullptr && t->host == "192.168.1.10") {
        std::puts("RESOLVE_HOST:OK");
    } else {
        std::puts("RESOLVE_HOST:FAIL");
    }

    // RESOLVE_NAME: resolve() returns the correct service name
    if (t != nullptr && t->name == "web-service") {
        std::puts("RESOLVE_NAME:OK");
    } else {
        std::puts("RESOLVE_NAME:FAIL");
    }

    // RESOLVE_UNKNOWN: resolve() returns nullptr for an unregistered port
    const ServiceTarget* miss = r.resolve(9999);
    if (miss == nullptr) {
        std::puts("RESOLVE_UNKNOWN:OK");
    } else {
        std::puts("RESOLVE_UNKNOWN:FAIL");
    }

    // ROUTE_COUNT: get_routes() returns the correct number of entries
    if (r.get_routes().size() == 2) {
        std::puts("ROUTE_COUNT:OK");
    } else {
        std::puts("ROUTE_COUNT:FAIL");
    }

    // OVERWRITE: add_route() overwrites an existing entry for the same port
    r.add_route(8080, {"new-service", "10.10.10.10", 5001});
    const ServiceTarget* overwritten = r.resolve(8080);
    if (overwritten != nullptr && overwritten->name == "new-service") {
        std::puts("OVERWRITE:OK");
    } else {
        std::puts("OVERWRITE:FAIL");
    }

    return 0;
}
