// proxy_session.cpp - bidirectional forwarding loop using select with half-close support
#include "proxy_session.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include "common/types.h"

static bool forward_data(fd_t from_fd, fd_t to_fd) {
    uint8_t buf[BUFFER_SIZE];
    ssize_t n = read(from_fd, buf, sizeof(buf));
    if (n == 0) {
        // sender closed its write end
        shutdown(to_fd, SHUT_WR);
        return false;
    }
    if (n < 0) {
        if (errno == EINTR)return true;
        std::fprintf(stderr, "read() failed: %s\n", std::strerror(errno));
        return false;
    }
    ssize_t written = 0;
    while (written < n) {
        ssize_t w = write(to_fd, buf + written, static_cast<size_t>(n - written));
        if (w < 0) {
            if (errno == EINTR)continue;
            std::fprintf(stderr, "write() failed: %s\n", std::strerror(errno));
            return false;
        }
        written += w;
    }
    return true;
}
void run_bidirectional(ProxySession& session) {
    fd_t max_fd = session.client_fd;
    if (session.backend_fd > max_fd) max_fd = session.backend_fd;
    bool client_open = true,backend_open = true;
    while (client_open || backend_open) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        if (client_open)FD_SET(session.client_fd, &read_fds);
        if (backend_open)FD_SET(session.backend_fd, &read_fds);
        int ready = select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);
        if (ready < 0) {
            if (errno == EINTR)continue;
            std::fprintf(stderr, "select() failed: %s\n", std::strerror(errno));
            break;
        }
        if (client_open && FD_ISSET(session.client_fd, &read_fds)){
            //client→backend
            client_open = forward_data(session.client_fd, session.backend_fd);
        }
        if (backend_open && FD_ISSET(session.backend_fd, &read_fds)){
            //backend → client
            backend_open = forward_data(session.backend_fd, session.client_fd);
        }
    }
}
