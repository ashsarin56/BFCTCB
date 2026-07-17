// main.cpp - gateway entry point: listens on 8080, accepts one client, logs IP
#include <cstdio>

#include "core/socket_utils.h"

static constexpr uint16_t LISTEN_PORT = 8080;

int main() {
    fd_t listener_fd = create_listener(LISTEN_PORT);
    if (listener_fd == INVALID_FD) {
        std::fprintf(stderr, "failed to create listener on port %u\n", LISTEN_PORT);
        return 1;
    }

    std::fprintf(stdout, "gateway listening on port %u...\n", LISTEN_PORT);
    std::fflush(stdout);

    std::string client_ip;
    fd_t client_fd = accept_client(listener_fd, client_ip);
    if (client_fd == INVALID_FD) {
        std::fprintf(stderr, "failed to accept client\n");
        close_fd(listener_fd);
        return 1;
    }

    std::fprintf(stdout, "client connected: %s\n", client_ip.c_str());
    std::fflush(stdout);

    close_fd(client_fd);
    close_fd(listener_fd);

    std::fprintf(stdout, "gateway shut down.\n");
    return 0;
}
