// main.cpp - gateway entry point: builds a Router and runs the epoll event loop
#include <cstdio>
#include <iostream>
#include <csignal>

#include "core/event_loop.h"
#include "routing/router.h"
#include "routing/load_balancer.h"
#include "config/config_parser.h"
#include <thread>
#include <vector>
#include <atomic>
#include <memory>
#include "core/health_checker.h"
static std::vector<EventLoop*> g_loops;
static std::atomic<bool> g_running{true};
static HealthChecker* g_health_checker = nullptr;
void signal_handler(int sig) {
    if (sig == SIGHUP) {
        for (auto* loop : g_loops) {
            if (loop) loop->request_reload();
        }
    } else {
        g_running.store(false);
        if (g_health_checker) {
            g_health_checker->stop();
        }
        for (auto* loop : g_loops) {
            if (loop) loop->shutdown();
        }
    }
}
int main(int argc, char** argv) {
    std::string config_path = "config/gateway.yaml";
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        }
    }
    GatewayConfig config;
    try {
        config = ConfigParser::parse(config_path);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing config: " << e.what() << std::endl;
        return 1;
    }
    Router router = Router::from_config(config);
    LoadBalancer load_balancer;
    
    HealthChecker health_checker(router, 5);
    if (!health_checker.init()) {
        std::fprintf(stderr, "failed to initialise health checker\n");
        return 1;
    }
    g_health_checker = &health_checker;
    std::thread health_thread([&health_checker]() {
        health_checker.run();
    });
    
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);
    int num_workers = std::thread::hardware_concurrency();
    if (num_workers == 0) num_workers = 1;
    std::vector<std::unique_ptr<EventLoop>> loops;
    std::vector<std::thread> threads;
    for (int i = 0; i < num_workers; ++i) {
        auto loop = std::make_unique<EventLoop>();
        if (!loop->init(config_path, router, load_balancer)) {
            std::fprintf(stderr, "failed to initialise event loop\n");
            return 1;
        }
        g_loops.push_back(loop.get());
        threads.emplace_back([l = loop.get()]() {
            l->run();
        });
        loops.push_back(std::move(loop));
    }
    std::fprintf(stdout, "gateway started with %d worker threads\n", num_workers);

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    if (g_health_checker) {
        g_health_checker->stop();
    }
    if (health_thread.joinable()) {
        health_thread.join();
    }
    
    std::fprintf(stdout, "gateway shut down.\n");
    return 0;
}
