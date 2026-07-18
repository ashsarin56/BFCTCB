// main.cpp - gateway entry point: builds a Router and runs the epoll event loop
#include <cstdio>
#include <iostream>
#include <csignal>

#include "core/event_loop.h"
#include "routing/router.h"
#include "config/config_parser.h"
static EventLoop* g_loop = nullptr;
void signal_handler(int) {
    if (g_loop) {
        g_loop->shutdown();
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
    EventLoop loop;
    g_loop = &loop;
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    if (!loop.init(router)) {
        std::fprintf(stderr, "failed to initialise event loop\n");
        return 1;
    }
    loop.run();
    std::fprintf(stdout, "gateway shut down.\n");
    return 0;
}
