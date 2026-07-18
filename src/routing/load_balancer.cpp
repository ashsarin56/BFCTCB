#include "load_balancer.h"
#include <random>
#include <vector>

BackendInstance* LoadBalancer::choose_server(const std::vector<std::shared_ptr<BackendInstance>>& pool) {
    std::vector<int> healthy_indices;
    for (int i = 0; i < static_cast<int>(pool.size()); ++i) {
        if (pool[i]->is_healthy.load())healthy_indices.push_back(i);
    }

    if (healthy_indices.empty())return nullptr;
    if (healthy_indices.size() == 1)return pool[healthy_indices[0]].get();
    thread_local std::mt19937 rng(std::random_device{}());
    int size = static_cast<int>(healthy_indices.size());
    std::uniform_int_distribution<int> dist_first(0, size - 1);
    int idx1 = dist_first(rng);

    std::uniform_int_distribution<int> dist_second(0, size - 2);
    int idx2 = dist_second(rng);
    if (idx2 >= idx1)idx2 += 1;

    int a = healthy_indices[idx1];
    int b = healthy_indices[idx2];

    if (pool[a]->active_connections.load() <= pool[b]->active_connections.load())return pool[a].get();
    return pool[b].get();
}
