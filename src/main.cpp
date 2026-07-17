// main.cpp - gateway entry point: uses epoll event loop to accept multiple clients
#include <cstdio>

#include "core/event_loop.h"

static constexpr uint16_t LISTEN_PORT = 8080;

int main() {
    EventLoop loop;
    if (!loop.init(LISTEN_PORT)) {
        std::fprintf(stderr, "failed to initialise event loop on port %u\n", LISTEN_PORT);
        return 1;
    }
    loop.run();
    std::fprintf(stdout, "gateway shut down.\n");
    return 0;
}
