#pragma once
#include "router.h"
#include <memory>
#include <vector>

class LoadBalancer {
public:
    BackendInstance* choose_server(const std::vector<std::shared_ptr<BackendInstance>>& pool);
};
