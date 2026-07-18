// event_loop.cpp - epoll event loop: one listener per route, routes clients to the correct backend
#include "event_loop.h"
#include "socket_utils.h"
#include "data_forwarder.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

static constexpr int MAX_EVENTS = 64;

EventLoop::EventLoop()
    : epoll_fd_(INVALID_FD), running_(false), router_(nullptr) {}

EventLoop::~EventLoop() {
    for (auto& pair : connections_) {
        close_fd(pair.first);
    }
    connections_.clear();
    for (auto& pair : listeners_) {
        close_fd(pair.first);
    }
    listeners_.clear();
    if (epoll_fd_ != INVALID_FD) {
        close_fd(epoll_fd_);
    }
}

bool EventLoop::init(const Router& router) {
    router_ = &router;

    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == INVALID_FD) {
        std::fprintf(stderr, "event_loop: epoll_create1 failed: %s\n", std::strerror(errno));
        return false;
    }

    for (const auto& entry : router.get_routes()) {
        uint16_t port = entry.first;
        const ServiceTarget& target = entry.second;

        fd_t listener_fd = create_listener(port);
        if (listener_fd == INVALID_FD) {
            std::fprintf(stderr, "event_loop: failed to create listener on port %u\n", port);
            return false;
        }
        if (!set_nonblocking(listener_fd)) {
            std::fprintf(stderr, "event_loop: failed to set listener non-blocking on port %u\n", port);
            close_fd(listener_fd);
            return false;
        }
        struct epoll_event ev{};
        ev.events  = EPOLLIN;
        ev.data.fd = listener_fd;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listener_fd, &ev) < 0) {
            std::fprintf(stderr, "event_loop: epoll_ctl add listener failed for port %u: %s\n",
                         port, std::strerror(errno));
            close_fd(listener_fd);
            return false;
        }
        listeners_[listener_fd] = port;
        if (!target.backends.empty()) {
            std::fprintf(stdout, "gateway listening on port %u -> %s:%u (%s)\n", port, target.backends[0]->host.c_str(), target.backends[0]->port, target.name.c_str());
        } else {
            std::fprintf(stdout, "gateway listening on port %u (%s, no backends)\n", port, target.name.c_str());
        }
        std::fflush(stdout);
    }
    return true;
}

void EventLoop::run() {
    running_ = true;
    struct epoll_event events[MAX_EVENTS];
    while (running_) {
        int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::fprintf(stderr, "event_loop: epoll_wait failed: %s\n", std::strerror(errno));
            break;
        }
        for (int i = 0; i < n; ++i) {
            fd_t fd = events[i].data.fd;
            uint32_t ev = events[i].events;
            if (listeners_.count(fd)) {
                handle_accept(fd);
                continue;
            }
            if (ev & EPOLLIN) {
                handle_read(fd);
            }
            if (connections_.count(fd) == 0) {
                continue;
            }
            if (ev & EPOLLOUT) {
                handle_write(fd);
            }
            if (connections_.count(fd) == 0) {
                continue;
            }
            if (ev & (EPOLLERR | EPOLLHUP)) {
                handle_disconnect(fd);
            }
        }
    }
}

void EventLoop::shutdown() {
    running_ = false;
}

void EventLoop::handle_accept(fd_t listener_fd) {
    auto lit = listeners_.find(listener_fd);
    if (lit == listeners_.end()) {
        return;
    }
    uint16_t listen_port = lit->second;
    const ServiceTarget* target = router_->resolve(listen_port);
    if (target == nullptr) {
        std::fprintf(stderr, "event_loop: no route for port %u\n", listen_port);
        return;
    }
    while (true) {
        std::string client_ip;
        fd_t client_fd = accept_client(listener_fd, client_ip);
        if (client_fd == INVALID_FD) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            std::fprintf(stderr, "event_loop: accept_client failed: %s\n", std::strerror(errno));
            break;
        }
        if (target->backends.empty()) {
            std::fprintf(stderr, "event_loop: no backends configured for port %u\n", listen_port);
            close_fd(client_fd);
            continue;
        }
        BackendInstance* chosen = target->backends[0].get();
        std::fprintf(stdout, "client connected: %s -> %s:%u\n",
                     client_ip.c_str(), chosen->host.c_str(), chosen->port);
        std::fflush(stdout);

        fd_t backend_fd = connect_to_backend(chosen->host, chosen->port);
        if (backend_fd == INVALID_FD) {
            std::fprintf(stderr, "event_loop: backend connection failed, dropping client\n");
            close_fd(client_fd);
            continue;
        }
        if (!set_nonblocking(client_fd) || !set_nonblocking(backend_fd)) {
            std::fprintf(stderr, "event_loop: failed to set fds non-blocking\n");
            close_fd(client_fd);
            close_fd(backend_fd);
            continue;
        }
        auto conn = std::make_shared<Connection>();
        conn->client_fd  = client_fd;
        conn->backend_fd = backend_fd;

        if (!DataForwarder::init_pipes(conn.get())) {
            close_fd(client_fd);
            close_fd(backend_fd);
            continue;
        }
        struct epoll_event ev{};
        ev.events  = EPOLLIN;
        ev.data.fd = client_fd;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
            std::fprintf(stderr, "event_loop: epoll_ctl add client_fd failed: %s\n", std::strerror(errno));
            close_fd(client_fd);
            close_fd(backend_fd);
            continue;
        }

        ev.data.fd = backend_fd;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, backend_fd, &ev) < 0) {
            std::fprintf(stderr, "event_loop: epoll_ctl add backend_fd failed: %s\n", std::strerror(errno));
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
            close_fd(client_fd);
            close_fd(backend_fd);
            continue;
        }
        connections_[client_fd]  = conn;
        connections_[backend_fd] = conn;
        std::fprintf(stdout, "paired client fd %d with backend fd %d\n", client_fd, backend_fd);
        std::fflush(stdout);
    }
}

