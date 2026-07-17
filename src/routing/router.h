#pragma once
// router.h — Route table mapping listen ports to backend service targets.
#include <cstdint>
#include <string>
#include <unordered_map>

// Describes a backend service the gateway can forward traffic to.
struct ServiceTarget {
    std::string name;  // for logging
    std::string host;  // Backend host 
    uint16_t    port;  // Backend port
};


class Router {
public:
    // Add a route 
    void add_route(uint16_t listen_port, ServiceTarget target);

    // Resolve a listen port to its backend target.
    const ServiceTarget* resolve(uint16_t listen_port) const;

    // Returns the full route table (used by the event loop)
    const std::unordered_map<uint16_t, ServiceTarget>& get_routes() const;

private:
    std::unordered_map<uint16_t, ServiceTarget> routes_;
};
