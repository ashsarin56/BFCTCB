// router.cpp — Implementation of the Router route table.
#include "router.h"

void Router::add_route(uint16_t listen_port, ServiceTarget target) {
    // Inserting with [] overwrites any previously registered route for this port (last write wins).
    routes_[listen_port] = std::move(target);
}

const ServiceTarget* Router::resolve(uint16_t listen_port) const {
    auto it = routes_.find(listen_port);
    if (it == routes_.end()) {
        return nullptr;  // No route registered for this port
    }
    return &it->second;
}

const std::unordered_map<uint16_t, ServiceTarget>& Router::get_routes() const {
    return routes_;
}