void EventLoop::handle_read(fd_t fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) {
        return;
    }
    auto conn = it->second;

    fd_t peer_fd;
    fd_t pipe_write;
    fd_t pipe_read;
    size_t* pipe_bytes;

    if (fd == conn->client_fd) {
        peer_fd   = conn->backend_fd;
        pipe_write = conn->pipe_c2b[1];
        pipe_read = conn->pipe_c2b[0];
        pipe_bytes = &conn->c2b_pipe_bytes;
    } else {
        peer_fd   = conn->client_fd;
        pipe_write = conn->pipe_b2c[1];
        pipe_read = conn->pipe_b2c[0];
        pipe_bytes = &conn->b2c_pipe_bytes;
    }

    ssize_t bytes = splice(fd, nullptr, pipe_write, nullptr, BUFFER_SIZE, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        remove_connection(fd);
        return;
    }
    if (bytes == 0) {
        if (fd == conn->client_fd) {
            while (*pipe_bytes > 0) {
                ssize_t w = splice(pipe_read, nullptr, peer_fd, nullptr, *pipe_bytes, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
                if (w <= 0) break;
                *pipe_bytes -= w;
            }
            ::shutdown(peer_fd, SHUT_WR);
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
        } else {
            while (*pipe_bytes > 0) {
                ssize_t w = splice(pipe_read, nullptr, peer_fd, nullptr, *pipe_bytes, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
                if (w <= 0) break;
                *pipe_bytes -= w;
            }
            remove_connection(fd);
        }
        return;
    }
    *pipe_bytes += bytes;

    ssize_t w = splice(pipe_read, nullptr, peer_fd, nullptr, *pipe_bytes, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
    if (w > 0) {
        *pipe_bytes -= w;
    } else if (w < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        remove_connection(fd);
        return;
    }
    if (*pipe_bytes > 0) {
        struct epoll_event ev{};
        ev.events  = EPOLLIN | EPOLLOUT;
        ev.data.fd = peer_fd;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, peer_fd, &ev) < 0) {
            if (errno == ENOENT) {
                ev.events = EPOLLOUT;
                epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, peer_fd, &ev);
            }
        }
    }
}

void EventLoop::handle_write(fd_t fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) {
        return;
    }
    auto conn = it->second;

    fd_t pipe_read;
    size_t* pipe_bytes;

    if (fd == conn->client_fd) {
        pipe_read = conn->pipe_b2c[0];
        pipe_bytes = &conn->b2c_pipe_bytes;
    } else {
        pipe_read = conn->pipe_c2b[0];
        pipe_bytes = &conn->c2b_pipe_bytes;
    }

    if (*pipe_bytes == 0) {
        struct epoll_event ev{};
        ev.events  = EPOLLIN;
        ev.data.fd = fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
        return;
    }

    ssize_t written = splice(pipe_read, nullptr, fd, nullptr, *pipe_bytes, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
    if (written < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        remove_connection(fd);
        return;
    }
    
    *pipe_bytes -= written;
    if (*pipe_bytes == 0) {
        struct epoll_event ev{};
        ev.events  = EPOLLIN;
        ev.data.fd = fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
    }
}

void EventLoop::handle_disconnect(fd_t fd) {
    remove_connection(fd);
}

void EventLoop::remove_connection(fd_t fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) {
        return;
    }
    auto conn = it->second;
    
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, conn->client_fd, nullptr);
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, conn->backend_fd, nullptr);
    close_fd(conn->client_fd);
    close_fd(conn->backend_fd);
    DataForwarder::close_pipes(conn.get());
    connections_.erase(conn->client_fd);
    connections_.erase(conn->backend_fd);
}
